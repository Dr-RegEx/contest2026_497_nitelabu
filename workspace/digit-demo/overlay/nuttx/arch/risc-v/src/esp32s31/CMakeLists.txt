# ##############################################################################
# arch/risc-v/src/esp32s31/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

include(${CMAKE_CURRENT_SOURCE_DIR}/esp32s31_espressif.cmake)

if(CONFIG_ESPRESSIF_SIMPLE_BOOT)
  # Current openvela CMake only emits its generic hex output.  Produce the
  # Espressif ROM image as a separate S31 target, then add the digest required
  # by the S31 ROM.  The resulting image is flashed at 0x2000 (see Config.mk).

  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(ESP32S31_ESPTOOL NAMES esptool esptool.py REQUIRED)

  if(CONFIG_ESPRESSIF_FLASH_16M)
    set(ESP32S31_FLASH_SIZE 16MB)
  elseif(CONFIG_ESPRESSIF_FLASH_8M)
    set(ESP32S31_FLASH_SIZE 8MB)
  elseif(CONFIG_ESPRESSIF_FLASH_4M)
    set(ESP32S31_FLASH_SIZE 4MB)
  else()
    set(ESP32S31_FLASH_SIZE 2MB)
  endif()

  if(CONFIG_ESPRESSIF_FLASH_MODE_QIO)
    set(ESP32S31_FLASH_MODE qio)
  elseif(CONFIG_ESPRESSIF_FLASH_MODE_QOUT)
    set(ESP32S31_FLASH_MODE qout)
  elseif(CONFIG_ESPRESSIF_FLASH_MODE_DOUT)
    set(ESP32S31_FLASH_MODE dout)
  else()
    set(ESP32S31_FLASH_MODE dio)
  endif()

  if(CONFIG_ESPRESSIF_FLASH_FREQ_80M)
    set(ESP32S31_FLASH_FREQ 80m)
  elseif(CONFIG_ESPRESSIF_FLASH_FREQ_48M)
    set(ESP32S31_FLASH_FREQ 48m)
  elseif(CONFIG_ESPRESSIF_FLASH_FREQ_40M)
    set(ESP32S31_FLASH_FREQ 40m)
  elseif(CONFIG_ESPRESSIF_FLASH_FREQ_26M)
    set(ESP32S31_FLASH_FREQ 26m)
  else()
    set(ESP32S31_FLASH_FREQ 20m)
  endif()

  set(ESP32S31_SIMPLE_BOOT_IMAGE ${CMAKE_BINARY_DIR}/nuttx.bin)
  set(ESP32S31_SIMPLE_BOOT_DIGEST
      ${NUTTX_DIR}/tools/espressif/esp32s31_simple_boot_digest.py)
  add_custom_command(
    OUTPUT ${ESP32S31_SIMPLE_BOOT_IMAGE}
    COMMAND
      ${ESP32S31_ESPTOOL} --chip esp32s31 elf2image --ram-only-header
      --flash-size ${ESP32S31_FLASH_SIZE} --flash-mode ${ESP32S31_FLASH_MODE}
      --flash-freq ${ESP32S31_FLASH_FREQ} -o ${ESP32S31_SIMPLE_BOOT_IMAGE}
      $<TARGET_FILE:nuttx>
    COMMAND ${Python3_EXECUTABLE} ${ESP32S31_SIMPLE_BOOT_DIGEST}
            ${ESP32S31_SIMPLE_BOOT_IMAGE}
    DEPENDS nuttx ${ESP32S31_SIMPLE_BOOT_DIGEST}
    COMMENT "Generating ESP32-S31 simple-boot image for offset 0x2000"
    VERBATIM)
  add_custom_target(esp32s31_simple_boot_image
                    DEPENDS ${ESP32S31_SIMPLE_BOOT_IMAGE})
  add_custom_target(nuttx_post_build
                    DEPENDS esp32s31_simple_boot_image)
endif()

target_compile_definitions(arch PRIVATE _RETARGETABLE_LOCKING)
target_sources(arch PRIVATE esp_chip_rev.c)

if(CONFIG_ESP32S31_KORVO_RGB565)
  target_sources(arch PRIVATE esp32s31_lcd.c
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_lcd/lcd_hal.c
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_lcd/${CHIP_SERIES}/lcd_periph.c)
endif()

