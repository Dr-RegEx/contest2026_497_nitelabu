# Wi-Fi 四方向 300 秒速率实测与失败记录

## 实测范围与结论

仅为诊断例程，不计新增 xTS 通过。64KiB 候选 TCP RX 完成双端 225MiB、6.29Mbit/s、300 秒；UDP RX 9.86Mbit/s、76% 丢包且最终 ACK 缺失，随后 TCP/UDP TX 未完成，IOB 128 个仅剩 1 个可用。历史完整 TCP TX 3.38、UDP TX 对端 3.77Mbit/s 属于较早候选和环境，不能拼成同一镜像四向通过。

本次上传整理不重新操作板卡。历史固件、配置及日志保存在本目录；新编译镜像是复现候选，编译成功不替代新实板结果。

## 本目录内容

`src/` 是实际源码，`config/` 包含本配置全部继承文件，`evidence/` 保存测试原始记录（凭据脱敏见根目录 REDACTIONS.json），`case.json` 记录文件来源。没有符号链接。

## 从完整工程开始

Ubuntu 22.04/Linux x86_64，准备 GitHub、Gitee、PyPI 网络访问。在新的目录执行；补丁应用只对干净固定基线执行一次。

```bash
sudo apt-get update
sudo apt-get install -y git git-lfs repo python3 python3-venv python3-pip \
  cmake ninja-build build-essential bison flex gperf gettext texinfo \
  libncurses-dev libssl-dev genromfs xz-utils unzip patch curl
mkdir s31-review && cd s31-review
repo init -u https://github.com/Dr-RegEx/contest2026_497_nitelabu.git \
  -b dev-ai-contest-2026 -m contest2026_497_nitelabu.xml
GIT_LFS_SKIP_SMUDGE=1 repo sync -c -j8
./contest2026_497_nitelabu/workspace/apply-openvela-snapshot.sh "$PWD"
./contest2026_497_nitelabu/workspace/setup-dependencies.sh "$PWD"
```

S31 HAL 与工具链按锁定版本准备。不要混用旧 Make 生成的源树 `.config`/`include/nuttx/config.h`。

## 构建

在 openvela 工作区根目录执行：

```bash
./contest2026_497_nitelabu/workspace/build.sh "$PWD" demo-wifi-rate-window64 8
```

输出：`out/esp32s31-demo-wifi-rate-window64/`。修改例程源码时，将 `src/` 中相同相对路径的文件同步到工作区后重新构建；`src/esp-hal-3rdparty/` 对应 `.s31-deps/esp-hal-3rdparty/`。配置 `config/` 对应工作区 `nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/configs/`。不要仅修改副本却构建旧代码。

## 刷写与终端

确认实际 S31 串口及硬件，进入下载模式。首次刷写前备份 Flash，保存到不覆盖旧备份的位置；不执行全片擦除。以下在工作区根目录操作：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  read-flash 0 0x1000000 original-flash.bin
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash 0x2000 out/esp32s31-demo-wifi-rate-window64/nuttx.bin \
  0x200000 out/esp32s31-demo-wifi-rate-window64/appfs.img
