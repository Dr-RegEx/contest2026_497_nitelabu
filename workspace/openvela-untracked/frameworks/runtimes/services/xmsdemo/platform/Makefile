# SPDX-License-Identifier: Apache-2.0
include $(APPDIR)/Make.defs
PROGNAME = s31product
PRIORITY = 100
STACKSIZE = 16384
CXXEXT = .cpp
CXXSRCS = PlatformActivity.cpp
ifeq ($(CONFIG_XMSDEMO_PLATFORM_MEDIA),y)
CXXSRCS += MediaController.cpp
endif
MAINSRC = PlatformApplication.cpp
include $(APPDIR)/Application.mk
