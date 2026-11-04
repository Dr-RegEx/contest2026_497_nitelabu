# ##############################################################################
# arch/risc-v/src/esp32s31/esp32s31_espressif.cmake
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

# Keep this adapter local to ESP32-S31.  openvela's shared Espressif port uses
# Make.defs and its source layout differs from the newer upstream CMake port.

set(ESP_COMMON_DIR ${CMAKE_CURRENT_LIST_DIR}/../common/espressif)
set(ESP_COMMON_SRCS
    ${ESP_COMMON_DIR}/esp_head.S
    ${ESP_COMMON_DIR}/esp_vectors.S
    ${ESP_COMMON_DIR}/esp_allocateheap.c
    ${ESP_COMMON_DIR}/esp_start.c
    ${ESP_COMMON_DIR}/esp_idle.c
    ${ESP_COMMON_DIR}/esp_irq.c
    ${ESP_COMMON_DIR}/esp_gpio.c
    ${ESP_COMMON_DIR}/esp_rtc_gpio.c
    ${ESP_COMMON_DIR}/esp_libc_stubs.c
    ${ESP_COMMON_DIR}/esp_lowputc.c
    ${ESP_COMMON_DIR}/esp_serial.c
    ${ESP_COMMON_DIR}/esp_systemreset.c)

if(CONFIG_SCHED_TICKLESS)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_tickless.c)
else()
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_timerisr.c)
endif()

if(CONFIG_ESPRESSIF_WDT)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_wdt.c)
endif()

if(CONFIG_DEV_RANDOM OR CONFIG_DEV_URANDOM_ARCH)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_random.c)
endif()

if(CONFIG_TIMER)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_timer.c)
endif()

if(CONFIG_ONESHOT)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_oneshot.c)
endif()

if(CONFIG_RTC)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_rtc.c)
endif()

if(CONFIG_ESPRESSIF_HR_TIMER)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_hr_timer.c
                             ${ESP_COMMON_DIR}/esp_ets_timer_legacy.c)
endif()

if(CONFIG_ESPRESSIF_DMA)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_dma.c)
endif()

if(CONFIG_ESPRESSIF_TWAI)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_twai.c)
endif()

if(CONFIG_ESPRESSIF_LEDC)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_ledc.c)
endif()

if(CONFIG_ESP_PCNT_AS_QE)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_qencoder.c)
endif()

if(CONFIG_ESPRESSIF_USBSERIAL)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_usbserial.c)
endif()

if(CONFIG_ESP_RMT)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_rmt.c)
  if(CONFIG_WS2812_NON_SPI_DRIVER)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_ws2812.c)
  endif()
endif()

if(CONFIG_ESPRESSIF_TEMP)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_temperature_sensor.c)
endif()

if(CONFIG_ESPRESSIF_I2C)
  if(CONFIG_ESPRESSIF_I2C_PERIPH)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_i2c.c)
  endif()
  if(CONFIG_ESPRESSIF_I2C_BITBANG)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_i2c_bitbang.c)
  endif()
endif()

if(CONFIG_ESPRESSIF_SPI)
  if(CONFIG_ESPRESSIF_SPI_PERIPH)
    list(APPEND ESP_COMMON_SRCS ${CMAKE_CURRENT_LIST_DIR}/esp32s31_spi.c)
  endif()
  if(CONFIG_SPI_SLAVE)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_spi_slave.c)
  endif()
  if(CONFIG_ESPRESSIF_SPI_BITBANG)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_spi_bitbang.c)
  endif()
endif()

if(CONFIG_ESPRESSIF_SPIFLASH)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_spiflash.c)
  if(CONFIG_ESPRESSIF_MTD)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_spiflash_mtd.c)
  endif()
endif()

if(CONFIG_ESP_WIRELESS)
  if(CONFIG_ESPRESSIF_WIFI)
    list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_wifi_init.c
                               ${ESP_COMMON_DIR}/esp_wlan.c)
  endif()
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_wifi_utils.c)
endif()

