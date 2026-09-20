/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_sdmmc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/mmcsd.h>
#include <nuttx/mutex.h>
#include <nuttx/sdio.h>

#include "esp32s31_sdmmc.h"

#if defined(CONFIG_ESP32S31_SDMMC) && defined(CONFIG_MMCSD) && \
    defined(CONFIG_MMCSD_SDIO)

/* These headers are from the locked ESP-IDF 6.1 SDMMC HAL.  The source tree
 * intentionally keeps the HAL implementation out of the default profile;
 * weak references below let this bridge compile before that HAL dependency
 * graph is added to an isolated image. */

#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_private/esp_cache_private.h"
#include "freertos/FreeRTOS.h"
#ifdef CONFIG_ESP32S31_SDMMC_IDF_HOST
#  include "hal/sdmmc_ll.h"
#  include "esp_timer.h"
#endif

#ifndef CONFIG_ESP32S31_SDMMC_IDF_HOST
#pragma weak sdmmc_host_init
#pragma weak sdmmc_host_init_slot
#pragma weak sdmmc_host_set_bus_width
#pragma weak sdmmc_host_get_slot_width
#pragma weak sdmmc_host_set_bus_ddr_mode
#pragma weak sdmmc_host_set_card_clk
#pragma weak sdmmc_host_set_cclk_always_on
#pragma weak sdmmc_host_do_transaction
#pragma weak sdmmc_host_deinit_slot
#pragma weak sdmmc_host_io_int_enable
#pragma weak sdmmc_host_io_int_wait
#pragma weak sdmmc_host_get_real_freq
#pragma weak sdmmc_host_set_input_delay
#pragma weak sdmmc_host_set_input_delayline
#pragma weak sdmmc_host_check_buffer_alignment
#pragma weak sdmmc_host_is_slot_set_to_uhs1
#endif

struct esp32s31_sdmmc_dev_s
{
  struct sdio_dev_s dev;
  sdmmc_host_t host;
  sdmmc_slot_config_t slot;
  sdmmc_command_t command;
  void *retained_buffer;
  void *buffer;
  size_t nbytes;
  bool read;
  int setup_error;
  uint32_t timeout_ms;
  sdio_eventset_t completed;
  uint32_t blocklen;
  uint32_t blockcount;
  sdio_eventset_t wait_events;
  sdio_statset_t status_bits;
  bool host_ready;
  bool slot_ready;
  int last_error;
};

static struct esp32s31_sdmmc_dev_s *sdmmc_priv(struct sdio_dev_s *dev)
{
  return (struct esp32s31_sdmmc_dev_s *)dev;
}

static int sdmmc_error(esp_err_t error)
{
  if (error == ESP_OK)
    {
      return OK;
    }

  switch (error)
    {
      case ESP_ERR_TIMEOUT: return -ETIMEDOUT;
      case ESP_ERR_NO_MEM: return -ENOMEM;
      case ESP_ERR_INVALID_ARG: return -EINVAL;
      case ESP_ERR_INVALID_CRC: return -EILSEQ;
      default: return -EIO;
    }
}

static int sdmmc_host_prepare(struct esp32s31_sdmmc_dev_s *priv)
{
  esp_err_t error;

  if (priv->slot_ready)
    {
      return OK;
    }

#ifndef CONFIG_ESP32S31_SDMMC_IDF_HOST
  if (sdmmc_host_init == NULL || sdmmc_host_init_slot == NULL ||
      priv->host.init == NULL)
    {
      return -ENOSYS;
    }
#endif

  if (!priv->host_ready)
    {
      error = priv->host.init();
      if (error != ESP_OK)
        {
          return sdmmc_error(error);
        }

      priv->host_ready = true;
    }

  error = sdmmc_host_init_slot(priv->host.slot, &priv->slot);
  if (error != ESP_OK)
    {
      return sdmmc_error(error);
    }

  priv->slot_ready = true;
  return OK;
}

static void sdmmc_reset(struct sdio_dev_s *dev)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);

  memset(&priv->command, 0, sizeof(priv->command));
  priv->blocklen = 0;
  priv->blockcount = 0;
  priv->wait_events = 0;
  /* No card-detect wire: NuttX requires polling hosts to attempt a probe.
   * This bit is not a claim that a card has answered any command. */
  priv->status_bits = SDIO_STATUS_PRESENT;
  priv->buffer = NULL;
  priv->nbytes = 0;
  priv->setup_error = OK;
  priv->timeout_ms = 1000;
  priv->completed = 0;
  priv->last_error = OK;
}

static sdio_capset_t sdmmc_capabilities(struct sdio_dev_s *dev)
{
  UNUSED(dev);
  return SDIO_CAPS_4BIT | SDIO_CAPS_DMABEFOREWRITE;
}

