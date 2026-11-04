# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31MEDIACHECK)
  nuttx_add_application(NAME s31mediacheck STACKSIZE 8192 PRIORITY 100
    SRCS s31mediacheck_main.c DEPENDS libmedia)
endif()
