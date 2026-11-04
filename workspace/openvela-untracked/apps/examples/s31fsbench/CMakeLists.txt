# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31FSBENCH)
  nuttx_add_application(NAME s31fsbench
    STACKSIZE ${CONFIG_EXAMPLES_S31FSBENCH_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_S31FSBENCH}
    SRCS s31fsbench_main.c
    COMPILE_FLAGS -Werror)
endif()
