/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <nuttx/arch.h>
#include <nuttx/can/can.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/spinlock.h>
#include <nuttx/wdog.h>
#include "espressif/esp_gpio.h"
#include "espressif/esp_irq.h"
#include "esp_private/esp_clk.h"
#include "hal/twaifd_ll.h"
#include "hal/twai_ll.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_pins.h"
#include "soc/interrupts.h"
#include "esp32s31_canfd.h"

#define HW TWAIFD_LL_GET_HW(0)
#define CAN_IRQ ESP_SOURCE2IRQ(ETS_TWAI0_INTR_SOURCE)
#define TX_TICKS MAX(1, MSEC2TICK(500))
#define ERR_INTERRUPTS (TWAIFD_LL_INTR_BUS_ERR | \
                        TWAIFD_LL_INTR_FSM_CHANGE | \
                        TWAIFD_LL_INTR_DATA_OVERRUN)

/* CAN0 is exclusively owned by this single-mailbox lower half. Queueing
 * and multiple readers belong to the standard CAN upper half. Failed TX
 * is retired exactly once, with an error frame, so close cannot wait forever
 * for a mailbox that has stopped. Reopen is required after a TX timeout or
 * bus-off. RX filtering, CAN1, timestamps and SMP are not implemented.
 */

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static struct wdog_s g_timeout;
static twai_timing_advanced_config_t g_nominal;
static twai_timing_advanced_config_t g_data;
static int g_txpin;
static int g_rxpin;
static int g_cpuint = -1;
static bool g_registered;
static bool g_clocked;
static bool g_active;
static bool g_online;
static bool g_busy;
static bool g_rxenabled;
static uint32_t g_pending_error;

#ifdef CONFIG_CAN_LOOPBACK
#  define LOOPBACK true
#else
#  define LOOPBACK false
#endif

/* The vendor command helper waits indefinitely for self-clearing bits.
 * Keep this lower half bounded even if the peripheral stops responding.
 */

