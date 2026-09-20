/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_lcd.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Native NuttX framebuffer for ESP32-S31-Korvo-1 / SUB3 (ST7262).
 * The initial profile is FLAT, single-core and awake. The HAL GDMA adapter
 * uses NuttX synchronization; no ESP-IDF application or FreeRTOS
 * runtime is linked by this driver.
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/signal.h>
#include <nuttx/spinlock.h>
#include <nuttx/video/fb.h>

#include "espressif/esp_gpio.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_clk_tree.h"
#include "esp_memory_utils.h"
#include "esp_private/esp_clk_tree_common.h"
#include "esp_private/gdma.h"
#include "esp_private/periph_ctrl.h"
#include "esp_rom_sys.h"
#include "hal/axi_dma_ll.h"
#include "hal/dma_types.h"
#include "hal/gdma_channel.h"
#include "hal/lcd_hal.h"
#include "hal/lcd_ll.h"
#include "hal/lcd_periph.h"
#include "soc/gpio_sig_map.h"

#if !defined(CONFIG_BUILD_FLAT) || defined(CONFIG_SMP)
#  error "The initial S31 RGB framebuffer requires a single-core FLAT build"
#endif
#ifndef CONFIG_FB_UPDATE
#  error "S31 RGB framebuffer requires CONFIG_FB_UPDATE"
#endif

#define LCD_WIDTH       800
#define LCD_HEIGHT      480
#define LCD_STRIDE      (LCD_WIDTH * 2)
#define LCD_FBSIZE      (LCD_STRIDE * LCD_HEIGHT)
#define LCD_PCLK        18000000
#define LCD_DISP_PIN    38
#define LCD_ALIGNMENT   64
#define LCD_NODE_BYTES  DMA_DESCRIPTOR_BUFFER_MAX_SIZE_64B_ALIGNED
#define LCD_NODES       ((LCD_FBSIZE + LCD_NODE_BYTES - 1) / LCD_NODE_BYTES)
#define LCD_DESC_COUNT  ((LCD_NODES + 3) & ~3)

/* The public framebuffer is a staging buffer. FBIO_UPDATE copies it into
 * the inactive scan buffer, writes back PSRAM, and waits for AXI GDMA's
 * link-switch event before releasing the old scan buffer. An ordinary EOF
 * is insufficient because AXI GDMA can have prefetched the old chain.
 * The isolated diagnostic polls the actual raw link-switch event with a
 * scheduler sleep. It does not enable GDMA interrupts: the first native
 * board run stopped responding immediately after its first DMA interrupt.
 *
 * HAL heap_caps currently ignores INTERNAL/SPIRAM capabilities in NuttX.
 * Keep descriptors in static internal SRAM and check actual frame addresses.
 */

static DRAM_ATTR dma_descriptor_align8_t g_desc[2][LCD_DESC_COUNT]
  __attribute__((aligned(LCD_ALIGNMENT)));

static const uint8_t g_data_pins[16] =
{
  8, 9, 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 33, 34, 35, 36
};

static mutex_t g_lcd_lock = NXMUTEX_INITIALIZER;
static lcd_hal_context_t g_lcd;
static gdma_channel_handle_t g_dma;
static int g_channel = -1;
static uint8_t *g_frame;
static uint8_t *g_scan[2];
static unsigned int g_front;
static bool g_ready;
static bool g_clock_enabled;
static bool g_bus_acquired;
static bool g_source_enabled;
static bool g_core_source_enabled;
static bool g_dma_connected;
static bool g_first_update;
static int g_dma_error;

static int s31_lcd_result(esp_err_t result)
{
  if (result == ESP_OK)
    {
      return OK;
    }

  if (result == ESP_ERR_NO_MEM)
    {
      return -ENOMEM;
    }

  if (result == ESP_ERR_TIMEOUT)
    {
      return -ETIMEDOUT;
    }

  return -EIO;
}

static int s31_lcd_pin(int pin, int signal)
{
  int ret = esp_configgpio(pin, OUTPUT_FUNCTION_2);
  if (ret >= 0)
    {
      esp_gpio_matrix_out(pin, signal, false, false);
    }

  return ret;
}

static void s31_lcd_stop(void)
{
  esp_gpiowrite(LCD_DISP_PIN, false);
  if (g_clock_enabled)
    {
      lcd_ll_stop(g_lcd.dev);
    }

  if (g_dma != NULL)
    {
      gdma_stop(g_dma);
      gdma_reset(g_dma);
    }

}

