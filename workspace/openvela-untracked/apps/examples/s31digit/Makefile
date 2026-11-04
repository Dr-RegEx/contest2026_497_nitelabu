# SPDX-License-Identifier: Apache-2.0
include $(APPDIR)/Make.defs
PROGNAME = s31digit
PRIORITY = 100
STACKSIZE = 16384
MODULE = $(CONFIG_EXAMPLES_S31DIGIT)
MAINSRC = s31digit_main.c
ifeq ($(CONFIG_EXAMPLES_S31DIGIT_SIMD),y)
ASRCS = s31digit_ops.S
endif
include $(APPDIR)/Application.mk
