# BLE bttool candidate 939 — BUILD ONLY

Latest isolated FLAT bttool candidate: 1209988 bytes. Includes the controller,
54 real OSAL interfaces, original Zblue host and original openvela bttool.
All changes and limitations described in checkpoint937-bttool.md apply.
938 corrected BLE initialization failure state; 939 sets two host advertising
sets and passes the same capacity into the pinned controller configuration,
matching original two-advertiser cases at source lines 5023, 5388 and 5417.
This is capacity preparation, not proof of address selection, timing or RF.

The current successful build receipt still references the active output image.
A frozen byte-identical copy and ELF/map/config are in checkpoint939-bttool.
Before overwriting that output, redirect the receipt to the frozen copy.
No serial access, controller initialization or BLE xTS execution occurred.
Longrun 864 remains the UART owner. Use ble-target-sequence.md after it ends.
