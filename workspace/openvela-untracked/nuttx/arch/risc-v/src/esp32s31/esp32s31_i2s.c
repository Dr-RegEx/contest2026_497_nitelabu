/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_i2s.c
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>
#include <nuttx/audio/audio.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/spinlock.h>
#include <nuttx/signal.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
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

#if !defined(CONFIG_BUILD_FLAT) || defined(CONFIG_SMP)
#  error "The initial S31 I2S port requires single-core FLAT"
#endif

/* AHB GDMA pair 0 is reserved exclusively for this isolated audio profile.
 * The static buffers are in internal DRAM, never in the PSRAM user heap.
 * Separate cache lines prevent DMA invalidation from discarding CPU state.
 */

#define S31_DMA          AHB_DMA_LL_GET_HW(0)
#define S31_DMA_CHANNEL  0
#define S31_MCLK         12288000u
#define S31_MCLK_44K     11289600u
#define S31_BUFFER_SIZE  32768
#define S31_DESC_SIZE    4032
#define S31_QUEUE_LIMIT  8
#define S31_RX_SOURCE    ETS_AHB_PDMA_IN_CH0_INTR_SOURCE
#define S31_TX_SOURCE    ETS_AHB_PDMA_OUT_CH0_INTR_SOURCE
#define S31_RX_ERRORS    (GDMA_LL_EVENT_RX_DESC_ERROR | \
                          GDMA_LL_EVENT_RX_ERR_EOF | \
                          GDMA_LL_EVENT_RX_FIFO_OVF)
#define S31_TX_ERRORS    (GDMA_LL_EVENT_TX_DESC_ERROR | \
                          GDMA_LL_EVENT_TX_FIFO_UDF)

struct s31_request_s
{
  struct s31_request_s *next;
  struct ap_buffer_s *apb;
  i2s_callback_t callback;
  void *arg;
  uint32_t timeout;
  unsigned int generation;
  bool rx;
};

struct s31_i2s_s
{
  struct i2s_dev_s dev;
  i2s_hal_context_t hal;
  mutex_t lock;
  sem_t queued;
  sem_t done;
  struct s31_request_s *head;
  struct s31_request_s *tail;
  unsigned int pending;
  unsigned int generation;
  uint32_t rate;
  uint8_t channels;
  bool rx;
  bool ready;
  volatile int result;
};

static struct s31_i2s_s g_i2s;
static DRAM_ATTR uint8_t g_buffer[S31_BUFFER_SIZE]
  __attribute__((aligned(64)));
static DRAM_ATTR uint8_t g_descriptors[128]
  __attribute__((aligned(64)));

/* Non-cacheable internal SRAM needs no cache operation.  The pinned cache
 * API reports that case as ESP_ERR_NOT_SUPPORTED, rather than ESP_OK.
 */

static int s31_sync(void *buffer, size_t size, int flags)
{
  int ret = esp_cache_msync(buffer, size, flags);
  return ret == ESP_OK || ret == ESP_ERR_NOT_SUPPORTED ? 0 : -EIO;
}

static void s31_stop_hardware(struct s31_i2s_s *priv)
{
  ahb_dma_ll_rx_enable_interrupt(S31_DMA, S31_DMA_CHANNEL, UINT32_MAX, false);
  ahb_dma_ll_tx_enable_interrupt(S31_DMA, S31_DMA_CHANNEL, UINT32_MAX, false);
  i2s_ll_rx_stop(priv->hal.dev);
  i2s_ll_tx_stop(priv->hal.dev);
  ahb_dma_ll_rx_stop(S31_DMA, S31_DMA_CHANNEL);
  ahb_dma_ll_tx_stop(S31_DMA, S31_DMA_CHANNEL);
  ahb_dma_ll_rx_reset_channel(S31_DMA, S31_DMA_CHANNEL);
  ahb_dma_ll_tx_reset_channel(S31_DMA, S31_DMA_CHANNEL);
  esp32s31_audio_pa(false);
}