if(CONFIG_ESP_MCPWM)
  list(APPEND ESP_COMMON_SRCS ${ESP_COMMON_DIR}/esp_mcpwm.c)
endif()

# This port is validated against a locked HAL snapshot.  Never clone an
# arbitrary repository from CMake: fail with an actionable dependency error.

set(ESP32S31_HAL_REVISION 290edc31b50decca660c1a11ce3506fd9b2e1e27)
if("$ENV{ESP_HAL_3RDPARTY_LOCAL}" STREQUAL "")
  message(
    FATAL_ERROR
      "ESP32-S31 requires ESP_HAL_3RDPARTY_LOCAL to point to esp-hal-3rdparty at ${ESP32S31_HAL_REVISION}"
  )
endif()

get_filename_component(ESP_HAL_3RDPARTY_REPO
                       "$ENV{ESP_HAL_3RDPARTY_LOCAL}" REALPATH)
if(NOT IS_DIRECTORY ${ESP_HAL_3RDPARTY_REPO})
  message(FATAL_ERROR "ESP32-S31 HAL directory not found: ${ESP_HAL_3RDPARTY_REPO}")
endif()

execute_process(
  COMMAND git -C ${ESP_HAL_3RDPARTY_REPO} rev-parse HEAD
  OUTPUT_VARIABLE ESP32S31_HAL_HEAD
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE ESP32S31_HAL_GIT_RESULT)
if(NOT ESP32S31_HAL_GIT_RESULT EQUAL 0 OR
   NOT ESP32S31_HAL_HEAD STREQUAL ESP32S31_HAL_REVISION)
  message(
    FATAL_ERROR
      "ESP32-S31 HAL revision mismatch: expected ${ESP32S31_HAL_REVISION}, got ${ESP32S31_HAL_HEAD} at ${ESP_HAL_3RDPARTY_REPO}"
  )
endif()

string(REPLACE "\"" "" CHIP_SERIES "${CONFIG_ESPRESSIF_CHIP_SERIES}")
if(NOT CHIP_SERIES STREQUAL "esp32s31")
  message(FATAL_ERROR "ESP32-S31 CMake adapter selected for ${CHIP_SERIES}")
endif()

set(ESP32S31_GPIO_SIG_MAP
    ${ESP_HAL_3RDPARTY_REPO}/components/soc/${CHIP_SERIES}/include/soc/gpio_sig_map.h)
set(ESP32S31_IRQ_HEADER ${NUTTX_CHIP_ABS_DIR}/include/irq.h)
set(ESP32S31_ARCH_INCLUDE ${CMAKE_BINARY_DIR}/include/arch/chip)
set(ESP32S31_CONFIG_GPIO_SIG_MAP ${ESP32S31_ARCH_INCLUDE}/gpio_sig_map.h)
set(ESP32S31_CONFIG_IRQ_HEADER ${ESP32S31_ARCH_INCLUDE}/irq.h)

add_custom_command(
  OUTPUT ${ESP32S31_CONFIG_GPIO_SIG_MAP} ${ESP32S31_CONFIG_IRQ_HEADER}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${ESP32S31_ARCH_INCLUDE}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ESP32S31_GPIO_SIG_MAP}
          ${ESP32S31_CONFIG_GPIO_SIG_MAP}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${ESP32S31_IRQ_HEADER}
          ${ESP32S31_CONFIG_IRQ_HEADER}
  DEPENDS ${ESP32S31_GPIO_SIG_MAP} ${ESP32S31_IRQ_HEADER}
  COMMENT "Installing locked ESP32-S31 architecture headers"
  VERBATIM)
add_custom_target(esp32s31_hal_headers
                  DEPENDS ${ESP32S31_CONFIG_GPIO_SIG_MAP}
                          ${ESP32S31_CONFIG_IRQ_HEADER})