static int s31_can_command(uint32_t command)
{
  unsigned int i;
  HW->command.val = command;
  for (i = 0; i < 100; i++)
    {
      up_udelay(1);
      if ((HW->command.val & command) == 0)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

static void s31_can_mask(void)
{
  uint32_t mask = 0;
  if (g_active)
    {
      if (g_rxenabled)
        {
          mask |= TWAIFD_LL_INTR_RX_NOT_EMPTY | ERR_INTERRUPTS;
        }

      if (g_busy && g_pending_error == 0)
        {
          mask |= TWAIFD_LL_INTR_TX_DONE | ERR_INTERRUPTS;
        }
    }

  twaifd_ll_enable_intr(HW, mask);
}

static void s31_can_error(struct can_dev_s *dev, uint32_t error)
{
  struct can_hdr_s hdr = {0};
  uint8_t data[CAN_ERROR_DLC] = {0};
  if (g_rxenabled && error != 0)
    {
      hdr.ch_error = 1;
      hdr.ch_id = error;
      hdr.ch_dlc = CAN_ERROR_DLC;
      can_receive(dev, &hdr, data);
    }
}

static void s31_can_complete(struct can_dev_s *dev, uint32_t error)
{
  if (g_busy)
    {
      wd_cancel(&g_timeout);
      twaifd_ll_set_tx_buffer_cmd(HW, 0, TWAIFD_LL_TX_CMD_EMPTY);
      g_busy = false;
      g_pending_error = 0;
      s31_can_mask();
      s31_can_error(dev, error);

      /* May immediately call co_send for the next queued message. */

      can_txdone(dev);
    }
}

static void s31_can_timeout(wdparm_t arg)
{
  struct can_dev_s *dev = (struct can_dev_s *)arg;
  uint32_t error = g_pending_error;
  if (g_active && g_busy)
    {
      if (error == 0)
        {
          error = CAN_ERROR_TXTIMEOUT;
          twaifd_ll_enable_hw(HW, false);
          g_online = false;
          twaifd_ll_clr_intr_status(HW, UINT32_MAX);
        }

      s31_can_complete(dev, error);
    }
}

/* Decode only complete, bounded RX records. RTR carries no payload, but the
 * character interface still uses DLC to size the record; return zero bytes
 * in that unused payload area instead of old mailbox data.
 */

static int s31_can_decode(const twaifd_frame_buffer_t *frame,
                          struct can_msg_s *msg)
{
  twai_frame_header_t header = {0};
  unsigned int bytes;
  twaifd_ll_parse_frame_header(frame, &header);
  bytes = can_dlc2bytes(header.dlc);
  if ((!header.fdf && header.dlc > 8) ||
      (header.fdf && header.rtr) || (!header.fdf && header.brs) ||
      frame->format.rwcnt != 3 + (header.rtr ? 0 : (bytes + 3) / 4))
    {
      return -EPROTO;
    }

  memset(msg, 0, sizeof(*msg));
  msg->cm_hdr.ch_id = header.id;
  msg->cm_hdr.ch_dlc = header.dlc;
  msg->cm_hdr.ch_extid = header.ide;
  msg->cm_hdr.ch_rtr = header.rtr;
  msg->cm_hdr.ch_edl = header.fdf;
  msg->cm_hdr.ch_brs = header.brs;
  msg->cm_hdr.ch_esi = header.esi;
  if (!header.rtr)
    {
      twaifd_ll_parse_frame_data(frame, msg->cm_data, bytes);
    }

  return OK;
}

static void s31_can_receive(struct can_dev_s *dev)
{
  struct can_msg_s msg;
  twaifd_frame_buffer_t frame;
  unsigned int n;
  unsigned int i;
  for (n = 0; n < 16 && !twaifd_ll_is_rx_buffer_empty(HW); n++)
    {
      memset(&frame, 0, sizeof(frame));
      frame.words[0] = HW->rx_data.rx_data;
      if (frame.format.rwcnt < 3 || frame.format.rwcnt > 19)
        {
          if (s31_can_command(TWAIFD_LL_HW_CMD_RX_FLUSH) < 0)
            {
              twaifd_ll_enable_hw(HW, false);
              g_online = false;
            }
          s31_can_error(dev, CAN_ERROR_PROTOCOL);
          break;
        }

      for (i = 1; i <= frame.format.rwcnt; i++)
        {
          frame.words[i] = HW->rx_data.rx_data;
        }

      if (s31_can_decode(&frame, &msg) < 0)
        {
          s31_can_error(dev, CAN_ERROR_PROTOCOL);
        }
      else if (g_rxenabled)
        {
          can_receive(dev, &msg.cm_hdr, msg.cm_data);
        }
    }

  /* Leave a non-empty level asserted when the per-interrupt budget expires. */

  if (twaifd_ll_is_rx_buffer_empty(HW))
    {
      twaifd_ll_clr_intr_status(HW, TWAIFD_LL_INTR_RX_NOT_EMPTY);
    }
}

static int s31_can_irq(int irq, void *context, void *arg)
{
  struct can_dev_s *dev = arg;
  uint32_t status = twaifd_ll_get_intr_status(HW);
  uint32_t error = 0;
  bool complete = false;
  twaifd_ll_clr_intr_status(HW, status & ~TWAIFD_LL_INTR_RX_NOT_EMPTY);
  if (!g_active)
    {
      return OK;
    }

  if ((status & TWAIFD_LL_INTR_RX_NOT_EMPTY) != 0)
    {
      s31_can_receive(dev);
    }

  if ((status & TWAIFD_LL_INTR_DATA_OVERRUN) != 0)
    {
      if (s31_can_command(TWAIFD_LL_HW_CMD_CLR_OVERRUN) < 0)
        {
          twaifd_ll_enable_hw(HW, false);
          g_online = false;
          complete = g_busy;
        }
      error |= CAN_ERROR_CONTROLLER;
    }

  if ((status & TWAIFD_LL_INTR_BUS_ERR) != 0)
    {
      error |= CAN_ERROR_BUSERROR;
    }

  if ((status & TWAIFD_LL_INTR_FSM_CHANGE) != 0 &&
      twaifd_ll_get_fault_state(HW) == TWAI_ERROR_BUS_OFF)
    {
      g_online = false;
      twaifd_ll_enable_hw(HW, false);
      error |= CAN_ERROR_BUSOFF;
      complete = g_busy;
    }

  if ((status & TWAIFD_LL_INTR_TX_DONE) != 0 && g_busy &&
      g_pending_error == 0)
    {
      unsigned int state = twaifd_ll_get_tx_buffer_status(HW, 0);
      if (state == TWAIFD_LL_TX_STATUS_SUCCESS ||
          state == TWAIFD_LL_TX_STATUS_FAILED ||
          state == TWAIFD_LL_TX_STATUS_ABORTED)
        {
          complete = true;
          if (state != TWAIFD_LL_TX_STATUS_SUCCESS)
            {
              error |= CAN_ERROR_BUSERROR;
            }
        }
    }

  if (complete)
    {
      s31_can_complete(dev, error);
    }
  else
    {
      s31_can_error(dev, error);
    }

  return OK;
}

static void s31_can_errhandle(struct can_dev_s *dev)
{
  irqstate_t flags = enter_critical_section();
  if (g_active)
    {
      s31_can_irq(CAN_IRQ, NULL, dev);
    }

  leave_critical_section(flags);
}

static int s31_can_enable(void)
{
  unsigned int i;
  twaifd_ll_clr_intr_status(HW, UINT32_MAX);
  twaifd_ll_enable_hw(HW, true);
  for (i = 0; i < 1000; i++)
    {
      up_udelay(10);
      if ((twaifd_ll_get_intr_status(HW) & TWAIFD_LL_INTR_FSM_CHANGE) != 0 &&
          twaifd_ll_get_fault_state(HW) != TWAI_ERROR_BUS_OFF)
        {
          twaifd_ll_clr_intr_status(HW, TWAIFD_LL_INTR_FSM_CHANGE);
          g_online = true;
          return OK;
        }
    }

  twaifd_ll_enable_hw(HW, false);
  g_online = false;
  return -ETIMEDOUT;
}

static void s31_can_shutdown(struct can_dev_s *dev)
{
  irqstate_t flags = enter_critical_section();
  g_active = false;
  g_online = false;
  g_busy = false;
  g_rxenabled = false;
  g_pending_error = 0;
  wd_cancel(&g_timeout);
  if (g_clocked)
    {
      twaifd_ll_enable_intr(HW, 0);
      twaifd_ll_enable_hw(HW, false);
      twaifd_ll_clr_intr_status(HW, UINT32_MAX);
      twai_ll_enable_clock(0, false);
      twai_ll_enable_bus_clock(0, false);
      twai_ll_mem_power_by_pmu(0);
      g_clocked = false;
    }

  if (g_cpuint >= 0)
    {
      up_disable_irq(CAN_IRQ);
      irq_detach(CAN_IRQ);
      esp_teardown_irq(ETS_TWAI0_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
    }

  esp_gpio_matrix_in(GPIO_MATRIX_CONST_ONE_INPUT, TWAI0_RX_IDX, false);
  esp_gpio_matrix_out(g_txpin, SIG_GPIO_OUT_IDX, false, false);
  esp_configgpio(g_txpin, INPUT);
  if (g_rxpin != g_txpin)
    {
      esp_configgpio(g_rxpin, INPUT);
    }

  leave_critical_section(flags);
}

static void s31_can_reset(struct can_dev_s *dev)
{
  if (g_active)
    {
      s31_can_shutdown(dev);
    }
}

static int s31_can_setup(struct can_dev_s *dev)
{
  irqstate_t flags;
  unsigned int i;
  int ret;
  g_cpuint = esp_setup_irq(ETS_TWAI0_INTR_SOURCE,
                          ESP_IRQ_PRIORITY_DEFAULT, ESP_IRQ_TRIGGER_LEVEL);
  if (g_cpuint < 0)
    {
      ret = g_cpuint;
      g_cpuint = -1;
      return ret;
    }

  ret = irq_attach(CAN_IRQ, s31_can_irq, dev);
  if (ret < 0)
    {
      esp_teardown_irq(ETS_TWAI0_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
      return ret;
    }

  flags = enter_critical_section();
  twai_ll_enable_bus_clock(0, true);
  twai_ll_reset_register(0);
  twai_ll_set_clock_source(0, TWAI_CLK_SRC_XTAL);
  twai_ll_enable_clock(0, true);
  twai_ll_mem_force_power_on(0);
  g_clocked = true;
  twaifd_ll_reset(HW);
  for (i = 0; i < 100 && HW->mode_settings.rst; i++)
    {
      up_udelay(10);
    }

  ret = -ETIMEDOUT;
  if (HW->mode_settings.rst)
    {
      goto out;
    }

  twaifd_ll_enable_hw(HW, false);
  twaifd_ll_enable_intr(HW, 0);
  twaifd_ll_set_mode(HW, false, LOOPBACK, LOOPBACK);
  twaifd_ll_set_tx_retrans_limit(HW, 0);
  twaifd_ll_enable_fd_mode(HW, true);
  twaifd_ll_enable_rxfifo_auto_increase(HW, true);
  twaifd_ll_enable_filter_mode(HW, false);
  twaifd_ll_filter_drop_rtr(HW, false);
  twaifd_ll_enable_time_trig_trans_mode(HW, false);
  twaifd_ll_enable_bus_off_tx_fail_mode(HW, true);
  twaifd_ll_set_nominal_bitrate(HW, &g_nominal);
  twaifd_ll_set_fd_bitrate(HW, &g_data);
  twaifd_ll_config_secondary_sample_point(HW, TWAIFD_LL_SSP_SRC_NO_SSP, 0);
  twaifd_ll_set_tx_buffer_priority(HW, 0, 1);
  twaifd_ll_set_tx_buffer_cmd(HW, 0, TWAIFD_LL_TX_CMD_EMPTY);
  /* ILBP enables reception of our own frames; it does not route TX back
   * to RX. Loopback uses one board-reserved GPIO with both input/output
   * enabled, as in the pinned vendor tests. No external wire is required.
   */

  ret = esp_configgpio(g_rxpin, INPUT);
  if (ret < 0)
    {
      goto out;
    }

  esp_gpiowrite(g_txpin, true);
  ret = esp_configgpio(g_txpin, LOOPBACK ? INPUT | OUTPUT : OUTPUT);
  if (ret < 0)
    {
      goto out;
    }

  esp_gpio_matrix_in(g_rxpin, TWAI0_RX_IDX, false);
  esp_gpio_matrix_out(g_txpin, TWAI0_TX_IDX, false, false);

  ret = s31_can_enable();
  if (ret == OK)
    {
      g_active = true;
      up_enable_irq(CAN_IRQ);
    }

out:
  leave_critical_section(flags);
  if (ret < 0)
    {
      s31_can_shutdown(dev);
    }

  return ret;
}

static void s31_can_rxint(struct can_dev_s *dev, bool enable)
{
  g_rxenabled = enable;
  s31_can_mask();
}

static void s31_can_txint(struct can_dev_s *dev, bool enable)
{
  /* Pending hardware transfers must complete even while close drains TX. */

  s31_can_mask();
}

static bool s31_can_txready(struct can_dev_s *dev)
{
  return g_active && !g_busy;
}

static bool s31_can_txempty(struct can_dev_s *dev)
{
  return !g_busy;
}

static int s31_can_send(struct can_dev_s *dev, struct can_msg_s *msg)
{
  const struct can_hdr_s *hdr = &msg->cm_hdr;
  twaifd_frame_buffer_t frame = {0};
  twai_frame_header_t header = {0};
  if (!g_active || g_busy)
    {
      return -EBUSY;
    }

  g_pending_error = 0;
  if (!g_online)
    {
      g_pending_error = CAN_ERROR_BUSOFF;
    }
  else if (hdr->ch_error || hdr->ch_esi ||
           hdr->ch_id > (hdr->ch_extid ? TWAI_EXT_ID_MASK : TWAI_STD_ID_MASK) ||
           (!hdr->ch_edl && (hdr->ch_dlc > 8 || hdr->ch_brs)) ||
           (hdr->ch_edl && hdr->ch_rtr))
    {
      g_pending_error = CAN_ERROR_PROTOCOL;
    }

  /* Retire invalid queued messages asynchronously too. Returning an error
   * here would leave them queued in the generic upper half indefinitely.
   */

  g_busy = true;
  wd_start(&g_timeout, g_pending_error ? 1 : TX_TICKS,
           s31_can_timeout, (wdparm_t)dev);
  if (g_pending_error == 0)
    {
      header.id = hdr->ch_id;
      header.dlc = hdr->ch_dlc;
      header.ide = hdr->ch_extid;
      header.rtr = hdr->ch_rtr;
      header.fdf = hdr->ch_edl;
      header.brs = hdr->ch_brs;
      twaifd_ll_format_frame_header(&header, hdr->ch_dlc, &frame);
      if (!hdr->ch_rtr)
        {
          twaifd_ll_format_frame_data(msg->cm_data,
                                      can_dlc2bytes(hdr->ch_dlc), &frame);
        }

      twaifd_ll_mount_tx_buffer(HW, &frame, 0);
      s31_can_mask();
      twaifd_ll_set_tx_buffer_cmd(HW, 0, TWAIFD_LL_TX_CMD_READY);
    }

  return OK;
}

static int s31_can_ioctl(struct can_dev_s *dev, int cmd, unsigned long arg)
{
  if (cmd == CANIOC_GET_BITTIMING)
    {
      struct canioc_bittiming_s *bt = (void *)arg;
      if (bt == NULL || (bt->bt_type != CAN_BITTIMING_NOMINAL &&
                         bt->bt_type != CAN_BITTIMING_DATA))
        {
          return -EINVAL;
        }

      bt->bt_baud = bt->bt_type == CAN_BITTIMING_DATA ?
                   CONFIG_ESP32S31_CANFD_DATA_BITRATE :
                   CONFIG_ESP32S31_CANFD_BITRATE;
      bt->bt_tseg1 = 15;
      bt->bt_tseg2 = 4;
      bt->bt_sjw = 4;
      return OK;
    }

  if (cmd == CANIOC_GET_CONNMODES)
    {
      struct canioc_connmodes_s *modes = (void *)arg;
      if (modes == NULL)
        {
          return -EINVAL;
        }

      memset(modes, 0, sizeof(*modes));
      modes->bm_loopback = LOOPBACK;
      return OK;
    }

  if (cmd == CANIOC_IFLUSH)
    {
      return s31_can_command(TWAIFD_LL_HW_CMD_RX_FLUSH);
    }

  if (cmd == CANIOC_OFLUSH || cmd == CANIOC_IOFLUSH)
    {
      /* Upper half has already discarded the software TX queue. */

      wd_cancel(&g_timeout);
      g_busy = false;
      g_pending_error = 0;
      twaifd_ll_enable_hw(HW, false);
      g_online = false;
      s31_can_mask();
      return s31_can_enable();
    }

  return -ENOTTY;
}

static const struct can_ops_s g_ops =
{
  .co_reset = s31_can_reset,
  .co_setup = s31_can_setup,
  .co_shutdown = s31_can_shutdown,
  .co_rxint = s31_can_rxint,
  .co_txint = s31_can_txint,
  .co_ioctl = s31_can_ioctl,
  .co_send = s31_can_send,
  .co_errhandle = s31_can_errhandle,
  .co_txready = s31_can_txready,
  .co_txempty = s31_can_txempty,
};

static struct can_dev_s g_can = { .cd_ops = &g_ops };

int esp32s31_canfd_initialize(int tx_pin, int rx_pin)
{
  uint32_t clock = esp_clk_xtal_freq();
  uint32_t nominal = CONFIG_ESP32S31_CANFD_BITRATE;
  uint32_t data = CONFIG_ESP32S31_CANFD_DATA_BITRATE;
  int ret;
  if (tx_pin < 0 || tx_pin >= SOC_GPIO_PIN_COUNT ||
      rx_pin < 0 || rx_pin >= SOC_GPIO_PIN_COUNT ||
      (LOOPBACK ? tx_pin != rx_pin : tx_pin == rx_pin) ||
      (SOC_GPIO_VALID_OUTPUT_GPIO_MASK & (1ull << tx_pin)) == 0 ||
      (SOC_GPIO_VALID_GPIO_MASK & (1ull << rx_pin)) == 0)
    {
      return -EINVAL;
    }

  /* Fixed 20 time quanta, 80% primary sample point, exact prescaler only. */

  if (nominal == 0 || data == 0 || clock == 0 ||
      nominal > clock / 20 || data > clock / 20 ||
      clock % (20 * nominal) != 0 || clock % (20 * data) != 0 ||
      clock / (20 * nominal) > TWAI_LL_BRP_MAX ||
      clock / (20 * data) > TWAI_LL_BRP_MAX_FD)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_registered)
    {
      ret = tx_pin == g_txpin && rx_pin == g_rxpin ? OK : -EBUSY;
    }
  else
    {
      g_txpin = tx_pin;
      g_rxpin = rx_pin;
      g_nominal = (twai_timing_advanced_config_t)
        { .brp = clock / (20 * nominal), .tseg_1 = 15, .tseg_2 = 4, .sjw = 4 };
      g_data = g_nominal;
      g_data.brp = clock / (20 * data);
      ret = can_register("/dev/can0", &g_can);
      g_registered = ret == OK;
    }

  nxmutex_unlock(&g_lock);
  return ret;
}
