/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ESP32S31_SDMMC_IDF_COMPAT_H
#define ESP32S31_SDMMC_IDF_COMPAT_H
/* Include only for the locked IDF SD host objects. */
#include <stddef.h>
#include <sys/lock.h>
#include "freertos/task.h"
#ifndef __containerof
#define __containerof(ptr, type, member) \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif
#endif
