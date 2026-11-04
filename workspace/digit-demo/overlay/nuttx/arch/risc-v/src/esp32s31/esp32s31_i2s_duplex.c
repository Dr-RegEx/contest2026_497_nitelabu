/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_i2s_duplex.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/atomic.h>
#include <nuttx/audio/audio.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#ifdef CONFIG_ARCH_ADDRENV
#  include <nuttx/addrenv.h>
#  include <nuttx/sched.h>
#endif
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <errno.h>
#include <debug.h>
#include <stdint.h>
#include <string.h>
#ifdef CONFIG_ESP32S31_FLASH_AUDIO_TRACE
#  include <stdio.h>
#  include <nuttx/sched.h>
#  include <nuttx/signal.h>
#  include "riscv_internal.h"
#endif
#include "esp32s31_i2s.h"
#include "esp_irq.h"
#include "espressif/esp_gpio.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_clk.h"
#include "hal/ahb_dma_ll.h"
#include "hal/dma_types.h"
#include "hal/gdma_ll.h"
#include "hal/i2s_hal.h"
#include "hal/gdma_channel.h"
#include "soc/gpio_sig_map.h"
#include "soc/interrupts.h"

/* Deliberately isolated from the half-duplex driver. RX owns the continuous
 * 48 kHz clock, including while no capture buffer is queued. TX is a slave.
 * All DMA memory is internal SRAM. Each descriptor occupies its own cache
 * line; this implementation additionally requires non-cacheable SRAM.
 */

#define DUP_DMA AHB_DMA_LL_GET_HW(0)
#define DUP_BLOCK 512
#define DUP_SLOTS 4
#define DUP_LIMIT 8
#define DUP_RX_SOURCE ETS_AHB_PDMA_IN_CH0_INTR_SOURCE
#define DUP_TX_SOURCE ETS_AHB_PDMA_OUT_CH0_INTR_SOURCE
#define DUP_RX_ERRORS (GDMA_LL_EVENT_RX_DESC_ERROR | \
                       GDMA_LL_EVENT_RX_ERR_EOF | \
                       GDMA_LL_EVENT_RX_DESC_EMPTY | GDMA_LL_EVENT_RX_FIFO_OVF)
#define DUP_TX_ERRORS (GDMA_LL_EVENT_TX_DESC_ERROR | GDMA_LL_EVENT_TX_FIFO_UDF)

#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
#  define DUP_RATE 16000u
#elif defined(CONFIG_ESP32S31_I2S_DUPLEX_44K)
#  define DUP_RATE 44100u
#else
#  define DUP_RATE 48000u
#endif
#define DUP_MCLK (DUP_RATE * 256u)
#define DUP_BYTES_PER_SECOND (DUP_RATE * 4u)

struct dup_request_s
{
  struct dup_request_s *next;
  struct ap_buffer_s *apb;
  i2s_callback_t callback;
  void *arg;
  size_t bytes;
  size_t scheduled;
  size_t completed;
  clock_t queued;
  clock_t queue_timeout;
  clock_t started;
  clock_t timeout;
  bool started_valid;
  int result;
#ifdef CONFIG_ARCH_ADDRENV
  struct addrenv_s *addrenv;
#endif
};

struct dup_endpoint_s
{
  struct i2s_dev_s dev;
  struct dup_request_s *head;
  struct dup_request_s *tail;
  struct dup_request_s *slotreq[DUP_SLOTS];
  size_t slotbytes[DUP_SLOTS];
  unsigned int pending;
  unsigned int index;
  bool rx;
  bool active;
  bool dma_running;
  atomic_t errors;
};

struct dup_desc_s
{
  dma_descriptor_t desc;
  uint8_t padding[64 - sizeof(dma_descriptor_t)];
} __attribute__((aligned(64)));

static struct dup_endpoint_s g_ep[2]; /* 0: playback, 1: capture */
static i2s_hal_context_t g_hal;
static mutex_t g_lock = NXMUTEX_INITIALIZER;
static sem_t g_wake;
static bool g_ready;
#ifdef CONFIG_ESP32S31_FLASH_AUDIO_TRACE
volatile bool g_esp32s31_audio_trace_active;

