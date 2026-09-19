# Mesh 入口首次板测

2026-09-19，冻结1766固件哈希校验后刷入，NSH启动正常。执行mesh后返回 `Bluetooth init failed (err -19)`，未进入Mesh初始化，不通过。原始日志[mesh-board-init-uart.log](mesh-board-init-uart.log)，刷写[mesh-board-init-flash.log](mesh-board-init-flash.log)。

用户随后要求优先主机工作，已停止串口采集，不再刷板；当前板上仍为此Mesh隔离镜像。离线修复入口初始化中。
