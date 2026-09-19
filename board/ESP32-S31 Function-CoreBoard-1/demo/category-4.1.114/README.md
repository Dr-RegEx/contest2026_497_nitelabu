# NetApp--curl网页能力测试

## 已通过范围

- 4.1.114：🟢 已通过；PASS1334 原始公网网页命令通过

联网后使用自己可访问的 HTTP 地址，下载文件核对长度/内容；网页请求需核对实际响应。

本目录按现有验收清单整理历史通过例程，没有重新执行板上测试。新编译的镜像需要按下述步骤自行验证；不能把旧日志当作新镜像的测试结果。

## 源码与配置

- [external/curl/curl/src](../_sources/external/curl/curl/src)：原工程 `external/curl/curl/src`。
- [external/curl/CMakeLists.txt](../_sources/external/curl/CMakeLists.txt)：原工程 `external/curl/CMakeLists.txt`。
- [external/curl/Kconfig](../_sources/external/curl/Kconfig)：原工程 `external/curl/Kconfig`。

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

1、设备进入到NuttX shell
2、设备配网：
ifup wlan0
wapi mode wlan0 2
wapi psk wlan0 <热点密码> 3
wapi essid wlan0 <热点名称> 1
renew wlan0 ；
3、执行命令 curl www.baidu.com

**预期结果：**

2、设备配网成功
3、 网页内容打印到串口中

[完整原始用例（含前提配置）](original-case.md)；原文定位：[项目保存的 xTS 原文](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint988-clock24h-review/original-openvela-xts-test-cases.md) 第 5050 行。原文设备节点、挂载目录等通用占位符应使用上文 S31 实际映射，不能照抄不存在的设备。

## 历史通过依据

- [checkpoint1334-curl-web/network1332-dns-fix.md](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1334-curl-web/network1332-dns-fix.md)
- [checkpoint1334-curl-web/SHA256SUMS](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1334-curl-web/SHA256SUMS)
- [logs/network1334-curl-web.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/logs/network1334-curl-web.log)
- [network1334-curl-web-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/network1334-curl-web-result.json)
- [checkpoint1334-curl-web/network1334-curl-web.log](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1334-curl-web/network1334-curl-web.log)
- [checkpoint1334-curl-web/network1334-curl-web-result.json](../../../../workspace/evidence/backups-2026-09-10-scan-stress/checkpoint1334-curl-web/network1334-curl-web-result.json)
- [xts-category-status.md](../../../../docs/acceptance/xts-category-status.md)

[当前验收清单](../../../../docs/acceptance/比赛必须适配清单.md) 是归类依据；旧失败、人工确认及观测缺口均保留。历史脚本可能带旧绝对路径，供审核，不作为一键运行入口。