/* One-shot diagnostic: copy task state under scheduler protection and print
 * only after traversal. This observer sleeps while playback starts.
 */

static struct
{
  pid_t pid;
  int priority;
  int state;
  int locks;
  uintptr_t pc;
  char name[32];
} g_trace_tasks[32];
static unsigned int g_trace_ntasks;

static void dup_trace_task(struct tcb_s *tcb, void *arg)
{
  unsigned int n = g_trace_ntasks;

  if (n < 32)
    {
      g_trace_tasks[n].pid = tcb->pid;
      g_trace_tasks[n].priority = tcb->sched_priority;
      g_trace_tasks[n].state = tcb->task_state;
      g_trace_tasks[n].locks = tcb->lockcount;
      g_trace_tasks[n].pc = tcb->xcp.regs ? tcb->xcp.regs[REG_EPC] : 0;
      strlcpy(g_trace_tasks[n].name, tcb->name,
              sizeof(g_trace_tasks[n].name));
      g_trace_ntasks++;
    }
}

static int dup_trace_observer(int argc, char *argv[])
{
  while (!g_esp32s31_audio_trace_active)
    {
      nxsig_usleep(100000);
    }

  nxsig_usleep(10000000);
  nxsched_foreach(dup_trace_task, NULL);
  for (unsigned int n = 0; n < g_trace_ntasks; n++)
    {
      printf("\nTASKTRACE pid=%d prio=%d state=%d locks=%d pc=%08lx %s\n",
             g_trace_tasks[n].pid, g_trace_tasks[n].priority,
             g_trace_tasks[n].state, g_trace_tasks[n].locks,
             (unsigned long)g_trace_tasks[n].pc, g_trace_tasks[n].name);
    }

  return 0;
}
#endif
static DRAM_ATTR struct dup_desc_s g_desc[2][DUP_SLOTS];
static DRAM_ATTR uint8_t g_data[2][DUP_SLOTS][DUP_BLOCK]
  __attribute__((aligned(64)));

static void dup_stop_dma(struct dup_endpoint_s *ep)
{
  if (ep->rx)
    {
      ahb_dma_ll_rx_enable_interrupt(DUP_DMA, 0, UINT32_MAX, false);
      i2s_ll_rx_stop(g_hal.dev);
      ahb_dma_ll_rx_stop(DUP_DMA, 0);
      ahb_dma_ll_rx_reset_channel(DUP_DMA, 0);
    }
  else
    {
      esp32s31_audio_pa(false);
      ahb_dma_ll_tx_enable_interrupt(DUP_DMA, 0, UINT32_MAX, false);
      i2s_ll_tx_stop(g_hal.dev);
      ahb_dma_ll_tx_stop(DUP_DMA, 0);
      ahb_dma_ll_tx_reset_channel(DUP_DMA, 0);
    }

  ep->dma_running = false;
#ifdef CONFIG_ESP32S31_FLASH_AUDIO_TRACE
  if (!g_ep[0].dma_running && !g_ep[1].dma_running)
    {
      g_esp32s31_audio_trace_active = false;
    }
#endif
}

static void dup_cancel(struct dup_endpoint_s *ep, int result)
{
  struct dup_request_s *req;
  unsigned int i;

  ep->active = false;
  if (!ep->rx || !g_ep[0].active)
    {
      dup_stop_dma(ep);
    }

  for (i = 0; i < DUP_SLOTS; i++)
    {
      ep->slotreq[i] = NULL;
      ep->slotbytes[i] = 0;
    }

  for (req = ep->head; req != NULL; req = req->next)
    {
      req->result = result;
      req->completed = req->bytes;
    }

  if (!g_ep[0].active && !g_ep[1].active)
    {
      dup_stop_dma(&g_ep[1]);
    }
}

