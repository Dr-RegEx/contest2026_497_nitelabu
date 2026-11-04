# SPDX-License-Identifier: Apache-2.0
# Integrate with add_subdirectory after review; this does not package or flash SD.
if(NOT CONFIG_GRAPHICS_LVGL OR NOT CONFIG_LV_USE_LIBPNG OR
   NOT CONFIG_LV_USE_FS_POSIX OR NOT CONFIG_LIB_PNG OR NOT CONFIG_LIB_ZLIB)
  message(FATAL_ERROR "coffee_assets requires LVGL POSIX + libpng + zlib")
endif()
nuttx_add_library(coffee_assets STATIC)
target_sources(coffee_assets PRIVATE coffee_assets.c)
target_include_directories(coffee_assets PUBLIC ${CMAKE_CURRENT_LIST_DIR}
  PRIVATE ${NUTTX_APPS_DIR}/system/zlib/zlib)
nuttx_add_dependencies(TARGET coffee_assets DEPENDS lvgl png_static zlib)
target_compile_definitions(coffee_assets PRIVATE
  "COFFEE_ASSETS_DEFAULT_DIRECTORY=\"/etc/factory-demo/coffee\"")
# The native factory application adds coffee_assets to its DEPENDS list.
# Compile ui/*.c, screens, components and fonts; do not add ui/images/*.c.
