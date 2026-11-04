# SPDX-License-Identifier: Apache-2.0

set(SRCS esp32s31_boot.c esp32s31_bringup.c)

if(CONFIG_ESP32S31_LPI2C_CODEC_TEST)
  list(APPEND SRCS esp32s31_lpi2c_test.c)
endif()

if(CONFIG_ESP32S31_LPUART_LOOPBACK_TEST)
  list(APPEND SRCS esp32s31_lpuart_test.c)
endif()

if(CONFIG_ESP32S31_CANFD_LOOPBACK_TEST)
  list(APPEND SRCS esp32s31_canfd_test.c)
endif()

if(CONFIG_ESP32S31_COMP_LOOPBACK_TEST)
  list(APPEND SRCS esp32s31_comp_test.c)
endif()

if(CONFIG_ESP32S31_MCPWM_LOOPBACK_TEST)
  list(APPEND SRCS esp32s31_mcpwm_test.c)
endif()

if(CONFIG_ESP32S31_PCNT_MATRIX_TEST)
  list(APPEND SRCS esp32s31_pcnt_test.c)
endif()

if(CONFIG_ESP32S31_KORVO_ADC_BUTTONS)
  list(APPEND SRCS esp32s31_adc_buttons.c)
endif()

if(CONFIG_ESP32S31_AUDIO)
  list(APPEND SRCS esp32s31_audio.c)
endif()

if(CONFIG_ESP32S31_KORVO_AUDIO)
  list(APPEND SRCS esp32s31_korvo_audio.c esp32s31_pcm.c esp32s31_es8389.c)
endif()

if(CONFIG_ESP32S31_CAMERA_OV2640 OR CONFIG_ESP32S31_CAMERA_OV3660)
  list(APPEND SRCS esp32s31_camera.c)
endif()

if(CONFIG_ESP32S31_CAMERA_OV3660)
  list(APPEND SRCS esp32s31_ov3660.c)
endif()

if(CONFIG_ESP32S31_CAMERA_V4L2)
  list(APPEND SRCS esp32s31_camera_v4l2.c)
endif()

if(CONFIG_ESP32S31_XTS_PWM)
  list(APPEND SRCS esp32s31_pwm.c)
endif()

if(CONFIG_ESP32S31_XTS_GPIO)
  list(APPEND SRCS esp32s31_xts_gpio.c)
endif()

if(CONFIG_ESP32S31_XTS_BMI160)
  list(APPEND SRCS esp32s31_bmi160.c)
endif()

if(CONFIG_ESP32S31_XTS_BMI160_UORB)
  list(APPEND SRCS esp32s31_bmi160_uorb.c)
endif()

if(CONFIG_ESP32S31_XTS_FLASH)
  list(APPEND SRCS esp32s31_xts_flash.c)
endif()

if(CONFIG_ESP32S31_KORVO_HMI_DATA)
  list(APPEND SRCS esp32s31_hmi_storage.c)
endif()

if(CONFIG_BOARDCTL_RESET OR CONFIG_BOARDCTL_RESET_CAUSE)
  list(APPEND SRCS esp32s31_reset.c)
endif()

if(CONFIG_ESP32S31_MQUEUE_TEST)
  list(APPEND SRCS esp32s31_mqueue_test.c)
endif()

if(CONFIG_ESP32S31_WIFI_EVENT_TEST)
  list(APPEND SRCS esp32s31_event_test.c)
endif()

if(CONFIG_ARCH_BUTTONS)
  list(APPEND SRCS esp32s31_buttons.c)
endif()

