/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <stdint.h>
#include <termios.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/mutex.h>
#include <nuttx/serial/serial.h>
#include "espressif/esp_irq.h"
#include "esp_private/esp_clk.h"
#include "hal/uart_hal.h"
#include "soc/interrupts.h"
#include "soc/lp_gpio_struct.h"
#include "soc/lp_gpio_sig_map.h"
#include "soc/lp_iomux_struct.h"
#include "esp32s31_lpuart.h"

#define HW (&LP_UART)
#define LP_IRQ ESP_SOURCE2IRQ(ETS_LP_UART_INTR_SOURCE)
#define RX_MASK (UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT | \
                 UART_INTR_RXFIFO_OVF | UART_INTR_PARITY_ERR | \
                 UART_INTR_FRAM_ERR)
#define TX_MASK (UART_INTR_TXFIFO_EMPTY | UART_INTR_TX_DONE)

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static int g_cpuint = -1;
static int g_txpin;
static int g_rxpin;
static bool g_loopback;
static bool g_registered;
static bool g_active;
static bool g_online;
static bool g_pins;
static uint32_t g_baud = CONFIG_ESP32S31_LPUART_BAUD;
static tcflag_t g_format = CS8;
static uint32_t g_clock;
static uint32_t g_memory;
static uint32_t g_pad_mux;
static uint32_t g_pad_enable;
static uint32_t g_pad_config[2];
static uint32_t g_pad_output[2];
static uint32_t g_input;

/* LP UART uses a separate clock domain. Do not use the unbounded vendor
 * register-update helper: a stopped peripheral must not hang an open/ioctl.
 */

static int s31_lp_sync(void)
{
  unsigned int i;
  HW->reg_update.reg_update = 1;
  for (i = 0; i < 1000; i++)
    {
      up_udelay(1);
      if (!HW->reg_update.reg_update)
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

static int s31_lp_pins(void)
{
  lp_gpio_funca_in_sel_cfg_reg_t input = {0};
  lp_gpio_funcn_out_sel_cfg_reg_t output = {0};
  uint32_t mask;
  unsigned int i;
  int pin;

  if (g_loopback)
    {
      return OK;
    }

  LP_PERI_CLKRST.iomux_ctrl.lp_iomux_clk_en = 1;
  LP_GPIO.clock_gate.clk_en = 1;
  for (i = 0; i < 1000; i++)
    {
      if (LP_PERI_CLKRST.iomux_ctrl.lp_iomux_clk_en &&
          LP_GPIO.clock_gate.clk_en)
        {
          break;
        }

      up_udelay(1);
    }

  if (i == 1000)
    {
      return -ETIMEDOUT;
    }

  mask = (1u << g_txpin) | (1u << g_rxpin);
  g_pad_mux = HAL_FORCE_READ_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel) & mask;
  g_pad_enable = LP_GPIO.enable.val & mask;
  g_input = LP_GPIO.funca_in_sel_cfg[LP_UART_RXD_PAD_IN_IDX].val;
  LP_GPIO.enable_w1tc.val = mask;
  for (i = 0; i < 2; i++)
    {
      pin = i == 0 ? g_txpin : g_rxpin;
      g_pad_config[i] = LP_IO_MUX.gpion[pin].val;
      g_pad_output[i] = LP_GPIO.funcn_out_sel_cfg[pin].val;
      LP_IO_MUX.gpion[pin].gpion_mcu_sel = 1;
      LP_IO_MUX.gpion[pin].gpion_fun_ie = i == 1;
    }

  HAL_FORCE_MODIFY_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel,
    HAL_FORCE_READ_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel) | mask);
  input.funca_in_sel = g_rxpin;
  input.siga_in_sel = 1;
  LP_GPIO.funca_in_sel_cfg[LP_UART_RXD_PAD_IN_IDX].val = input.val;
  output.funcn_out_sel = LP_UART_TXD_PAD_OUT_IDX;
  output.funcn_oe_sel = 1;
  LP_GPIO.funcn_out_sel_cfg[g_txpin].val = output.val;
  LP_GPIO.enable_w1ts.val = 1u << g_txpin;
  g_pins = true;
  return OK;
}

