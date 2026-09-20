# media1759 Camera OV2640 probe source verification

## Scope

This record verifies the isolated board-side OV2640/SCCB probe source for the
Function-CoreBoard-1 camera candidate. It is a source and cross-compile check
only; no sensor is connected and no image capture or V4L2 frame path is claimed.

## Source boundary

`openvela-dev/nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/src/esp32s31_camera.c`

The source reuses NuttX's ESP I2C bus 0 and calls `ov2640_initialize()`. It
requires the isolated camera profile pin contract (`I2C0 SCL=GPIO1`, `SDA=GPIO0`)
and remains behind `CONFIG_ESP32S31_CAMERA_OV2640`; it does not register a
capture device or claim DVP frame acquisition.

## Offline result

Using the existing `esp32s31-xts-flat-camera-1690` compile command, with only
temporary command-line configuration definitions for the camera option and
I2C0 pin contract, the source compiled successfully for the pinned S31
RISC-V toolchain:

```text
CAMERA_PROBE_RC=0
text data bss dec hex
92    0    0   92  5c  /tmp/s31_camera_probe.o
```

The generated build output predates the current isolated profile definitions,
so the command-line defines were intentionally kept in `/tmp` and did not
modify the repository configuration or default image.

## Status

This advances the candidate from static API review to source-level
cross-compile evidence. Board sensor registration, capture, pixel output, and
the corresponding xTS/user-added camera acceptance remain blocked until an
OV2640-compatible module and physical wiring are available.
