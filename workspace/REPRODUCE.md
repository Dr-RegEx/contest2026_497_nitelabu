# 本机恢复与开发

入口接受显式路径，支持路径中的空格。下列变量均由工程师设为本机路径，不依赖原开发机目录。使用 Linux x86_64、Ubuntu 22.04 / CPython 3.10；源码接收目录必须区分大小写。其他宿主尚未资格验证。

## 1. 已验证来源：离线产品与 SDK

准备独立交付的 Factory 产品目录和配套 SDK 包。产品含 `source/{source.tar.gz,manifest.jsonl,receipt.json}`、`dependencies.json` 与 HAL/IDF bundles；SDK 含 `sdk.json`、`dist/`、Python 归档及约束文件。此 Git 仓的入口脚本本身不是完整 SDK。

```sh
CONTEST_REPO=/path/to/contest-repository
PRODUCT=/path/to/korvo-factory-product
SDK_PACKAGE=/path/to/korvo-sdk
RECEIVER=/path/to/new-source-receiver
PLATFORM_ROOT="$RECEIVER/workspace"
SDK=/path/to/new-sdk-receiver
SDK_CHECK=/path/to/new-sdk-readback

python3 "$CONTEST_REPO/workspace/restore-offline.py" source \
  --package "$PRODUCT" --output "$RECEIVER" --dry-run
python3 "$CONTEST_REPO/workspace/restore-offline.py" source \
  --package "$PRODUCT" --output "$RECEIVER"
python3 "$CONTEST_REPO/workspace/restore-offline.py" sdk \
  --package "$SDK_PACKAGE" --platform-root "$PLATFORM_ROOT" --output "$SDK"
python3 "$CONTEST_REPO/workspace/restore-offline.py" verify-sdk \
  --package "$SDK_PACKAGE" --sdk "$SDK" --output "$SDK_CHECK"
```

接收器创建 `RECEIVER/workspace`，该目录就是 **platform-root**：

```text
platform-root/
  openvela-dev/{nuttx,apps,external,frameworks,packages,tests,vendor,build,prebuilts}
  tools/build-support/
  tools/tests/korvo-adc-keys/build.py
  s31-reference/deps/esp-hal-3rdparty/
  s31-reference/tmp/esp-idf-clean/
  s31-reference/.venv-nuttx/
```

包装器调用仓内原样保存的 `restore_product.py`、`offline_sdk.py`、`verify_sdk.py`。实际恢复校验归档、成员、链接与依赖版本，SDK 安装禁用下载函数；缺包或坏包应补齐交付材料后换新目录重试，不在线兜底。`--dry-run` 仅检查元数据和路径，不哈希多 GiB 归档，不代表恢复或 SDK 回读成功。失败的接收目录保留诊断，不覆盖重用。

系统已有依赖仍需齐全：bash、Git、CMake、Ninja、make、Perl、xxd、dpkg-deb、ldd 和常用工具；默认源码复制模式另需 rsync。aidl、jidl、genromfs、Kconfig Python 和两份运行库包由产品源码闭包携带。构建时的 `environment.py` 会验证锁、解出运行库、执行小型生成器探测。空间须覆盖约 5.4 GB 源码、SDK、归档和构建输出；历史建议至少 12 GiB，默认复制源码模式还要另留一份源码及输出空间。

依赖 HEAD：HAL `290edc31b50decca660c1a11ce3506fd9b2e1e27`；IDF `14f663f003eb8fd9a688c301a412a9540d29dacf`。仅 HEAD 相同不代表工作区修改相同，产品 manifest 才记录实际交付字节。保留所有依赖和资源的 LICENSE、NOTICE、UPSTREAM 与源码说明。

## 2. 构建新镜像

```sh
BUILD_OUTPUT=/path/to/new-factory-build
bash "$CONTEST_REPO/workspace/build.sh" --list
bash "$CONTEST_REPO/workspace/build.sh" \
  --platform-root "$PLATFORM_ROOT" --sdk "$SDK" \
  --profile korvo-openvela-factory --output "$BUILD_OUTPUT" --jobs 4 --dry-run
# 检查上一步列出的路径、配置 include 闭包和实际命令后，启动构建：
bash "$CONTEST_REPO/workspace/build.sh" \
  --platform-root "$PLATFORM_ROOT" --sdk "$SDK" \
  --profile korvo-openvela-factory --output "$BUILD_OUTPUT" --jobs 4
```

入口实际执行 `PLATFORM_ROOT/tools/tests/korvo-adc-keys/build.py`。该 helper 使用同级 `tools/build-support/environment.py`，由 CMake/Ninja 构建 `openvela-dev/nuttx`，传入 apps 路径、当前 Python、离线 FetchContent 设置和并行度。它不是旧的扁平化 `.s31-deps` 构建脚本。