static sdio_statset_t sdmmc_status(struct sdio_dev_s *dev)
{
  return sdmmc_priv(dev)->status_bits;
}

static void sdmmc_widebus(struct sdio_dev_s *dev, bool enable)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);

  if (priv->host.set_bus_width != NULL && priv->slot_ready)
    {
      priv->last_error = sdmmc_error(
          priv->host.set_bus_width(priv->host.slot, enable ? 4 : 1));
    }
}

static void sdmmc_clock(struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  uint32_t freq = 400;

  switch (rate)
    {
      case CLOCK_MMC_TRANSFER:
      case CLOCK_SD_TRANSFER_4BIT:
        freq = 20000;
        break;
      case CLOCK_SD_TRANSFER_1BIT:
        freq = 20000;
        break;
      case CLOCK_SDIO_DISABLED:
      case CLOCK_IDMODE:
      default:
        freq = 400;
        break;
    }

  if (priv->host.set_card_clk != NULL && priv->slot_ready)
    {
      priv->last_error = sdmmc_error(
          priv->host.set_card_clk(priv->host.slot, freq));
    }
}

static int sdmmc_attach(struct sdio_dev_s *dev)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  return priv->slot_ready ? OK : sdmmc_host_prepare(priv);
}

static int sdmmc_response_flags(uint32_t cmd)
{
  switch (cmd & MMCSD_RESPONSE_MASK)
    {
      case MMCSD_R1_RESPONSE: return SCF_RSP_R1;
      case MMCSD_R1B_RESPONSE: return SCF_RSP_R1B;
      case MMCSD_R2_RESPONSE: return SCF_RSP_R2;
      case MMCSD_R3_RESPONSE: return SCF_RSP_R3;
      case MMCSD_R4_RESPONSE: return SCF_RSP_R4;
      case MMCSD_R5_RESPONSE: return SCF_RSP_R5;
      case MMCSD_R6_RESPONSE: return SCF_RSP_R6;
      case MMCSD_R7_RESPONSE: return SCF_RSP_R7;
      default: return SCF_RSP_R0;
    }
}

static int sdmmc_sendcmd(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t arg)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  bool data = (cmd & MMCSD_DATAXFR_MASK) != MMCSD_NODATAXFR;
  bool read = (cmd & MMCSD_WRXFR) == 0;
  void *bounce = NULL;
  size_t alignment = 0;
  esp_err_t error;
  int ret = OK;

  priv->completed = 0;
  memset(&priv->command, 0, sizeof(priv->command));
  priv->command.opcode = cmd & MMCSD_CMDIDX_MASK;
  priv->command.arg = arg;
  priv->command.flags = sdmmc_response_flags(cmd) |
                        (data ? SCF_CMD_ADTC : SCF_CMD_AC);
  priv->command.timeout_ms = priv->wait_events ? priv->timeout_ms : 1000;
#ifdef CONFIG_ESP32S31_SDMMC_IDF_HOST
  if (priv->retained_buffer != NULL)
    {
      if (!sdmmc_ll_is_dma_reset_done(&SDMMC))
        {
          ret = -EBUSY;
          goto done;
        }

      heap_caps_free(priv->retained_buffer);
      priv->retained_buffer = NULL;
    }
