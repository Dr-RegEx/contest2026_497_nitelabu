/* SPDX-License-Identifier: Apache-2.0 */

#include <nuttx/config.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/mutex.h>
#include "esp_private/esp_clk.h"
#include "hal/i2c_ll.h"
#include "soc/lp_gpio_struct.h"
#include "soc/lp_gpio_sig_map.h"
#include "soc/lp_iomux_struct.h"
#include "soc/lp_system_struct.h"
#include "esp32s31_lpi2c.h"

#define HW (&LP_I2C)
#define FIFO_BYTES 16
#define WAIT_STEPS 2500
#define SUPPORTED_FLAGS (I2C_M_READ | I2C_M_NOSTOP | I2C_M_NOSTART)

static mutex_t g_lock = NXMUTEX_INITIALIZER;
static int g_refs;
static bool g_configured;
static int g_pin[2];             /* SCL, SDA */
static uint32_t g_clock;
static uint32_t g_mux;
static uint32_t g_enable;
static uint32_t g_iomux[2];
static uint32_t g_pincfg[2];
static uint32_t g_out[2];
static uint32_t g_in[2];
static uint32_t g_frequency;

static int lp_pins(void)
{
  lp_gpio_funca_in_sel_cfg_reg_t input = {0};
  lp_gpio_funcn_out_sel_cfg_reg_t output = {0};
  uint32_t mask = (1u << g_pin[0]) | (1u << g_pin[1]);
  uint32_t mux;
  unsigned int i;
  int pin;

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

  mux = HAL_FORCE_READ_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel);
  g_mux = mux & mask;
  g_enable = LP_GPIO.enable.val & mask;
  LP_GPIO.enable_w1tc.val = mask;
  for (i = 0; i < 2; i++)
    {
      pin = g_pin[i];
      g_iomux[i] = LP_IO_MUX.gpion[pin].val;
      g_pincfg[i] = LP_GPIO.pinn[pin].val;
      g_out[i] = LP_GPIO.funcn_out_sel_cfg[pin].val;
      g_in[i] = LP_GPIO.funca_in_sel_cfg[i].val;
      LP_IO_MUX.gpion[pin].gpion_mcu_sel = 1;
      LP_IO_MUX.gpion[pin].gpion_fun_ie = 1;
      LP_IO_MUX.gpion[pin].gpion_fun_wpu = 1;
      LP_IO_MUX.gpion[pin].gpion_fun_wpd = 0;
      LP_IO_MUX.gpion[pin].gpion_slp_sel = 0;
      LP_GPIO.pinn[pin].pinn_pad_driver = 1;
      input.funca_in_sel = pin;
      input.siga_in_sel = 1;
      LP_GPIO.funca_in_sel_cfg[i].val = input.val;
      output.funcn_out_sel = i == 0 ? LP_I2C_SCL_PAD_OUT_IDX :
                                      LP_I2C_SDA_PAD_OUT_IDX;
      output.funcn_oe_sel = 0;  /* Peripheral controls open-drain enable. */
      LP_GPIO.funcn_out_sel_cfg[pin].val = output.val;
    }

  HAL_FORCE_MODIFY_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel, mux | mask);
  LP_GPIO.enable_w1ts.val = mask;
  return OK;
}

static void lp_unpins(void)
{
  uint32_t mask = (1u << g_pin[0]) | (1u << g_pin[1]);
  uint32_t mux;
  unsigned int i;
  int pin;

  LP_GPIO.enable_w1tc.val = mask;
  for (i = 0; i < 2; i++)
    {
      pin = g_pin[i];
      LP_GPIO.funca_in_sel_cfg[i].val = g_in[i];
      LP_GPIO.funcn_out_sel_cfg[pin].val = g_out[i];
      LP_GPIO.pinn[pin].val = g_pincfg[i];
      LP_IO_MUX.gpion[pin].val = g_iomux[i];
    }

  mux = HAL_FORCE_READ_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel);
  HAL_FORCE_MODIFY_U32_REG_FIELD(LP_SYS.padctrl, pad_mux_sel,
                                (mux & ~mask) | g_mux);
  LP_GPIO.enable_w1ts.val = g_enable;

  /* LP GPIO clocks are shared; do not turn them off here. */
}