static void s31_lp_unpins(void)
{
  uint32_t mask;
  if (!g_pins)
    {
      return;
    }

  mask = (1u << g_txpin) | (1u << g_rxpin);
  LP_GPIO.enable_w1tc.val = mask;
  LP_GPIO.funca_in_sel_cfg[LP_UART_RXD_PAD_IN_IDX].val = g_input;
  LP_GPIO.funcn_out_sel_cfg[g_txpin].val = g_pad_output[0];
  LP_GPIO.funcn_out_sel_cfg[g_rxpin].val = g_pad_output[1];
  LP_IO_MUX.gpion[g_txpin].val = g_pad_config[0];
  LP_IO_MUX.gpion[g_rxpin].val = g_pad_config[1];
  HAL_FORCE_MODIFY_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel,
    (HAL_FORCE_READ_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel) & ~mask) |
    g_pad_mux);
  LP_GPIO.enable_w1ts.val = g_pad_enable;
  g_pins = false;

  /* LP GPIO clocks are shared with other LP devices; leave them enabled. */
}

static int s31_lp_configure(uint32_t baud, tcflag_t format)
{
  uint64_t source = esp_clk_xtal_freq();
  uint64_t divisor;
  uint32_t prescale;
  uint32_t bits;

  if (baud == 0 || baud > 1000000 ||
      (format & (CCTS_OFLOW | CRTS_IFLOW)) != 0)
    {
      return -EINVAL;
    }

  switch (format & CSIZE)
    {
      case CS5:
        bits = 0;
        break;
      case CS6:
        bits = 1;
        break;
      case CS7:
        bits = 2;
        break;
      case CS8:
        bits = 3;
        break;
      default:
        return -EINVAL;
    }

  divisor = (uint64_t)UART_CLKDIV_V * baud;
  prescale = (source + divisor - 1) / divisor;
  if (prescale == 0 ||
      prescale > LP_PERICLKRST_LP_UART_CLK_DIV_NUM_V + 1)
    {
      return -EINVAL;
    }

  divisor = (source << 4) / ((uint64_t)baud * prescale);
  if (divisor < 16 || (divisor >> 4) > UART_CLKDIV_V)
    {
      return -EINVAL;
    }

  HAL_FORCE_MODIFY_U32_REG_FIELD(LP_PERI_CLKRST.uart_ctrl,
                                lp_uart_clk_div_num, prescale - 1);
  HW->clkdiv_sync.clkdiv = divisor >> 4;
  HW->clkdiv_sync.clkdiv_frag = divisor & 15;
  HW->conf0_sync.bit_num = bits;
  HW->conf0_sync.parity_en = (format & PARENB) != 0;
  HW->conf0_sync.parity = (format & PARODD) != 0;
  HW->conf0_sync.stop_bit_num = (format & CSTOPB) ? 3 : 1;
  HW->conf0_sync.loopback = g_loopback;
  return s31_lp_sync();
}

static void s31_lp_shutdown(uart_dev_t *dev)
{
  if (g_active)
    {
      uart_ll_disable_intr_mask(HW, UINT32_MAX);
      s31_lp_unpins();
      lp_uart_ll_reset_register(4);
      lp_uart_ll_sclk_disable(4);
      LP_SYS.uart_mem_lp_ctrl.val = g_memory;
      LP_PERI_CLKRST.uart_ctrl.val = g_clock;
      g_active = false;
      g_online = false;
    }
}

