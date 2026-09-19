# NetApp--设备支持通过scp方式向其他终端传输本地文件

## 已通过范围

- 4.1.116：🟢 已通过；PASS1454 original SCP upload

主机准备允许连接的 SSH/SCP 服务、临时测试账号和文件，分别上传/下载比对；仓库不提供测试密码。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [external/libssh/libssh/examples](../_sources/external/libssh/libssh/examples)：原工程 `external/libssh/libssh/examples`。
- [external/libssh/CMakeLists.txt](../_sources/external/libssh/CMakeLists.txt)：原工程 `external/libssh/CMakeLists.txt`。
- [external/libssh/Kconfig](../_sources/external/libssh/Kconfig)：原工程 `external/libssh/Kconfig`。

`src/` 提供以上源码的相对符号链接，源码实体统一保存在 `demo/_sources/`，可在本仓内直接阅读；共享源文件保留原许可证；curl/libssh 在此只整理命令行例程源码和集成描述，链接库由锁定的完整工作区提供。它们是本仓已上传复现快照的可读副本，实际编译仍使用工作区原路径，修改副本不会自动修改编译树。

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

1、设备进入NuttX shell进行配网:
执行：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <热点名称> 1
renew wlan0
2、执行scp /data/<本地文件> <终端用户名@HostIP:文件所在绝对路径>
eg:scp /data/full_ota.zip <user>@<file_server_ip>:/home/sss/Downloads/
3、输入 yes, 回车, 回车, 输密码
eg:
scp /data/full_ota.zip <user>@<file_server_ip>:/home/sss/Downloads/
4、等待数据传输成功后，在终端上查看文件大小是否和设备上的本地的文件大小相同

**预期结果：**

1、设备配网成功
2-3、传输无报错
4、终端上的文件大小和设备上的本地的文件大小相同

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 4880 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1454-scp-bidirectional/README.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1454-scp-bidirectional/README.md)
- [checkpoint1454-scp-bidirectional/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1454-scp-bidirectional/SHA256SUMS)
- [logs/network1454-scp-upload.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1454-scp-upload.log)
- [network1454-scp-upload-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1454-scp-upload-result.json)
- [checkpoint1454-scp-bidirectional/network1446-prepare.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1454-scp-bidirectional/network1446-prepare.log)
- [checkpoint1454-scp-bidirectional/network1454-scp-upload.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1454-scp-bidirectional/network1454-scp-upload.log)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