static void lp_frequency(uint32_t frequency)
{
  i2c_hal_clk_config_t timing = {0};
  i2c_ll_master_cal_bus_clk(esp_clk_xtal_freq(), frequency, &timing);
  i2c_ll_master_set_bus_timing(HW, &timing);
  i2c_ll_update(HW);
  g_frequency = frequency;
}

static void lp_hwinit(uint32_t frequency)
{
  lp_i2c_ll_enable_bus_clock(0, true);
  lp_i2c_ll_reset_register(0);
  lp_i2c_ll_set_source_clk(HW, LP_I2C_SCLK_XTAL);
  i2c_ll_disable_intr_mask(HW, UINT32_MAX);
  i2c_ll_clear_intr_mask(HW, UINT32_MAX);
  i2c_ll_set_mode(HW, I2C_BUS_MODE_MASTER);
  i2c_ll_enable_pins_open_drain(HW, true);
  i2c_ll_enable_arbitration(HW, true);
  i2c_ll_master_rx_full_ack_level(HW, false);
  i2c_ll_set_data_mode(HW, I2C_DATA_MODE_MSB_FIRST, I2C_DATA_MODE_MSB_FIRST);
  i2c_ll_enable_fifo_mode(HW, true);
  i2c_ll_txfifo_rst(HW);
  i2c_ll_rxfifo_rst(HW);
  i2c_ll_master_set_filter(HW, 3);
  lp_frequency(frequency);
}

/* Called with g_lock held; character opens and direct clients share refs. */

static int lp_acquire(void)
{
  int ret;
  if (!g_configured)
    {
      return -ENODEV;
    }

  if (g_refs == INT_MAX)
    {
      return -EMFILE;
    }

  if (g_refs == 0)
    {
      g_clock = LP_PERI_CLKRST.i2c_ctrl.val;
      lp_hwinit(100000);
      ret = lp_pins();
      if (ret < 0)
        {
          LP_PERI_CLKRST.i2c_ctrl.val = g_clock;
          return ret;
        }
    }

  g_refs++;
  return OK;
}

static int lp_setup(struct i2c_master_s *dev)
{
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = lp_acquire();
  nxmutex_unlock(&g_lock);
  return ret;
}

static int lp_shutdown(struct i2c_master_s *dev)
{
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_refs == 0)
    {
      ret = -EINVAL;
    }
  else if (--g_refs == 0)
    {
      i2c_ll_disable_intr_mask(HW, UINT32_MAX);
      lp_unpins();
      lp_i2c_ll_reset_register(0);
      LP_PERI_CLKRST.i2c_ctrl.val = g_clock;
    }

  nxmutex_unlock(&g_lock);
  return ret;
}

static void lp_command(unsigned int index, unsigned int op,
                        unsigned int bytes, bool nack)
{
  i2c_ll_hw_cmd_t command = {0};
  command.op_code = op;
  command.byte_num = bytes;
  command.ack_en = op == I2C_LL_CMD_WRITE;
  command.ack_val = nack;
  i2c_ll_master_write_cmd_reg(HW, command, index);
}

/* Each hardware list ends with END (hold bus) or STOP (release bus).
 * LP FIFO capacity is 16, unlike the shared LL's 32-byte HP default.
 */

