# Diagnostic-only stack: E158/E159 measured 17544 B; retain >14 KiB margin.
# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31FOUNDATION)
  nuttx_add_application(NAME s31foundation STACKSIZE 32768 PRIORITY 100
    MODULE ${CONFIG_EXAMPLES_S31FOUNDATION}
    SRCS s31foundation_main.c
    DEPENDS libfeature libqjs libyoga protobuf-c png_static freetype
    INCLUDE_DIRECTORIES
      ${NUTTX_APPS_DIR}/interpreters/quickjs/quickjs
      ${NUTTX_APPS_DIR}/external/yoga/yoga
      ${NUTTX_APPS_DIR}/external/protobuf-c/protobuf-c
      ${NUTTX_APPS_DIR}/external/libpng/libpng
      ${NUTTX_APPS_DIR}/frameworks/runtimes/feature/include)
endif()