if(CONFIG_ESP32S31_FLASH_APPFS)
  # Keep the U-mode application image outside the NuttX ELF.  The flash
  # helper writes this image at CONFIG_ESPRESSIF_STORAGE_MTD_OFFSET.

  find_program(GENROMFS genromfs REQUIRED)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(APPFS_IMAGE ${CMAKE_BINARY_DIR}/appfs.img)
  set(APPFS_ROOT ${CMAKE_BINARY_DIR}/appfs-root)
  set(APPFS_PREPARE
      ${NUTTX_DIR}/tools/espressif/esp32s31_prepare_appfs.py)
  set(APPFS_MANIFEST ${CMAKE_BINARY_DIR}/appfs-targets.txt)
  file(GENERATE OUTPUT ${APPFS_MANIFEST}
       CONTENT "$<JOIN:$<TARGET_PROPERTY:nuttx,NUTTX_LOADABLE_APPS>,\n>\n")
  if(CONFIG_ESP32S31_FLASH_APPFS_E0_CORPUS)
    set(APPFS_CORPUS_ARG --e0-corpus)
  endif()
  if(CONFIG_TESTING_NIST_STS)
    find_file(APPFS_NIST_TEMPLATE template9
      PATHS ${NUTTX_APPS_DIR}/testing/drivers/nist-sts/sts/templates
            ${NUTTX_APPS_DIR}/testing/drivers/nist-sts/nist-sts/sts/templates
      NO_DEFAULT_PATH REQUIRED)
    set(APPFS_NIST_COMMAND COMMAND ${CMAKE_COMMAND} -E copy
        ${APPFS_NIST_TEMPLATE} ${APPFS_ROOT}/nist-template9)
    set(APPFS_NIST_DEPENDS ${APPFS_NIST_TEMPLATE})
  endif()
  add_custom_command(
    OUTPUT ${APPFS_IMAGE}
    COMMAND ${Python3_EXECUTABLE} ${APPFS_PREPARE}
            --source ${CMAKE_BINARY_DIR}/bin
            --output ${APPFS_ROOT}
            --max-size ${CONFIG_ELF_APP_MAX_FILESIZE}
            --target-manifest ${APPFS_MANIFEST}
            ${APPFS_CORPUS_ARG}
    ${APPFS_NIST_COMMAND}
    COMMAND ${GENROMFS} -f ${APPFS_IMAGE} -d ${APPFS_ROOT}
            -V S31AppFS
    DEPENDS $<TARGET_PROPERTY:nuttx,NUTTX_LOADABLE_APPS> ${APPFS_PREPARE}
            ${APPFS_MANIFEST} ${APPFS_NIST_DEPENDS}
    COMMENT "Generating external ESP32-S31 application ROMFS"
    VERBATIM)
  add_custom_target(board_appfs DEPENDS ${APPFS_IMAGE})
  add_dependencies(board board_appfs)
elseif(CONFIG_BUILD_KERNEL)
  # Pack the separately linked U-mode applications after their targets have
  # been created.  A local target anchors the generated source in this board
  # directory without introducing an apps_post dependency cycle.

  set(ROMFS_BOOT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/romfs_boot.c)
  add_custom_command(
    OUTPUT ${ROMFS_BOOT_SOURCE}
    COMMAND genromfs -f romfs.img -d ${CMAKE_BINARY_DIR}/bin -V NuttXBootVol
    COMMAND xxd -i romfs.img romfs_boot.c
    COMMAND sed -i
            -e "1i#include <nuttx/compiler.h>"
            -e "s/romfs_img/g_s31_appromfs/g"
            -e "s/^unsigned char/const unsigned char aligned_data(4)/"
            -e "s/^unsigned int/const unsigned int/" romfs_boot.c
    DEPENDS $<TARGET_PROPERTY:nuttx,NUTTX_LOADABLE_APPS>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM)
  add_custom_target(board_romfs DEPENDS ${ROMFS_BOOT_SOURCE})
  add_dependencies(board board_romfs)

  # The board library is declared in the parent directory.  Older openvela
  # CMake does not propagate GENERATED across that directory boundary.

  set_property(SOURCE ${ROMFS_BOOT_SOURCE} TARGET_DIRECTORY board
               PROPERTY GENERATED TRUE)
  list(APPEND SRCS ${ROMFS_BOOT_SOURCE})
endif()


if(CONFIG_ESP32S31_XTS_MEDIA_VOLUME)
  list(APPEND SRCS esp32s31_xts_media_volume.c)
endif()

if(CONFIG_ESP32S31_KORVO_TOUCH)
  list(APPEND SRCS esp32s31_touch.c)
endif()

