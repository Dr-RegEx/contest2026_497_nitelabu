# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31SIMD)
  nuttx_add_application(NAME s31simd MODULE ${CONFIG_EXAMPLES_S31SIMD}
    SRCS s31simd_main.c s31simd_ops.S STACKSIZE 8192 PRIORITY 100)
endif()
