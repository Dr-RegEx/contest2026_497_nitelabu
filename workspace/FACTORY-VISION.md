# Factory 与原版双核数字识别

配置 `korvo-factory-vision` 在同一份 openvela 固件中运行 Factory 主界面和原版数字识别。上电进入 Factory，摄像头显示实时画面；点击 **Digit recognition** 进入原版 VGA 取景与识别侧栏，点击右下角 **Return** 回到 Factory。切换时协作停止摄像任务并交接显示设备，不需要重新烧录或复位。

本组合已通过实板开机和用户触摸验收：识别画面不再闪烁、识别正常，Return 后实时预览恢复，精确镜像见 [验收记录](../docs/validation/factory-vision.json)。

识别仍使用原来的模型、预处理、字体和 SIMD 内核：CPU0 负责摄像与显示，CPU1 负责推理。Factory 通过 openvela Activity/Window 服务接收触摸，内核 `/dev/vision` 管理原版识别任务与摄像头、LCD 的所有权。咖啡机页面保留原图片和 libpng 解码，临时解码像素通过可回收的共享页分配。

## 构建

完成 [公共源码准备](REPRODUCE.md) 后执行：

```bash
bash "$CONTEST_REPO/workspace/build.sh" --workspace "$CHECKOUT" \
  --profile korvo-factory-vision --output /path/to/new-factory-vision --jobs 4
```

产物为 `build/nuttx.bin` 和 `build/appfs.img`，两者必须来自同一次构建。`receipt.json` 保存二者及 `.config` 的 SHA256。独立配置 `korvo-openvela-factory` 和 `korvo-digit-rebuild` 继续保留，不能将它们的镜像与此配置混配。

## 部署

适用板卡为 16 MiB Flash 的 ESP32-S31 Korvo-1。按 [部署说明](FLASH-MANUAL.md) 确认目标串口、安全状态并保存本板完整备份，再设置相同的 `ESPTOOL`、`PORT` 和本次 `FIRMWARE` 输出目录。

| 区域 | 起始地址 | 范围 |
| --- | --- | --- |
| 内核 | `0x2000` | 不得超过 `0x200000` |
| 只读 AppFS | `0x200000` | 最多 `0xc00000` 字节 |
| 可写应用数据 | `0xe00000` | `0x200000` 字节 |

```sh
"$ESPTOOL" --chip esp32s31 --port "$PORT" write-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 "$FIRMWARE/nuttx.bin" 0x200000 "$FIRMWARE/appfs.img"
"$ESPTOOL" --chip esp32s31 --port "$PORT" verify-flash \
  --flash-mode keep --flash-freq keep --flash-size keep \
  0x2000 "$FIRMWARE/nuttx.bin" 0x200000 "$FIRMWARE/appfs.img"
```

第一次从其他配置切换且已保存备份时，初始化本配置的数据区；此操作删除该区域原有应用数据：

```sh
"$ESPTOOL" --chip esp32s31 --port "$PORT" erase-region 0xe00000 0x200000
"$ESPTOOL" --chip esp32s31 --port "$PORT" --after hard-reset run
```

串口速率为 115200。启动应出现 `Factory HOME startup` 和 `VISION START cpu=0`；进入识别后应出现 `VISION AI cpu=1 SIMD parity_errors=0 heldout=100/100`。`cat /dev/vision` 可查看摄像、显示、推理计数。模型自检通过不能代替实物数字识别及触摸验收，实际验证状态见 [配置登记表](profiles.json)。

## 配置范围

本组合配置使用双核 KERNEL/MMU。SD、USB Host、ADC 按键受现有 SMP 配置限制而关闭；这些外设的历史专项结果不作为此镜像的通过记录。MEDIA 服务随系统启动，但本轮不包含听音验收。此配置也不作无线联网、长时负载或帧率提升的声明。
