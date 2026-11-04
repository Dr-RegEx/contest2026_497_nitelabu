# 独立 Factory 演示固件

本目录保留原生 openvela Factory 演示的已验证镜像，包含摄像头主面板和移植的咖啡机界面。默认上台演示使用旁边的 `korvo-digit-stable`；两份镜像分别烧录，未实现按钮跳转。

在本目录运行 `sha256sum -c SHA256SUMS` 核对本精简包；随后按仓库 `workspace/FLASH-MANUAL.md` 的 Korvo 16 MiB / `0x2000` 写入步骤操作。该页 `verify-artifacts.py` 用于包含 ELF 的完整构建包，不适用于这里的 bin/config 精简包。保留原 Flash 备份，首次切换配置按文档处理 HMI 数据区。

启动后可在 115200 串口依次运行 `set S31FACTORY_COMPONENT org.openvela.s31.factorydemo/FactoryActivity` 和 `am start -t $S31FACTORY_COMPONENT -a s31.factory.snapshot` 打开 Factory；该固件保持原来的服务启动方式，不包含新的识别入口。摄像头主面板、咖啡界面、键盘与服务已有验收；音频队列溢出和听感验收仍有边界。参见 `docs/validation/README.md`。

源码及 UI 资源许可证随平台源码快照保留，实际开发请使用 `workspace/REPRODUCE.md` 的 canonical 配置与源码。