static int dup_irq(int irq, void *context, void *arg)
{
  struct dup_endpoint_s *ep = arg;
  uint32_t status;

  if (ep->rx)
    {
      status = ahb_dma_ll_rx_get_interrupt_status(DUP_DMA, 0, false);
      ahb_dma_ll_rx_clear_interrupt_status(DUP_DMA, 0, status);
      atomic_or(&ep->errors, status & DUP_RX_ERRORS);
    }
  else
    {
      status = ahb_dma_ll_tx_get_interrupt_status(DUP_DMA, 0, false);
      ahb_dma_ll_tx_clear_interrupt_status(DUP_DMA, 0, status);
      atomic_or(&ep->errors, status & DUP_TX_ERRORS);
    }

  nxsem_post(&g_wake);
  return OK;
}

static void dup_configure(void)
{
  i2s_hal_slot_config_t slot =
  {
    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
    .slot_mode = I2S_SLOT_MODE_STEREO,
    .std =
    {
      .slot_mask = I2S_STD_SLOT_BOTH,
      .ws_width = 16,
      .bit_shift = true,
#if defined(CONFIG_ESP32S31_I2S_DUPLEX_44K) || \
    defined(CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO)
      .left_align = true,
#endif
    },
  };
  i2s_hal_clock_info_t clk =
  {
    .sclk = esp_clk_xtal_freq(),
    .mclk = DUP_MCLK,
    .bclk = DUP_RATE * 32,
    .bclk_div = 8,
  };
  int __DECLARE_RCC_ATOMIC_ENV = 0;
  irqstate_t flags = enter_critical_section();

#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
  slot.slot_mode = I2S_SLOT_MODE_MONO;
  slot.std.slot_mask = I2S_STD_SLOT_LEFT;
#endif
  i2s_hal_std_set_rx_slot(&g_hal, false, &slot);
  i2s_hal_std_enable_rx_channel(&g_hal);
#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
  slot.slot_mode = I2S_SLOT_MODE_STEREO;
  slot.std.slot_mask = I2S_STD_SLOT_BOTH;
#endif
  i2s_hal_std_set_tx_slot(&g_hal, true, &slot);
  i2s_hal_std_enable_tx_channel(&g_hal);
  i2s_ll_share_bck_ws(g_hal.dev, true);
  /* Program the slave clock gate/divider too; RX is programmed last so
   * MCLK remains bound to the master, as in the pinned IDF duplex setup.
   */

  i2s_hal_set_tx_clock(&g_hal, &clk, I2S_CLK_SRC_XTAL, NULL);
  i2s_hal_set_rx_clock(&g_hal, &clk, I2S_CLK_SRC_XTAL, NULL);
  i2s_ll_rx_enable_recomb(g_hal.dev, false);
  leave_critical_section(flags);
  esp_gpio_matrix_out(52, I2S0_MCLK_PAD_OUT_IDX, false, false);
  esp_gpio_matrix_out(53, I2S0_I_BCK_PAD_OUT_IDX, false, false);
  esp_gpio_matrix_out(55, I2S0_I_WS_PAD_OUT_IDX, false, false);
}

