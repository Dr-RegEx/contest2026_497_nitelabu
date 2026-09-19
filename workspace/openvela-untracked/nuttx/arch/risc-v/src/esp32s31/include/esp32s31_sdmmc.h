/****************************************************************************
 * arch/risc-v/src/esp32s31/include/esp32s31_sdmmc.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_SDMMC_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_SDMMC_H

#include <nuttx/config.h>

#include <nuttx/sdio.h>

#if defined(CONFIG_ESP32S31_SDMMC) && defined(CONFIG_MMCSD) && \
    defined(CONFIG_MMCSD_SDIO)

/* Return a NuttX SDIO interface for the fixed Function-CoreBoard-1 slot 0.
 * The caller owns registration with mmcsd_slotinitialize(). */

struct sdio_dev_s *esp32s31_sdmmc_initialize(int slotno);

/* Initialize and register one slot with the NuttX mmcsd upper half. */

int esp32s31_sdmmc_register(int minor);

#endif /* CONFIG_ESP32S31_SDMMC && CONFIG_MMCSD && CONFIG_MMCSD_SDIO */

#endif /* __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_SDMMC_H */
