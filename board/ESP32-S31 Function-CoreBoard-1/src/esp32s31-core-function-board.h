/****************************************************************************
 * boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31-core-function-board.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_SRC_ESP32S31_CORE_FUNCTION_BOARD_H
#define __BOARDS_RISCV_ESP32S31_CORE_FUNCTION_BOARD_SRC_ESP32S31_CORE_FUNCTION_BOARD_H

#define ESP32S31_APPFS_BLOCKDEV "/dev/appfs"
#define ESP32S31_APPS_MTDDEV    "/dev/apps"
#define ESP32S31_APPFS_SEED_DIR "/system/bin"
#define ESP32S31_APPS_DIR       "/apps"

int esp_bringup(void);

#ifdef CONFIG_ESP32S31_BLE
int esp32s31_ble_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_AUDIO
int esp32s31_audio_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_ADC
int esp32s31_adc_setup(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_PWM
int esp32s31_pwm_setup(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_GPIO
int esp32s31_xts_gpio_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_BMI160
int esp32s31_bmi160_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_BMI160_UORB
int esp32s31_bmi160_uorb_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_FLASH
int esp32s31_xts_flash_initialize(void);
#endif

#ifdef CONFIG_ESP32S31_XTS_MEDIA_VOLUME
int esp32s31_xts_media_volume_initialize(void);
#endif

#endif