static int dup_start_dma(struct dup_endpoint_s *ep)
{
  dma_descriptor_t *desc;
  unsigned int i;
  unsigned int retry;

  ep->index = 0;
  atomic_xchg(&ep->errors, 0);
  memset(g_data[ep->rx], 0, sizeof(g_data[ep->rx]));
  for (i = 0; i < DUP_SLOTS; i++)
    {
      desc = &g_desc[ep->rx][i].desc;
      memset(desc, 0, sizeof(*desc));
      desc->dw0.size = DUP_BLOCK;
      desc->dw0.length = ep->rx ? 0 : DUP_BLOCK;
      desc->dw0.suc_eof = !ep->rx;
      desc->buffer = g_data[ep->rx][i];
      desc->next = &g_desc[ep->rx][(i + 1) % DUP_SLOTS].desc;
      ep->slotreq[i] = NULL;
      ep->slotbytes[i] = 0;
      desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    }

  __sync_synchronize();
  desc = &g_desc[ep->rx][0].desc;
  if (ep->rx)
    {
      i2s_ll_rx_reset_fifo(g_hal.dev);
      ahb_dma_ll_rx_reset_channel(DUP_DMA, 0);
      ahb_dma_ll_rx_clear_interrupt_status(DUP_DMA, 0, UINT32_MAX);
      ahb_dma_ll_rx_set_desc_addr(DUP_DMA, 0, (uintptr_t)desc);
      i2s_ll_rx_set_eof_num(g_hal.dev, DUP_BLOCK);
      ahb_dma_ll_rx_enable_interrupt(DUP_DMA, 0,
        DUP_RX_ERRORS | GDMA_LL_EVENT_RX_SUC_EOF, true);
      ahb_dma_ll_rx_start(DUP_DMA, 0);
      g_hal.dev->rx_conf.rx_update = 1;
    }
  else
    {
      i2s_ll_tx_reset_fifo(g_hal.dev);
      ahb_dma_ll_tx_reset_channel(DUP_DMA, 0);
      ahb_dma_ll_tx_clear_interrupt_status(DUP_DMA, 0, UINT32_MAX);
      ahb_dma_ll_tx_set_desc_addr(DUP_DMA, 0, (uintptr_t)desc);
      ahb_dma_ll_tx_enable_interrupt(DUP_DMA, 0,
        DUP_TX_ERRORS | GDMA_LL_EVENT_TX_EOF, true);
      ahb_dma_ll_tx_start(DUP_DMA, 0);
      g_hal.dev->tx_conf.tx_update = 1;
    }

  for (retry = 0; retry < 10000; retry++)
    {
      if (!(ep->rx ? g_hal.dev->rx_conf.rx_update :
                    g_hal.dev->tx_conf.tx_update))
        {
          if (ep->rx)
            {
              g_hal.dev->rx_conf.rx_start = 1;
            }
          else
            {
              esp32s31_audio_pa(true);
              g_hal.dev->tx_conf.tx_start = 1;
            }

          ep->dma_running = true;
#ifdef CONFIG_ESP32S31_FLASH_AUDIO_TRACE
          g_esp32s31_audio_trace_active = true;
#endif
          return OK;
        }

      up_udelay(1);
    }

  dup_stop_dma(ep);
  return -ETIMEDOUT;
}

/* Only the worker touches request offsets and ring payloads, under g_lock.
 * A descriptor is returned to DMA only after its CPU payload work is done.
 */

static void dup_service(struct dup_endpoint_s *ep)
{
  struct dup_request_s *req;
  volatile dma_descriptor_t *desc;
  uint8_t *buffer;
  size_t amount;
  size_t offset;
  unsigned int count;
  unsigned int index;
#ifdef CONFIG_ARCH_ADDRENV
  struct addrenv_s *oldenv;
#endif

  if (!ep->dma_running)
    {
      return;
    }

  for (count = 0; count < DUP_SLOTS; count++)
    {
      index = ep->index;
      desc = &g_desc[ep->rx][index].desc;
      if (desc->dw0.owner != DMA_DESCRIPTOR_BUFFER_OWNER_CPU)
        {
          break;
        }

      __sync_synchronize();
      buffer = g_data[ep->rx][index];
      if (ep->rx)
        {
          if (desc->dw0.err_eof || desc->dw0.length != DUP_BLOCK)
            {
              auderr("RX descriptor: err_eof=%u length=%u\n",
                     desc->dw0.err_eof, desc->dw0.length);
              atomic_or(&ep->errors, DUP_RX_ERRORS);
              return;
            }

          offset = 0;
          for (req = ep->head; req != NULL && offset < DUP_BLOCK;
               req = req->next)
            {
              if (!ep->active || req->result != OK ||
                  req->completed == req->bytes)
                {
                  continue;
                }

              if (!req->started_valid)
                {
                  req->started = clock_systime_ticks();
                  req->started_valid = true;
                }

              amount = req->bytes - req->completed;
              if (amount > DUP_BLOCK - offset)
                {
                  amount = DUP_BLOCK - offset;
                }

#ifdef CONFIG_ARCH_ADDRENV
              DEBUGVERIFY(addrenv_select(req->addrenv, &oldenv));
#endif
              memcpy(req->apb->samp + req->completed, buffer + offset,
                     amount);
#ifdef CONFIG_ARCH_ADDRENV
              DEBUGVERIFY(addrenv_restore(oldenv));
#endif
              req->completed += amount;
              offset += amount;
            }
        }
      else
        {
          req = ep->slotreq[index];
          if (req != NULL)
            {
              req->completed += ep->slotbytes[index];
            }

          ep->slotreq[index] = NULL;
          ep->slotbytes[index] = 0;
          memset(buffer, 0, DUP_BLOCK);
          for (req = ep->head; req != NULL; req = req->next)
            {
              if (req->result == OK && req->scheduled < req->bytes)
                {
                  break;
                }
            }

          if (req != NULL)
            {
              if (!req->started_valid)
                {
                  req->started = clock_systime_ticks();
                  req->started_valid = true;
                }

              amount = req->bytes - req->scheduled;
              if (amount > DUP_BLOCK)
                {
                  amount = DUP_BLOCK;
                }

#ifdef CONFIG_ARCH_ADDRENV
              DEBUGVERIFY(addrenv_select(req->addrenv, &oldenv));
#endif
              memcpy(buffer, req->apb->samp + req->apb->curbyte +
                     req->scheduled, amount);
#ifdef CONFIG_ARCH_ADDRENV
              DEBUGVERIFY(addrenv_restore(oldenv));
#endif
              req->scheduled += amount;
              ep->slotreq[index] = req;
              ep->slotbytes[index] = amount;
            }
        }

      desc->dw0.length = ep->rx ? 0 : DUP_BLOCK;
      desc->dw0.err_eof = 0;
      __sync_synchronize();
      desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
      __sync_synchronize();
      ep->index = (index + 1) % DUP_SLOTS;
    }
}