static int s31_interrupt(int irq, void *context, void *arg)
{
  struct s31_i2s_s *priv = arg;
  uint32_t status;
  uint32_t errors;
  uint32_t eof;

  if (irq == ESP_SOURCE2IRQ(S31_RX_SOURCE))
    {
      status = ahb_dma_ll_rx_get_interrupt_status(S31_DMA, 0, false);
      ahb_dma_ll_rx_clear_interrupt_status(S31_DMA, 0, status);
      errors = S31_RX_ERRORS;
      eof = GDMA_LL_EVENT_RX_SUC_EOF;
    }
  else
    {
      status = ahb_dma_ll_tx_get_interrupt_status(S31_DMA, 0, false);
      ahb_dma_ll_tx_clear_interrupt_status(S31_DMA, 0, status);
      errors = S31_TX_ERRORS;
      eof = GDMA_LL_EVENT_TX_TOTAL_EOF;
    }

  if (status & (errors | eof))
    {
      priv->result = (status & errors) ? -EIO : 0;
      ahb_dma_ll_rx_enable_interrupt(S31_DMA, 0, UINT32_MAX, false);
      ahb_dma_ll_tx_enable_interrupt(S31_DMA, 0, UINT32_MAX, false);
      nxsem_post(&priv->done);
    }

  return 0;
}

static uint32_t s31_mclk_for_rate(uint32_t rate)
{
  return rate == 44100 ? S31_MCLK_44K : S31_MCLK;
}

static void s31_configure(struct s31_i2s_s *priv, bool rx)
{
  uint32_t mclk = s31_mclk_for_rate(priv->rate);
  i2s_hal_slot_config_t slot =
  {
    .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
    .slot_mode = priv->channels == 1 ? I2S_SLOT_MODE_MONO :
                                     I2S_SLOT_MODE_STEREO,
    .std =
    {
      .slot_mask = rx && priv->channels == 1 ? I2S_STD_SLOT_LEFT :
                                              I2S_STD_SLOT_BOTH,
      .ws_width = 16,
      .bit_shift = true,
    },
  };
  i2s_hal_clock_info_t clk =
  {
    .sclk = esp_clk_xtal_freq(),
    .mclk = mclk,
    .bclk = priv->rate * 32,
    .bclk_div = mclk / (priv->rate * 32),
  };
  int __DECLARE_RCC_ATOMIC_ENV = 0;
  irqstate_t flags = enter_critical_section();

  i2s_ll_share_bck_ws(priv->hal.dev, false);
  if (rx)
    {
      i2s_hal_std_set_rx_slot(&priv->hal, false, &slot);
      i2s_hal_std_enable_rx_channel(&priv->hal);
      i2s_hal_set_rx_clock(&priv->hal, &clk, I2S_CLK_SRC_XTAL, NULL);
      i2s_ll_rx_enable_recomb(priv->hal.dev, false);
      i2s_ll_rx_reset_fifo(priv->hal.dev);
    }
  else
    {
      i2s_hal_std_set_tx_slot(&priv->hal, false, &slot);
      i2s_hal_std_enable_tx_channel(&priv->hal);
      i2s_hal_set_tx_clock(&priv->hal, &clk, I2S_CLK_SRC_XTAL, NULL);
      i2s_ll_tx_reset_fifo(priv->hal.dev);
    }

  leave_critical_section(flags);
  esp_gpio_matrix_out(52, I2S0_MCLK_PAD_OUT_IDX, false, false);
  esp_gpio_matrix_out(53, rx ? I2S0_I_BCK_PAD_OUT_IDX :
                             I2S0_O_BCK_PAD_OUT_IDX, false, false);
  esp_gpio_matrix_out(55, rx ? I2S0_I_WS_PAD_OUT_IDX :
                             I2S0_O_WS_PAD_OUT_IDX, false, false);
}

