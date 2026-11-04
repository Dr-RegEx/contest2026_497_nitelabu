# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31PCMCHECK)
  nuttx_add_application(NAME s31pcmcheck
    STACKSIZE ${CONFIG_EXAMPLES_S31PCMCHECK_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_S31PCMCHECK}
    SRCS s31pcmcheck_main.c
    COMPILE_FLAGS -Werror)
endif()
