# Korvo USB Host and SD storage

This single-core FLAT configuration combines the maintained USB Host
control, bulk and interrupt paths with the real SDMMC controller bridge.
MSC, boot keyboard and boot mouse classes are enabled. Connect a USB device
directly to the Korvo Type-A Host port and keep the USB-UART console attached.
The onboard microSD slot uses the Korvo power-control pin.

Build in a new output directory using the repository build helper and
`--profile korvo-usb-sd --workspace-source`. Preserve the current firmware
before deployment. This configuration does not start network activity or
automatically mount, format, or modify removable storage.

Use new directories and synthetic files for file-level verification, compare
the complete content after unmount/remount, and unmount before removal.
An SD memory card does not qualify SDIO I/O functions or DAT1 interrupts.
Host Hub/split, isochronous transfers, Device/ADB, SMP coexistence and actual
device interoperability need their own evidence. Building this profile does
not establish those results.
