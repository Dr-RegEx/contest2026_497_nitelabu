# JPEG / RGB host smoke recheck (1715)

日期：2026-09-18。针对清单中的用户自添加第 3、4、6、7 项，重新从仓库源码构建 vendored libjpeg-turbo，并运行现有烟测程序。该验证不使用开发板，不能替代板级接入验收。

```text
PASS jpeg_encode=690 jpeg_decode=2x2 rgb888_rgb565=PASS
```

构建过程出现 `setenv` 的 C99 原型警告，但无编译错误，程序退出状态为 0。结果与 1711 一致：RGB888→JPEG 编码成功，内存 JPEG 解码回 2×2，RGB888↔RGB565 转换通过。板级 Camera、JPEG HAL 和显示/帧缓冲链路仍待实物验证。