#endif

  if (data)
    {
      if (priv->setup_error != OK || priv->buffer == NULL ||
          priv->nbytes == 0 || priv->read != read)
        {
          ret = priv->setup_error != OK ? priv->setup_error : -EINVAL;
          goto done;
        }

      if (priv->blocklen == 0 || priv->blockcount == 0 ||
          priv->nbytes / priv->blocklen != priv->blockcount ||
          priv->nbytes % priv->blocklen != 0)
        {
          ret = -EINVAL;
          goto done;
        }

      /* Keep DMA/cache ownership private even when the upper-half buffer is
       * unaligned or read-only.  Query the actual internal cache alignment. */
      error = esp_cache_get_alignment(0, &alignment);
      if (error != ESP_OK)
        {
          ret = sdmmc_error(error);
          goto done;
        }

      if (alignment < 4)
        {
          alignment = 4;
        }

      if ((alignment & (alignment - 1)) != 0 ||
          priv->nbytes > SIZE_MAX - (alignment - 1))
        {
          ret = -EINVAL;
          goto done;
        }

      priv->command.buflen = (priv->nbytes + alignment - 1) &
                             ~(alignment - 1);
      bounce = heap_caps_aligned_alloc(alignment, priv->command.buflen,
          MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
      if (bounce == NULL)
        {
          ret = -ENOMEM;
          goto done;
        }

      priv->command.data = bounce;
      priv->command.datalen = priv->nbytes;
      priv->command.blklen = priv->blocklen;
      if (read)
        {
          priv->command.flags |= SCF_CMD_READ;
        }
      else
        {
          memcpy(bounce, priv->buffer, priv->nbytes);
        }
    }

  ret = sdmmc_host_prepare(priv);
  if (ret != OK)
    {
      goto done;
    }

  if (priv->host.do_transaction == NULL)
    {
      ret = -ENOSYS;
      goto done;
    }

  if (data && priv->host.check_buffer_alignment != NULL &&
      !priv->host.check_buffer_alignment(priv->host.slot, bounce,
                                         priv->command.buflen))
    {
      ret = -EINVAL;
      goto done;
    }

  error = priv->host.do_transaction(priv->host.slot, &priv->command);
#ifdef CONFIG_ESP32S31_SDMMC_IDF_HOST
  if (data)
    {
      /* Also quiesce the IDMAC on early command-start errors.  The locked
       * HAL stops it on normal completion/event timeout, but not every
       * pre-event failure path.  The SDIO device mutex serializes this host. */
      int64_t deadline = esp_timer_get_time() + 100000;
      sdmmc_ll_stop_dma(&SDMMC);
      while (!sdmmc_ll_is_dma_reset_done(&SDMMC))
        {
          if (esp_timer_get_time() >= deadline)
            {
              /* Never free memory still potentially owned by DMA.  A later
               * call may reclaim it after reset completes; until then the
               * host rejects new commands instead of reusing the buffer. */
              priv->retained_buffer = bounce;
              bounce = NULL;
              ret = -ETIMEDOUT;
              goto done;
            }
        }
    }
#endif
  ret = sdmmc_error(error != ESP_OK ? error : priv->command.error);
  if (ret == OK)
    {
      if (data && read)
        {
          memcpy(priv->buffer, bounce, priv->nbytes);
        }

      priv->completed = SDIOWAIT_CMDDONE | SDIOWAIT_RESPONSEDONE;
      if (data)
        {
          priv->completed |= SDIOWAIT_TRANSFERDONE;
        }
    }

done:
  heap_caps_free(bounce);
  priv->command.data = NULL;
  if (data)
    {
      priv->buffer = NULL;
      priv->nbytes = 0;
      priv->setup_error = OK;
    }

  priv->last_error = ret;
  if (ret != OK)
    {
      priv->completed = ret == -ETIMEDOUT ? SDIOWAIT_TIMEOUT : SDIOWAIT_ERROR;
    }

  return ret;
}

#ifdef CONFIG_SDIO_BLOCKSETUP
static void sdmmc_blocksetup(struct sdio_dev_s *dev, unsigned int blocklen,
                             unsigned int nblocks)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  priv->blocklen = blocklen;
  priv->blockcount = nblocks;
}
#endif

static int sdmmc_setup(struct sdio_dev_s *dev, void *buffer,
                         size_t nbytes, bool read)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  priv->buffer = buffer;
  priv->nbytes = nbytes;
  priv->read = read;
  priv->setup_error = buffer == NULL || nbytes == 0 ? -EINVAL : OK;
  return priv->setup_error;
}

static int sdmmc_recvsetup(struct sdio_dev_s *dev, uint8_t *buffer,
                           size_t nbytes)
{
  return sdmmc_setup(dev, buffer, nbytes, true);
}

static int sdmmc_sendsetup(struct sdio_dev_s *dev, const uint8_t *buffer,
                           size_t nbytes)
{
  return sdmmc_setup(dev, (void *)buffer, nbytes, false);
}

static int sdmmc_cancel(struct sdio_dev_s *dev)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  priv->command.data = NULL;
  priv->command.datalen = 0;
  priv->command.buflen = 0;
  priv->buffer = NULL;
  priv->nbytes = 0;
  priv->setup_error = -ECANCELED;
  priv->completed = SDIOWAIT_ERROR;
  priv->last_error = -ECANCELED;
  return OK;
}

static int sdmmc_waitresponse(struct sdio_dev_s *dev, uint32_t cmd)
{
  UNUSED(cmd);
  return sdmmc_priv(dev)->last_error;
}

static int sdmmc_recv_r1(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t *r1)
{
  UNUSED(cmd);
  if (r1 == NULL) return -EINVAL;
  *r1 = sdmmc_priv(dev)->command.response[0];
  return sdmmc_priv(dev)->last_error;
}

static int sdmmc_recv_r2(struct sdio_dev_s *dev, uint32_t cmd,
                         uint32_t r2[4])
{
  UNUSED(cmd);
  if (r2 == NULL) return -EINVAL;
  /* IDF stores least-significant word first; NuttX decodes CSD[0] as
   * bits 127..96.  Preserve bits, reverse only the four words. */
  for (int i = 0; i < 4; i++)
    {
      r2[i] = sdmmc_priv(dev)->command.response[3 - i];
    }
  return sdmmc_priv(dev)->last_error;
}

