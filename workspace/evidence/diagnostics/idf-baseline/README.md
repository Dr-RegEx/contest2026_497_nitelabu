# S31 IDF 实板对照（2026-09-11）

用户已明确授权：双份读回备份并校验后临时切换 IDF 引导/分区，测试后
恢复 openvela。不下载依赖、不改参考仓库、不改路由器或安全熔丝。

本目录是独立诊断工程，不是主工程或替代移植；没有初始化 Git 仓库。
锁定 IDF：`14f663f003eb8fd9a688c301a412a9540d29dacf`，本地路径
`../../s31-reference/tmp/esp-idf-clean`。构建关闭 component manager 和
submodule 自动处理，使用现有离线保护命令；参考依赖复验通过。

## Flash 安全和恢复

持久备份：`../../backups/2026-09-10-scan-stress/idf170-flash-backup/`。
`before-a.bin`、`before-b.bin` 是板上 0x0–0x4fffff 两次完整读回，
长度均为 5242880，逐字节相同，SHA-256：
`d3486d22ec287c1aca5c0da1ba5728e46d11095e8c7b0b8bd2ec2fda3a7c1d44`。
备份目录权限 0700，文件 0600。只读安全查询：Secure Boot/Flash
Encryption 均 Disabled，MAC `30:ed:a0:f3:f7:b0`，CP2102N 序列号
`f65e4ef67f71f011975a049f1045c30f`。

实际擦写扇区（半开区间）：`[0x2000,0x7000)`、`[0x8000,0x9000)`、
`[0x10000,0xe8000)`。IDF 应用唯一分区 `[0x10000,0x200000)`；
不存在 NVS/OTA/PHY 数据分区，Wi-Fi 与 PHY 持久化均关闭。
凭据通过隐藏主机输入、无回显长度前缀 UART 帧提供，仅存 RAM。
没有擦除整片或访问 0x500000 起可写分区。0x200000 起 AppFS 不写入。

运行器在 finally 中恢复相同扇区的原始字节，然后重新读回 5 MiB
逐字节比较。正常异常、SIGINT/SIGTERM 会触发恢复；断电/SIGKILL
不能由软件保证恢复。遇到这两种情况，先核对串口占用与当前日志，
再执行对应恢复命令，不能直接烧录主源码最小 NSH 镜像：

```sh
# 在工作区根目录；使用现有支持 pyserial 的环境。
S31_IDF_RUN=170 s31-reference/.venv-nuttx/bin/python diagnostics/idf-baseline/flash-transaction.py restore
# 171 对照使用独立事务和日志，保留 170 的备份/结果。
S31_IDF_RUN=171 s31-reference/.venv-nuttx/bin/python diagnostics/idf-baseline/flash-transaction.py restore
```

恢复校验文件故意不可覆盖；若已存在，先检查其哈希与恢复日志，
不得未经分析删除旧校验结果或重复解释为新的成功。

板上内核与 CMake build168 镜像逐字节一致。板上 AppFS 来自 build150，
与当前重新生成的 AppFS 不同，这是既有 kernel-only 更新
的预期状态；恢复以读回原始字节为准，不替换成当前生成的 AppFS。
原始备份中 AppFS 的 541696 字节已计算 SHA-256，确实匹配 build150
收据 `c595f1aa3adc6e211856f54a49ac38dddd6ae3bdd7a2dfcbbfc9e6569baf46e1`。

## 命令与结果记录

所有完整构建/串口/烧录日志保存在
`../../backups/2026-09-10-scan-stress/logs/`；没有凭据写入源码或日志。

- `bash diagnostics/idf-baseline/build-offline.sh > .../idf-baseline170-build.log 2>&1`：
  退出 0，首个真实错误无。存在 IDF 原有组件私有 include 依赖警告。
- `bash diagnostics/idf-baseline/backup-flash.sh > .../idf170-backup-launch.log 2>&1`：
  退出 0，双份读回/长度/比较/SHA 校验全部通过。
- `python3 diagnostics/idf-baseline/test-flash-safety.py > .../idf170-host-safety.log 2>&1`：
  7 项测试通过，包括损坏镜像/备份、越界写入、恢复错误字节和回读不一致拒绝。
