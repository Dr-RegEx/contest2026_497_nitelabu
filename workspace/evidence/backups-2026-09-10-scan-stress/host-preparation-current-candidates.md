# 第三、四部分当前候选与实测缺口

2026-09-19。以《核验集》为验收主清单；本页仅定位最新候选，不增加项目、不修改通过口径。当前不刷板、不安排人工或外部设备测试。没有后台板测或构建在运行。

## 第三部分：17项尚未闭合

|清单项目|数量|下一步缺少的实际证据|
|---|---:|---|
|指定广告间隔|1|稳定扫描器记录目标间隔，满足原文容差|
|三种地址组合多路广播|3|对端同时观察广播名称与地址|
|扫描PHY、mode、组合|3|实际对端发现；原文2M疑点保留|
|BLE/Wi-Fi扫描、广播共存|2|原始配网与100次循环的板端/对端结果|
|发现/被发现时长|2|对端与计时记录，各10次|
|发现、被发现、发现5设备成功率|3|原始20次，最后一项需5个广播对端|
|配对成功率|1|实际配对20次|
|扫描效率及广播扫描共存效率|2|对端及规定距离实测|

BLE开关已通过，以上17项不因主机测试或参数回调而计通过。多路广播、共存的既有候选和配对镜像仍分别以 `ble1744-multi-adv.json`、`ble1745-coex-candidate.md` 为入口；它们不是本页下述新增能力镜像，不混用kernel/AppFS配对。

## 第四部分：最新候选

输出目录均相对于 `openvela-dev/out/`。都是隔离配置；不是一个包含全部已验证能力的通用镜像。

|用户项|当前候选目录|证据及仍需实测|
|---|---|---|
|第5项 Camera|esp32s31-camera-v4l2-lifecycle-final|[生命周期修复](camera-v4l2-lifecycle-fix.md)；需OV2640、核定接线、SCCB和真实帧输出|
|第8项 BLE版本|沿用1747实际HCI读取证据|[控制器实测](ble-controller-live-result.md)；版本值已取得，不能替代第三部分对端用例|
|第9项 LE Audio|esp32s31-ble-audio-lc3-iso-ownership|[四角色/LC3及发送所有权](ble-iso-ownership-result.md)；需实际controller初始化、CIS/BIG、音频对端|
|第10项 Mesh|esp32s31-xts-flat-ble-mesh-h4-final-20260919|[最新集成](mesh-h4-final-integration.md)；需重新验证初始化、provision和cfg/health/SAR模型收发；当前不持久化|
|第11项 SD|esp32s31-xts-flat-sdmmc-width-20260919|[宽度/探卡修复](sd-bus-width-fix.md)；需外接卡、容量识别、真实1/4-bit读写及FAT|
|第12项 USB Host|esp32s31-usb-host-ping|[NYET/PING修复](usb-host-ping-host-result.md)；需Type-A接真实设备验证枚举、MSC读写；当前是同步PIO，周期/异步/Hub split仍未实现|
|第12项 USB Device|esp32s31-usb-device-utmi|[UTMI修复](usb-device-utmi-audit.md)；当前FS Device候选，需先核定设备角色的连接电气条件，再做真实主机枚举/ADB；不能把Type-A Host或USB串口作为ADB通过证据|

第1/2/3/4/6/7项既有实测范围保留，不重复测试。当前没有新增实物PASS；xTS仍为用户口径78/105。SD新候选包含维护HAL补丁，原始F.0锁的零补丁摘要不再匹配，完整补丁和依赖核验差异见SD报告；不得删除补丁后仍使用该镜像结果。

## 当前执行边界

已发现的主机可复现阻断已分别修复并归档。当前未发现另一项证据明确、尚未处理且能直接闭合这些测试的主机任务。外设缺失及当前不安排刷板的限制，使上述实测无法继续；代码审阅/构建成功不能保证首次板测通过。已记录的软件能力限制保留，不将其改写为已完成，也不为凑通过数扩展新的测试目标。
