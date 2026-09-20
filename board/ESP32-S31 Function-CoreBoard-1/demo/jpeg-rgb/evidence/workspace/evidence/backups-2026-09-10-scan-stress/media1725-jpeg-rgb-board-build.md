# 1725 JPEG/RGB board image build evidence

- Date: 2026-09-18
- Profile: `xts-flat-jpeg-rgb`
- Image: `openvela-dev/out/esp32s31-xts-flat-jpeg-rgb-1690/nuttx.bin` (573604 bytes)
- SHA-256: `e54449238f7bece937fa5677cd3b18aeb5d7e69fe011ed5794ad1c6573fe0e69`
- Receipt: `build1725-jpeg-rgb.sha256`

The image enables `CONFIG_LIB_JPEG_TURBO` and includes the `jpegtest` command.
The command performs a fixed 2x2 RGB888 -> JPEG -> RGB565 round trip, checks
JPEG SOI/header and decoded geometry, and checks RGB565 quantization bounds.
The image was flashed at `0x2000` and `jpegtest` completed on the board:
`jpegtest: PASS JPEG bytes=713 decode=2x2 RGB888=12 bytes RGB565=8 bytes`.
The runtime log is `jpeg1728-board-test/uart.log`; this validates the fixed
codec and pixel-format path, not camera sensor capture.
