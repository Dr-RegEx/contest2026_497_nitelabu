/****************************************************************************
 * arch/risc-v/src/esp32s31/esp32s31_usbhost.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <nuttx/arch.h>

#include "riscv_internal.h"
#include "hardware/esp32s31_otg.h"
#include "hal/usb_dwc_ll.h"
#include "hal/usb_utmi_ll.h"
#include "soc/usb_dwc_struct.h"
#include "esp32s31_usbhost.h"

#if defined(CONFIG_ESP32S31_USBHOST) && defined(CONFIG_USBHOST)

/* The host candidate uses the same S31 DWC2 core as the existing device
 * driver.  Keep the register-header offsets checked at compile time so a
 * generic OTG header cannot silently drift from the pinned S31 HAL. */

_Static_assert(offsetof(usb_dwc_dev_t, grstctl_reg) ==
               ESP32S31_OTG_GRSTCTL_OFFSET, "S31 reset register offset");
_Static_assert(offsetof(usb_dwc_dev_t, hcfg_reg) ==
               ESP32S31_OTG_HCFG_OFFSET, "S31 host config offset");
_Static_assert(offsetof(usb_dwc_dev_t, hprt_reg) ==
               ESP32S31_OTG_HPRT_OFFSET, "S31 host port offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hcchar_reg) ==
               ESP32S31_OTG_HCCHAR_OFFSET(0), "S31 channel offset");
_Static_assert(offsetof(usb_dwc_dev_t, haint_reg) ==
               ESP32S31_OTG_HAINT_OFFSET, "S31 host interrupt offset");
_Static_assert(offsetof(usb_dwc_dev_t, haintmsk_reg) ==
               ESP32S31_OTG_HAINTMSK_OFFSET, "S31 host interrupt mask offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hcint_reg) ==
               ESP32S31_OTG_HCINT_OFFSET(0), "S31 channel interrupt offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hcintmsk_reg) ==
               ESP32S31_OTG_HCINTMSK_OFFSET(0),
               "S31 channel interrupt mask offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hctsiz_reg) ==
               ESP32S31_OTG_HCTSIZ_OFFSET(0), "S31 channel size offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hcdma_reg) ==
               ESP32S31_OTG_HCDMA_OFFSET(0), "S31 channel DMA offset");
_Static_assert(offsetof(usb_dwc_dev_t, host_chans[0].hcdmab_reg) ==
               ESP32S31_OTG_HCDMAB_OFFSET(0),
               "S31 channel DMA buffer offset");

/*
 * S31's Type-A connector is wired as an A-device.  The core has host-mode
 * registers and experimental synchronous PIO HCD callbacks with a polling
 * root port. Interrupt routing is still unimplemented. Keep
 * this initialization separate from the existing Device/ADB driver so an
 * experimental host image cannot silently change the normal image's role.
 */

