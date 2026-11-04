/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_board_resources.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_BOARD_RESOURCES_H
#define __BOARDS_RISCV_ESP32S31_BOARD_RESOURCES_H

#include <arch/board/board.h>

/* These checks also apply to saved configs that bypass Kconfig dependencies.
 * They cover the fixed board fixtures and DVP collisions listed below, not
 * every possible external wiring or peripheral combination.
 */

#if defined(CONFIG_ESP32S31_BOARD_FUNCTION) && \
    defined(CONFIG_ESP32S31_BOARD_KORVO)
#  error "Select exactly one ESP32-S31 physical board"
#endif

#if defined(CONFIG_ESP32S31_KORVO_RGB565) && !BOARD_IS_KORVO
#  error "Korvo framebuffer requires the Korvo physical board variant"
#endif

#if BOARD_IS_KORVO
#  if defined(CONFIG_ESP32S31_I2S) && !defined(CONFIG_ESP32S31_KORVO_AUDIO)
#    error "Korvo I2S requires the ES8389 board transport, not Function pins"
#  endif
#  ifdef CONFIG_ESP32S31_AUDIO
#    error "ES8311/PA57 board audio is Function-only; Korvo requires ES8389/PA7"
#  endif
#  ifdef CONFIG_ARCH_BUTTONS
#    error "GPIO61 BOOT input conflicts with Korvo LCD SCK; use the ADC42 key lower half"
#  endif
#  if defined(CONFIG_ESP32S31_XTS_ADC) || defined(CONFIG_ESP32S31_XTS_PWM) || \
      defined(CONFIG_ESP32S31_XTS_GPIO) || \
      defined(CONFIG_ESP32S31_XTS_BMI160) || \
      defined(CONFIG_ESP32S31_XTS_BMI160_UORB)
#    error "Function J2 fixed xTS fixtures overlap Korvo camera/LCD wiring"
#  endif
#endif

#ifdef CONFIG_ESP32S31_KORVO_AUDIO
#  if !BOARD_IS_KORVO
#    error "Korvo PCM requires the Korvo board"
#  endif
#  if defined(CONFIG_BUILD_KERNEL)
#    if !defined(CONFIG_ESP32S31_SMP) || !defined(CONFIG_SMP) || \
        !defined(CONFIG_ARCH_USE_S_MODE) || \
        !defined(CONFIG_AUDIO_PROCESS_OWNERSHIP)
#      error "Korvo kernel PCM requires S-mode SMP and process-owned audio"
#    endif
#  elif !defined(CONFIG_BUILD_FLAT) || defined(CONFIG_SMP)
#    error "Korvo PCM requires FLAT single-core or S-mode KERNEL/SMP"
#  endif
#  if !defined(CONFIG_ESP32S31_I2S_DUPLEX) || \
      defined(CONFIG_AUDIO_MULTI_SESSION) || defined(CONFIG_AUDIO_EXCLUDE_STOP)
#    error "Korvo PCM needs continuous I2S and single-session STOP support"
#  endif
#  if !defined(CONFIG_ESPRESSIF_I2C0) || \
      CONFIG_ESPRESSIF_I2C0_SCLPIN != 1 || CONFIG_ESPRESSIF_I2C0_SDAPIN != 0
#    error "Korvo ES8389 shares I2C0 on SCL1/SDA0"
#  endif
#  if defined(CONFIG_ESP32S31_I2S_DUPLEX_44K) || \
      defined(CONFIG_ESP32S31_I2S_CAPTURE_16K_MONO)
#    error "Korvo ES8389 requires its fixed stereo 16 kHz format"
#  endif
#endif

#if defined(CONFIG_ESP32S31_XTS_GPIO) && defined(CONFIG_ESP32S31_XTS_PWM)
#  error "GPIO loopback and PWM fixture both own GPIO48"
#endif

