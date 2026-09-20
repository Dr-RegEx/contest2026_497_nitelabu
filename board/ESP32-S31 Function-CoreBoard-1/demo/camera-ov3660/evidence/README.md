# Korvo-1 camera-to-screen bring-up

**最新结论：Korvo + OV3660 + SUB3 摄像头真实画面上屏通过。** 修复后600/600帧命令PASS，640×480 RGB565居中显示到800×480屏幕，约4.43fps；同一镜像再次预览，用户目视反馈“完全正常”（景物变化、红绿蓝颜色及完整性）。最终候选 `preview-color`，SHA256 `b38087502aff3465b8d49e02f4256d2ff581a3ab0a2ac3fe1596be8501d95efc`。下文按时间保留失败及修复过程，历史“待确认”不代表当前结论。

目标：使用现有openvela移植在Korvo-1上采集真实摄像头画面并显示到SUB3；不以屏幕测试图案替代摄像头验证。

当前实物：CP2102N已通过usbipd接入WSL /dev/ttyUSB0；芯片ESP32-S31 rev0.0，Flash16MB，MAC30:ed:a0:f3:df:34。原固件已运行队友NuttX RGB565屏幕演示，启动记录original-boot.log显示800x480 framebuffer与FIRST_FRAME_UPDATE=PASS。SCCB 实测传感器 ID 为 0x3660，确认 OV3660（probe/boot.log）。

原16MB Flash：original-flash-16MB.bin，完整校验凭据backup.json；当前屏幕基线可恢复。不是原厂出厂镜像。原始备份仅作本地恢复，不自动发布。

独立配置：korvo-camera-preview。先读取OV3660地址0x3c的ID寄存器，只有匹配0x3660才写入初始化；若不匹配或无应答，应继续识别实际摄像头，不声明传感器损坏。

官方资料：
- https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s31/esp32-s31-korvo-1/user_guide.html
- reference/保存官方factory_demo提交3c0f632的相关文件，其中同时配置OV3660和SC101IOT；不能据此认定实物型号。

阶段：整片备份完成；LCD/CAM 共享时钟/复位修复、V4L2→FB 预览应用已构建并上板。真实采集已启动，目视确认待补。

实测演进（失败日志原样保留）：
- probe：OV3660 识别成功，/dev/video0 与 /dev/fb0 注册成功。
- preview、preview-stage、preview-dma-stage、preview-irq-stage：首帧前停止响应；DMA 启动可返回，但中断未进入 GDMA 回调。
- preview-native-irq：GDMA 改用 NuttX 分配/分发后，60 帧真实采集及 framebuffer 更新全部成功（约 26.9 秒）；FLAT framebuffer munmap 返回 EINVAL，命令整体 FAIL。
- preview-clean：修复 FLAT framebuffer 清理及首个 DMA 缓冲指针发布顺序；20/20 帧 PASS（约 4.67 秒）；第二次打开 0/600 超时。摄像头 stop 后 HAL 未重新开启接收流，继续修复中。

以上仅限 Korvo + OV3660 + SUB3 独立 FLAT 单核配置，不能据此扩大旧 Function 板或其它摄像头的验收范围。

- preview-restart：接收器/DMA 恢复顺序候选未出帧，0/20 与 0/600 超时；原始日志保留。
- preview-restart-order：调整为先恢复接收器/FIFO，再启动 DMA。首轮 20/20 PASS（4.75 秒），不重启再次启动 600 帧预览，已收到真实帧；完整结果与目视确认待收尾。

本轮实质修复：
1. GDMA 通过 NuttX `esp_setup_irq/irq_attach` 分配/分发，避免 vendor 独立向量表绕过 NuttX；保留同源 RX/TX 共享、状态过滤、最后使用者释放与错误回退。
2. DMA 启动前发布 `cur_buf`，防止首个中断抢先到达、旧任务覆盖下一帧指针。
3. 重新 start 时恢复 camera 接收器，避免 stop 后再次预览超时。
4. FLAT 模式直接使用 framebuffer 驱动地址，不对未登记的映射执行 munmap；内核模式仍使用 mmap/munmap。

主机回归：`irq-test/result.log`，实际中断适配头文件以 `-Wall -Wextra -Werror -fsanitize=address,undefined` 编译运行；覆盖共享收发分发、禁用回调、最后使用者释放、不兼容优先级、attach 失败回退和重新分配。不能代替摄像头板测。
源文件快照及哈希：`source-snapshot/manifest.json`；当前镜像配置与写入校验：`preview-restart-order/receipt.json`、`.config`、`verify.log`。


2026-09-20 收尾进展：`preview-restart-order` 已完成 20/20 和不重启重开 600/600，均 PASS；600 帧 135.23 秒（约 4.44 fps），SHA256 `2f4eeae4a81e5ce3f1abdb037c29d0d3a70ea0e86107d58560816a9423aea921`。用户确认画面随景物变化，但“色彩完全不对”，因此目视验收未通过。增加 OV3660 RGB565 高/低字节转换，生成 `preview-color` 候选进行颜色复测；不是以软件更新成功代替颜色通过。


颜色修复候选 `preview-color`：600/600 PASS，135.35 秒，约 4.43 fps，退出正常；镜像 SHA256 `b38087502aff3465b8d49e02f4256d2ff581a3ab0a2ac3fe1596be8501d95efc`。OV3660 交付处转换高低字节以符合 V4L2 RGB565 LE，实际转换循环主机 ASan/UBSan 色块测试通过（color-copy-test.log）。`source-snapshot-color/` 保存本候选源码。已通知用户再次观察，**颜色目视确认待反馈**；未计为完整目视验收通过。

复现：在已有依赖工作区运行 `bash backups/2026-09-20-korvo/build-preview.sh`；目标配置 `esp32s31-core-function-board:korvo-camera-preview`。使用本目录候选镜像时先核对对应 receipt/verify.log，平坦镜像写入 0x2000。NSH 命令 `s31preview --frames 600`，640×480 居中显示于 800×480 屏幕；不是 JPEG 路径。原整片备份可恢复先前屏幕固件。

用户本轮反馈：“颜色基本正确，再来一次确认”。保持 b3808750 镜像，再开 `preview-color-confirm` 600 帧目视复核；不重新刷写。

最终目视复核 `preview-color-confirm/uart.log`：600/600 PASS，135.33秒，退出正常；同镜像用户确认“完全正常”。复核串口重开有启动日志，不冒称本轮无重启；不重启启停另见 preview-restart-order。
