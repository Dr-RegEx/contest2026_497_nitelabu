# 旧工程未被新测试覆盖：核验结论

入口是用户指定的xts-current-status.md，核对历史冻结清单、原哈希清单和源码快照；不是只看本轮品类状态。

## 已证明的保留范围

- 冻结989清单和当前主清单均33行、涵盖35标题，旧用例标签无缺失；仅Flash块、Watchdog、Crypto三行被后续真实上板结果更新，未删除旧用例。
- 11份既有历史SHA清单中的133项全部匹配，无缺失或摘要变化。覆盖选定旧源码/恢复镜像/验收归档；详见audit1317-existing-artifact-integrity.json。
- 用970冻结Git HEAD、working patch在临时目录还原旧源码后比较：NuttX/apps当前HEAD与旧HEAD完全相同；旧未跟踪源码NuttX76份、apps45份全部仍存在，未丢失。NuttX其中9份有后续修改，apps45份均相同。
- 比较覆盖的旧修改中，启动、syscall入口、SMP supervisor、IRQ、RTC、链接脚本、原复位实现、KASAN、堆配置和Wi-Fi适配源码与970快照相同；不代表所有间接依赖均无变化。
- 当前重连异常涉及的esp_wlan.c及esp32c6/esp_wifi_adapter.c与970相同，未发现是近期音频/存储测试覆写这两份网络源码造成。

## 不能据此冒称的事项

- 工作树整体并非旧版本原封不动。后续有Flash内部栈调度/临时诊断、VFS路径、LittleFS sync、scanf EOF、音频/BLE、部分SHA/Watchdog及配置修改。
- 原镜像和原验收记录保留，不等于最新单一镜像已再次通过所有旧用例。当前板上1078是SMP/MMU网络镜像，历史FLAT测试镜像用于其各自验收范围，不把分镜像成绩说成单镜像全集通过。
- 不全量重测。网络优先继续；最终镜像集成时仅对实际依赖变更涉及的旧用例定向检查，例如scanf EOF改动涉及旧scanftest、共用Flash/VFS改动涉及相应存储路径。SHA1010/Watchdog1016等已有后续有效证据直接沿用。

相关机器可读证据：audit1317-checklist-retention.json、audit1317-source-retention.json，以及audit1315-existing-common-evidence.json（27份旧日志成功/子项/终态记录复核）。原证据均未重写。