- `python3 diagnostics/idf-baseline/flash-transaction.py prepare > .../idf170-flash-plan.log 2>&1`：
  通过，生成独立恢复切片及审查清单；此命令不访问开发板。
- `s31-reference/.venv-nuttx/bin/python -u diagnostics/idf-baseline/run-comparison.py`：
  170 对照使用实际 IDF HE20/HT20，各 3 轮。网关 ping 6 轮均 8/8；
  TCP 客户端只有 HE 第 3 轮通过，其余在 receive 超时。
  初版 TCP 发送后立即停止 Wi-Fi，不能据此将超时归因于 Wi-Fi 驱动。
- 171 修正诊断程序：发送 4096 字节后先 shutdown(SHUT_WR)，等待
  客户端读完数据与 EOF 并关闭，再停止 Wi-Fi。不是 openvela 驱动修改。
  `bash diagnostics/idf-baseline/build-offline.sh > .../idf-baseline171-build.log 2>&1`：
  退出 0，首个真实错误无。
  `S31_IDF_RUN=171 python3 diagnostics/idf-baseline/test-flash-safety.py > .../idf171-host-safety.log 2>&1`：7 项通过。

## 已完成实板结果

171、172、173 每组都完成 HE20/HT20 各 3 轮，总计 18 轮全部通过：
DHCP、每轮网关 ping 8/8、Windows 客户端 4096 字节确定性 TCP 回环及
内容 SHA-256 校验、正常关闭 Wi-Fi。每组结束均恢复原始扇区并完整
读回 5 MiB，与最初两份备份逐字节一致。

| 构建 | PHY 库 | Wi-Fi 库 | HE20 | HT20 | 恢复回读 |
|---|---|---|---|---|---|
| 171 | IDF 锁定版本 | IDF 锁定版本 | 3/3 | 3/3 | 一致 |
| 172 | HAL 锁定版本 | IDF 锁定版本 | 3/3 | 3/3 | 一致 |
| 173 | HAL 锁定版本 | HAL 锁定版本 | 3/3 | 3/3 | 一致 |

172/173 为诊断混合链接，不是供应商发布的完整新 SDK，也没有改变
openvela 的锁定依赖。仅通过当前工程 CMake imported target 选择现有
预编译归档，未改库文件、HAL/IDF 源码、子模块 HEAD 或 Git 元数据。
两套 esp_wifi.h/wifi_os_adapter.h 逐字节相同；各构建均链接成功，
三个 sdkconfig 逐字节相同，map 文件核对了实际 PHY/Wi-Fi 库来源。
不同构建仍包含生成时间/布局等差异，不能声称所有二进制字节只改一处。
IDF 的软件共存未开启，不能把 173 称为“所有 HAL 子系统均已验证”。

命令（工作区根目录；完整日志分别以 172/173 命名）：

```sh
S31_IDF_VARIANT=hal-phy bash diagnostics/idf-baseline/build-offline.sh
S31_IDF_VARIANT=hal-wifi bash diagnostics/idf-baseline/build-offline.sh
S31_IDF_RUN=173 python3 diagnostics/idf-baseline/test-flash-safety.py
S31_IDF_RUN=173 python3 diagnostics/idf-baseline/flash-transaction.py prepare
S31_IDF_RUN=173 s31-reference/.venv-nuttx/bin/python -u diagnostics/idf-baseline/run-comparison.py
```

172/173 构建均退出 0、首个真实错误无；各 7 项 Flash 主机安全测试通过。
恢复日志分别为 idf171-restore.log、idf172-restore.log、idf173-restore.log。
紧急恢复命令的 `S31_IDF_RUN` 必须对应最后安装的构建号，不混用日志。

结论：同板、同授权家庭网、同 HAL PHY/Wi-Fi 库能够在 IDF 上完成 HE
数据通信。因此继续定位 openvela 的配置/初始化/OS 接口差异；不再
把“芯片/AP 不支持 HE”或“PHY 库版本本身一定坏了”当作已知根因。
尚未证明 openvela HE 修好，未测试此基准的 UDP、DNS、IPv6 或吞吐。

## 回到 openvela 后的受控排查

