# USB/ADB target sequence — BUILD ONLY, connection pending

Original xTS source line 2708 (4.2.2) requires starting the ADB task and
connecting successfully with adb shell. TCP ADB and a UART shell do not
replace this USB case.

## Board connection

The board schematic sheet 2 connects the native HS OTG D+/D- to J4 Type-A.
J1 Type-C is fixed USB Serial/JTAG on GPIO33/34; J3 Type-C is the USB-UART
bridge and remains the console. Neither Type-C port provides the native
OTG device endpoints used by this driver.

J4 VBUS_OUT is supplied through TPS2051 U5. Its enable net is biased by
R25/R29, without a GPIO control connection in the schematic. A plain cable
connecting two powered host ports is not the target setup. Before hardware
execution, verify a device-mode adapter with the host and board VBUS sources
isolated, data and ground connected, and the board powered by its normal
console/power connection. Do not change wiring or power during longrun 864.
The initial firmware declares a self-powered device and forces B-session
valid because this board connection has no verified device VBUS sense.

## Software candidate

Profile: xts-flat-usb-adb, separate FLAT/internal-SRAM image. The endpoint
engine derives from the existing NuttX ESP32-S3 implementation, with S31
UTMI clock/reset controls, RESET_DONE acknowledgement, RISC-V IRQ routing,
and full-speed-over-HS PHY encoding. Initial transfers are full-speed PIO;
no high-speed, DMA, isochronous, sleep or USB host qualification is claimed.
USB initialization occurs only when the original ADB class registers.

After longrun ends, receipt checks and the physical connection verification:

```text
mount -t binfs /bin
adbd &
```

Use the original host adb tool to enumerate this device and connect with
adb shell. The embedded shell path is /bin/nsh, supplied by BinFS. Capture
boot, controller initialization, USB enumeration and an actual shell command
response. Local class registration or an adbd process alone is not PASS.
The firmware has no TCP ADB transport enabled. USB flashing has not been
added to the automated flash helper until the connection is verified.