static int dup_worker(int argc, char *argv[])
{
  struct dup_endpoint_s *ep;
  struct dup_request_s *req;
  struct dup_request_s *done;
  struct dup_request_s *last;
  unsigned int direction;
  uint32_t errors[2];

  for (;;)
    {
      nxsem_tickwait_uninterruptible(&g_wake, MSEC2TICK(5) + 1);
      nxmutex_lock(&g_lock);
      /* ISR publication and worker read/clear must use the same atomic
       * protocol.  A mutex cannot protect against an interrupt, including
       * one delivered on the other CPU.
       */

      errors[0] = atomic_xchg(&g_ep[0].errors, 0);
      errors[1] = atomic_xchg(&g_ep[1].errors, 0);
      if (errors[0] != 0 || errors[1] != 0)
        {
          auderr("DMA errors TX=%08lx RX=%08lx\n",
                 (unsigned long)errors[0], (unsigned long)errors[1]);
        }

      if (errors[1] != 0)
        {
          /* A failed clock master also invalidates its slave. */

          dup_cancel(&g_ep[0], -EIO);
          dup_cancel(&g_ep[1], -EIO);
        }
      else if (errors[0] != 0)
        {
          dup_cancel(&g_ep[0], -EIO);
        }

      dup_service(&g_ep[1]);
      dup_service(&g_ep[0]);
      for (direction = 0; direction < 2; direction++)
        {
          ep = &g_ep[direction];
          for (req = ep->head; req != NULL; req = req->next)
            {
              if (req->completed < req->bytes &&
                  ((req->started_valid &&
                    clock_systime_ticks() - req->started >= req->timeout) ||
                   (!req->started_valid &&
                    clock_systime_ticks() - req->queued >=
                    req->queue_timeout)))
                {
                  dup_cancel(ep, -ETIMEDOUT);
                  break;
                }
            }

          done = NULL;
          last = NULL;
          while (ep->head != NULL &&
                 ep->head->completed == ep->head->bytes)
            {
              req = ep->head;
              ep->head = req->next;
              ep->pending--;
              if (ep->head == NULL)
                {
                  ep->tail = NULL;
                }

              req->next = NULL;
              if (last == NULL)
                {
                  done = req;
                }
              else
                {
                  last->next = req;
                }

              last = req;
            }

          nxmutex_unlock(&g_lock);
          while (done != NULL)
            {
              req = done;
              done = req->next;
#ifdef CONFIG_ARCH_ADDRENV
              struct addrenv_s *oldenv;

              DEBUGVERIFY(addrenv_select(req->addrenv, &oldenv));
#endif
              if (req->result == OK)
                {
                  if (ep->rx)
                    {
                      req->apb->nbytes = req->bytes;
                      req->apb->curbyte = 0;
                    }
                  /* TX offsets are tracked in req. Leave the caller's
                   * cursor unchanged: the audio upper half recycles mmap
                   * periods by resetting nbytes, without resetting curbyte.
                   */
                }

              req->callback(&ep->dev, req->apb, req->arg, req->result);
              apb_free(req->apb);
#ifdef CONFIG_ARCH_ADDRENV
              DEBUGVERIFY(addrenv_restore(oldenv));
              addrenv_drop(req->addrenv, false);
#endif
              kmm_free(req);
            }

          nxmutex_lock(&g_lock);
        }

      nxmutex_unlock(&g_lock);
    }

  return OK;
}