static int lp_execute(bool stop)
{
  uint32_t status;
  unsigned int i;
  i2c_ll_clear_intr_mask(HW, UINT32_MAX);
  i2c_ll_update(HW);
  i2c_ll_start_trans(HW);
  for (i = 0; i < WAIT_STEPS; i++)
    {
      up_udelay(20);
      i2c_ll_get_intr_raw_mask(HW, &status);
      if (status & I2C_LL_INTR_NACK)
        {
          return -ENXIO;
        }

      if (status & (I2C_LL_INTR_TIMEOUT | I2C_LL_INTR_ST_TO))
        {
          return -ETIMEDOUT;
        }

      if (status & I2C_LL_INTR_ARBITRATION)
        {
          return -EAGAIN;
        }

      if (status & (stop ? I2C_LL_INTR_MST_COMPLETE : I2C_LL_INTR_END_DETECT))
        {
          return OK;
        }
    }

  return -ETIMEDOUT;
}

static int lp_address(const struct i2c_msg_s *msg)
{
  uint8_t address = (msg->addr << 1) | ((msg->flags & I2C_M_READ) != 0);
  i2c_ll_txfifo_rst(HW);
  i2c_ll_rxfifo_rst(HW);
  i2c_ll_write_txfifo(HW, &address, 1);
  lp_command(0, I2C_LL_CMD_RESTART, 0, false);
  lp_command(1, I2C_LL_CMD_WRITE, 1, false);
  lp_command(2, I2C_LL_CMD_END, 0, false);
  return lp_execute(false);
}

static int lp_data(struct i2c_msg_s *msg, bool continuation)
{
  ssize_t offset = 0;
  uint32_t available;
  unsigned int bytes;
  unsigned int commands;
  bool read = (msg->flags & I2C_M_READ) != 0;
  bool nack;
  int ret;

  while (offset < msg->length)
    {
      bytes = msg->length - offset > FIFO_BYTES ? FIFO_BYTES :
                                                msg->length - offset;
      commands = 0;
      nack = read && !continuation && offset + bytes == msg->length;
      i2c_ll_txfifo_rst(HW);
      i2c_ll_rxfifo_rst(HW);
      if (!read)
        {
          i2c_ll_write_txfifo(HW, msg->buffer + offset, bytes);
          lp_command(commands++, I2C_LL_CMD_WRITE, bytes, false);
        }
      else
        {
          if (bytes > (unsigned int)nack)
            {
              lp_command(commands++, I2C_LL_CMD_READ, bytes - nack, false);
            }

          if (nack)
            {
              lp_command(commands++, I2C_LL_CMD_READ, 1, true);
            }
        }

      lp_command(commands, I2C_LL_CMD_END, 0, false);
      ret = lp_execute(false);
      if (ret < 0)
        {
          return ret;
        }

      if (read)
        {
          i2c_ll_get_rxfifo_cnt(HW, &available);
          if (available != bytes)
            {
              return -EIO;
            }

          i2c_ll_read_rxfifo(HW, msg->buffer + offset, bytes);
        }

      offset += bytes;
    }

  return OK;
}

static int lp_validate(struct i2c_msg_s *msgs, int count)
{
  int i;
  if (msgs == NULL || count < 1)
    {
      return -EINVAL;
    }

  for (i = 0; i < count; i++)
    {
      if (msgs[i].flags & ~SUPPORTED_FLAGS)
        {
          return -ENOTSUP;
        }

      if (msgs[i].addr > 0x7f || msgs[i].length < 0 ||
          (msgs[i].length && msgs[i].buffer == NULL) ||
          (!msgs[i].length && (msgs[i].flags & I2C_M_READ)) ||
          msgs[i].frequency < 10000 || msgs[i].frequency > 400000)
        {
          return -EINVAL;
        }

      if (msgs[i].flags & I2C_M_NOSTART)
        {
          if (i == 0 || msgs[i].addr != msgs[i - 1].addr ||
              ((msgs[i].flags ^ msgs[i - 1].flags) & I2C_M_READ))
            {
              return -EINVAL;
            }
        }

      if (i > 0 && ((msgs[i].flags & I2C_M_NOSTART) ||
                    (msgs[i - 1].flags & I2C_M_NOSTOP)) &&
          msgs[i].frequency != msgs[i - 1].frequency)
        {
          return -EINVAL;
        }
    }

  /* A call owns a complete transaction, never a bus held across clients. */

  return msgs[count - 1].flags & I2C_M_NOSTOP ? -ENOTSUP : OK;
}

