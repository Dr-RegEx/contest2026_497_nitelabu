# Diagnostic-only stack: E158/E159 measured 17544 B; retain >14 KiB margin.
# SPDX-License-Identifier: Apache-2.0
include $(APPDIR)/Make.defs
PROGNAME = s31foundation
PRIORITY = 100
STACKSIZE = 32768
MAINSRC = s31foundation_main.c
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/interpreters/quickjs/quickjs
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/yoga/yoga
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/protobuf-c/protobuf-c
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/external/libpng/libpng
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/runtimes/feature/include
include $(APPDIR)/Application.mk
