/* SPDX-License-Identifier: Apache-2.0 */
/* S31 hardware SPI2 polling master, using the pinned GPSPI HAL. */
#include <nuttx/config.h>
#include <nuttx/spi/spi.h>
#include <nuttx/mutex.h>
#include <nuttx/clock.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <debug.h>
#include <nuttx/arch.h>
#include "espressif/esp_gpio.h"
#include "espressif/esp_spi.h"
#include "hal/spi_hal.h"
#include "esp_clk_tree.h"
#include "esp_private/periph_ctrl.h"

struct s31_spi_s
{
  struct spi_dev_s dev;
  mutex_t lock;
  spi_hal_context_t hal;
  spi_hal_dev_config_t cfg;
  bool initialized;
  int bits;
};

static int s31_lock(struct spi_dev_s *dev, bool lock)
{
  struct s31_spi_s *p = (struct s31_spi_s *)dev;
  return lock ? nxmutex_lock(&p->lock) : nxmutex_unlock(&p->lock);
}

static void s31_select(struct spi_dev_s *dev, uint32_t id, bool selected)
{
  /* Software CS holds the entire exchange, including FIFO-sized chunks. */
  esp_gpiowrite(CONFIG_ESPRESSIF_SPI2_CSPIN, !selected);
}

static uint32_t s31_frequency(struct spi_dev_s *dev, uint32_t hz)
{
  struct s31_spi_s *p = (struct s31_spi_s *)dev;
  spi_hal_timing_param_t timing =
    {
      .expected_freq = hz,
      .duty_cycle = 128,
      .use_gpio = true
    };

  if (hz == 0 || esp_clk_tree_src_get_freq_hz(SPI_CLK_SRC_XTAL,
      ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &timing.clk_src_hz) != ESP_OK)
    {
      return 0;
    }

  /* S31 master clock has a mandatory divide-by-two stage. */
  timing.clk_src_hz /= 2;
  if (spi_hal_cal_clock_conf(&timing, &p->cfg.timing_conf) != ESP_OK)
    {
      return 0;
    }

  return p->cfg.timing_conf.real_freq;
}

static void s31_mode(struct spi_dev_s *dev, enum spi_mode_e mode)
{
  struct s31_spi_s *p = (struct s31_spi_s *)dev;
  DEBUGASSERT(mode >= SPIDEV_MODE0 && mode <= SPIDEV_MODE3);
  p->cfg.mode = mode;
}

static void s31_bits(struct spi_dev_s *dev, int bits)
{
  struct s31_spi_s *p = (struct s31_spi_s *)dev;
  p->bits = bits;
}

static uint8_t s31_status(struct spi_dev_s *dev, uint32_t id)
{
  return SPI_STATUS_PRESENT;
}

static void s31_exchange(struct spi_dev_s *dev, const void *tx,
                         void *rx, size_t words)
{
  struct s31_spi_s *p = (struct s31_spi_s *)dev;
  const uint8_t *src = tx;
  uint8_t *dst = rx;
  uint8_t filler[64];

  if (p->bits != 8)
    {
      spierr("ERROR: S31 SPI polling supports 8-bit words only\n");
      return;
    }

  memset(filler, 0xff, sizeof(filler));
  spi_hal_setup_device(&p->hal, &p->cfg);
  while (words > 0)
    {
      size_t n = words > sizeof(filler) ? sizeof(filler) : words;
      spi_hal_trans_config_t t =
        {
          .tx_bitlen = n * 8,
          .rx_bitlen = n * 8,
          .send_buffer = (uint8_t *)(src ? src : filler),
          .rcv_buffer = dst,
          .line_mode = {.cmd_lines = 1, .addr_lines = 1, .data_lines = 1}
        };
      clock_t start = clock_systime_ticks();

      spi_hal_setup_trans(&p->hal, &p->cfg, &t);
      spi_ll_write_buffer(p->hal.hw, t.send_buffer, n * 8);
      spi_hal_enable_data_line(p->hal.hw, true, dst != NULL);
      spi_hal_user_start(&p->hal);
      while (!spi_hal_usr_is_done(&p->hal))
        {
          if (clock_systime_ticks() - start > SEC2TICK(1))
            {
              spierr("ERROR: S31 SPI transaction timed out\n");
              return;
            }
        }

      if (dst)
        {
          spi_ll_read_buffer(p->hal.hw, dst, n * 8);
          dst += n;
        }

      if (src)
        {
          src += n;
        }

      words -= n;
    }
}