/* Called only while holding g_lcd_lock, before registration or after users
 * have closed/unmapped the framebuffer. The generic framebuffer lifecycle
 * must not call up_fbuninitialize while users still hold mappings.
 */

static void s31_lcd_release(void)
{
  s31_lcd_stop();
  if (g_dma != NULL)
    {
      if (g_dma_connected)
        {
          gdma_disconnect(g_dma);
          g_dma_connected = false;
        }

      gdma_del_channel(g_dma);
      g_dma = NULL;
      g_channel = -1;
    }

  for (unsigned int i = 0; i < 2; i++)
    {
      kmm_free(g_scan[i]);
      g_scan[i] = NULL;
    }

  kmm_free(g_frame);
  g_frame = NULL;
  if (g_clock_enabled)
    {
      PERIPH_RCC_ATOMIC()
        {
          lcd_ll_enable_clock(g_lcd.dev, false);
        }

      g_clock_enabled = false;
    }

  if (g_bus_acquired)
    {
      PERIPH_RCC_RELEASE_ATOMIC(soc_lcd_rgb_signals[0].module, refs)
        {
          if (refs == 0)
            {
              lcd_ll_enable_bus_clock(0, false);
            }
        }

      g_bus_acquired = false;
    }

  if (g_core_source_enabled)
    {
      esp_clk_tree_enable_src((soc_module_clk_t)LCD_CORE_CLK_SRC_DEFAULT,
                              false);
      g_core_source_enabled = false;
    }

  if (g_source_enabled)
    {
      esp_clk_tree_enable_src((soc_module_clk_t)LCD_CLK_SRC_DEFAULT, false);
      g_source_enabled = false;
    }

  g_ready = false;
  g_dma_error = 0;
}

static int s31_lcd_getvideo(FAR struct fb_vtable_s *vtable,
                            FAR struct fb_videoinfo_s *info)
{
  (void)vtable;
  if (info == NULL)
    {
      return -EINVAL;
    }

  memset(info, 0, sizeof(*info));
  info->fmt = FB_FMT_RGB16_565;
  info->xres = LCD_WIDTH;
  info->yres = LCD_HEIGHT;
  info->nplanes = 1;
  return OK;
}

static int s31_lcd_getplane(FAR struct fb_vtable_s *vtable, int plane,
                            FAR struct fb_planeinfo_s *info)
{
  (void)vtable;
  if (plane != 0 || info == NULL)
    {
      return -EINVAL;
    }

  if (!g_ready)
    {
      return -ENODEV;
    }

  memset(info, 0, sizeof(*info));
  info->fbmem = g_frame;
  info->fblen = LCD_FBSIZE;
  info->stride = LCD_STRIDE;
  info->bpp = 16;
  info->xres_virtual = LCD_WIDTH;
  info->yres_virtual = LCD_HEIGHT;
  return OK;
}

