# SPDX-License-Identifier: Apache-2.0

set(SRCS esp32s31_boot.c esp32s31_bringup.c)

if(CONFIG_ESP32S31_AUDIO)
  list(APPEND SRCS esp32s31_audio.c)
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
    DEPENDS $<TARGET_PROPERTY:nuttx,NUTTX_LOADABLE_APPS>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
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