static int s31_start(struct s31_i2s_s *priv, bool rx, size_t bytes)
{
  dma_descriptor_t *desc = (dma_descriptor_t *)g_descriptors;
  size_t offset = 0;
  size_t count = (bytes + S31_DESC_SIZE - 1) / S31_DESC_SIZE;
  size_t i;
  unsigned int retry;

  memset(g_descriptors, 0, sizeof(g_descriptors));
  for (i = 0; i < count; i++)
    {
      size_t size = bytes - offset;
      if (size > S31_DESC_SIZE)
        {
          size = S31_DESC_SIZE;
        }

      desc[i].dw0.size = size;
      desc[i].dw0.length = rx ? 0 : size;
      desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
      desc[i].dw0.suc_eof = !rx && i + 1 == count;
      desc[i].buffer = g_buffer + offset;
      desc[i].next = i + 1 < count ? &desc[i + 1] : NULL;
      offset += size;
    }

  if (s31_sync(g_buffer, sizeof(g_buffer),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                      ESP_CACHE_MSYNC_FLAG_INVALIDATE) != 0 ||
      s31_sync(g_descriptors, sizeof(g_descriptors),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                      ESP_CACHE_MSYNC_FLAG_INVALIDATE) != 0)
    {
      return -EIO;
    }

  while (nxsem_trywait(&priv->done) == 0);
  priv->result = -EINPROGRESS;
  s31_configure(priv, rx);
  if (rx)
    {
      ahb_dma_ll_rx_reset_channel(S31_DMA, 0);
      ahb_dma_ll_rx_clear_interrupt_status(S31_DMA, 0, UINT32_MAX);
      ahb_dma_ll_rx_set_desc_addr(S31_DMA, 0, (uintptr_t)desc);
      i2s_ll_rx_set_eof_num(priv->hal.dev, bytes);
      ahb_dma_ll_rx_enable_interrupt(S31_DMA, 0,
        S31_RX_ERRORS | GDMA_LL_EVENT_RX_SUC_EOF, true);
      ahb_dma_ll_rx_start(S31_DMA, 0);
      priv->hal.dev->rx_conf.rx_update = 1;
    }
  else
    {
      ahb_dma_ll_tx_reset_channel(S31_DMA, 0);
      ahb_dma_ll_tx_clear_interrupt_status(S31_DMA, 0, UINT32_MAX);
      ahb_dma_ll_tx_set_desc_addr(S31_DMA, 0, (uintptr_t)desc);
      ahb_dma_ll_tx_enable_interrupt(S31_DMA, 0,
        S31_TX_ERRORS | GDMA_LL_EVENT_TX_TOTAL_EOF, true);
      ahb_dma_ll_tx_start(S31_DMA, 0);
      priv->hal.dev->tx_conf.tx_update = 1;
    }

  /* The pinned LL start helpers wait indefinitely for UPDATE.  Bound that
   * handshake so a missing peripheral clock returns a real failure.
   */

  for (retry = 0; retry < 10000; retry++)
    {
      if (!(rx ? priv->hal.dev->rx_conf.rx_update :
                 priv->hal.dev->tx_conf.tx_update))
        {
          if (rx)
            {
              priv->hal.dev->rx_conf.rx_start = 1;
            }
          else
            {
              esp32s31_audio_pa(true);
              priv->hal.dev->tx_conf.tx_start = 1;
            }

          return 0;
        }

      up_udelay(1);
    }

  return -ETIMEDOUT;
}