add_dependencies(arch esp32s31_hal_headers)
add_dependencies(nuttx_context esp32s31_hal_headers)

target_compile_options(
  arch PRIVATE -Wno-undef -Wno-unused-variable -fno-jump-tables
               -fno-tree-switch-conversion)

# CMake de-duplicates repeated plain -u options.  SHELL keeps every startup
# registration symbol in the final link.

set(ESP32S31_STARTUP_LINK_OPTIONS
    "SHELL:-u esp_system_include_startup_funcs"
    "SHELL:-u esp_timer_init_include_func"
    "SHELL:-u esp_flash_spi_init_include_func")
if(CONFIG_ESPRESSIF_EFUSE)
  list(APPEND ESP32S31_STARTUP_LINK_OPTIONS
       "SHELL:-u esp_efuse_startup_include_func")
endif()
# openvela's esp_irq.c owns the non-IRAM interrupt masks used by its IRQ
# allocator.  The locked HAL also exports same-named IDF helpers from
# intr_alloc.c; keep those private so the NuttX implementation remains the
# single public owner.

set_property(
  SOURCE
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hw_support/intr_alloc.c
  TARGET_DIRECTORY arch
  PROPERTY COMPILE_DEFINITIONS
           "esp_intr_noniram_disable=esp32s31_hal_intr_noniram_disable;esp_intr_noniram_enable=esp32s31_hal_intr_noniram_enable"
)

if(CONFIG_ARCH_USE_S_MODE)
  # CLIC enable registers and MHCR are M-mode-only on S31.  Route the HAL
  # cache-off guard through the narrow monitor ABI and omit its duplicate
  # direct branch-predictor CSR accesses.

  set_property(
    SOURCE
    ${ESP_HAL_3RDPARTY_REPO}/components/spi_flash/cache_utils.c
    TARGET_DIRECTORY arch
    PROPERTY COMPILE_DEFINITIONS
             "esp_intr_noniram_disable=esp32s31_flash_guard_enter;esp_intr_noniram_enable=esp32s31_flash_guard_leave"
  )
  set_property(
    SOURCE
    ${ESP_HAL_3RDPARTY_REPO}/components/spi_flash/cache_utils.c
    TARGET_DIRECTORY arch
    APPEND
    PROPERTY COMPILE_OPTIONS
             "-include${NUTTX_CHIP_ABS_DIR}/hal_backports/include/esp32s31_cache_utils_compat.h"
  )
endif()

target_link_options(nuttx PRIVATE ${ESP32S31_STARTUP_LINK_OPTIONS})

include(${NUTTX_CHIP_ABS_DIR}/hal_${CHIP_SERIES}.cmake)

target_include_directories(
  arch
  PRIVATE
    ${ESP_COMMON_DIR}
    ${ESP_COMMON_DIR}/platform_include
    ${ESP_HAL_3RDPARTY_REPO}/components/spi_flash/include/esp_private)
target_sources(arch PRIVATE ${ESP_COMMON_SRCS})

if(CONFIG_BUILD_KERNEL)
  # Application ELFs are linked by a custom command, so directory-scoped
  # add_link_options() does not reach them.  Suppress the ESP toolchain's
  # newlib crt0; NuttX supplies its own STARTUP_OBJS below.

  nuttx_add_elf_link_options(-nostdlib -e __start)

  nuttx_add_aux_library(STARTUP_OBJS
                        ${NUTTX_DIR}/arch/risc-v/src/common/crt0.c)
  target_compile_options(
    STARTUP_OBJS
    PRIVATE
      $<GENEX_EVAL:$<TARGET_PROPERTY:nuttx_global,NUTTX_ELF_APP_COMPILE_OPTIONS>>)
endif()

if(NOT TARGET bootloader)
  add_custom_target(bootloader COMMAND ${CMAKE_COMMAND} -E echo
                                       "Using ESP32-S31 simple boot.")
endif()
