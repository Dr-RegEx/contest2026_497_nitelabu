# SPDX-License-Identifier: Apache-2.0

set(S31_BT ${ESP_HAL_3RDPARTY_REPO}/components/bt)
set(S31_BT_PORT ${S31_BT}/porting_btdm)
set(S31_TINYCRYPT ${S31_BT}/common/tinycrypt)

target_compile_definitions(arch PRIVATE ESP_PLATFORM=1)
set_property(SOURCE ${S31_BT}/controller/esp32s31/bt.c
  TARGET_DIRECTORY arch APPEND PROPERTY COMPILE_OPTIONS -includeesp_system.h)

target_include_directories(arch PRIVATE
  ${S31_BT}/include/esp32s31/include
  ${S31_BT}/include
  ${S31_BT_PORT}/controller/btdm_common/include
  ${S31_BT_PORT}/controller/ble/include
  ${S31_BT_PORT}/transport/include
  ${S31_TINYCRYPT}/include
  ${S31_TINYCRYPT}/port
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_security/src/esp32s31
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hw_support/modem/include
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/include
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/esp32s31/include
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_coex/include
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_wifi/include
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_wifi/include/local
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_pm/include)

target_sources(arch PRIVATE
  esp32s31_ble_osal.c
  esp32s31_ble_controller.c
  esp32s31_ble.c
  ${S31_BT}/controller/esp32s31/bt.c
  ${S31_BT_PORT}/controller/btdm_common/src/btdm_coex.c
  ${S31_BT_PORT}/controller/btdm_common/src/btdm_external.c
  ${S31_BT_PORT}/controller/btdm_common/src/btdm_log.c
  ${S31_BT_PORT}/controller/btdm_common/src/btdm_lp.c
  ${S31_BT_PORT}/controller/ble/src/ble_msys.c
  esp32s31_ble_hci_transport.c
  ${S31_BT_PORT}/transport/driver/vhci/hci_driver_standard.c
  ${S31_TINYCRYPT}/src/aes_encrypt.c
  ${S31_TINYCRYPT}/src/cmac_mode.c
  ${S31_TINYCRYPT}/src/utils.c
  ${S31_TINYCRYPT}/src/ecc.c
  ${S31_TINYCRYPT}/src/ecc_dh.c
  ${S31_TINYCRYPT}/port/esp_tinycrypt_port.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_security/src/esp_crypto_lock.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_security/src/esp_crypto_periph_clk.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_security/src/esp32s31/esp_crypto_clk.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_security/ecc_hal.c
  esp32s31_btbb.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/src/lib_printf.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/src/phy_common.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/src/phy_init.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/src/phy_override.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/esp32s31/phy_init_data.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hw_support/sar_periph_ctrl_common.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hw_support/sar_tsens_ctrl.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hw_support/port/esp32s31/sar_periph_ctrl.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_ana_conv/temperature_sensor_hal.c
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_hal_ana_conv/esp32s31/temperature_sensor_periph.c
  esp32s31_phy_timer.c)

target_link_options(nuttx PRIVATE "LINKER:--wrap=esp_timer_get_time")
set_property(
  SOURCE ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/src/phy_common.c
  TARGET_DIRECTORY arch APPEND PROPERTY COMPILE_OPTIONS
  -include${NUTTX_CHIP_ABS_DIR}/hal_backports/include/esp32s31_phy_timer.h)

nuttx_add_extra_library(
  ${S31_BT}/controller/lib_esp32s31/esp32s31-bt-lib/libbtdm_common.a
  ${S31_BT}/controller/lib_esp32s31/esp32s31-bt-lib/libble_app.a
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/lib/esp32s31/libphy.a
  ${ESP_HAL_3RDPARTY_REPO}/components/esp_phy/lib/esp32s31/libbtbb.a)

if(CONFIG_BUILD_KERNEL)
  target_sources(arch PRIVATE esp32s31_ble_dispatch.c)
endif()
