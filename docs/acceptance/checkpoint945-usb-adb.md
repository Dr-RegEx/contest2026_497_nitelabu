# S31 USB/ADB candidate 945 — BUILD ONLY

Original USB ADB class, microADB daemon and its actual nsh/PTY shell transport
are linked with a staged S31 UTMI USB device controller. Full-speed PIO only.
The endpoint engine derives from the existing NuttX ESP32-S3 implementation;
S31-specific differences include native 0x20300000 register base, UTMI clock
and PHY controls, RESET_DONE acknowledgement, full-speed-over-HS encoding,
RISC-V IRQ routing, deferred pull-up until class registration, and bounded
initial reset waits. Key register offsets are checked against the pinned
S31 HAL structure at compile time; actual core endpoint/FIFO capacity is
checked before programming FIFO layout.

940 Kconfig dependency loop and 941 missing libuv TLS settings were corrected.
942 first built; 943 had misspelled offset-check macros; 944 fixed those; 945
keeps the device detached while preparing the endpoint engine. Logs retained.

No USB enumeration, controller register read or adb shell session has occurred.
The board native USB is J4 Type-A with resistor-enabled VBUS sourcing, so the
physical device connection must be verified before testing. Existing Type-C
UART and Serial/JTAG ports do not substitute for this interface. Automated
flash helper intentionally has no USB profile entry pending that connection.
See usb-adb-target-sequence.md. This does not claim HS/USB host/DMA/PM support.

Superseded by965: explicit pinned16-bit UTMI interface and timeout calibration.
945 preserved BUILD ONLY; see checkpoint965-usb-utmi.md.
