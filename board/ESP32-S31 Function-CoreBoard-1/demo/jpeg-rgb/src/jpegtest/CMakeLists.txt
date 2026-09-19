# ##############################################################################
# apps/examples/jpegtest/CMakeLists.txt
# ##############################################################################

if(CONFIG_EXAMPLES_JPEGTEST)
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_JPEGTEST_PROGNAME}
    PRIORITY
    ${CONFIG_EXAMPLES_JPEGTEST_PRIORITY}
    STACKSIZE
    ${CONFIG_EXAMPLES_JPEGTEST_STACKSIZE}
    MODULE
    ${CONFIG_EXAMPLES_JPEGTEST}
    SRCS
    jpegtest_main.c)

  # libjpeg-turbo exports this include path to its own target.  The public
  # jpeglib API is also consumed directly by this application target.
  target_include_directories(
    apps_jpegtest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../external/libjpeg-turbo/libjpeg-turbo)
endif()
