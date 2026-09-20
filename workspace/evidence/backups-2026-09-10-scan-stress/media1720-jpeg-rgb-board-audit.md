# 1720 镜像 JPEG / RGB / Camera 板上核验

日期：2026-09-18。该核验只读取当前已上板的 1720 镜像，不重刷、不改网络和媒体配置。

## 串口结果

完整原始记录：[media1720-jpeg-rgb-board-audit.log](media1720-jpeg-rgb-board-audit.log)

- `help media`：`nsh: media: command not found`
- `help jpeg`：`nsh: jpeg: command not found`
- `help camera`：`nsh: camera: command not found`
- `/system/bin` 仅含当前网络、蓝牙、板级 demo 等程序，没有 media/jpeg/camera 可执行文件；`/dev` 没有 video/camera 节点。

## 配置交叉核对

`openvela-dev/out/esp32s31-spawn-kernel1720/.config` 中以下选项均为关闭：

```text
# CONFIG_DRIVERS_VIDEO is not set
# CONFIG_VIDEO is not set
# CONFIG_EXAMPLES_MEDIA is not set
# CONFIG_LIB_JPEG_TURBO is not set
# CONFIG_MEDIA is not set
```

因此当前镜像无法在板上执行 JPEG 编码/解码、RGB888/RGB565 链路或 Camera 采集；这不是测试失败，而是目标驱动、库和用户态入口尚未进入该镜像。已有 1711/1715 主机烟测仍有效，但不能升级为板级通过。Camera 还需要实际摄像头、引脚和 capture lower-half。
