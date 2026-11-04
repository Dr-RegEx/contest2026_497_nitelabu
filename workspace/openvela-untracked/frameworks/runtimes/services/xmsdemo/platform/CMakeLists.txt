# SPDX-License-Identifier: Apache-2.0
if(CONFIG_XMSDEMO_PLATFORM)
  nuttx_add_library(s31product STATIC)
  target_sources(s31product PRIVATE PlatformActivity.cpp)
  target_include_directories(s31product PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  nuttx_add_dependencies(TARGET s31product DEPENDS android_binder activity_manager uikit)
  if(CONFIG_XMSDEMO_PLATFORM_MEDIA)
    target_sources(s31product PRIVATE MediaController.cpp)
    nuttx_add_dependencies(TARGET s31product DEPENDS libmedia)
  endif()
  nuttx_add_application(NAME s31product STACKSIZE ${CONFIG_SYSTEM_SERVER_APP_STACKSIZE} PRIORITY 100
    SRCS PlatformApplication.cpp INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}
    DEPENDS s31product)
endif()
