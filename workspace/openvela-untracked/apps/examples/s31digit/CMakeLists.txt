# SPDX-License-Identifier: Apache-2.0
if(CONFIG_EXAMPLES_S31DIGIT)
  set(SRCS s31digit_main.c)
  if(CONFIG_EXAMPLES_S31DIGIT_SIMD)
    list(APPEND SRCS s31digit_ops.S)
  endif()
  nuttx_add_application(NAME s31digit MODULE ${CONFIG_EXAMPLES_S31DIGIT}
    SRCS ${SRCS} STACKSIZE 16384 PRIORITY 100)
endif()