if(CONFIG_ESP32S31_USBDEV)
  target_sources(arch PRIVATE esp32s31_usbdev.c)
endif()

if(CONFIG_ESP32S31_USBHOST)
  target_sources(arch PRIVATE esp32s31_usbhost.c)
endif()

if(CONFIG_ESP32S31_USBHOST_HCD_SKELETON)
  target_sources(arch PRIVATE esp32s31_usbhost_hcd_skeleton.c)
endif()

if(CONFIG_ESP32S31_SDMMC)
  target_sources(arch PRIVATE esp32s31_sdmmc.c esp32s31_sdmmc_os.c)
  target_include_directories(arch PRIVATE
    ${ESP_HAL_3RDPARTY_REPO}/components/upper_hal_sdmmc/legacy/include
    ${ESP_HAL_3RDPARTY_REPO}/components/upper_hal_sdmmc/include
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_sd/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../s31-reference/tmp/esp-idf-clean/components/sdmmc/include)
endif()


if(CONFIG_ESP32S31_SDMMC_IDF_HOST)
  target_link_options(nuttx PRIVATE "LINKER:--wrap=esp_timer_get_time")
  set(S31_SD_ROOT ${ESP_HAL_3RDPARTY_REPO}/components)
  set(S31_SD_SOURCES
    ${S31_SD_ROOT}/upper_hal_sdmmc/src/sd_host_sdmmc.c
    ${S31_SD_ROOT}/upper_hal_sdmmc/src/sd_trans_sdmmc.c
    ${S31_SD_ROOT}/upper_hal_sdmmc/legacy/src/sdmmc_host.c
    ${S31_SD_ROOT}/upper_hal_sdmmc/legacy/src/sdmmc_transaction.c
    ${S31_SD_ROOT}/upper_hal_sd_intf/sd_host.c
    ${S31_SD_ROOT}/esp_hal_sd/sdmmc_hal.c
    ${S31_SD_ROOT}/esp_hal_sd/${CHIP_SERIES}/sdmmc_periph.c)
  target_sources(arch PRIVATE ${S31_SD_SOURCES})
  target_include_directories(arch PRIVATE
    ${S31_SD_ROOT}/upper_hal_sd_intf/include
    ${S31_SD_ROOT}/esp_hal_sd/${CHIP_SERIES}/include)
  set_source_files_properties(${S31_SD_SOURCES} TARGET_DIRECTORY arch PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/include/esp32s31_sdmmc_idf_compat.h;-Wall;-Werror")
endif()

if(CONFIG_ESP32S31_CAMERA_DVP)
  set(ESP32S31_CAM_ROOT ${ESP_HAL_3RDPARTY_REPO}/components/upper_hal_cam)
  # The locked GCC toolchain requires the post-20191213 Zifencei spelling for
  # the common RISC-V FPU context assembly.  Keep this correction local to the
  # build-only camera profile until the shared toolchain configuration is
  # audited separately.
  target_compile_options(arch PRIVATE -march=rv32imafc_zicsr_zifencei)
  target_sources(arch PRIVATE
    esp32s31_camera_dvp.c
    ${ESP32S31_CAM_ROOT}/esp_cam_ctlr.c
    ${ESP32S31_CAM_ROOT}/dvp_share_ctrl.c
    ${ESP32S31_CAM_ROOT}/dvp/src/esp_cam_ctlr_dvp_cam.c
    ${ESP32S31_CAM_ROOT}/dvp/src/esp_cam_ctlr_dvp_gdma.c
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_cam/cam_hal.c
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_cam/${CHIP_SERIES}/cam_periph.c
    ${ESP_HAL_3RDPARTY_REPO}/components/hal/color_hal.c)
  target_include_directories(arch PRIVATE
    ${ESP32S31_CAM_ROOT}/include
    ${ESP32S31_CAM_ROOT}/interface
    ${ESP32S31_CAM_ROOT}/dvp/include
    ${ESP32S31_CAM_ROOT}/dvp/private_include
    ${ESP_HAL_3RDPARTY_REPO}/components/hal/include)
endif()

if(CONFIG_ESP32S31_BLE)
  include(${CMAKE_CURRENT_SOURCE_DIR}/esp32s31_bluetooth.cmake)
endif()

if(CONFIG_ESP32S31_I2S)
  if(CONFIG_ESP32S31_I2S_DUPLEX)
    target_sources(arch PRIVATE esp32s31_i2s_duplex.c)
  else()
    target_sources(arch PRIVATE esp32s31_i2s.c)
  endif()
  target_sources(arch PRIVATE
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_i2s/i2s_hal.c)
endif()

if(CONFIG_ESP32S31_XTS_PWM)
  target_sources(arch PRIVATE esp32s31_pwm.c)
endif()

if(CONFIG_ESP32S31_XTS_ADC)
  target_sources(arch PRIVATE esp32s31_adc.c)
endif()

if(CONFIG_BUILD_FLAT AND CONFIG_ESPRESSIF_SPIRAM_USER_HEAP
   AND CONFIG_MM_REGIONS GREATER 1)
  target_sources(arch PRIVATE esp32s31_heap.c)
  target_link_options(nuttx PRIVATE -Wl,--wrap=heap_caps_add_region_with_caps)
endif()

if(CONFIG_ESP32S31_CRYPTO_AES_CBC)
  target_sources(arch PRIVATE esp32s31_crypto.c
    ${ESP_HAL_3RDPARTY_REPO}/components/esp_security/src/esp_crypto_lock.c)
endif()

if(CONFIG_ESP32S31_CRYPTO_ECC_POINT_MULT)
  target_sources(arch PRIVATE esp32s31_ecc.c)
endif()

if(CONFIG_ESP32S31_CRYPTO_ECDSA_VERIFY)
  target_sources(arch PRIVATE esp32s31_ecdsa.c)
endif()

if(CONFIG_ESP32S31_CRYPTO_SHA)
  target_sources(arch PRIVATE esp32s31_sha.c)
endif()

if(CONFIG_RTC_DRIVER AND CONFIG_RTC_PERIODIC)
  target_sources(arch PRIVATE esp32s31_rtc_periodic.c)
endif()

if(CONFIG_ARCH_RISCV_INTXCPT_EXTENSIONS)
  target_sources(arch PRIVATE esp32s31_intctx.c)
endif()

if(CONFIG_ESPRESSIF_WIFI)
  include(${CMAKE_CURRENT_SOURCE_DIR}/esp32s31_wireless.cmake)

  # The NuttX Wi-Fi OS adapter is shared by the RV32 Espressif targets.  S31
  # uses ABI v9; target-specific fields are selected in the adapter with
  # CONFIG_IDF_TARGET_ESP32S31.
  target_sources(arch PRIVATE ../esp32c6/esp_coex_adapter.c
                              ../esp32c6/esp_wifi_adapter.c)
  if(CONFIG_BUILD_KERNEL)
    target_sources(arch PRIVATE esp32s31_wifi_event.c)
  endif()
endif()

if(CONFIG_ARCH_USE_S_MODE)
  target_sources(arch PRIVATE esp32s31_supervisor.S esp32s31_monstat.c
                              esp32s31_flash_guard.c)
endif()

if(CONFIG_ESP32S31_SMP)
  target_sources(arch PRIVATE esp32s31_cpu1_boot.S esp32s31_cpu1_boot.c)
endif()

if(CONFIG_ESP32S31_SMP)
  target_sources(arch PRIVATE esp32s31_smp.c)
endif()

if(CONFIG_ESP32S31_SMP_TLB_SHOOTDOWN)
  target_sources(arch PRIVATE esp32s31_tlb.c)
endif()

if(CONFIG_BUILD_KERNEL)
  target_sources(arch PRIVATE esp32s31_mm_init.c esp32s31_pgalloc.c
                              esp32s31_smode_libc.c)
  target_link_options(nuttx PRIVATE
                      -Wl,-u,esp32s31_smode_libc_keep)
endif()

if(CONFIG_ESP32S31_RWDT)
  target_sources(arch PRIVATE esp32s31_wdt.c)
endif()

# Flash instructions and read-only data use the same unified cache virtual
# address window.  The linker script intentionally overlays a NOLOAD rodata
# cursor on .flash.text.

target_link_options(nuttx PRIVATE -Wl,--no-check-sections)

if(CONFIG_ESP32S31_WDT_ROM_DELAY)
  target_sources(arch PRIVATE esp32s31_wdt_delay.c)
endif()