static int dup_enqueue(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                       i2s_callback_t callback, void *arg, uint32_t timeout,
                       bool rx)
{
  struct dup_endpoint_s *ep = (struct dup_endpoint_s *)dev;
  struct dup_request_s *req;
  struct dup_request_s *prior;
  size_t ahead = 0;
  uint32_t byte_rate = DUP_BYTES_PER_SECOND;
#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
  if (rx)
    {
      byte_rate = DUP_RATE * 2u;
    }
#endif
  size_t bytes;
  int ret = OK;

  if (ep->rx != rx || apb == NULL || apb->samp == NULL ||
      callback == NULL || apb->curbyte > apb->nbytes ||
      apb->nbytes > apb->nmaxbytes)
    {
      return -EINVAL;
    }

  bytes = rx ? apb->nmaxbytes : apb->nbytes - apb->curbyte;
  /* The dedicated nxlooper profile uses 4096-byte buffers. Reject partial
   * ring blocks rather than silently pad a successful request with silence.
   */

  /* A zero-length TX FINAL is an ordered queue marker, not DMA data.
   * It is appended normally and retired by the worker only after every
   * earlier request completed. Callbacks remain outside g_lock.
   */

  if ((bytes == 0 && (rx || (apb->flags & AUDIO_APB_FINAL) == 0)) ||
      bytes > 32768 || (bytes % DUP_BLOCK) != 0)
    {
      auderr("Invalid %s buffer: curbyte=%u nbytes=%u max=%u flags=%x\n",
             rx ? "RX" : "TX", apb->curbyte, apb->nbytes,
             apb->nmaxbytes, apb->flags);
      return -EINVAL;
    }

  req = kmm_zalloc(sizeof(*req));
  if (req == NULL)
    {
      return -ENOMEM;
    }

  nxmutex_lock(&g_lock);
  if (ep->pending >= DUP_LIMIT)
    {
      ret = -EAGAIN;
    }
  else if (!ep->active)
    {
      ret = -ESHUTDOWN;
    }
  else
    {
#ifdef CONFIG_ARCH_ADDRENV
      req->addrenv = this_task()->addrenv_curr;
      addrenv_take(req->addrenv);
#endif
      req->apb = apb;
      req->callback = callback;
      req->arg = arg;
      req->bytes = bytes;
      req->queued = clock_systime_ticks();
      for (prior = ep->head; prior != NULL; prior = prior->next)
        {
          ahead += prior->bytes - prior->completed;
        }

      /* Include accepted work ahead of this request, plus one second for
       * clock progress. Eight maximum-size buffers are a legal queue.
       */

      req->queue_timeout = MSEC2TICK(1000 +
                           (ahead * 1000 + byte_rate - 1) / byte_rate);
      req->timeout = timeout != 0 ? timeout :
                     MSEC2TICK(1000 + bytes * 1000 / byte_rate);
      apb_reference(apb);
      if (ep->tail != NULL)
        {
          ep->tail->next = req;
        }
      else
        {
          ep->head = req;
        }

      ep->tail = req;
      ep->pending++;
      nxsem_post(&g_wake);
    }

  nxmutex_unlock(&g_lock);
  if (ret != OK)
    {
      kmm_free(req);
    }

  return ret;
}