static int s31_worker(int argc, char *argv[])
{
  struct s31_i2s_s *priv = &g_i2s;
  struct s31_request_s *req;
  struct ap_buffer_s *apb;
  clock_t start;
  clock_t timeout;
  size_t bytes;
  size_t i;
  int ret;

  for (;;)
    {
      nxsem_wait_uninterruptible(&priv->queued);
      nxmutex_lock(&priv->lock);
      req = priv->head;
      if (req == NULL)
        {
          nxmutex_unlock(&priv->lock);
          continue;
        }

      priv->head = req->next;
      if (priv->head == NULL)
        {
          priv->tail = NULL;
        }

      apb = req->apb;
      bytes = req->rx ? apb->nmaxbytes : apb->nbytes - apb->curbyte;
      ret = req->generation == priv->generation ? 0 : -ECANCELED;
      timeout = req->timeout;
      if (timeout == 0)
        {
          timeout = MSEC2TICK(1000 + bytes * 1000 / (priv->rate * 2));
        }

      if (ret == 0 && !req->rx)
        {
          memcpy(g_buffer, apb->samp + apb->curbyte, bytes);
        }

      start = clock_systime_ticks();
      if (ret == 0 && bytes != 0)
        {
          ret = s31_start(priv, req->rx, bytes);
        }

      nxmutex_unlock(&priv->lock);
      if (ret == 0 && bytes != 0)
        {
          ret = nxsem_tickwait_uninterruptible(&priv->done, timeout);
          if (ret == 0)
            {
              ret = priv->result;
            }
        }

      /* DMA EOF only means that FIFO received the last word.  Wait for the
       * actual I2S transmitter to become idle before disabling the PA.
       */

      while (ret == 0 && bytes != 0 && !req->rx &&
             !priv->hal.dev->state.tx_idle)
        {
          if (clock_systime_ticks() - start >= timeout)
            {
              ret = -ETIMEDOUT;
              break;
            }

          nxsig_usleep(100);
        }

      nxmutex_lock(&priv->lock);
      s31_stop_hardware(priv);
      if (req->generation != priv->generation)
        {
          ret = -ECANCELED;
        }

      if (ret == 0 && req->rx)
        {
          dma_descriptor_t *desc = (dma_descriptor_t *)g_descriptors;
          size_t received = 0;
          if (s31_sync(g_descriptors, sizeof(g_descriptors),
                              ESP_CACHE_MSYNC_FLAG_DIR_M2C) != 0 ||
              s31_sync(g_buffer, sizeof(g_buffer),
                              ESP_CACHE_MSYNC_FLAG_DIR_M2C) != 0)
            {
              ret = -EIO;
            }

          for (i = 0; ret == 0 && i <
               (bytes + S31_DESC_SIZE - 1) / S31_DESC_SIZE; i++)
            {
              if (desc[i].dw0.owner != DMA_DESCRIPTOR_BUFFER_OWNER_CPU ||
                  desc[i].dw0.err_eof ||
                  desc[i].dw0.length > desc[i].dw0.size)
                {
                  ret = -EIO;
                }

              received += desc[i].dw0.length;
            }

          if (ret == 0 && received != bytes)
            {
              ret = -EIO;
            }

          if (ret == 0)
            {
              memcpy(apb->samp, g_buffer, bytes);
              apb->nbytes = bytes;
              apb->curbyte = 0;
            }
        }
      else if (ret == 0)
        {
          apb->curbyte = apb->nbytes;
        }

      priv->pending--;
      nxmutex_unlock(&priv->lock);
      req->callback(&priv->dev, apb, req->arg, ret);
      apb_free(apb);
      kmm_free(req);
    }

  return 0;
}

static int s31_enqueue(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                      i2s_callback_t callback, void *arg, uint32_t timeout,
                      bool rx)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  struct s31_request_s *req;
  size_t bytes;
  int ret = 0;

  if (apb == NULL || apb->samp == NULL || callback == NULL ||
      apb->curbyte > apb->nbytes || apb->nbytes > apb->nmaxbytes)
    {
      return -EINVAL;
    }

  bytes = rx ? apb->nmaxbytes : apb->nbytes - apb->curbyte;
  if ((rx && bytes == 0) || bytes > S31_BUFFER_SIZE || (bytes & 3) != 0)
    {
      return -EINVAL;
    }

  req = kmm_zalloc(sizeof(*req));
  if (req == NULL)
    {
      return -ENOMEM;
    }

  nxmutex_lock(&priv->lock);
  if (priv->pending >= S31_QUEUE_LIMIT)
    {
      ret = -EAGAIN;
    }
  else if (priv->pending != 0 && priv->rx != rx)
    {
      ret = -EBUSY;
    }
  else
    {
      req->apb = apb;
      req->callback = callback;
      req->arg = arg;
      req->timeout = timeout;
      req->rx = rx;
      req->generation = priv->generation;
      apb_reference(apb);
      if (priv->tail != NULL)
        {
          priv->tail->next = req;
        }
      else
        {
          priv->head = req;
        }

      priv->tail = req;
      priv->pending++;
      priv->rx = rx;
      nxsem_post(&priv->queued);
    }

  nxmutex_unlock(&priv->lock);
  if (ret < 0)
    {
      kmm_free(req);
    }

  return ret;
}