# Register MEDIA assets before the HMI creates /etc. Both services share one
# ROMFS; only the standalone qualification uses the minimal MEDIA startup.
if(CONFIG_ESP32S31_KORVO_MEDIA)
  if(NOT CONFIG_ESP32S31_KORVO_HMI AND NOT CONFIG_ESP32S31_KORVO_SMP_HMI_QUALIFICATION)
    add_board_rcraws(
      ${CMAKE_CURRENT_LIST_DIR}/etc/init.d/rc.sysinit
      ${CMAKE_CURRENT_LIST_DIR}/etc/init.d/rcS)
  endif()
  if(CONFIG_XMSDEMO_FACTORY)
    add_board_rcraws(
      ${CMAKE_CURRENT_LIST_DIR}/factory/etc/media/graph.conf
      ${CMAKE_CURRENT_LIST_DIR}/factory/etc/media/criteria.txt
      ${CMAKE_CURRENT_LIST_DIR}/factory/etc/media/settings.pfw)
  else()
    add_board_rcraws(
      ${CMAKE_CURRENT_LIST_DIR}/etc/media/graph.conf
      ${CMAKE_CURRENT_LIST_DIR}/etc/media/criteria.txt
      ${CMAKE_CURRENT_LIST_DIR}/etc/media/settings.pfw)
  endif()
  if(CONFIG_BLUETOOTH_LEAUDIO_SERVER)
    add_board_rcraws(
      ${CMAKE_CURRENT_LIST_DIR}/etc/media-lea/graph.conf
      ${CMAKE_CURRENT_LIST_DIR}/etc/media-lea/criteria.txt
      ${CMAKE_CURRENT_LIST_DIR}/etc/media-lea/settings.pfw)
  endif()
endif()

if(CONFIG_ESP32S31_KORVO_HMI OR CONFIG_ESP32S31_KORVO_SMP_HMI_QUALIFICATION)
  # PackageManager scans the real board ROMFS, independently of the upstream
  # demos' simulator resource copies. configure_file tracks manifest changes.
  set(HMI_DEMOS ${NUTTX_APPS_DIR}/frameworks/runtimes/services/xmsdemo)
  set(HMI_APPS)
  if(CONFIG_XMSDEMO_LAUNCHER)
    list(APPEND HMI_APPS launcher)
  endif()
  if(CONFIG_XMSDEMO_LVGLDEMO)
    list(APPEND HMI_APPS lvgldemo)
  endif()
  if(CONFIG_XMSDEMO_PLATFORM)
    list(APPEND HMI_APPS platform)
  endif()
  if(CONFIG_XMSDEMO_FACTORY)
    list(APPEND HMI_APPS factory)
    add_board_rcraws(${HMI_DEMOS}/factory/assets/etc/factory-demo)
  endif()
  foreach(HMI_APP ${HMI_APPS})
    if(HMI_APP STREQUAL "launcher")
      set(HMI_NAME xmslauncher)
    elseif(HMI_APP STREQUAL "lvgldemo")
      set(HMI_NAME xmslvgl)
    elseif(HMI_APP STREQUAL "factory")
      set(HMI_NAME s31factory)
    else()
      set(HMI_NAME s31product)
    endif()
    configure_file(${HMI_DEMOS}/${HMI_APP}/manifest.json
                   ${CMAKE_CURRENT_BINARY_DIR}/etc/app/${HMI_NAME}/manifest.json
                   COPYONLY)
    if(CONFIG_ESP32S31_VISION AND HMI_APP STREQUAL "factory")
      file(READ ${HMI_DEMOS}/factory/manifest.json FACTORY_MANIFEST)
      string(JSON FACTORY_MANIFEST SET "${FACTORY_MANIFEST}" activities 0
             intent-filter "{\"actions\":[\"action.system.HOME\"]}")
      file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/etc/app/${HMI_NAME}/manifest.json
           "${FACTORY_MANIFEST}\n")
    endif()
    list(APPEND HMI_MANIFESTS
         ${CMAKE_CURRENT_BINARY_DIR}/etc/app/${HMI_NAME}/manifest.json)
  endforeach()
  if(CONFIG_EXAMPLES_S31FOUNDATION OR CONFIG_XMSDEMO_PLATFORM)
    set(FOUNDATION_ASSETS ${NUTTX_APPS_DIR}/examples/s31foundation/assets)
    foreach(FOUNDATION_ASSET Montserrat-Regular.ttf OFL.txt)
      configure_file(${FOUNDATION_ASSETS}/${FOUNDATION_ASSET}
                     ${CMAKE_CURRENT_BINARY_DIR}/etc/fonts/${FOUNDATION_ASSET}
                     COPYONLY)
      list(APPEND HMI_MANIFESTS
           ${CMAKE_CURRENT_BINARY_DIR}/etc/fonts/${FOUNDATION_ASSET})
    endforeach()
  endif()
  if(CONFIG_LIB_CURL)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/hmi/certs/ISRG_Root_X1.pem
                   ${CMAKE_CURRENT_BINARY_DIR}/etc/ssl/curl/ca-certificates.crt
                   COPYONLY)
    list(APPEND HMI_MANIFESTS
         ${CMAKE_CURRENT_BINARY_DIR}/etc/ssl/curl/ca-certificates.crt)
  endif()
  if(CONFIG_XMSDEMO_PLATFORM_MEDIA)
    configure_file(${HMI_DEMOS}/platform/assets/demo-tone.wav
                   ${CMAKE_CURRENT_BINARY_DIR}/etc/media/demo-tone.wav COPYONLY)
    list(APPEND HMI_MANIFESTS
         ${CMAKE_CURRENT_BINARY_DIR}/etc/media/demo-tone.wav)
  endif()
  set(HMI_INIT ${CMAKE_CURRENT_SOURCE_DIR}/hmi/etc/init.d)
  if(CONFIG_ESP32S31_VISION)
    set(HMI_INIT ${CMAKE_CURRENT_SOURCE_DIR}/vision/etc/init.d)
    add_board_rcraws(${HMI_INIT}/start-system)
  elseif(CONFIG_ESP32S31_KORVO_SMP_HMI_QUALIFICATION)
    set(HMI_INIT ${CMAKE_CURRENT_SOURCE_DIR}/smp-hmi/etc/init.d)
    add_board_rcraws(${HMI_INIT}/start-system)
  endif()
  nuttx_add_romfs(
    NAME etc
    MOUNTPOINT etc
    RCSRCS ${HMI_INIT}/rc.sysinit
           ${HMI_INIT}/rcS
    RCRAWS ${CMAKE_CURRENT_SOURCE_DIR}/hmi/etc/package.cfg
           ${CMAKE_CURRENT_SOURCE_DIR}/hmi/etc/build.prop
    DEPENDS ${HMI_MANIFESTS}
    PATH ${CMAKE_CURRENT_BINARY_DIR}/etc)
  target_link_libraries(board PRIVATE romfs_etc)