static int s31_lcd_update(FAR struct fb_vtable_s *vtable,
                          FAR const struct fb_area_s *area)
{
  unsigned int next;
  uint32_t status;
  clock_t deadline;
  irqstate_t flags;
  int ret;

  (void)vtable;
  if (area == NULL || area->x >= LCD_WIDTH || area->y >= LCD_HEIGHT ||
      area->w > LCD_WIDTH - area->x || area->h > LCD_HEIGHT - area->y)
    {
      return -EINVAL;
    }

  if (area->w == 0 || area->h == 0)
    {
      return OK;
    }

  ret = nxmutex_lock(&g_lcd_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (!g_ready || g_dma_error != 0)
    {
      ret = g_ready ? g_dma_error : -ENODEV;
      goto out;
    }

  /* Copy the whole staging image also for partial updates: either scan
   * buffer can then become the complete next frame without stale regions.
   */

  next = g_front ^ 1;
  if (g_first_update)
    {
      syslog(LOG_INFO, "S31 LCD: UPDATE_STAGE=copy\n");
    }

  memcpy(g_scan[next], g_frame, LCD_FBSIZE);
  if (g_first_update)
    {
      syslog(LOG_INFO, "S31 LCD: UPDATE_STAGE=cache-writeback\n");
    }

  ret = s31_lcd_result(esp_cache_msync(g_scan[next], LCD_FBSIZE,
                                     ESP_CACHE_MSYNC_FLAG_DIR_C2M));
  if (ret < 0)
    {
      goto out;
    }

  status = axi_dma_ll_tx_get_interrupt_status(&AXI_DMA, g_channel, true);
  if ((status & GDMA_LL_EVENT_TX_DESC_ERROR) != 0)
    {
      axi_dma_ll_tx_clear_interrupt_status(&AXI_DMA, g_channel, status);
      ret = -EIO;
      goto handoff_done;
    }

  if (g_first_update)
    {
      syslog(LOG_INFO, "S31 LCD: UPDATE_STAGE=link-request channel=%d\n",
             g_channel);
    }

  flags = enter_critical_section();
  axi_dma_ll_tx_clear_interrupt_status(&AXI_DMA, g_channel,
                                       GDMA_LL_EVENT_TX_LINK_SWITCH);
  for (unsigned int i = 0; i < 2; i++)
    {
      g_desc[i][LCD_NODES - 1].next = &g_desc[next][0];
    }

  __asm__ volatile ("fence rw, rw" ::: "memory");
  axi_dma_ll_tx_request_link_switch_event(&AXI_DMA, g_channel);
  leave_critical_section(flags);

  /* The deadline is absolute and is not extended by interrupted sleeps.
   * IRQs remain enabled and nxsig_usleep lets NSH and the timer run while
   * the hardware completes the descriptor switch.
   */

  deadline = clock_systime_ticks() + SEC2TICK(1);
  if (g_first_update)
    {
      syslog(LOG_INFO, "S31 LCD: UPDATE_STAGE=link-wait\n");
    }

  for (;;)
    {
      status = axi_dma_ll_tx_get_interrupt_status(&AXI_DMA, g_channel, true);
      if ((status & GDMA_LL_EVENT_TX_DESC_ERROR) != 0)
        {
          axi_dma_ll_tx_clear_interrupt_status(&AXI_DMA, g_channel, status);
          ret = -EIO;
          break;
        }

      if ((status & GDMA_LL_EVENT_TX_LINK_SWITCH) != 0)
        {
          axi_dma_ll_tx_clear_interrupt_status(&AXI_DMA, g_channel,
                                               GDMA_LL_EVENT_TX_LINK_SWITCH);
          ret = OK;
          break;
        }

      if (clock_compare(deadline, clock_systime_ticks()))
        {
          ret = -ETIMEDOUT;
          break;
        }

      ret = nxsig_usleep(1000);
      if (ret < 0 && ret != -EINTR)
        {
          break;
        }
    }

handoff_done:
  if (ret < 0)
    {
      /* Never reuse a scan buffer after an unconfirmed hand-off. Keep all
       * mapped storage alive, stop scanout, and reject further updates.
       */

      g_dma_error = ret;
      s31_lcd_stop();
      syslog(LOG_ERR, "S31 LCD: DMA hand-off failed: %d raw=%08lx\n",
             ret, (unsigned long)status);
    }
  else
    {
      g_front = next;
      if (g_first_update)
        {
          syslog(LOG_INFO, "S31 LCD: UPDATE_STAGE=complete raw=%08lx\n",
                 (unsigned long)status);
          g_first_update = false;
        }
    }

out:
  nxmutex_unlock(&g_lcd_lock);
  return ret;
}

static struct fb_vtable_s g_lcd_vtable =
{
  .getvideoinfo = s31_lcd_getvideo,
  .getplaneinfo = s31_lcd_getplane,
  .updatearea = s31_lcd_update,
};

int up_fbinitialize(int display)
{
  gdma_channel_alloc_config_t allocation = {0};
  gdma_strategy_config_t strategy = {.owner_check = true};
  gdma_transfer_config_t transfer =
  {
    .max_data_burst_size = 64,
    .access_ext_mem = true,
  };
  hal_utils_clk_div_t divider = {0};
  uint32_t source_hz;
  uint32_t pixel_hz;
  size_t internal_align;
  size_t external_align;
  int ret;

  if (display != 0)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&g_lcd_lock);
  if (ret < 0)
    {
      return ret;
    }

  if (g_ready)
    {
      nxmutex_unlock(&g_lcd_lock);
      return OK;
    }

  /* Descriptor writes must be visible to DMA without a cached alias. */

  if (!esp_ptr_internal(g_desc) || !esp_ptr_dma_capable(g_desc) ||
      esp_cache_msync(g_desc, sizeof(g_desc),
                      ESP_CACHE_MSYNC_FLAG_DIR_C2M) != ESP_ERR_NOT_SUPPORTED)
    {
      ret = -ENOTSUP;
      goto fail;
    }

  ret = s31_lcd_pin(LCD_DISP_PIN, SIG_GPIO_OUT_IDX);
  if (ret < 0)
    {
      goto fail;
    }

  esp_gpiowrite(LCD_DISP_PIN, false);

  /* LCD and CAM share the core clock/reset, but have independent pixel
   * clocks.  Every owner holds a core-source reference; only the first
   * owner programs the common clock, matching the CAM HAL configuration.
   */

  ret = s31_lcd_result(esp_clk_tree_enable_src(
                       (soc_module_clk_t)LCD_CORE_CLK_SRC_DEFAULT, true));
  if (ret < 0)
    {
      goto fail;
    }

  g_core_source_enabled = true;
  PERIPH_RCC_ACQUIRE_ATOMIC(soc_lcd_rgb_signals[0].module, refs)
    {
      if (refs == 0)
        {
          lcd_ll_enable_bus_clock(0, true);
          lcd_ll_reset_register(0);
          lcd_ll_select_core_clk_src(0, LCD_CORE_CLK_SRC_DEFAULT);
          lcd_ll_set_core_clock_divider(0, 2, 0, 0);
        }
    }

  g_bus_acquired = true;
  lcd_hal_init(&g_lcd, 0);
  PERIPH_RCC_ATOMIC()
    {
      lcd_ll_enable_clock(g_lcd.dev, true);
      lcd_ll_enable_interrupt(g_lcd.dev, UINT32_MAX, false);
    }

  g_clock_enabled = true;
  lcd_ll_mem_force_power_on(g_lcd.dev);
  lcd_ll_enable_trans_buffer(g_lcd.dev, true);
  ret = s31_lcd_result(esp_clk_tree_enable_src(
                       (soc_module_clk_t)LCD_CLK_SRC_DEFAULT, true));
  if (ret < 0)
    {
      goto fail;
    }

  g_source_enabled = true;
  ret = s31_lcd_result(esp_clk_tree_src_get_freq_hz(
                       (soc_module_clk_t)LCD_CLK_SRC_DEFAULT,
                       ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &source_hz));
  if (ret < 0)
    {
      goto fail;
    }

  PERIPH_RCC_ATOMIC()
    {
      lcd_ll_select_clk_src(0, LCD_CLK_SRC_DEFAULT);
    }

  lcd_ll_reset(g_lcd.dev);
  lcd_ll_fifo_reset(g_lcd.dev);
  pixel_hz = lcd_hal_cal_pclk_freq(&g_lcd, source_hz, LCD_PCLK, &divider);
  if (pixel_hz == 0)
    {
      ret = -EINVAL;
      goto fail;
    }

  PERIPH_RCC_ATOMIC()
    {
      lcd_ll_set_group_clock_coeff(0, divider.integer,
                                   divider.denominator, divider.numerator);
    }

  lcd_ll_set_clock_idle_level(g_lcd.dev, false);
  lcd_ll_set_pixel_clock_edge(g_lcd.dev, true);
  lcd_ll_enable_rgb_mode(g_lcd.dev, true);
  lcd_ll_set_data_wire_width(g_lcd.dev, 16);
  lcd_ll_set_dma_read_stride(g_lcd.dev, 16);
  lcd_ll_enable_color_convert(g_lcd.dev, false);
  lcd_ll_set_phase_cycles(g_lcd.dev, 0, 0, 1);
  lcd_ll_enable_output_always_on(g_lcd.dev, true);
  lcd_ll_set_idle_level(g_lcd.dev, true, true, false);
  lcd_ll_set_blank_cycles(g_lcd.dev, 1, 1);
  lcd_ll_set_horizontal_timing(g_lcd.dev, 40, 40, LCD_WIDTH, 48);
  lcd_ll_set_vertical_timing(g_lcd.dev, 23, 32, LCD_HEIGHT, 13);
  lcd_ll_enable_output_hsync_in_porch_region(g_lcd.dev, true);
  lcd_ll_set_hsync_position(g_lcd.dev, 0);
  lcd_ll_enable_auto_next_frame(g_lcd.dev, true);

  for (unsigned int i = 0; i < 16; i++)
    {
      ret = s31_lcd_pin(g_data_pins[i], soc_lcd_rgb_signals[0].data_sigs[i]);
      if (ret < 0)
        {
          goto fail;
        }
    }

  if ((ret = s31_lcd_pin(40, soc_lcd_rgb_signals[0].pclk_sig)) < 0 ||
      (ret = s31_lcd_pin(43, soc_lcd_rgb_signals[0].de_sig)) < 0 ||
      (ret = s31_lcd_pin(44, soc_lcd_rgb_signals[0].hsync_sig)) < 0 ||
      (ret = s31_lcd_pin(45, soc_lcd_rgb_signals[0].vsync_sig)) < 0)
    {
      goto fail;
    }

  ret = s31_lcd_result(gdma_new_axi_channel(&allocation, &g_dma, NULL));
  if (ret < 0)
    {
      goto fail;
    }

  ret = s31_lcd_result(gdma_connect(g_dma,
                                    GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0)));
  if (ret < 0)
    {
      goto fail;
    }

  g_dma_connected = true;
  if ((ret = s31_lcd_result(gdma_apply_strategy(g_dma, &strategy))) < 0 ||
      (ret = s31_lcd_result(gdma_config_transfer(g_dma, &transfer))) < 0 ||
      (ret = s31_lcd_result(gdma_get_alignment_constraints(g_dma,
                              &internal_align, &external_align))) < 0)
    {
      goto fail;
    }

  if (external_align == 0 || external_align > LCD_ALIGNMENT ||
      LCD_ALIGNMENT % external_align != 0)
    {
      ret = -ENOTSUP;
      goto fail;
    }

  g_frame = kmm_memalign(LCD_ALIGNMENT, LCD_FBSIZE);
  if (g_frame == NULL)
    {
      ret = -ENOMEM;
      goto fail;
    }

  memset(g_frame, 0, LCD_FBSIZE);
  memset(g_desc, 0, sizeof(g_desc));
  for (unsigned int i = 0; i < 2; i++)
    {
      g_scan[i] = kmm_memalign(LCD_ALIGNMENT, LCD_FBSIZE);
      if (g_scan[i] == NULL)
        {
          ret = -ENOMEM;
          goto fail;
        }

      if (!esp_ptr_external_ram(g_scan[i]) ||
          !esp_ptr_dma_ext_capable(g_scan[i]))
        {
          ret = -ENOTSUP;
          goto fail;
        }

      memset(g_scan[i], 0, LCD_FBSIZE);
      ret = s31_lcd_result(esp_cache_msync(g_scan[i], LCD_FBSIZE,
                                         ESP_CACHE_MSYNC_FLAG_DIR_C2M));
      if (ret < 0)
        {
          goto fail;
        }

      for (unsigned int n = 0; n < LCD_NODES; n++)
        {
          dma_descriptor_align8_t *desc = &g_desc[i][n];
          size_t offset = n * LCD_NODE_BYTES;
          size_t bytes = LCD_FBSIZE - offset;
          if (bytes > LCD_NODE_BYTES)
            {
              bytes = LCD_NODE_BYTES;
            }

          desc->dw0.size = bytes;
          desc->dw0.length = bytes;
          desc->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
          desc->dw0.suc_eof = n == LCD_NODES - 1;
          desc->buffer = g_scan[i] + offset;
          desc->next = &g_desc[i][(n + 1) % LCD_NODES];
        }
    }

  ret = s31_lcd_result(gdma_get_channel_id(g_dma, &g_channel));
  if (ret < 0)
    {
      goto fail;
    }

  axi_dma_ll_tx_enable_interrupt(&AXI_DMA, g_channel, UINT32_MAX, false);
  axi_dma_ll_tx_clear_interrupt_status(&AXI_DMA, g_channel, UINT32_MAX);
  g_dma_error = 0;
  g_first_update = true;
  g_front = 0;
  __asm__ volatile ("fence rw, rw" ::: "memory");
  ret = s31_lcd_result(gdma_reset(g_dma));
  if (ret < 0)
    {
      goto fail;
    }

  ret = s31_lcd_result(gdma_start(g_dma, (uintptr_t)&g_desc[0][0]));
  if (ret < 0)
    {
      goto fail;
    }

  esp_rom_delay_us(1);
  lcd_ll_start(g_lcd.dev);
  esp_gpiowrite(LCD_DISP_PIN, true);
  g_ready = true;
  syslog(LOG_INFO, "S31 LCD: NuttX framebuffer 800x480 RGB565 "
                   "pclk=%lu, two DMA scan buffers, raw link-switch polling\n",
                   (unsigned long)pixel_hz);
  nxmutex_unlock(&g_lcd_lock);
  return OK;

fail:
  syslog(LOG_ERR, "S31 LCD: initialization failed: %d\n", ret);
  s31_lcd_release();
  nxmutex_unlock(&g_lcd_lock);
  return ret;
}

FAR struct fb_vtable_s *up_fbgetvplane(int display, int vplane)
{
  return display == 0 && vplane == 0 && g_ready ? &g_lcd_vtable : NULL;
}

void up_fbuninitialize(int display)
{
  if (display == 0 && nxmutex_lock(&g_lcd_lock) == OK)
    {
      s31_lcd_release();
      nxmutex_unlock(&g_lcd_lock);
    }
}
