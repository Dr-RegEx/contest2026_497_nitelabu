# BLE控制器版本采集恢复准备

2026-09-19：1747内核和AppFS哈希与收据一致。修复采集脚本启动前台bttool后仍等待nsh提示符的阻断，改等bttool提示符；补全启动/目录准备超时输出归档。Python编译及--help通过，尚未上板采集HCI版本，不改变BLE5.4未确认状态。

待GPIO操作完成后再切换板卡镜像。执行入口：`flash1747-ble-controller-info.sh` 与 `ble1747-controller-info-capture.py`。使用串口独占，配套刷写内核和AppFS，禁止仅替换其一。

补充：采集完整 HCI 正则匹配后才结束等待，防止旧提示符和分段版本前缀导致提前返回；新增 `--output` 支持新目录保存后续运行。已用合成分段串口输入验证等待完整行，非真实HCI结果。`py_compile` 和 `--help`通过。