static int lp_transfer(struct i2c_master_s *dev, struct i2c_msg_s *msgs,
                        int count)
{
  int ret = lp_validate(msgs, count);
  int i;
  bool continuation;
  if (ret < 0)
    {
      return ret;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_refs == 0)
    {
      ret = -ENODEV;
      goto out;
    }

  if (i2c_ll_is_bus_busy(HW))
    {
      ret = -EBUSY;
      goto out;
    }

  for (i = 0; i < count; i++)
    {
      continuation = i + 1 < count && (msgs[i + 1].flags & I2C_M_NOSTART);
      if (g_frequency != msgs[i].frequency)
        {
          lp_frequency(msgs[i].frequency);
        }

      if (!(msgs[i].flags & I2C_M_NOSTART))
        {
          ret = lp_address(&msgs[i]);
          if (ret < 0)
            {
              break;
            }
        }

      ret = lp_data(&msgs[i], continuation);
      if (ret < 0)
        {
          break;
        }

      if (!(msgs[i].flags & I2C_M_NOSTOP) && !continuation)
        {
          lp_command(0, I2C_LL_CMD_STOP, 0, false);
          ret = lp_execute(true);
          if (ret < 0)
            {
              break;
            }
        }
    }

  if (ret < 0)
    {
      /* Preserve the original error and release/reset our controller. */

      lp_command(0, I2C_LL_CMD_STOP, 0, false);
      lp_execute(true);
      lp_hwinit(g_frequency);
    }

out:
  nxmutex_unlock(&g_lock);
  return ret;
}

#ifdef CONFIG_I2C_RESET
static int lp_reset(struct i2c_master_s *dev)
{
  unsigned int i;
  int ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_refs == 0)
    {
      ret = -ENODEV;
      goto out;
    }

  lp_hwinit(g_frequency);
  i2c_ll_master_clr_bus(HW, 9, true);
  for (i = 0; i < WAIT_STEPS; i++)
    {
      up_udelay(20);
      if (!i2c_ll_master_is_bus_clear_done(HW))
        {
          break;
        }
    }

  i2c_ll_master_clr_bus(HW, 9, false);
  ret = i == WAIT_STEPS ? -ETIMEDOUT : OK;
out:
  nxmutex_unlock(&g_lock);
  return ret;
}
#endif

static const struct i2c_ops_s g_ops =
{
  .transfer = lp_transfer,
#ifdef CONFIG_I2C_RESET
  .reset = lp_reset,
#endif
  .setup = lp_setup,
  .shutdown = lp_shutdown
};

static struct i2c_master_s g_dev = {.ops = &g_ops};

struct i2c_master_s *esp32s31_lpi2cbus_initialize(int scl, int sda)
{
  int ret;
  if (scl < 0 || scl > 7 || sda < 0 || sda > 7 || scl == sda)
    {
      set_errno(EINVAL);
      return NULL;
    }

  ret = nxmutex_lock(&g_lock);
  if (ret < 0)
    {
      set_errno(-ret);
      return NULL;
    }

  if (g_configured && (scl != g_pin[0] || sda != g_pin[1]))
    {
      ret = -EBUSY;
    }
  else
    {
      g_pin[0] = scl;
      g_pin[1] = sda;
      g_configured = true;
      ret = lp_acquire();
    }

  nxmutex_unlock(&g_lock);
  if (ret < 0)
    {
      set_errno(-ret);
      return NULL;
    }

  return &g_dev;
}

int esp32s31_lpi2cbus_uninitialize(struct i2c_master_s *dev)
{
  return dev == &g_dev ? lp_shutdown(dev) : -EINVAL;
}
