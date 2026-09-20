# BLE 1725 独立构建与命令覆盖审计

日期：2026-09-18  
目标板：ESP32-S31 Function-CoreBoard-1  
范围：比赛必须适配清单中的 BLE xTS 18 项；本记录只证明镜像构建和命令/配置覆盖，不把它当作手机或空口验收。

## 构建结果

使用现有 `demo-rmt-bttool-kernel` 配置，在独立输出目录
`openvela-dev/out/esp32s31-ble-static1725` 完成全量构建（3460 个 Ninja 目标）。
当前构建包含 `nuttx.bin`、`appfs.img` 和 `appfs-root/bttool`。构建阶段出现已有第三方代码的编译警告，但没有错误，最终镜像生成成功。

SHA-256 配对收据：[`build1725-ble-static.sha256`](build1725-ble-static.sha256)

配置中同时启用：

* `ESP32S31_BLE`、`SMP`、`ESP32S31_SMP`、`ARCH_ADDRENV`；
* BLE 广播、扫描和 GATT server；
* `BT_PERIPHERAL`、`BT_CENTRAL`、`BT_BROADCASTER`、`BT_OBSERVER`；
* `BT_EXT_ADV`、`BT_PHY_UPDATE`、`BT_DATA_LEN_UPDATE`、`BT_SMP`；
* `BLUETOOTH_TOOLS`，并生成 `bttool` 应用。

## 命令覆盖

源码和独立构建共同确认以下命令路径已进入镜像：

|能力|实现/命令|覆盖情况|
|---|---|---|
|广播启动/停止|`bttool adv start`、`adv stop`|支持 legacy/ext/auto；回调输出 handle、adv_id、status|
|广播间隔|`adv start -i`|接受 `0x20..0x4000`（0.625 ms 单位）；四档实际误差仍需对端计时|
|地址类型|`adv start -R`、`-O`|支持 public/random/public_id/random_id/anonymous；多路对端观察仍待测|
|扫描启动/停止|`bttool scan start`、`scan stop`|支持 passive/active 和停止回调|
|扫描 PHY|`scan start -p 1M/2M/Coded`|三种参数在工具层有映射；2M/Coded 射频可用性仍需 controller HCI/对端验证|
|扫描 mode|`scan start -m 0/1/2`|对应 low-power/balanced/low-latency；包数/成功率仍待原始流程|
|连接/配对|`leconnect`、`createbond`、`BT_SMP`|接口和配置进入镜像；20 次成功率仍需手机/对端|
|GATT|`gatts`、已有 1544/1551/1564|GATT server 配置和旧实机记录有效；不替代 BLE xTS 发现/配对统计|
|PHY/数据长度|`setphy`、`BT_PHY_UPDATE`、`BT_DATA_LEN_UPDATE`|工具/API和配置具备；对端协商仍待测|

## 判定

本轮没有刷写开发板，也没有新增原始 xTS 通过项。`adv`/`scan`/PHY/mode/配对等项目可从“缺镜像能力”推进为“独立镜像已具备、待空口闭合”；四档广告间隔、两路地址、扫描成功率/效率、共存流程仍必须按原文使用手机或 BLE 对端完成。BLE 5.4、LE Audio、Mesh 1.1 仍不因本构建改变其待核验状态。

复现命令：

```sh
backups/2026-09-10-scan-stress/build1725-ble-static.sh \
  /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/build1725-ble-static.sha256
```
