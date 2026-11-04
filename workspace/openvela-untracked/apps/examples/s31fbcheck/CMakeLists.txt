# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31FBCHECK)
  nuttx_add_application(NAME s31fbcheck
    STACKSIZE ${CONFIG_EXAMPLES_S31FBCHECK_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_S31FBCHECK}
    SRCS s31fbcheck_main.c
    COMPILE_FLAGS -Werror)
endif()