static int s31_receive(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                      i2s_callback_t callback, void *arg, uint32_t timeout)
{
  return s31_enqueue(dev, apb, callback, arg, timeout, true);
}

static int s31_send(struct i2s_dev_s *dev, struct ap_buffer_s *apb,
                   i2s_callback_t callback, void *arg, uint32_t timeout)
{
  return s31_enqueue(dev, apb, callback, arg, timeout, false);
}

static int s31_channels(struct i2s_dev_s *dev, uint8_t channels)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  int ret = 0;
  if (channels != 1 && channels != 2)
    {
      return -EINVAL;
    }

  nxmutex_lock(&priv->lock);
  if (priv->pending != 0)
    {
      ret = -EBUSY;
    }
  else
    {
      priv->channels = channels;
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

static uint32_t s31_rate(struct i2s_dev_s *dev, uint32_t rate)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  uint32_t result = 0;
  if (rate != 8000 && rate != 12000 && rate != 16000 && rate != 24000 &&
      rate != 32000 && rate != 44100 && rate != 48000)
    {
      return 0;
    }

  nxmutex_lock(&priv->lock);
  if (priv->pending == 0)
    {
      priv->rate = rate;
      result = rate;
    }

  nxmutex_unlock(&priv->lock);
  return result;
}

static uint32_t s31_width(struct i2s_dev_s *dev, int bits)
{
  return bits == 16 ? 16 : 0;
}

static uint32_t s31_getmclk(struct i2s_dev_s *dev)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  uint32_t frequency;

  if (nxmutex_lock(&priv->lock) < 0)
    {
      return 0;
    }

  frequency = s31_mclk_for_rate(priv->rate);
  nxmutex_unlock(&priv->lock);
  return frequency;
}

static uint32_t s31_setmclk(struct i2s_dev_s *dev, uint32_t frequency)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  uint32_t result = 0;

  if (nxmutex_lock(&priv->lock) < 0)
    {
      return 0;
    }

  /* MCLK follows the sample rate. Do not accept an independent frequency
   * or reconfiguration while a buffer is queued or being transferred.
   */

  if (priv->pending == 0 && frequency == s31_mclk_for_rate(priv->rate))
    {
      result = frequency;
    }

  nxmutex_unlock(&priv->lock);
  return result;
}

