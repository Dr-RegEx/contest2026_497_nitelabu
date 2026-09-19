# 1726 Camera/V4L2 build evidence

- Date: 2026-09-18
- Profile: `xts-flat-camera`
- Image: `openvela-dev/out/esp32s31-xts-flat-camera-1690/nuttx.bin`
- Receipt: `build1726-camera.sha256`
- Image size: 590444 bytes

The candidate enables `DRIVERS_VIDEO`, `VIDEO_STREAM`, `VIDEO_OV2640`,
`OV2640_RGB565_COLORFMT`, the NuttX `camera` example, and the JPEG/RGB smoke
command. The V4L2 interface and OV2640 source compile successfully after
fixing `IMGDATA_SET_BUF` to return `-ENOTTY` (its callback type is `int`; the
old `NULL` fallback failed the compiler's pointer/integer check).

The Function-CoreBoard currently has no connected OV2640/DVP sensor and no
board-level sensor registration. `ov2640.c` is therefore compile evidence,
while runtime capture and JPEG frame output remain pending physical wiring.
This candidate was not flashed and does not replace the existing image.
