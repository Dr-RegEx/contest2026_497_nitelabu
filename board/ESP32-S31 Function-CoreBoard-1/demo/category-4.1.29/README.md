# WiFi--2.4G--wapi config保存后设备reboot再reconnect

## 已通过范围

- 4.1.29：🟢 已通过；PASS1325 保存配置后原始reboot重连通过

需要自己的 2.4GHz WPA2 AP，按原文使用设备名 wlan0。SSID、密码、主机 IP 在本机设置，归档日志脱敏；扫描/连接压力保持原文 100 轮，不缩减为短测。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/wireless/wapi](../_sources/apps/wireless/wapi)：原工程 `apps/wireless/wapi`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

- 构建配置：[demo-rmt-netapps-competition](../../configs/demo-rmt-netapps-competition/defconfig)。
- 镜像类型：Kernel（nuttx.bin 与同目录 appfs.img 成对）。
- 公共准备、刷写/配对规则：[demo 总说明](../README.md)。

## 构建

先完成仓库根 README 的 repo sync、补丁应用和依赖准备，然后在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-rmt-netapps-competition 8
```

产物位于 `out/esp32s31-demo-rmt-netapps-competition/`。这里指定的是当前源码的对应构建入口；历史固件的精确配置、哈希及修改点以证据记录为准，不承诺新旧镜像逐字节相同。

## 运行步骤与预期结果

1、设备进入到NuttX shell；
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0
3、执行wapi save_config wlan0；
4、 执行reboot；
5、执行 ifup wlan0
6、执行wapi reconnect wlan0
7、执行renew wlan0
8、执行ping router_ip (注：router_ip为路由器网关IP)

**预期结果：**

1-7、执行成功无报错；
8、设备重新配网成功，可以ping通路由器IP

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 3778 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1325-reboot-reconnect/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1325-reboot-reconnect/SHA256SUMS)
- [logs/network1325-reboot-reconnect.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1325-reboot-reconnect.log)
- [network1325-reboot-reconnect-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1325-reboot-reconnect-result.json)
- [checkpoint1325-reboot-reconnect/network1325-reboot-reconnect.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1325-reboot-reconnect/network1325-reboot-reconnect.log)
- [checkpoint1325-reboot-reconnect/network1325-reboot-reconnect-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1325-reboot-reconnect/network1325-reboot-reconnect-result.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