static int s31_ioctl(struct i2s_dev_s *dev, int cmd, unsigned long arg)
{
  struct s31_i2s_s *priv = (struct s31_i2s_s *)dev;
  struct audio_buf_desc_s *desc = (struct audio_buf_desc_s *)arg;

  switch (cmd)
    {
      case AUDIOIOC_START:
        return 0;

      case AUDIOIOC_STOP:
      case AUDIOIOC_SHUTDOWN:
        nxmutex_lock(&priv->lock);
        priv->generation++;
        s31_stop_hardware(priv);
        priv->result = -ECANCELED;
        nxsem_post(&priv->done);
        nxmutex_unlock(&priv->lock);
        return 0;

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

static const struct i2s_ops_s g_ops =
{
  .i2s_rxchannels = s31_channels,
  .i2s_rxsamplerate = s31_rate,
  .i2s_rxdatawidth = s31_width,
  .i2s_receive = s31_receive,
  .i2s_txchannels = s31_channels,
  .i2s_txsamplerate = s31_rate,
  .i2s_txdatawidth = s31_width,
  .i2s_send = s31_send,
  .i2s_getmclkfrequency = s31_getmclk,
  .i2s_setmclkfrequency = s31_setmclk,
  .i2s_ioctl = s31_ioctl,
};

struct i2s_dev_s *esp32s31_i2s_initialize(int port)
{
  struct s31_i2s_s *priv = &g_i2s;
  int rxirq;
  int txirq;
  int ret;
  int pin;
  irqstate_t flags;

  if (port != 0)
    {
      return NULL;
    }

  if (priv->ready)
    {
      return &priv->dev;
    }

  if (!esp_ptr_internal(g_buffer) || !esp_ptr_dma_capable(g_buffer) ||
      !esp_ptr_internal(g_descriptors) ||
      !esp_ptr_dma_capable(g_descriptors))
    {
      return NULL;
    }

  priv->dev.ops = &g_ops;
  priv->rate = 48000;
  priv->channels = 1;
  nxmutex_init(&priv->lock);
  nxsem_init(&priv->queued, 0, 0);
  nxsem_init(&priv->done, 0, 0);
  esp32s31_audio_pa(false);
  for (pin = 52; pin <= 56; pin++)
    {
      ret = esp_configgpio(pin, pin == 54 ? INPUT : OUTPUT);
      if (ret < 0)
        {
          goto fail_sem;
        }
    }

  esp_gpio_matrix_in(54, I2S0_I_SD_PAD_IN_IDX, false);
  esp_gpio_matrix_out(56, I2S0_O_SD_PAD_OUT_IDX, false, false);
  flags = enter_critical_section();
  i2s_ll_enable_bus_clock(0, true);
  i2s_ll_reset_register(0);
  i2s_hal_init(&priv->hal, 0);
  i2s_ll_enable_core_clock(priv->hal.dev, true);
  gdma_ll_enable_bus_clock(0, true);
  ahb_dma_ll_force_enable_reg_clock(S31_DMA, true);
  ahb_dma_ll_set_default_memory_range(S31_DMA);
  ahb_dma_ll_rx_reset_channel(S31_DMA, 0);
  ahb_dma_ll_tx_reset_channel(S31_DMA, 0);
  ahb_dma_ll_rx_connect_to_periph(S31_DMA, 0, SOC_GDMA_TRIG_PERIPH_I2S0CH0);
  ahb_dma_ll_tx_connect_to_periph(S31_DMA, 0, SOC_GDMA_TRIG_PERIPH_I2S0CH0);
  ahb_dma_ll_rx_enable_owner_check(S31_DMA, 0, true);
  ahb_dma_ll_tx_enable_owner_check(S31_DMA, 0, true);
  ahb_dma_ll_tx_enable_auto_write_back(S31_DMA, 0, true);
  ahb_dma_ll_tx_set_eof_mode(S31_DMA, 0, 1);
  s31_stop_hardware(priv);
  leave_critical_section(flags);
  s31_configure(priv, false);

  rxirq = esp_setup_irq(S31_RX_SOURCE, 1, ESP_IRQ_TRIGGER_LEVEL);
  if (rxirq < 0)
    {
      goto fail_sem;
    }

  txirq = esp_setup_irq(S31_TX_SOURCE, 1, ESP_IRQ_TRIGGER_LEVEL);
  if (txirq < 0)
    {
      goto fail_rx;
    }

  ret = irq_attach(ESP_SOURCE2IRQ(S31_RX_SOURCE), s31_interrupt, priv);
  if (ret < 0)
    {
      goto fail_tx;
    }

  ret = irq_attach(ESP_SOURCE2IRQ(S31_TX_SOURCE), s31_interrupt, priv);
  if (ret < 0)
    {
      goto fail_rx_attach;
    }

  up_enable_irq(ESP_SOURCE2IRQ(S31_RX_SOURCE));
  up_enable_irq(ESP_SOURCE2IRQ(S31_TX_SOURCE));
  ret = kthread_create("s31_i2s", 110, 4096, s31_worker, NULL);
  if (ret < 0)
    {
      up_disable_irq(ESP_SOURCE2IRQ(S31_RX_SOURCE));
      up_disable_irq(ESP_SOURCE2IRQ(S31_TX_SOURCE));
      irq_detach(ESP_SOURCE2IRQ(S31_TX_SOURCE));
      goto fail_rx_attach;
    }

  priv->ready = true;
  return &priv->dev;

fail_rx_attach:
  irq_detach(ESP_SOURCE2IRQ(S31_RX_SOURCE));
fail_tx:
  esp_teardown_irq(S31_TX_SOURCE, txirq);
fail_rx:
  esp_teardown_irq(S31_RX_SOURCE, rxirq);
fail_sem:
  nxsem_destroy(&priv->done);
  nxsem_destroy(&priv->queued);
  nxmutex_destroy(&priv->lock);
  return NULL;
}
