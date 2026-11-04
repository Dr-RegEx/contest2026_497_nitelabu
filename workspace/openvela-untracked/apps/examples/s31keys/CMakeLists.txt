# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31KEYS)
  nuttx_add_application(NAME s31keys
    STACKSIZE ${CONFIG_EXAMPLES_S31KEYS_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_S31KEYS}
    SRCS s31keys_main.c)
endif()