int esp32s31_usbhost_initialize(void)
{
  uint32_t regval;
  unsigned int retry;

  /* The S31 HS block is clocked through the UTMI companion.  These are the
   * same pinned Espressif LL steps used by the existing device path, with
   * host-required 15k line pulldowns enabled. */

  usb_utmi_ll_enable_bus_clock(true);
  usb_utmi_ll_reset_register();
  usb_utmi_ll_enable_precise_detection(true);
  usb_utmi_ll_configure_ls(&USB_UTMI, true);
  usb_utmi_ll_enable_data_pulldowns(true);
  usb_utmi_ll_set_suspend_state(false);
  putreg32(0, ESP32S31_OTG_PCGCCTL);

  /* A missing/unmapped core must fail before touching OTG role registers. */

  if (getreg32(ESP32S31_OTG_GSNPSID) == UINT32_MAX ||
      (getreg32(ESP32S31_OTG_GSNPSID) & 0xffff0000u) != 0x4f540000u)
    {
      return -ENODEV;
    }

  /* The S31 PHY is a 16-bit UTMI interface.  Keep timeout calibration in
   * sync with the official device initialization before changing role. */

  usb_dwc_ll_gusbcfg_set_timeout_cal(&USB_OTGHS, 5);
  usb_dwc_ll_gusbcfg_set_utmi_phy(&USB_OTGHS);

  /* Wait for the AHB master to become idle, then perform the S31 core reset.
   * Bit 29 is the S31-specific reset-done indication documented by the
   * pinned HAL. */

  for (retry = 0; retry < 1000; retry++)
    {
      if ((getreg32(ESP32S31_OTG_GRSTCTL) & OTG_GRSTCTL_AHBIDL) != 0)
        {
          break;
        }

      up_udelay(3);
    }

  if (retry == 1000)
    {
      return -ETIMEDOUT;
    }

  regval = getreg32(ESP32S31_OTG_GRSTCTL) | OTG_GRSTCTL_CSRST;
  putreg32(regval, ESP32S31_OTG_GRSTCTL);
  for (retry = 0; retry < 1000; retry++)
    {
      if ((getreg32(ESP32S31_OTG_GRSTCTL) & (1u << 29)) != 0)
        {
          break;
        }

      up_udelay(3);
    }

  if (retry == 1000)
    {
      return -ETIMEDOUT;
    }

  regval = getreg32(ESP32S31_OTG_GRSTCTL);
  regval &= ~OTG_GRSTCTL_CSRST;
  regval |= 1u << 29;
  putreg32(regval, ESP32S31_OTG_GRSTCTL);
  up_udelay(3);

  /* Stop the device-side global interrupt gate while changing roles. */

  regval = getreg32(ESP32S31_OTG_GAHBCFG);
  putreg32(regval & ~OTG_GAHBCFG_GINTMSK, ESP32S31_OTG_GAHBCFG);

  /* Force host mode.  PHY power/clock ownership remains with the future HCD. */

  regval = getreg32(ESP32S31_OTG_GUSBCFG);
  regval &= ~OTG_GUSBCFG_FDMOD;
  regval |= OTG_GUSBCFG_FHMOD;
  putreg32(regval, ESP32S31_OTG_GUSBCFG);

  for (retry = 0; retry < 100; retry++)
    {
      if ((getreg32(ESP32S31_OTG_GINTSTS) & OTG_GINTSTS_CMOD) != 0)
        {
          break;
        }

      up_udelay(10);
    }

  if (retry == 100)
    {
      return -EIO;
    }

  /* S31 uses HS UTMI, not the FSLS PHY. Pinned HAL only selects 48/6 MHz
   * for hsphy_type == 0. Keep UTMI's 30/60 MHz encoding and permit HS.
   */

  regval = getreg32(ESP32S31_OTG_HCFG);
  regval &= ~(OTG_HCFG_FSLSPCS_MASK | OTG_HCFG_FSLSS);
  regval &= ~(1u << 23); /* PIO, not descriptor DMA. */
  putreg32(regval, ESP32S31_OTG_HCFG);

  /* Serialized PIO partition: enough RX space for a 512-byte HS packet
   * plus status, and a 256-word non-periodic transmit FIFO. Fail explicitly
   * if the actual core cannot provide this memory.
   */

  if (usb_dwc_ll_ghwcfg_get_fifo_depth(&USB_OTGHS) < 768)
    {
      return -ENOSPC;
    }

  if (!USB_OTGHS.ghwcfg2_reg.dynfifosizing ||
      USB_OTGHS.ghwcfg2_reg.numhstchnl != 15)
    {
      return -ENOTSUP;
    }

  usb_dwc_ll_gahbcfg_en_slave_mode(&USB_OTGHS);
  if (USB_OTGHS.gahbcfg_reg.dmaen)
    {
      return -ENOTSUP;
    }
  usb_dwc_ll_grxfsiz_set_fifo_size(&USB_OTGHS, 512);
  usb_dwc_ll_gnptxfsiz_set_fifo_size(&USB_OTGHS, 512, 256);

  /* Root-port polling worker is started separately by board bringup. */

  regval = getreg32(ESP32S31_OTG_HPRT);
  regval &= ~(OTG_HPRT_PCDET | OTG_HPRT_PENCHNG |
              OTG_HPRT_POCCHNG | OTG_HPRT_PENA);
  regval |= OTG_HPRT_PPWR;
  putreg32(regval, ESP32S31_OTG_HPRT);

  return 0;
}

#endif /* CONFIG_ESP32S31_USBHOST && CONFIG_USBHOST */
