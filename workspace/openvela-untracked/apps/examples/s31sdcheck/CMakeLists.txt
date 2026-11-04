# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31SDCHECK)
  nuttx_add_application(NAME s31sdcheck
    STACKSIZE ${CONFIG_EXAMPLES_S31SDCHECK_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_S31SDCHECK}
    SRCS s31sdcheck_main.c
    COMPILE_FLAGS -Werror)
endif()