static int dup_receive(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                       i2s_callback_t cb, void *arg, uint32_t timeout)
{
  return dup_enqueue(dev, apb, cb, arg, timeout, true);
}

static int dup_send(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                    i2s_callback_t cb, void *arg, uint32_t timeout)
{
  return dup_enqueue(dev, apb, cb, arg, timeout, false);
}

static int dup_channels(struct i2s_dev_s *dev, uint8_t channels)
{
#ifdef CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO
  struct dup_endpoint_s *ep = (struct dup_endpoint_s *)dev;
  return channels == (ep->rx ? 1 : 2) ? OK : -EINVAL;
#else
  return channels == 2 ? OK : -EINVAL;
#endif
}

static uint32_t dup_rate(struct i2s_dev_s *dev, uint32_t rate)
{
  return rate == DUP_RATE ? rate : 0;
}

static uint32_t dup_width(struct i2s_dev_s *dev, int bits)
{
  return bits == 16 ? 16 : 0;
}

static uint32_t dup_getmclk(struct i2s_dev_s *dev)
{
  return DUP_MCLK;
}

static uint32_t dup_setmclk(struct i2s_dev_s *dev, uint32_t frequency)
{
  return frequency == DUP_MCLK ? frequency : 0;
}

static int dup_ioctl(struct i2s_dev_s *dev, int cmd, unsigned long arg)
{
  struct dup_endpoint_s *ep = (struct dup_endpoint_s *)dev;
  struct audio_buf_desc_s *desc = (struct audio_buf_desc_s *)arg;
  int ret = OK;

  switch (cmd)
    {
      case AUDIOIOC_START:
        nxmutex_lock(&g_lock);
        if (ep->active || ep->pending != 0)
          {
            ret = -EBUSY;
          }
        else
          {
            ep->active = true;
            if (!g_ep[1].dma_running)
              {
                ret = dup_start_dma(&g_ep[1]);
              }

            if (ret == OK && !ep->rx)
              {
                ret = dup_start_dma(ep);
              }

            if (ret != OK)
              {
                dup_cancel(ep, ret);
              }
          }

        nxmutex_unlock(&g_lock);
        return ret;

      case AUDIOIOC_STOP:
      case AUDIOIOC_SHUTDOWN:
        nxmutex_lock(&g_lock);
        dup_cancel(ep, -ECANCELED);
        nxsem_post(&g_wake);
        nxmutex_unlock(&g_lock);
        return OK;

      case AUDIOIOC_ALLOCBUFFER:
        return desc == NULL ? -EINVAL : apb_alloc(desc);

      case AUDIOIOC_FREEBUFFER:
        if (desc == NULL || desc->u.buffer == NULL)
          {
            return -EINVAL;
          }

        apb_free(desc->u.buffer);
        return sizeof(*desc);

      default:
        return -ENOTTY;
    }
}

static const struct i2s_ops_s g_dup_ops =
{
  .i2s_rxchannels = dup_channels,
  .i2s_rxsamplerate = dup_rate,
  .i2s_rxdatawidth = dup_width,
  .i2s_receive = dup_receive,
  .i2s_txchannels = dup_channels,
  .i2s_txsamplerate = dup_rate,
  .i2s_txdatawidth = dup_width,
  .i2s_send = dup_send,
  .i2s_getmclkfrequency = dup_getmclk,
  .i2s_setmclkfrequency = dup_setmclk,
  .i2s_ioctl = dup_ioctl,
};

struct i2s_dev_s *esp32s31_i2s_duplex_initialize(bool rx)
{
  int cpuint[2];
  int source[2] = {DUP_TX_SOURCE, DUP_RX_SOURCE};
  int direction;
  int ret;
  int pin;
  irqstate_t flags;

  if (g_ready)
    {
      return &g_ep[rx].dev;
    }

