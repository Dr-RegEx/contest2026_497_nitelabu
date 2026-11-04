# SPDX-License-Identifier: Apache-2.0
if(CONFIG_XMSDEMO_FACTORY)
  add_subdirectory(assets)
  nuttx_add_library(s31factory_backend STATIC)
  target_sources(s31factory_backend PRIVATE FactoryBackend.cpp)
  target_compile_definitions(s31factory_backend PRIVATE FACTORY_BACKEND_MOUSE_HOTPLUG_SAFE=1)
  target_include_directories(s31factory_backend PUBLIC ${CMAKE_CURRENT_LIST_DIR})
  nuttx_add_dependencies(TARGET s31factory_backend DEPENDS libmedia)
  nuttx_add_library(s31factory_ui STATIC)
  target_sources(s31factory_ui PRIVATE
    FactoryActivity.cpp FactoryDashboard.cpp CoffeeWindow.c FactoryLifecycle.cpp
    coffee/ui/components/ui_comp.c
    coffee/ui/components/ui_comp_button.c
    coffee/ui/components/ui_comp_cta_container.c
    coffee/ui/components/ui_comp_header.c
    coffee/ui/components/ui_comp_home_sliders.c
    coffee/ui/components/ui_comp_homeslider.c
    coffee/ui/components/ui_comp_hook.c
    coffee/ui/components/ui_comp_parameter.c
    coffee/ui/components/ui_comp_progress.c
    coffee/ui/components/ui_comp_ready_button.c
    coffee/ui/components/ui_comp_selector.c
    coffee/ui/components/ui_comp_separator.c
    coffee/ui/components/ui_comp_slider1.c
    coffee/ui/components/ui_comp_slider3.c
    coffee/ui/components/ui_comp_steps.c
    coffee/ui/fonts/ui_font_H1_bold.c
    coffee/ui/fonts/ui_font_H2_bold.c
    coffee/ui/fonts/ui_font_H3_bold.c
    coffee/ui/fonts/ui_font_Min.c
    coffee/ui/fonts/ui_font_P1_bold.c
    coffee/ui/fonts/ui_font_P1_medium.c
    coffee/ui/fonts/ui_font_P2_medium.c
    coffee/ui/fonts/ui_font_badge2.c
    coffee/ui/fonts/ui_font_exclamation.c
    coffee/ui/fonts/ui_font_icon.c
    coffee/ui/screens/ui_Screen_Add_coffee.c
    coffee/ui/screens/ui_Screen_Add_water.c
    coffee/ui/screens/ui_Screen_Brewing.c
    coffee/ui/screens/ui_Screen_Brewing_Done.c
    coffee/ui/screens/ui_Screen_Customize.c
    coffee/ui/screens/ui_Screen_Customize2.c
    coffee/ui/screens/ui_Screen_Home.c
    coffee/ui/screens/ui_Screen_Home_espresso.c
    coffee/ui/screens/ui_Screen_Home_ristretto.c
    coffee/ui/screens/ui_Screen_Ready.c
    coffee/ui/screens/ui_Screen_Settings.c
    coffee/ui/screens/ui_Screen_Splash_screen.c
    coffee/ui/ui.c
    coffee/ui/ui_helpers.c
    coffee/ui/ui_theme_manager.c
    coffee/ui/ui_themes.c
)
  target_include_directories(s31factory_ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/coffee/ui)
  nuttx_add_dependencies(TARGET s31factory_ui
    DEPENDS android_binder activity_manager uikit s31factory_backend coffee_assets)
  nuttx_add_application(NAME s31factory STACKSIZE ${CONFIG_SYSTEM_SERVER_APP_STACKSIZE}
    PRIORITY 100 SRCS FactoryApplication.cpp
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}
    DEPENDS s31factory_ui)
endif()
