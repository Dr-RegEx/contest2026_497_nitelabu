# NetApp--设备支持ftpd

## 已通过范围

- 4.1.117：🟢 已通过；PASS1337 复核原始1336双向传输通过

主机使用 FTP 客户端完成双向文件传输并比对；历史 1336/1337 为实际传输及复核。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [apps/examples/ftpd](../_sources/apps/examples/ftpd)：原工程 `apps/examples/ftpd`。

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

1、设备端执行：ftpd_start -4 &
2、同一局域网对端设备PC执行：ftp <设备IP>
输入用户名：
password:
3、PC 端进入data目录
cd data
ls
1）get <设备data目录下的某个文件> <文件在pc上想要保存的名称>
2）put <pc 当前目录下某个文件> <文件在设备上想要保存的名称>
4、PC 执行quit 退出ftp

**预期结果：**

2、PC端可以正常登录设备的ftp；
3、传输文件正常，无报错，设备端log无异常；
1）设备上的文件通过get拉取到了pc上；
2）pc上的文件通过put传输到了设备中
4、PC正常退出，设备端串口log无异常

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 4838 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1337-ftpd-review/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1337-ftpd-review/SHA256SUMS)
- [network1337-ftpd-review.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1337-ftpd-review.json)
- [checkpoint1337-ftpd-review/network1336-ftpd.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1337-ftpd-review/network1336-ftpd.log)
- [checkpoint1337-ftpd-review/network1337-ftpd-review.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1337-ftpd-review/network1337-ftpd-review.json)
- [checkpoint1337-ftpd-review/network1336-ftpd-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1337-ftpd-review/network1336-ftpd-result.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