#if defined(CONFIG_ESP32S31_XTS_GPIO) && defined(CONFIG_ESP32S31_XTS_ADC)
#  error "GPIO loopback and ADC fixture both own GPIO47"
#endif

#ifdef CONFIG_ESP32S31_KORVO_ADC_BUTTONS
#  if !BOARD_IS_KORVO || !defined(CONFIG_BUILD_FLAT) || defined(CONFIG_SMP)
#    error "Korvo ADC keys currently require the Korvo FLAT single-core profile"
#  endif
#  ifndef CONFIG_SCHED_LPWORK
#    error "Korvo ADC keys require the low-priority work queue"
#  endif
#  if defined(CONFIG_ESPRESSIF_WIFI) || defined(CONFIG_ESP32S31_BLE) || \
      defined(CONFIG_ESPRESSIF_TEMP)
#    error "ADC keys need shared SAR power ownership before RF/temperature coexistence"
#  endif
#  if defined(CONFIG_INPUT_BUTTONS_LOWER) || defined(CONFIG_ESP32S31_XTS_ADC)
#    error "ADC keys own their ADC client and /dev/buttons exclusively"
#  endif
#endif

#ifdef CONFIG_ESP32S31_CAMERA_DVP
#  define BOARD_DVP_USES_PIN(p) \
    (CONFIG_ESP32S31_CAMERA_DVP_D0 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D1 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D2 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D3 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D4 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D5 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D6 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_D7 == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_PCLK == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_XCLK == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_DE == (p) || \
     CONFIG_ESP32S31_CAMERA_DVP_VSYNC == (p))

#  if defined(CONFIG_ESP32S31_XTS_GPIO) && \
      (BOARD_DVP_USES_PIN(47) || BOARD_DVP_USES_PIN(48))
#    error "GPIO loopback fixture overlaps configured DVP pins"
#  endif
#  if defined(CONFIG_ESP32S31_XTS_PWM) && BOARD_DVP_USES_PIN(48)
#    error "PWM fixture overlaps a configured DVP pin"
#  endif
#  if defined(CONFIG_ESP32S31_XTS_ADC) && BOARD_DVP_USES_PIN(47)
#    error "ADC fixture overlaps a configured DVP pin"
#  endif
#  if defined(CONFIG_ESP32S31_AUDIO) && BOARD_DVP_USES_PIN(57)
#    error "Function audio PA57 overlaps a configured DVP pin"
#  endif
#  if defined(CONFIG_ESP32S31_KORVO_AUDIO) && \
      (BOARD_DVP_USES_PIN(3) || BOARD_DVP_USES_PIN(4) || \
       BOARD_DVP_USES_PIN(5) || BOARD_DVP_USES_PIN(6) || \
       BOARD_DVP_USES_PIN(7))
#    error "Korvo PCM signals overlap configured DVP pins"
#  endif
#  if defined(CONFIG_ARCH_BUTTONS) && BOARD_DVP_USES_PIN(61)
#    error "Function BOOT button overlaps a configured DVP pin"
#  endif
#  if defined(CONFIG_ESP_RMT) && defined(CONFIG_WS2812_NON_SPI_DRIVER) && \
      BOARD_DVP_USES_PIN(BOARD_RGBLED_PIN)
#    error "Board RGB LED overlaps a configured DVP pin"
#  endif
#  if defined(CONFIG_ESPRESSIF_I2C0) && \
      (BOARD_DVP_USES_PIN(CONFIG_ESPRESSIF_I2C0_SCLPIN) || \
       BOARD_DVP_USES_PIN(CONFIG_ESPRESSIF_I2C0_SDAPIN))
#    error "I2C0 signals overlap configured DVP pins"
#  endif
#  if defined(CONFIG_ESP32S31_KORVO_ADC_BUTTONS) && BOARD_DVP_USES_PIN(42)
#    error "Korvo ADC keys overlap a configured DVP pin"
#  endif
#endif

#endif /* __BOARDS_RISCV_ESP32S31_BOARD_RESOURCES_H */
