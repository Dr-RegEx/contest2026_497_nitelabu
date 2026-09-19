/****************************************************************************
 * arch/risc-v/src/esp32s31/include/esp32s31_usbhost.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_USBHOST_H
#define __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_USBHOST_H

#include <nuttx/config.h>

#ifdef CONFIG_USBHOST
#  include <nuttx/usb/usbhost.h>
#endif

#ifdef CONFIG_ESP32S31_USBHOST

/*
 * Prepare the S31 OTG core for host operation.  This is intentionally a
 * small, opt-in hardware candidate: it does not claim NuttX enumeration,
 * endpoint transfers, or class-driver support.
 */

int esp32s31_usbhost_initialize(void);

#ifdef CONFIG_ESP32S31_USBHOST_HCD_SKELETON
/* Experimental synchronous control/bulk HCD; periodic/async unsupported. */
FAR struct usbhost_driver_s *esp32s31_usbhost_hcd_skeleton(void);

/* Polling root-port connection and NuttX common enumeration callbacks. */
FAR struct usbhost_connection_s *esp32s31_usbhost_hcd_connection_skeleton(void);

/* Channel IRQ stage. The future launcher must configure FIFO/DMA/HCTSIZ,
 * call arm, then launch HCCHAR and enable HAINT/GINT routing. These functions
 * perform real MMIO; they are not called by the compile-only contract.
 * Stop takes -ECANCELED or -ETIMEDOUT and does not release the channel until
 * the hardware CHH interrupt. IRQ attachment and waiter wakeup remain pending.
 */
int esp32s31_usbhost_hcd_channel_arm(unsigned int channel, uint32_t programmed);
int esp32s31_usbhost_hcd_channel_stop(unsigned int channel, int reason);
int esp32s31_usbhost_hcd_channel_interrupt(int irq, void *context, void *arg);

/* Validate the pure DWC2 channel encoder without touching MMIO. */
int esp32s31_usbhost_hcd_skeleton_contract(void);
int esp32s31_usbhost_hcd_start(void);
#endif

#endif /* CONFIG_ESP32S31_USBHOST */

#endif /* __ARCH_RISCV_SRC_ESP32S31_INCLUDE_ESP32S31_USBHOST_H */