  /* DMA descriptors and payload must be in uncached internal SRAM. The S31
   * linker places DRAM_ATTR in this region, outside both flash/PSRAM caches.
   */

  if (!esp_ptr_internal(g_data) || !esp_ptr_dma_capable(g_data) ||
      !esp_ptr_internal(g_desc) || !esp_ptr_dma_capable(g_desc) ||
      esp_cache_msync(g_data, sizeof(g_data),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M) !=
                      ESP_ERR_NOT_SUPPORTED ||
      esp_cache_msync(g_desc, sizeof(g_desc),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M) !=
                      ESP_ERR_NOT_SUPPORTED)
    {
      return NULL;
    }

  nxsem_init(&g_wake, 0, 0);
  esp32s31_audio_pa(false);
  for (pin = 52; pin <= 56; pin++)
    {
      if (esp_configgpio(pin, pin == 54 ? INPUT : OUTPUT) < 0)
        {
          goto fail_sem;
        }
    }

  esp_gpio_matrix_in(54, I2S0_I_SD_PAD_IN_IDX, false);
  esp_gpio_matrix_out(56, I2S0_O_SD_PAD_OUT_IDX, false, false);
  flags = enter_critical_section();
  i2s_ll_enable_bus_clock(0, true);
  i2s_ll_reset_register(0);
  i2s_hal_init(&g_hal, 0);
  i2s_ll_enable_core_clock(g_hal.dev, true);
  gdma_ll_enable_bus_clock(0, true);
  ahb_dma_ll_force_enable_reg_clock(DUP_DMA, true);
  ahb_dma_ll_set_default_memory_range(DUP_DMA);
  ahb_dma_ll_rx_connect_to_periph(DUP_DMA, 0, SOC_GDMA_TRIG_PERIPH_I2S0CH0);
  ahb_dma_ll_tx_connect_to_periph(DUP_DMA, 0, SOC_GDMA_TRIG_PERIPH_I2S0CH0);
  ahb_dma_ll_rx_enable_owner_check(DUP_DMA, 0, true);
  ahb_dma_ll_tx_enable_owner_check(DUP_DMA, 0, true);
  ahb_dma_ll_tx_enable_auto_write_back(DUP_DMA, 0, true);
  ahb_dma_ll_tx_set_eof_mode(DUP_DMA, 0, 1);
  for (direction = 0; direction < 2; direction++)
    {
      g_ep[direction].rx = direction != 0;
      g_ep[direction].dev.ops = &g_dup_ops;
      dup_stop_dma(&g_ep[direction]);
    }

  leave_critical_section(flags);
  dup_configure();
  for (direction = 0; direction < 2; direction++)
    {
      cpuint[direction] = esp_setup_irq(source[direction], 1,
                                      ESP_IRQ_TRIGGER_LEVEL);
      if (cpuint[direction] < 0)
        {
          goto fail_irq;
        }

      ret = irq_attach(ESP_SOURCE2IRQ(source[direction]), dup_irq,
                       &g_ep[direction]);
      if (ret < 0)
        {
          esp_teardown_irq(source[direction], cpuint[direction]);
          goto fail_irq;
        }

      up_enable_irq(ESP_SOURCE2IRQ(source[direction]));
    }

  ret = kthread_create("s31_duplex",
                       CONFIG_ESP32S31_I2S_DUPLEX_WORKER_PRIORITY,
                       4096, dup_worker, NULL);
  if (ret < 0)
    {
      goto fail_irq;
    }

  g_ready = true;
#ifdef CONFIG_ESP32S31_FLASH_AUDIO_TRACE
  kthread_create("s31_observe", 254, 4096, dup_trace_observer, NULL);
#endif
  return &g_ep[rx].dev;

fail_irq:
  while (--direction >= 0)
    {
      up_disable_irq(ESP_SOURCE2IRQ(source[direction]));
      irq_detach(ESP_SOURCE2IRQ(source[direction]));
      esp_teardown_irq(source[direction], cpuint[direction]);
    }

fail_sem:
  nxsem_destroy(&g_wake);
  return NULL;
}