#define SDMMC_RECV_SHORT(name) \
  static int name(struct sdio_dev_s *dev, uint32_t cmd, uint32_t *value) \
  { \
    UNUSED(cmd); \
    if (value == NULL) return -EINVAL; \
    *value = sdmmc_priv(dev)->command.response[0]; \
    return sdmmc_priv(dev)->last_error; \
  }

SDMMC_RECV_SHORT(sdmmc_recv_r3)
SDMMC_RECV_SHORT(sdmmc_recv_r4)
SDMMC_RECV_SHORT(sdmmc_recv_r5)
SDMMC_RECV_SHORT(sdmmc_recv_r6)
SDMMC_RECV_SHORT(sdmmc_recv_r7)

static void sdmmc_waitenable(struct sdio_dev_s *dev,
                             sdio_eventset_t eventset, uint32_t timeout)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  priv->wait_events = eventset;
  priv->completed = 0;
  priv->timeout_ms = timeout < portTICK_PERIOD_MS ? portTICK_PERIOD_MS :
                     (timeout > INT_MAX ? INT_MAX : timeout);
}

static sdio_eventset_t sdmmc_eventwait(struct sdio_dev_s *dev)
{
  struct esp32s31_sdmmc_dev_s *priv = sdmmc_priv(dev);
  sdio_eventset_t events = priv->completed &
      (priv->wait_events | SDIOWAIT_ERROR | SDIOWAIT_TIMEOUT);
  priv->wait_events = 0;
  priv->completed = 0;

  /* This adapter completes the transaction synchronously in sendcmd.
   * A wait without a completed command must never fabricate success. */
  return events != 0 ? events : SDIOWAIT_ERROR;
}

static void sdmmc_callbackenable(struct sdio_dev_s *dev,
                                 sdio_eventset_t eventset)
{
  UNUSED(dev);
  UNUSED(eventset);
}

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
static int sdmmc_registercallback(struct sdio_dev_s *dev, worker_t callback,
                                  void *arg)
{
  UNUSED(dev);
  UNUSED(callback);
  UNUSED(arg);
  return -ENOSYS;
}
#endif

static void sdmmc_gotextcsd(struct sdio_dev_s *dev, const uint8_t *buffer)
{
  UNUSED(dev);
  UNUSED(buffer);
}

static struct esp32s31_sdmmc_dev_s g_sdmmc =
{
  .dev =
    {
      .reset = sdmmc_reset,
      .capabilities = sdmmc_capabilities,
      .status = sdmmc_status,
      .widebus = sdmmc_widebus,
      .clock = sdmmc_clock,
      .attach = sdmmc_attach,
      .sendcmd = sdmmc_sendcmd,
#ifdef CONFIG_SDIO_BLOCKSETUP
      .blocksetup = sdmmc_blocksetup,
#endif
      .recvsetup = sdmmc_recvsetup,
      .sendsetup = sdmmc_sendsetup,
      .cancel = sdmmc_cancel,
      .waitresponse = sdmmc_waitresponse,
      .recv_r1 = sdmmc_recv_r1,
      .recv_r2 = sdmmc_recv_r2,
      .recv_r3 = sdmmc_recv_r3,
      .recv_r4 = sdmmc_recv_r4,
      .recv_r5 = sdmmc_recv_r5,
      .recv_r6 = sdmmc_recv_r6,
      .recv_r7 = sdmmc_recv_r7,
      .waitenable = sdmmc_waitenable,
      .eventwait = sdmmc_eventwait,
      .callbackenable = sdmmc_callbackenable,
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
      .registercallback = sdmmc_registercallback,
#endif
      .gotextcsd = sdmmc_gotextcsd,
    },
  .host = SDMMC_HOST_DEFAULT(),
  .slot = SDMMC_SLOT_CONFIG_DEFAULT(),
};

struct sdio_dev_s *esp32s31_sdmmc_initialize(int slotno)
{
  if (slotno != SDMMC_HOST_SLOT_0)
    {
      return NULL;
    }

  g_sdmmc.host.slot = SDMMC_HOST_SLOT_0;
  g_sdmmc.slot.width = 4;
  nxmutex_init(&g_sdmmc.dev.mutex);
  sdmmc_reset(&g_sdmmc.dev);
  return &g_sdmmc.dev;
}

int esp32s31_sdmmc_register(int minor)
{
  struct sdio_dev_s *dev = esp32s31_sdmmc_initialize(SDMMC_HOST_SLOT_0);
  if (dev == NULL)
    {
      return -EINVAL;
    }
  return mmcsd_slotinitialize(minor, dev);
}

#endif /* CONFIG_ESP32S31_SDMMC && CONFIG_MMCSD && CONFIG_MMCSD_SDIO */