.s31-deps/venv/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200
```

本配置为 Kernel，内核与 AppFS 必须来自同一次构建；不要混用历史镜像。 串口一次只由一个程序占用，运行主机脚本前退出终端。

## 运行步骤

对端使用已核验的 Windows select1656 修正版 iperf2（SHA256 `73096a7826379d0b092c748e367489704953e240d34f2658b9b63acef3a3a4cd`），或明确记录所用主机工具版本与差异。旧 Windows 2.1.8 有超时兼容/计数问题。配置个人 AP 凭据，不使用历史日志中的测试密码；在板端 `wapi show wlan0` 及主机 WLAN 状态中保存实际 BSSID、信道和 RSSI。同 SSID 不等于同一个 AP。

以下 IP 为示例，改成实际板端/电脑地址。每项单独运行，保存双方完整日志，结束服务器后再做下一项。

|方向|开发板 NSH|电脑终端|
|---|---|---|
|TCP RX|`iperf2 -s -p 5001 -i 1 &`|`iperf2 -c 192.168.1.60 -p 5001 -t 300 -i 1 -l 16384`|
|TCP TX|`iperf2 -c 192.168.1.29 -p 5001 -t 300 -i 1 -l 16384`|先启动 `iperf2 -s -p 5001 -i 1`|
|UDP RX|`iperf2 -s -u -p 5001 -i 1 &`|`iperf2 -c 192.168.1.60 -u -p 5001 -b 40M -t 300 -i 1`|
|UDP TX|`iperf2 -c 192.168.1.29 -u -p 5001 -b 40M -t 300 -i 1`|先启动 `iperf2 -s -u -p 5001 -i 1`|

必须核对双方 300 秒持续时间、字节数和正常结束，UDP 用接收端吞吐、丢包和最终报告判读。主机发送速率不能冒充板端接收速率。若资源耗尽、超时或最终 ACK 缺失，保留失败并停止当轮，不把重启后的结果拼作连续通过。30 秒短测只作诊断。

## 历史证据

- `evidence/5.1.22-rx-result.json`
- `evidence/flash-receipt.json`
- `evidence/5.1.24-rx-result.json`
- `evidence/5.1.25-tx-result.json`
- `evidence/5.1.23-tx-result.json`
- `evidence/review.md`
- `evidence/auto-ap-arp-window/flash-receipt.json`
- `evidence/auto-ap-arp-window/review.md`
- `evidence/arp200-probe/flash-receipt.json`
- `evidence/arp200-probe/review.md`
- `evidence/window64/flash-receipt.json`
- `evidence/remaining-tx-udp/flash-receipt.json`
- `evidence/remaining-tx-udp/5.1.24-rx-result.json`
- `evidence/remaining-tx-udp/5.1.23-tx-result.json`
- `evidence/iperf-delay-path-audit/review.md`
- `evidence/window64-rates/5.1.22-rx-result.json`
- `evidence/window64-rates/flash-receipt.json`
- `evidence/window64-rates/5.1.24-rx-result.json`
- `evidence/window64-rates/5.1.25-tx-result.json`
- `evidence/window64-rates/5.1.23-tx-result.json`
- `evidence/window64-rates/review.md`
- `evidence/linux-package-peer/flash-receipt.json`
- `evidence/linux-peer/flash-receipt.json`
- `evidence/same-ap/flash-receipt.json`
- `evidence/tcp-window-compare/flash-receipt.json`
- `evidence/tcp-window-compare/review.md`
- `evidence/ap6-check/flash-receipt.json`
- `evidence/ap-selection/flash-receipt.json`
- `evidence/rx-observe/flash-receipt.json`
- `evidence/same-ap-rates/5.1.22-rx-result.json`
- `evidence/same-ap-rates/flash-receipt.json`
- `evidence/same-ap-rates/5.1.24-rx-result.json`
- `evidence/same-ap-rates/5.1.25-tx-result.json`
- `evidence/same-ap-rates/5.1.23-tx-result.json`
- `evidence/ap-selection-preparation/review.md`
- `evidence/window64-candidate/review.md`
- `evidence/window64-candidate/build-result.json`
- `evidence/auto-ap-short/5.1.22-rx-result.json`
- `evidence/auto-ap-short/flash-receipt.json`
- `evidence/auto-ap-short/5.1.24-rx-result.json`
- `evidence/auto-ap-short/5.1.25-tx-result.json`
- `evidence/auto-ap-short/5.1.23-tx-result.json`
- `evidence/auto-ap-short/review.md`
- `evidence/retry-remaining/5.1.22-rx-result.json`
- `evidence/retry-remaining/flash-receipt.json`
- `evidence/arp-budget-candidate/receipt.json`
- `evidence/arp-budget-candidate/review.md`
- `evidence/arp-budget-candidate/build-result.json`
- `evidence/arp200/flash-receipt.json`
- `evidence/select-host/5.1.22-rx-result.json`
- `evidence/select-host/flash-receipt.json`
- `evidence/select-host/5.1.24-rx-result.json`
- `evidence/select-host/5.1.23-tx-result.json`
- `evidence/linux-package/receipt.json`

各文件的来源与当前副本哈希见本目录 `case.json`。历史记录中较早的“待确认”或失败状态保留，当前结论以本页和最终复核记录为准。原始绝对路径和证据间链接只作历史定位，不是运行依赖。