默认 helper 复制源码到输出内；`--share-source` 显式选择其 `--workspace-source` / `NUTTX_PRESERVE_SOURCE=ON` 模式，省去源码复制。已有 factory 接收端采用后者完成独立构建。两者仍共享锁定依赖和 prebuilts；共享模式也不是不可变源码快照。不要并发修改本次构建所读源码。

输出不得已存在，也不得与源码、SDK 或 contest 仓重叠。结果在 `BUILD_OUTPUT/build/`；保留 `receipt.json`、`entrypoint-receipt.json`、`host-environment/receipt.json`、配置与构建日志。`--dry-run` 不执行环境准备、配置或编译，不能替代正式构建。离线拦截器是构建约束，不是操作系统网络隔离。

`function-ethernet` 的 canonical ELF、bin 和配置已与基础通信验收版本逐字节核对一致；见 [Ethernet 记录](../docs/validation/ethernet.json)。构建仍需对应的完整 NuttX/BSP/HAL 源码闭包，旧 factory 产品包不含该配置，不能只复制 defconfig。`nsh` 和 `demo-rmt-netapps-competition` 的旧构建身份见 [历史记录](../docs/validation/historical.json)；新入口未重新全量构建这些配置。其他配置按 canonical BSP 的完整继承闭包使用底层构建工具。

## 3. 薄快照回放的路径映射与未完成项

已有 `apply-openvela-snapshot.sh` / `apply-snapshot.py` 保留原样。它们把 `snapshot.json` 的 `path` 当作给定根目录的相对路径，例如 `nuttx`、`apps`、`frameworks/runtimes/services/am`，先核对 payload、项目 HEAD、工作区和补丁可应用性，再写入。

要使新接收 checkout 符合平台布局，应把 **repo checkout 根直接创建在 `platform-root/openvela-dev`**，然后将这个目录传给旧 apply 工具：

```sh
# 仅对已具备完整、刷新且核对过的载荷和干净固定基线运行一次。
# 不是获取缺失仓库的命令；本页不执行 repo sync 或依赖下载。
bash "$CONTEST_REPO/workspace/apply-openvela-snapshot.sh" "$PLATFORM_ROOT/openvela-dev"
```

已有扁平 checkout 的 `CHECKOUT/nuttx` 对应 `PLATFORM_ROOT/openvela-dev/nuttx`，不要把 `CHECKOUT` 直接传给新 build 入口。选择独立平台父目录，再将新 checkout 安排为其 `openvela-dev`；不要搬动正在开发的树或用软链接掩盖缺失依赖。团队仓可在 checkout 内或单独保存，`CONTEST_REPO` 始终显式指定。

回放还需要把本仓 `workspace/tools/` 的工具按相对路径安装到新 `platform-root/tools/`，并提供产品同版的两份运行库包、全部 `prebuilts` 生成器、Python 环境、HAL/IDF 实际修改和 Git 元数据。工具副本不含这些二进制依赖。旧 `setup-dependencies.sh` 的 `.s31-deps` 不提供新合同所需的 IDF/host-generator 布局，不能作为此路径的替代。

[snapshot.json](snapshot.json)、[manifest-locked.xml](manifest-locked.xml)和校验清单记录实际交付版本与文件；[source-closure.json](source-closure.json)保留对应审查快照。刷新时整体配套更新 manifest、patches、新文件、snapshot、checksum 与依赖合同。当前公共薄快照已同步源码、BSP 与依赖补丁并验证补丁重建；完整接收端构建仍待复验，入口脚本不自动补齐缺失的 IDF 或 SDK。旧 apply 脚本末尾的 setup/build 提示应结合本页平台布局使用。

## 4. 镜像与证据边界

Factory 原实板镜像 SHA256 为 `9a5b80836ee9613590bec695b19aacbf7ec234f82f01517d209dee3337859a1b`；旧接收端重建为 `7e4842c9de12b6e564bed234bb5e8906bcb351a8433fd15d336376f91f28369c`，该重建镜像尚未板验。默认演示保留已验收的原版双核数字识别独立固件；Factory 配置单独构建、烧录。两者尚未实现按钮跳转，未通过识别验收的新集成页不纳入交付。

Ethernet 的 canonical ELF、bin、配置与已验收版本逐字节一致，仅继承该版本的基础链路、ping、有限 TCP/UDP 接收及接口恢复范围，不宣称千兆吞吐率。完整记录见 [验证索引](../docs/validation/README.md)和 [profiles.json](profiles.json)。

部署先读 [FLASH-MANUAL.md](FLASH-MANUAL.md)，按对应镜像布局操作。验收固件可留在板上使用；恢复原固件是需要时的回退选项。Kernel/AppFS 必须同次构建配对，不套用 Factory 单镜像布局。
