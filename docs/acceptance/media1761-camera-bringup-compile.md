# media1761 Camera bring-up integration compile check

This is an incremental check after [media1759](media1759-camera-probe-compile.md),
not a duplicate sensor test. The existing `esp32s31_bringup.c` was compiled
with the camera option enabled and the isolated SCCB contract supplied as
temporary command-line definitions:

```text
CONFIG_ESP32S31_CAMERA_OV2640=1
CONFIG_ESPRESSIF_I2C0=1
CONFIG_ESPRESSIF_I2C_PERIPH=1
CONFIG_I2C_DRIVER=1
CONFIG_ESPRESSIF_I2C0_SCLPIN=1
CONFIG_ESPRESSIF_I2C0_SDAPIN=0
```

Result with the pinned S31 RISC-V toolchain and the existing
`esp32s31-xts-flat-camera-1690` include/compile environment:

```text
CAMERA_BRINGUP_PROBE_RC=0
text data bss dec hex
382   0    0   382 17e  /tmp/s31_bringup_camera_probe.o
```

The compile confirms the board bring-up call site and conditional source
integration are type-correct. It does not prove that an OV2640 is connected,
that SCCB responds, or that DVP/V4L2 frames can be captured. Temporary defines
were kept outside the repository; no default configuration or image was
changed.
