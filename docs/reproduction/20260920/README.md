# 2026-09-20 新增实测例程与复现验证

本次新增五个例程目录，全部包含普通源码文件、继承配置、证据和完整运行说明：

|例程|配置|证据范围|
|---|---|---|
|common-1.3.6|demo-gpio-loopback|GPIO47↔48，低/高各 4/4、真实上升沿 IRQ；同配对镜像分别启动|
|rgb565-display|korvo-rgb565|Korvo + SUB3 七种画面循环，队友固件串口自检与用户目视确认|
|camera-ov3660|korvo-camera-preview|Korvo + OV3660 + SUB3，RGB565 实物采集上屏；最终 600/600 与目视正常|
|wifi-rate-diagnostics|demo-wifi-rate-window64|四方向速率测量，保留丢包、ACK 缺失与资源耗尽；不计 xTS 通过|
|ble-controller-info|demo-rmt-bttool-kernel|实测 HCI=6.0/LE 特性；不证明全部 BLE 5.4 能力或互通通过|

## 源码与构建

在新目录 `/home/regex/work/s31-reproduce-demos-20260920` 按仓库 manifest 固定提交检出全部 232 个公共项目，并实际执行更新后的 `apply-openvela-snapshot.sh`；14 个源码项目补丁/新增文件均成功应用。

NuttX、Apps 和板级源码同步本次开发快照。HAL 补丁包含 LCD/CAM 共享资源、GDMA NuttX 中断适配、首帧/重开流修复及新增 `gdma_nuttx_irq.h`；新检出的 HAL 成功应用完整补丁。编译器、Python 环境及 HAL 子模块复用 2026-09-19 已验证依赖，以节省重复下载；没有使用开发树生成的 config.h 或旧构建产物。

五个配置均在新的独立输出目录完成完整 CMake/Ninja 编译链接。三个 Kernel 配置都产生同目录的 nuttx.bin/AppFS，大小未超出 0x200000～0x500000 的 AppFS 分区。两个 Korvo 配置为单镜像 FLAT。

构建日志在本目录，产物/最终配置 SHA256、大小及路径见 `results.json`。所有 demo 源码哈希、证据副本哈希和配置 include 闭包均核验通过；demo 不含符号链接。

## 历史证据与新构建的区别

- GPIO 历史 Kernel/AppFS 逐项匹配原 receipt：bd2e9463… / a7674391…。
- 独立 RGB565 演示历史固件匹配队友烧写及用户确认记录：397707e8…。
- Camera 最终历史固件匹配 preview-color 及最终目视确认：b3808750…；本次导出关键源文件与 source-snapshot-color/manifest.json 一致。
- GPIO 连续第二次启动异常保留；不将两次分别启动写成连续运行通过。
- Camera 最终颜色确认时 UART 重开有启动日志，不与此前不重启重开证据混为一谈。
- Wi-Fi 保留较早测量和 64KiB 候选最后四向结果，不拼接不同镜像的最好结果。
- 没有重新刷板、串口采集或拍摄实物视频。新二进制构建通过不继承旧固件的实板结论。

公开副本中的 Wi-Fi 测试口令已脱敏，具体文件及前后哈希见 demo/REDACTIONS.json；case.json 记录公开副本哈希，历史 receipt 中原始哈希不改写。本地原始日志、整片 Flash 私人备份不改动，整片备份不上传。

87 个例程目录中，85 个展示已通过的限定范围，2 个是实测诊断；覆盖 79 个绿色 xTS 条目及 7 个绿色用户新增能力条目，不重复计算 RGB565 的多个演示。
