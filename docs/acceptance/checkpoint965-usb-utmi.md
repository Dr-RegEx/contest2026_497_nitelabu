# USB965 — explicit S31 UTMI configuration, BUILD ONLY

Replaces945 by calling the pinned S31 usb_dwc_ll helpers to select the16-bit
UTMI interface and set timeout calibration to five PHY clocks. Previously the
port cleared PHYSEL/ULPISEL but left PHYIF dependent on its prior/reset value.
The locked esp_hal_usb/usb_dwc_hal.c uses these helpers for the S31 HS PHY.
This change makes the required interface setup explicit before core reset.

Build965 passed. Frozen ELF/map/config/source, binary and standalone archive
are covered by SHA256SUMS-xts965.945 receipt now references its byte-identical
frozen image. No USB enumeration, register reads or board commands occurred.
Full-speed PIO and all945 connection/feature limits still apply. J4 requires
the verified isolated-VBUS device connection described in the target sequence.
No automated USB flash whitelist was added.
