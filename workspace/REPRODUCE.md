# 队员复现与审核

唯一入口为[仓库 README](../README.md)。不要使用历史 `netlab` 地址。

## 固定版本如何恢复完整工程

`manifest-locked.xml` 固定公共源码版本；`snapshot.json` 明确记录每个修改项目的真实路径。
`patches/` 包含相对公共基线的差异（包括原本只提交在开发机上的提交），
`openvela-untracked/` 补齐新增文件。`source-sha256.json` 校验复现载荷。

`board/contest_board/` 是板级源码的可读副本，和补丁应用后的
`nuttx/boards/risc-v/esp32s31/esp32s31-core-function-board/` 对应。
manifest 的 linkfile 把板级目录映射到 vendor 下供发现和审核；真正编译路径
在 NuttX 中，由补丁安装，不要求手工复制任何源码。

HAL 使用 `78/esp-hal-3rdparty@290edc31b50decca660c1a11ce3506fd9b2e1e27`，
包括其固定子模块和 `dependencies/hal*.patch`。工具链为
`riscv32-esp-elf esp-15.2.0_20251204`，esptool 为 `5.4.0`。
不需要原作者的 `s31-reference/`、ESP-IDF export 脚本、虚拟环境或离线拦截器。
C6 辅助板为独立 ESP-IDF 工程，按各自 README 构建，不参与 S31 主工程编译。

## Kernel 镜像配对

`demo-rmt-netapps-competition` 的 `nuttx.bin` 与 `appfs.img` 在同一构建目录生成。
保留这两份文件及各自 SHA256，不允许和旧固件混用。
确认板卡型号和 16MiB Flash，先按主 README 完整备份，再按此配置布局写入：

```bash
.s31-deps/venv/bin/esptool --chip esp32s31 --port /dev/ttyACM0 \
  write-flash \
  0x2000 out/esp32s31-demo-rmt-netapps-competition/nuttx.bin \
  0x200000 out/esp32s31-demo-rmt-netapps-competition/appfs.img
```

不要执行整片擦除；`0x500000` 起为可写数据区域。其他配置应核对各自分区定义，
本命令不适用于任意历史配置。串口 115200，启动后核对 NSH 与应用装载信息。
网络测试需要队员自己的 AP 和凭据，不在仓库预置。历史 xTS 的具体命令、
配对镜像哈希、实物依赖见 `docs/acceptance/` 与 `workspace/evidence/`。

## 验证范围

本次执行源码完整性比对、补丁应用、依赖安装，以及 NSH / Camera / 竞赛网络
Kernel 三种配置的主机完整构建。源代码仓通过本机 Git 对象缓存独立检出，
没有复用原开发树的生成头文件或构建产物；涉及修正的四个公共基线还实际从
Gitee 网络 fetch 验证。此次未执行新的刷板或 xTS 实板测试。

历史 `build-configs/` 与 evidence 中的脚本是审计资料，未逐个验证可独立执行。
编译产物、下载工具链、Python 环境不入 Git；均可由 README 步骤生成。