共存条件候选涉及 esp_wifi_init.c、S31 sdkconfig.h 及对应主机测试，
仅在 Wi-Fi-only 时不初始化软件共存，BLE/外部共存路径保留。CMake174
和隔离 Make176 编译通过，但 network175 的 HE 三轮仍全部网关失败。
network177 使用相同固件、`S31_SKIP_EXPLICIT_SCAN=1` 跳过额外主动/
被动扫描，仍 DHCP 成功、网关失败和 TCP connect 超时。
build178 切回 b/g/n 的控制测试 network179 结果 `[1,0,0]`，第一轮关联后
sleep/ifdown 等待超时，后两轮含 DNS/TCP/UDP 全通过；各轮均硬复位清理。
没有把这些变化当作根因修复，也没有用它们更改默认生产策略。

候选补丁已保存到备份目录 `coexistence-candidate174-rejected.patch`，
源文件与隔离 Make 中的候选修改已逐项撤回；未使用 reset/checkout。
恢复基线后的 CMake180 编译和烧录成功，配置与归档 build168 相同；
smoke181 三次启动、三次无线启停、双核 NSH/挂载/定时器检查通过。
板上 AppFS 仍为 build150。主分支最新功能提交仍为 6e1020517c2，
66a0aa00a31 只记录本次已验证的对照结果。

完整构建命令（在工作区根目录，每次收据与日志均唯一）：

```sh
S31_BUILD_RECEIPT="$PWD/backups/2026-09-10-scan-stress/logs/build174.sha256" bash backups/2026-09-09-wifi/s31-build-current.sh olddefconfig > backups/2026-09-10-scan-stress/logs/production-cmake174-coexistence-gating.log 2>&1
bash backups/2026-09-10-scan-stress/build-production-make.sh > backups/2026-09-10-scan-stress/logs/production-make176-coexistence-gating.log 2>&1
S31_BUILD_RECEIPT="$PWD/backups/2026-09-10-scan-stress/logs/build178.sha256" bash backups/2026-09-09-wifi/s31-build-current.sh olddefconfig > backups/2026-09-10-scan-stress/logs/production-cmake178-bgn-coexistence-gating.log 2>&1
S31_BUILD_RECEIPT="$PWD/backups/2026-09-10-scan-stress/logs/build180.sha256" bash backups/2026-09-09-wifi/s31-build-current.sh > backups/2026-09-10-scan-stress/logs/production-cmake180-restored-baseline.log 2>&1
```

以上构建首个真实错误均无，保留原有 HAL shadow/GPIO 格式/PHY endif
警告。新增主机配置夹具第一次因未设置 Flash 频率而触发
`SPI timing flash clock is invalid`；补齐夹具的 80 MHz 选项后通过。
这不是目标固件编译失败。旧源码在新增 Wi-Fi-only 断言下按预期失败。
候选及测试都已归档，而非默默删除失败证据。

主机汇总测试使用显式 `S31_PROTOCOL_TEST_REVISION=6e1020517c29cb483c02faf731b09f945f9e5010`
测试已提交协议逻辑；它不覆盖尚未提交的 RX/TX 统计调用，不能宣称
当前所有诊断代码均经完整主机测试。实板 smoke 另有独立结果。

## 对照变量，不能忽略

IDF 与 NuttX/HAL 并非仅操作系统不同，原始锁定库也不同：

| 组件 | IDF 的实际子模块提交 | HAL 的实际子模块提交 |
|---|---|---|
| esp32-wifi-lib | 248fd630de1f6e4873085e3af9af280895b42d61 | 66c5b501748947811e5d6d33f4c70ab3e05944ff |
| esp-phy-lib | e294ff039e26b3486d6c9e5853d24d98ee3300b2 | 5695f4f38108658bc4a33e4712c1ebcb34911434 |

均从既有本地子模块 HEAD/gitlink 确认，没有切换子模块。HAL PHY
提交日志明确包含 S31 PHY version 100、track 更新。因此 IDF HE
ping 成功排除了“这块板/这个 AP 一概不能 HE 通信”，但还不能在
NuttX 适配与锁定 Wi-Fi/PHY 库版本之间归因。后续 172/173 的独立
诊断链接对照已补充上述证据；主工程仍未替换依赖。
