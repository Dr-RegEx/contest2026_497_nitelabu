# JPEG / RGB software smoke test (1711)

日期：2026-09-18。该记录只覆盖没有开发板和摄像头时可完成的工程验证，不能替代 S31 板级验收。

## 验证范围

- 使用仓库内 `apps/external/libjpeg-turbo/libjpeg-turbo` 源码构建静态库；
- 通过 `jpeg_mem_dest`/`jpeg_write_scanlines` 完成 RGB888（2×2）编码；
- 通过 `jpeg_mem_src`/`jpeg_read_scanlines` 完成 JPEG 解码并核对尺寸；
- 验证 RGB888↔RGB565 的 5/6/5 位打包、解包及量化误差（每通道不超过 7）。

## 可复现命令

```sh
cd /tmp/s31-jpeg-host
src=/home/regex/work/esp32s31-openvela/openvela-dev/apps/external/libjpeg-turbo/libjpeg-turbo
rm -f *.o libturbo.a media1711-jpeg-rgb-host-smoke
for f in "$src"/j*.c; do
  case "$f" in
    *jccolext.c|*jcstest.c|*jdcol565.c|*jdcolext.c|*jdmrg565.c|*jdmrgext.c|*jstdhuff.c) continue ;;
  esac
  gcc -std=c99 -O2 -I"$src" -Dread_stdin=libjpeg_read_stdin -Dmain=libjpeg_main -c "$f" || exit 1
done
ar rcs libturbo.a *.o
gcc -std=c99 -O2 -I"$src" \
  /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/media1711-jpeg-rgb-host-smoke.c \
  libturbo.a -lm -o media1711-jpeg-rgb-host-smoke
./media1711-jpeg-rgb-host-smoke
```

输出：

```text
PASS jpeg_encode=690 jpeg_decode=2x2 rgb888_rgb565=PASS
```

## 边界

这证明仓库软件编解码和像素格式规则可以在主机上构建运行。当前 `hal_esp32s31.cmake` 尚未把 `upper_hal_jpeg`/`esp_hal_jpeg` 接入 NuttX 用户态 API；开发板也没有摄像头、显示或帧缓冲实物。因此 JPEG 编码、JPEG 解码、Camera、RGB888、RGB565 仍须板级接入和实测，清单只记为“软件自测/待板级验证”，不记为通过。