static uint32_t s31_send(struct spi_dev_s *dev, uint32_t word)
{
  uint8_t tx = word;
  uint8_t rx = 0;
  s31_exchange(dev, &tx, &rx, 1);
  return rx;
}

static const struct spi_ops_s g_ops =
{
  .lock = s31_lock,
  .select = s31_select,
  .setfrequency = s31_frequency,
  .setmode = s31_mode,
  .setbits = s31_bits,
  .status = s31_status,
  .send = s31_send,
  .exchange = s31_exchange
};

static struct s31_spi_s g_spi =
{
  .dev = {.ops = &g_ops},
  .lock = NXMUTEX_INITIALIZER,
  .bits = 8
};

struct spi_dev_s *esp_spibus_initialize(int port)
{
  struct s31_spi_s *p = &g_spi;
  if (port != 2)
    {
      return NULL;
    }

  nxmutex_lock(&p->lock);
  if (!p->initialized)
    {

      PERIPH_RCC_ATOMIC()
        {

          spi_ll_enable_bus_clock(SPI2_HOST, true);

          spi_ll_reset_register(SPI2_HOST);

          spi_ll_clk_source_pre_div(SPI_LL_GET_HW(SPI2_HOST), 1, 2);

          spi_ll_set_clk_source(SPI_LL_GET_HW(SPI2_HOST), SPI_CLK_SRC_XTAL);

          spi_ll_enable_clock(SPI2_HOST, true);
        }


      esp_gpiowrite(CONFIG_ESPRESSIF_SPI2_CSPIN, true);
      esp_configgpio(CONFIG_ESPRESSIF_SPI2_CSPIN, OUTPUT_FUNCTION_2);
      esp_gpio_matrix_out(CONFIG_ESPRESSIF_SPI2_CSPIN, SIG_GPIO_OUT_IDX, 0, 0);
      esp_configgpio(CONFIG_ESPRESSIF_SPI2_CLKPIN, OUTPUT_FUNCTION_2);
      esp_gpio_matrix_out(CONFIG_ESPRESSIF_SPI2_CLKPIN, SPI2_CK_PAD_OUT_IDX, 0, 0);
      esp_configgpio(CONFIG_ESPRESSIF_SPI2_MOSIPIN, OUTPUT_FUNCTION_2);
      esp_gpio_matrix_out(CONFIG_ESPRESSIF_SPI2_MOSIPIN, SPI2_D_PAD_OUT_IDX, 0, 0);
      esp_configgpio(CONFIG_ESPRESSIF_SPI2_MISOPIN, INPUT_FUNCTION_2 | PULLUP);
      esp_gpio_matrix_in(CONFIG_ESPRESSIF_SPI2_MISOPIN, SPI2_Q_PAD_IN_IDX, 0);

      p->hal.hw = SPI_LL_GET_HW(SPI2_HOST);
      spi_ll_master_init(p->hal.hw);

      /* Poll raw completion status; do not raise an unmapped interrupt. */
      spi_ll_disable_int(p->hal.hw);
      spi_ll_set_mosi_delay(p->hal.hw, 0, 0);


      p->cfg.cs_pin_id = 0;
      p->cfg.timing_conf.clock_source = SPI_CLK_SRC_XTAL;
      PERIPH_RCC_ATOMIC()
        {
          spi_ll_clk_source_pre_div(p->hal.hw, 1, 2);
          spi_ll_set_clk_source(p->hal.hw, SPI_CLK_SRC_XTAL);
        }
      p->cfg.timing_conf.source_pre_div = 2;

      s31_frequency(&p->dev, 100000);

      p->initialized = true;
    }

  nxmutex_unlock(&p->lock);
  return &p->dev;
}

int esp_spibus_uninitialize(struct spi_dev_s *dev)
{
  /* Static board bus: retain initialization for subsequent opens. */
  return dev == &g_spi.dev ? OK : -EINVAL;
}