static int s31_lp_setup(uart_dev_t *dev)
{
  int ret;
  g_clock = LP_PERI_CLKRST.uart_ctrl.val;
  g_memory = LP_SYS.uart_mem_lp_ctrl.val;
  lp_uart_ll_enable_bus_clock(4, true);
  lp_uart_ll_reset_register(4);
  lp_uart_ll_mem_force_power_on();
  lp_uart_ll_set_source_clk(HW, LP_UART_SCLK_XTAL);
  lp_uart_ll_sclk_enable(4);
  g_active = true;
  uart_ll_disable_intr_mask(HW, UINT32_MAX);
  uart_ll_clr_intsts_mask(HW, UINT32_MAX);
  HW->conf0_sync.tx_flow_en = 0;
  HW->hwfc_conf_sync.rx_flow_en = 0;
  HW->swfc_conf0_sync.sw_flow_con_en = 0;
  HW->tout_conf_sync.rx_tout_en = 1;
  HW->tout_conf_sync.rx_tout_thrhd = 10;
  uart_ll_set_rxfifo_full_thr(HW, 1);
  uart_ll_set_txfifo_empty_thr(HW, 4);
  ret = s31_lp_configure(g_baud, g_format);
  if (ret == OK)
    {
      HW->conf0_sync.rxfifo_rst = 1;
      HW->conf0_sync.txfifo_rst = 1;
      ret = s31_lp_sync();
    }

  if (ret == OK)
    {
      HW->conf0_sync.rxfifo_rst = 0;
      HW->conf0_sync.txfifo_rst = 0;
      ret = s31_lp_sync();
    }

  if (ret == OK)
    {
      ret = s31_lp_pins();
    }

  if (ret < 0)
    {
      s31_lp_shutdown(dev);
      return ret;
    }

  g_online = true;
  return OK;
}

static int s31_lp_interrupt(int irq, void *context, void *arg)
{
  uart_dev_t *dev = arg;
  uint32_t status = uart_ll_get_intsts_mask(HW);
  uart_ll_clr_intsts_mask(HW, status);
  if (status & RX_MASK)
    {
      uart_recvchars(dev);
    }

  if (status & TX_MASK)
    {
      uart_xmitchars(dev);
    }

  return OK;
}

