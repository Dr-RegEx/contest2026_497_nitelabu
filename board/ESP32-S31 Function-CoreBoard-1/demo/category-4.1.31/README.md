# WiFi--2.4G--wapi disconnect后与外网不通

## 已通过范围

- 4.1.31：🟢 已通过；PASS1342 原始公网断网检查通过

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

1、设备进入到NuttX shell
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <路由器SSID> 1
renew wlan0 ；
3、执行ifconfig,查看wlan0的ip;
4、ping <外网ip地址> ，如www.baidu.com
5、执行wapi disconnect wlan0
6、ping <外网ip地址> ，如www.baidu.com

**预期结果：**

2、设备配网成功；
3、可查看到IP地址，该IP地址为路由器分配的ip；
4、执行成功；
5、ping不通，系统无异常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 3689 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1342-disconnect/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1342-disconnect/SHA256SUMS)
- [network1342-disconnect-review.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1342-disconnect-review.json)
- [logs/network1342-disconnect-state.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1342-disconnect-state.log)
- [checkpoint1342-disconnect/network1341-disconnect.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1342-disconnect/network1341-disconnect.log)
- [checkpoint1342-disconnect/network1340-ftp-cleanup.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1342-disconnect/network1340-ftp-cleanup.log)
- [checkpoint1342-disconnect/network1342-disconnect-state.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1342-disconnect/network1342-disconnect-state.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