endif()

if(CONFIG_ESP32S31_VISION)
  list(APPEND SRCS s31vision_dma.c s31vision.c s31vision_ops.S)
  # Preserve the original kernel's NuttX expf/roundf implementation. Native
  # MEDIA ELF processes also need libm functions absent from that small library.
  # Append the toolchain fallback to user ELF links only, never the kernel.
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} -march=rv32imafc_zicsr_zifencei -mabi=ilp32f
            --print-file-name=libm.a
    OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE S31_USER_LIBM
    COMMAND_ERROR_IS_FATAL ANY)
  if(NOT EXISTS "${S31_USER_LIBM}")
    message(FATAL_ERROR "Missing S31 user ELF math library: ${S31_USER_LIBM}")
  endif()
  set_property(TARGET nuttx_global APPEND PROPERTY
               NUTTX_ELF_LINK_EXTRA_LIBRARIES "${S31_USER_LIBM}")
endif()
target_sources(board PRIVATE ${SRCS})

string(REPLACE "\"" "" CHIP_SERIES "${CONFIG_ESPRESSIF_CHIP_SERIES}")
set(BOARD_COMMON_DIR "${NUTTX_BOARD_DIR}/../common")

set(LDSCRIPTS
    "${BOARD_COMMON_DIR}/scripts/${CHIP_SERIES}_aliases.ld"
    "${BOARD_COMMON_DIR}/scripts/${CHIP_SERIES}_flat_memory.ld")

if(CONFIG_ESPRESSIF_BOOTLOADER_MCUBOOT OR CONFIG_ESPRESSIF_SIMPLE_BOOT)
  list(APPEND LDSCRIPTS
       "${BOARD_COMMON_DIR}/scripts/${CHIP_SERIES}_sections.ld")
endif()

set_property(GLOBAL APPEND PROPERTY LD_SCRIPT ${LDSCRIPTS})