static int s31_lp_attach(uart_dev_t *dev)
{
  int ret;
  g_cpuint = esp_setup_irq(ETS_LP_UART_INTR_SOURCE,
                           ESP_IRQ_PRIORITY_DEFAULT, ESP_IRQ_TRIGGER_LEVEL);
  if (g_cpuint < 0)
    {
      return g_cpuint;
    }

  ret = irq_attach(LP_IRQ, s31_lp_interrupt, dev);
  if (ret < 0)
    {
      esp_teardown_irq(ETS_LP_UART_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
      return ret;
    }

  up_enable_irq(LP_IRQ);
  return OK;
}

static void s31_lp_detach(uart_dev_t *dev)
{
  if (g_cpuint >= 0)
    {
      uart_ll_disable_intr_mask(HW, UINT32_MAX);
      up_disable_irq(LP_IRQ);
      irq_detach(LP_IRQ);
      esp_teardown_irq(ETS_LP_UART_INTR_SOURCE, g_cpuint);
      g_cpuint = -1;
    }
}

static bool s31_lp_txempty(uart_dev_t *dev)
{
  return !g_online || uart_ll_is_tx_idle(HW);
}

static int s31_lp_ioctl(struct file *filep, int cmd, unsigned long arg)
{
#ifdef CONFIG_SERIAL_TERMIOS
  uart_dev_t *dev = filep->f_inode->i_private;
  struct termios *term = (struct termios *)arg;
  irqstate_t flags;
  uint32_t baud;
  uint32_t mask;
  int ret;

  if (cmd == TCGETS || cmd == TCSETS)
    {
      if (term == NULL)
        {
          return -EINVAL;
        }

      if (cmd == TCGETS)
        {
          term->c_cflag = g_format;
          cfsetispeed(term, g_baud);
          return OK;
        }

      baud = cfgetispeed(term);
      flags = enter_critical_section();
      if (!g_online || dev->xmit.head != dev->xmit.tail ||
          !s31_lp_txempty(dev))
        {
          leave_critical_section(flags);
          return -EBUSY;
        }

      mask = uart_ll_get_intr_ena_status(HW);
      uart_ll_disable_intr_mask(HW, UINT32_MAX);
      ret = s31_lp_configure(baud, term->c_cflag);
      if (ret == OK)
        {
          g_baud = baud;
          g_format = term->c_cflag & (CSIZE | PARENB | PARODD | CSTOPB);
        }
      else if (ret == -ETIMEDOUT &&
               s31_lp_configure(g_baud, g_format) < 0)
        {
          g_online = false;
          mask = 0;
        }

      uart_ll_ena_intr_mask(HW, mask);
      leave_critical_section(flags);
      return ret;
    }
#endif

  return -ENOTTY;
}

static int s31_lp_receive(uart_dev_t *dev, unsigned int *status)
{
  uint8_t byte;
  *status = uart_ll_get_intraw_mask(HW) &
    (UART_INTR_PARITY_ERR | UART_INTR_FRAM_ERR | UART_INTR_RXFIFO_OVF);
  uart_ll_read_rxfifo(HW, &byte, 1);
  return byte;
}

static void s31_lp_rxint(uart_dev_t *dev, bool enable)
{
  irqstate_t flags = enter_critical_section();
  if (enable && g_online)
    {
      uart_ll_ena_intr_mask(HW, RX_MASK);
    }
  else
    {
      uart_ll_disable_intr_mask(HW, RX_MASK);
    }

  leave_critical_section(flags);
}

static bool s31_lp_rxavailable(uart_dev_t *dev)
{
  return g_online && uart_ll_get_rxfifo_len(HW) != 0;
}

static void s31_lp_send(uart_dev_t *dev, int ch)
{
  uint8_t byte = ch;
  uart_ll_write_txfifo(HW, &byte, 1);
}

static void s31_lp_txint(uart_dev_t *dev, bool enable)
{
  irqstate_t flags = enter_critical_section();
  if (enable && g_online)
    {
      uart_ll_ena_intr_mask(HW, TX_MASK);
    }
  else
    {
      uart_ll_disable_intr_mask(HW, TX_MASK);
    }

  leave_critical_section(flags);
}

static bool s31_lp_txready(uart_dev_t *dev)
{
  return g_online && uart_ll_get_txfifo_len(HW) != 0;
}

static const struct uart_ops_s g_ops =
{
  .setup = s31_lp_setup,
  .shutdown = s31_lp_shutdown,
  .attach = s31_lp_attach,
  .detach = s31_lp_detach,
  .ioctl = s31_lp_ioctl,
  .receive = s31_lp_receive,
  .rxint = s31_lp_rxint,
  .rxavailable = s31_lp_rxavailable,
  .send = s31_lp_send,
  .txint = s31_lp_txint,
  .txready = s31_lp_txready,
  .txempty = s31_lp_txempty
};

static char g_rxbuffer[256];
static char g_txbuffer[256];
static uart_dev_t g_dev =
{
  .recv = {.size = sizeof(g_rxbuffer), .buffer = g_rxbuffer},
  .xmit = {.size = sizeof(g_txbuffer), .buffer = g_txbuffer},
  .ops = &g_ops
};

int esp32s31_lpuart_initialize(int txpin, int rxpin, bool loopback)
{
  int ret;
  if ((loopback && (txpin != -1 || rxpin != -1)) ||
      (!loopback && (txpin < 0 || txpin > 7 || rxpin < 0 || rxpin > 7 ||
                    txpin == rxpin)))
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
      ret = -EBUSY;
    }
  else
    {
      g_txpin = txpin;
      g_rxpin = rxpin;
      g_loopback = loopback;
      ret = uart_register("/dev/ttyLP0", &g_dev);
      g_registered = ret == OK;
    }

  nxmutex_unlock(&g_lock);
  return ret;
}
