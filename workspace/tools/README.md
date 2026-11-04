# 平台工具来源与安装位置

本目录保留比赛仓原有工具，新增 `build-support/` 与 `tests/korvo-adc-keys/build.py` 的当前平台副本。[provenance.json](provenance.json)记录每个新增副本的来源相对路径、大小及 SHA256；复制时未改写内容或许可标记。

`restore-offline.py` 以显式参数调用这里的 `restore_product.py`、`offline_sdk.py`、`verify_sdk.py`。构建入口调用 **恢复后 platform-root 自身**的 `tools/tests/korvo-adc-keys/build.py`，该 helper 按自身位置确定平台根，因此不要从比赛仓 `workspace/tools/tests/...` 直接启动构建。

离线产品已经含有正确位置的工具，无须覆盖。仅在新建完整快照回放树时，把这里 `build-support/` 和 `tests/korvo-adc-keys/build.py` 安装到新平台同名 `tools/` 下，同时提供同版运行库包和全部源码/SDK依赖。本目录不把 `.deb`、compiler、Python 或 prebuilts 当作源脚本捆绑发布；`host-packages.json` 列出的两份运行库来自产品导出，解包后保留其版权文件。

`offline-bin/` 的 curl/wget/Git 拦截器与原环境工具一起保留。它们防止构建意外下载或切换依赖，不构成网络沙箱。完整流程及资格限制见 [REPRODUCE.md](../REPRODUCE.md)。
