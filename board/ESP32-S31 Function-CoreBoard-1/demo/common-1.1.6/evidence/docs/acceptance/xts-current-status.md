**1682用户验收更新：按用户明确指示，通用1.3.16 RNG计为通过（用户确认）；原26项统计未定义说明和日志保留，不表示社区已审核。冲刺77/88=87.5%，通用33/35、品类44/53；另2项文件系统速率自测完成、待验收。通用仅剩GPIO、BMI160实物测试。当前清单sprint1682-current-cases.json。1681新候选1676的HTTP网页/status.json回归通过，demo进程PID27已启动。**

**1791 I2C/SPI 更新：**BMI160 实际函数主机 ASAN/UBSAN 检查通过；修复 SDMMC Kconfig 依赖回环后，`xts-flat-bmi160` 的 `olddefconfig` 已通过，但完整候选构建仍被既有 S31 CMake adapter 生成的 `-march=if/-mabi=f` 阻断。目标板和外接 BMI160 未执行，1.3.7 不计通过。

**1679：1676候选开启TCP SACK/16KiB乱序缓存后，原始TCP接收300秒双端169MiB/4.70Mbit/s（板300.60秒、PC300.62秒），IOB96/96。旧1659为1.64Mbit/s，本轮约2.87倍；保留候选，性能验收仍待确认，完整PASS76/88不变。证据：checkpoint1679-original-tcp-rx-sack。当前该测试已结束，demo尚未恢复。**

**1674当前1631镜像网页演示实测通过：s31demo状态正常，Windows成功获取首页/status.json，板型/IP一致。服务PID79保留，访问http://192.168.1.60:8080/；PCM节点缺失，音频仍须独立镜像，不宣称全功能整合。UDP1673已完整收尾，当前无xTS长测，实物夹具情况待用户回复。证据checkpoint1674-current-demo。**

**1673原始UDP Tx300秒完整收尾：双端125MiB/3.49Mbit/s，板300.01秒/PC300.03秒，丢包9/89057（0.01%），最终Server Report匹配，IOB96/96，电脑接收器已停止。1668早退已定位ARP失败，1672恢复后本轮完成，未宣称根因永久修复。两项FS速率按用户决定“自测完成、待验收”，不重跑；正在当前1631镜像验证1674网页演示。完整PASS76/88不变，性能记录待验收。**

**1668最新：Windows接收器规则已变为Allow（只读核查，本助手未修改），但恢复UDP Tx本轮提前退出：0秒/0字节，未进入300秒负载，仍INCOMPLETE；接收器已停止、/tmp/x保留。需定位提前退出，不能继续归因于旧防火墙阻止。冲刺76/88不变，交接network1668-handoff.json。**

**1666/1667：UDP接收器防火墙窄范围处理/恢复方案已完成只读预览，未执行修改；新版作品介绍MD/DOCX位于submission1667，补齐蓝牙/SIMD/冷启动及近期网络成果，结构和内容检查通过，尚未目视排版/上传。当前无活动测试，剩余实物、防火墙决定及性能/统计验收条件见handoff1667-external-conditions.json。冲刺76/88不变，不宣称整体适配完成。**

**1663 UDP Tx已收尾：板117MiB/300.01秒/3.27Mbit/s、IOB96/96；Windows1641接收器被明确入站Block规则阻止，无接收流量，整项INCOMPLETE（原板端解析标签保留，复核覆盖）。电脑接收器已停止，无活动网络测试。未改防火墙；剩余12项见remaining1665-no-repeat.md，最新明细sprint1665-current-cases.json。冲刺76/88不变。**

**1662原始UDP Rx300秒完整收尾：绑定WLAN后单会话，最终ACK/Server Report正常；板420MiB/300.03秒/11.7Mbit/s、24%丢包，PC554MiB/300.02秒，IOB96/96。性能门槛待验收，冲刺76/88不变。证据checkpoint1662-original-udp-rx；下一项1663 UDP Tx。**

**1661显式绑定WLAN的UDP20秒诊断完整收尾：单一192.168.1.29会话，最终ACK/Server Report正常，11.7Mbit/s、25%丢包。1662已按同接口启动原始300秒/-u/-b40M；无全局路由或代理改动，尚未判定原始结果。冲刺76/88不变，交接network1662-handoff.json。**

**1659原始TCP Rx300秒双端完整结束：PC300.69秒、板300.66秒，均58.6MiB/1.64Mbit/s，IOB96/96。1656 Windows发送超时兼容修正经定量1658及本轮验证；性能验收门槛仍未给出，冲刺76/88不变。UDP1651双会话含Meta虚拟地址198.18.0.1，已启动1661显式绑定WLAN短测；未改全局网络设置。**

**1658 TCP定量上板核对通过：Windows1656兼容修正版与1631板端均为1048576字节，正常收尾。1659已启动原始TCP Rx300秒，待双端时长/字节复核；这是修复后必要回归，尚无新增原始PASS。用户休息，所有实体操作后移。交接network1659-handoff.json。**

**1651 UDP Rx原300秒已结束：主会话400MiB/299.90秒/11.2Mbit/s、24%丢包，出现双会话且PC未收到最终ACK，保留INCOMPLETE；监听已停止、IOB96/96。1657电脑本地已复现原版TCP发送统计异常（报262144字节、实收1703936），1656修正版同条件实收262144；正在1658上板核对固定字节。冲刺76/88不变。**

**1654/1655：同1631镜像恢复后离线ps/free及SIMD检查正常；联网重试成功，已临时切至PC同AP。1651原始UDP Rx 300秒已启动，板端监听PID14、PC分段日志持续输出；尚未判定结果。用户休息，不请求任何实体操作；防休眠进程仍在。冲刺76/88不变，交接network1655-handoff.json。**

**1644～1650：临时切至PC同AP（60:ce:41:ab:02:d0/信道11）后UDP20秒恢复11.4Mbit/s、21%丢包且末ACK正常；TCP20秒出现PC3.50MiB/板7.13MiB与约40秒不一致，待核查Windows发送超时部分发送统计。1647后台监听启动无返回，未启动PC流量；现场归档后同1631镜像复位启动正常，但1650在wapi reconnect再次超过观察窗口未返回，当前无长测在跑、无串口所有者，需继续定位。冲刺76/88不变，交接network1650-handoff.json。**

**1638 TCP Rx连接超时，未进入300秒传输；板端监听任务已停止、IOB96/96，失败证据checkpoint1638-tcp-rx-connect-failure保留。下一步优先核查信道6弱AP与邻居通信，不直接重跑长测。当前无串口占用，交接network1638-handoff.json。**

**1637 UDP Rx未完整收尾：PC300秒/40.2Mbit/s发送，未收到末包ACK；板端接收极低且无最终汇总，复核不通过（原解析器误匹配区间的问题已纠正，原记录保留）。1638启动前发现开发板连接同SSID的信道6 AP60:ce:41:46:0e:ec，信号-73dBm，与用户PC信道11不同，先保留当前TCP Rx原始测试再核查选AP，不能直接认定驱动退化。Windows UDP兼容工具1641已通过1642空闲间隔/最终报告回归，待原始上板重测。冲刺76/88不变。**

**1636 UDP Tx原始命令结束但验收证据不完整：板端96.9MiB/300.01秒/2.71Mbit/s，正常退出、IOB96/96；Windows原版接收器177.28秒recvmsg超时拆分会话，板端收到旧段Server Report，不能记完整PASS。原解析结果保留，复核见checkpoint1636-udp-host-timeout。准备修复主机接收超时处理，同时继续板端UDP接收原始用例。冲刺76/88不变。**

**1634原始TCP Tx 300秒双端完整完成：板端300.06秒、PC300.05秒，同为56.6MiB/1.58Mbit/s；任务退出、IOB96/96回收。1631补tick与1623轮询修正已在原始负载验证计时一致，历史1478为296Kbit/s（不同时间无线环境，不作严格倍数基准）。原文无数值速率门槛，继续保留性能验收待定，冲刺76/88；证据checkpoint1634-original-tcp-clock。**

**1631/1633修复并短回归通过：S31按硬件时间补算延迟tick，每IRQ最多32且保留剩余差额。负载对照Windows20.04秒/板端20.59秒（含命令往返），修正前板端仅16.12秒；空闲对照正常。TCP30秒板端30.14/PC30.10秒、双端5.27MiB正常退出；SIMD两工作线程亲和性、INT8及完整寄存器切换回归PASS。当前1631镜像，原始300秒1634运行中，冲刺76/88不变。证据checkpoint1633-tick-catchup。**

**1626原始TCP Tx 300秒命令执行完成：两端同为52.9MiB，板端300.25秒/1.48Mbit/s，Windows接收端360.00秒/1.23Mbit/s；任务正常退出、IOB96/96回收，临时文件已归档清理。1625串口尾段缺失记录保留，不计PASS。当前1623镜像；计时差异需先核对板端/Linux/Windows时基，再验证漏tick假设，速率标准仍待确认。冲刺76/88不变，证据checkpoint1626-original-tcp-file。用户休息，无人工操作。**

**1624 TCP修正短测改善：S31发送回调保留协议轮询位，并限制每轮8次且遵守发送队列背压；同类20秒自定义接收器下板端4.70MiB/20.14秒=1.96Mbit/s，1622基线各秒约0.23～0.39Mbit/s。正常退出，无人工操作。原始300秒1625进行中；短测不计PASS，冲刺76/88。证据checkpoint1624-tcp-poll-improved。**

**1618窗口诊断完成：电脑通告sndwnd=65535、MSS=1460，板端仍以单MSS未确认为主，不能归因于对端窗口过小。20秒自定义接收器正常结束并清理临时文件，不计原始用例PASS。诊断配置缺NETDEV_STATISTICS，发送完成计数本轮无效；1619补齐后继续1620计数诊断。用户休息，暂停一切人工操作请求。冲刺76/88不变，证据checkpoint1618-tcp-window。**

**1616网络队列诊断完成：1615独立统计镜像仅新增NET_STATISTICS；20秒TCP期间8次有效采样，发送队列17520～18980字节，未确认数据0或1460字节，IOB空闲42～51个且无分配等待。应用供数充足，需继续核查对端窗口与TX完成/轮询衔接；不能据此直接认定回调丢失。测试正常结束、临时文件已清理，冲刺76/88不变。证据checkpoint1616-tcp-queue。**

**1612/1614修复回归：spawn文件操作临时恢复父地址空间，前台重定向及SIMD通过；NSH后台回退遇到无法打开的路径改为正常报错，随后SIMD再次通过。1613普通后台TCP重定向20秒完成并正常退出，文件已采集清理。1612后台SIMD并发cat曾无响应，证据保留未决，不声称所有后台并发均修复。当前镜像1612；未启用NET_STATISTICS，1613的/proc/net/tcp读取无效，需独立诊断配置。原始xTS冲刺76/88不变。**

**1608网络诊断：1565镜像短TCP对照中，默认ACK约251Kbit/s，电脑测试连接启用快速ACK后约1.05Mbit/s；接收间隔中位数60.72ms→13.61ms。仅一次对照、电脑为临时TCP接收器，不替代原始iperf300秒验收，不计新增PASS。1607冷ARP连接失败保留；测试socket已关闭，无全局网络修改。证据checkpoint1608-tcp-ack-diagnosis；下一步核查板端TCP发送流水及ACK节奏。冲刺76/88不变。**

**1606新增复核PASS：品类5.1.27原始定向扫描100轮，全部返回同一组3AP、无扫描异常，末次仍联网；BSSID/频率/加密/SSID集合稳定。RSSI及排序自然变化不作为AP列表增减，原脚本严格文本比较待复核结果原样保留。通用32/35、品类44/53，冲刺76/88=86.4%；另2项我方记录完成待社区验收，自测完成78/88=88.6%。旧1419漏AP记录保留，不声称软件根因已修复。证据checkpoint1606-directed-scan-pass。**

**1604新增原始用例PASS：通用2.1.3完成10次真实USB上下电，串口ROM日志至NSH平均0.265831秒（要求≤4秒），全部无启动异常；RESET保持期间等待不计入启动日志时间。通用32/35、品类43/53，冲刺75/88=85.2%；另2项我方记录完成待社区验收，我方自测完成77/88=87.5%。证据checkpoint1604-coldboot-pass，旧记录保留。**

**1603新增原始用例PASS：品类4.1.6空间变化掉电，3轮真实断电及四线程恢复校验通过，原程序Emmc size TEST PASS；各轮总空间1024KiB、可用空间恢复592KiB。三项文件系统掉电全部完成。通用31/35、品类43/53，冲刺74/88=84.1%；另2项我方记录完成待社区验收，我方自测完成76/88=86.4%。1597失败记录保留。证据checkpoint1603-poweroff03-pass。**

**1597空间掉电首轮未通过：1596在USB断开前出现write error；重挂后原testDir不存在，保留失败、不计轮次。已准备1598，等待用户就绪后启动并立即提示拔线，避免持续写入等待过久。4.1.4与4.1.5既有PASS保留，冲刺73/88不变。**

**1595新增原始用例PASS：品类4.1.5创建文件掉电，3轮各80个文件真实断电后保留，原程序恢复读写正常并输出Poweroff test open api passed；无格式化。通用31/35、品类42/53，冲刺73/88=83.0%；另2项我方记录完成待社区验收。证据checkpoint1595-poweroff02-pass。**

**1589新增原始用例PASS：品类4.1.4写入时掉电，3轮真实USB断电后原卷无格式化重挂，power_off_test01均TEST PASSED；恢复字节10100/28300/4400。通用31/35、品类41/53，冲刺72/88=81.8%；另2项我方记录完成待社区验收。证据checkpoint1589-poweroff01-pass。**

**1580新增原始用例PASS：通用1.2.2实体RESET按键启动异常测试，用户确认按键，完整ROM至NSH无异常，CPU1上线及ps/free正常。通用31/35、品类40/53，冲刺71/88=80.7%；另2项已完成我方记录待社区验收，我方自测完成73/88=83.0%。不包含10次真实上下电计时。证据checkpoint1580-physical-reset-pass。**

**1578优先级纠偏：暂停额外音频整合，回到剩余原始冲刺用例。1575配对镜像启动正常；1576 tmpfs录音挤占内核堆，1578 Flash录音仅4096字节，均未通过，证据已保留。冲刺70/88不变，旧音频通过记录不受覆盖。**

**1572–1575音频整合：关闭排空及进程地址空间保护已完成源码修改，FLAT/内核对象编译通过。1574完整构建成功但AppFS超限77824字节，未刷板；1575仅音频验证配置移除curl命令，网络原镜像/证据保留，重新构建中。当前板1565，冲刺70/88不变。**

**1570/1571音频MMU准备推进：APB记录所属堆，跨线程最终释放的主机回归旧版失败/修复通过；编解码器及I2S请求加入地址空间保留/切换，FLAT与内核对象编译通过。上层状态映射、关闭/退出排空尚未完成，未启用内核音频、不计上板PASS。当前板仍1565；冲刺70/88。**

**1567网络短回归完成：当前1565镜像电脑→板ping成功，反向ping未全通；邻居预热后TCP496KiB/367Kbit/s正常收尾，未归因于调度修复，不覆盖冷ARP失败或原300秒性能待验收状态。1568实体RESET记录脚本已准备，等待用户就绪；70/88不变。**

**1566 SIMD INT8计算演示通过：1565镜像两线程各100组16路有符号点积，全部与标量对照一致；跨核绑核与全寄存器保持同时通过。命令s31simd可直接演示实际PIE乘加，无标量回退；不宣称推理框架或加速倍数。冲刺70/88不变。证据checkpoint1566-simd-int8。**

**1564调度修复后的集成回归通过：同一1562镜像SIMD预检完成，GATT两读及13字节写入正确，Wi-Fi并发20/20、前后各3/3正常，服务退出完成。70/88不变。证据checkpoint1564-affinity-ble-wifi。**

**1563运行中绑核修复实板通过：两个线程明确从CPU0启动，各20次跨核切换检查通过，再在CPU1各100轮SIMD运算及完整寄存器保持零错误。修复调度器在无高优先级候选时未迁出禁止CPU的路径；1564同镜像BLE/Wi-Fi回归运行中。70/88不变；证据checkpoint1563-affinity-simd。**

**1561同镜像SIMD/BLE/Wi-Fi回归通过：先完成SIMD全寄存器验证，再进行GATT两读/13字节写入与Wi-Fi20/20并发收发，关闭蓝牙后Wi-Fi3/3正常。三者共镜像验证，不冒称三者同时满载或原始xTS新增PASS；70/88不变。证据checkpoint1561-simd-ble-wifi。**

**1560 SIMD全寄存器保持实测通过：1559镜像两个CPU1线程各100轮向量加法及完整PIE寄存器组睡眠/系统调用切换零错误。需创建时绑定CPU1，1556运行中迁移异常保留。1561同镜像Wi-Fi/BLE共存回归运行中；冲刺仍70/88。证据checkpoint1560-simd-fullbank。**

**1558 SIMD实板首项通过：1557镜像两个线程均确认CPU1，各100轮四路向量加法与Q0-Q7睡眠切换保持检查零错误。1556 CPU0异常保留，创建时绑核可用，运行中迁移问题未宣称修复。1559补齐QACC/UA_STATE/XACC/SAR验证候选正在构建；冲刺70/88不变。证据checkpoint1558-simd-q-context。**

**1556实际SIMD首测未通过：1555镜像启动正常，但worker执行首条向量加载时在CPU0触发非法指令，回到NSH。1557改为创建时设置CPU1亲和性并核验实际CPU，构建中；未宣称SIMD通过。失败证据checkpoint1556-simd-cpu0-fault。**

**1554 SIMD候选构建及启动完成：PIE上下文启用，SMP/MMU镜像CPU1上线并进入NSH；修复参数寄存器保护及汇编CFI跨度。尚未通过实际向量运算/任务切换验证，1555测试候选构建中。1552短TCP仍ARP超时，已归档不计通过；冲刺保持70/88。**

**1551共存开发验证完成：1550配对SMP/MMU镜像，GATT两读正确、完整13字节写入，Wi-Fi前3/3、并发20/20、后3/3均成功；服务退出正常。1549连接断开本轮未复现，仅增加诊断日志，不能宣称已消除间歇问题。证据checkpoint1551-coex-gatt-observed；冲刺完整PASS仍70/88=79.5%，SIMD后置。**

**1544/1545 蓝牙SMP/MMU集成回归通过：1543配对内核/AppFS镜像，CPU1上线、默认亲和性0x3；电脑两次读取Hello VELA!，板端收到完整13字节写入，服务停止/注销、关闭蓝牙、退出均正常。退出后仅保留CPU0专用ble_ctrl内核调度线程，无本次主机/控制器任务残留；Kmem空闲271744字节。修复用户态定时器/锁及TLS线程栈接口。证据checkpoint1544-ble-smp-mmu-gatt；1540失败原样保留。尚未验证Wi-Fi共存，不新增xTS原始用例PASS；冲刺70/88=79.5%，SIMD后置。**

**1537冷连接诊断仍失败：ARP重试间隔20→200ms未解决首次访问电脑时的ARP超时；不采纳延时调参，普通defconfig已恢复，1528发送轮询试改也已撤回。当前板为1534候选网络配对镜像，串口空闲。证据checkpoint1537-arp-window-negative；本轮未新增xTS PASS，冲刺70/88保持。**

**1533 TCP诊断：1528发送轮询候选251/252Kbit/s、收尾正常，但未显著超出既往波动，已撤回未证实有效的源码改动，不重复300秒。1524和1531两次冷连接均ARP超时；1534仅尝试ARP重试间隔20→200ms，待验证。冲刺仍70/88。证据checkpoint1527-tcp-rate-baseline、checkpoint1533-tx-burst-inconclusive。**

**1519 GATT实板回归通过：1520独立BLE FLAT镜像，电脑读取FF05/FF02均Hello VELA!；板端逐字节收到S31-GATT-1510（13字节），服务停止/注销、disable及退出NSH全部正常。已修复BLE-only误引用ATT-over-BR及服务注销误释放借用数据；旧1514/1516失败保留。证据checkpoint1519-gatt-read-write-cleanup。是开发链路验证，不替代xTS对端/间隔或SMP/MMU/Wi-Fi共存验收；冲刺完整PASS仍70/88=79.5%，SIMD后置。**

**1514/1516 GATT进展：1515 BLE-only镜像电脑端两次读取Hello VELA!、板端收到完整13字节写入成功；连接后自动停播，脚本重复等待停播回调超时。后续gatts stop 3触发内存断言，完整蓝牙仍未通过；1518修复借用user_data误释放，待构建上板。证据checkpoint1514-gatt-data-and-stop-fault，冲刺70/88不变，SIMD后置。**

**1509 BLE实际广播已验证：沿用1077独立FLAT镜像，100ms配置广播，Windows本机蓝牙30秒收到66条匹配响应（板MAC30:ed:a0:f3:f7:b1），板端停播/disable/退出正常。是广播功能开发验证，不替代手机nRF间隔验收，不加88项计数，也不代表Wi-Fi共存/SMP集成。证据checkpoint1509-ble-air-observed；继续GATT数据链路。**

**1499 UDP窗口候选原300秒完成：PC16.2Mbit/s/412783报文，板及PC Server Report均11.4Mbit/s/404MiB、30%丢包，最终确认正常，无页故障。较1486改善但实际发送速率不同，不作严格因果归因；性能仍待验收，70/88保持。证据checkpoint1499-original-udp-window。下一步蓝牙广播开发验证，SIMD继续后置。**

**1492短诊断定位：PC发送27648报文，板端驱动RX计数9063、队列丢弃352、UDP接收9006/层内丢弃0（均为快照，尾包可能未到）；OS队列丢包不足以解释差距。1493按锁定IDF建议试BA窗口12/静态RX16，正在构建，尚未刷板。原始1486性能未通过，70/88保持。证据checkpoint1492-rx-loss-location。**

**1486 UDP接收原300秒完成，未复现1479页故障，但性能仍不通过：PC32.3Mbit/s、板3.59Mbit/s，约88%丢包、末报文ACK缺失；板最终摘要在SIGTERM后输出。证据checkpoint1486-original-udp-rx。1489仅接收统计镜像已构建，待上板定位；普通defconfig已恢复，SIMD未启用。70/88不变。**

**1483信号量页表修复短回归完成：UDP收包后未复现1479页故障，shell/free正常；PC32.4MiB而板6.24MiB，末报文ACK仍缺失，不宣称性能通过。SIGTERM/SIGKILL后有服务线程残留，1484/1485记录保留；1487重启同一1480镜像清理后准备原始300秒1486。完整PASS70/88不变。证据checkpoint1483-sem-timeout-regression。**

**1479 UDP短诊断发生CPU0页故障，不通过：EPC4002a3fa定位nxsem_wait_irq在恢复内核页表后原子更新用户信号量。已归档checkpoint1479-udp-sem-mmu-fault；1480调整信号量更新至恢复页表之前，待构建/上板验证。70/88不变，SIMD仍后置。**

**1478 TCP发送原300秒完整回归：板301.07秒、PC301.01秒，双端10.6MiB/296Kbit/s，正常收尾无接收超时；1469板端关闭修复及1476 Windows select兼容版本见归档。原文速率标准未量化，记录原始负载完成、速率待验收，不加整项PASS。冲刺70/88保持。证据checkpoint1478-original-tcp-tx。**

**1472 TCP收尾短诊断修复有效：1469镜像下10秒发送两端339KiB一致、PC正常结束无接收超时，关闭回调在ESTABLISHED/SHUT_WR且队列已清空时执行。原始300秒1474运行中；短诊断不计xTS，70/88保持。证据checkpoint1472-tcp-close-recovered。**

**1467 TCP发送原300秒未通过：约30.6秒PC接收超时、板端连接断开；未触发关闭阶段诊断。1461镜像烧录/启动1462/重连1463正常。证据checkpoint1467-tcp-early-disconnect；总数70/88不变。正在有限短诊断定位，不替代原用例。**

**1458新增PASS：原始4.1.25两次RSSI查询分别-59/-60dBm，正常返回且动态变化。通用30/35、品类40/53，冲刺70/88=79.5%；另2项FS参赛者侧记录完成，参赛者侧72/88=81.8%。无需人工部分完整通过68/77=88.3%；人工部分2/11。证据checkpoint1458-sense-pass。**

**1457新增PASS：原始5.1.29断开/重新配网100/100轮全部通过，无自动重试，2617.36秒；旧1425失败保留，不据本轮成功宣称偶发错误根因已修复。通用30/35、品类39/53，冲刺69/88=78.4%；另2项FS参赛者侧性能记录已完成，参赛者侧71/88=80.7%。证据checkpoint1457-reconfigure100-pass。**

**1455执行口径澄清：完整PASS仍为68/88；另有1030两项FS性能依原文已完成参赛者侧记录要求、待社区验收。参赛者侧完成70/88=79.5%，不冒充70项最终PASS，不重跑旧测。详见review1455-selftest-vs-acceptance.md。**

**1453/1454新增PASS：原始4.1.115 SCP下载及4.1.116 SCP上传均通过，65536字节、退出0，往返逐字节一致。1444线程支持修复已构建/刷板/启动/重连验证；此前失败保留。通用30/35、品类38/53，冲刺68/88=77.3%。证据checkpoint1454-scp-bidirectional。**

**1435新增PASS：4.1.26原始配网/show参数查询通过，真实AP BSSID/信道/频率/IP/掩码/SSID正常，PTA明确不支持无查询错误。4.1.25两次sense均-72dBm，变化要求未通过，不加PASS。品类36，通用30/35；当前阶段66/88=75.0%。证据checkpoint1435-wapi-info。1430已刷板/启动验证；1425重配网66/100失败仍保留。**

**1425原始断开/重配网未通过：66/100轮完成，第67轮disconnect输出0x3006（ESP_ERR_WIFI_STATE）。1432现场检查shell/ps/free正常，wlan0已非RUNNING，未复位。证据checkpoint1425-reconfigure-state-error；不重试覆盖失败，不加PASS。**

**1430准备完成：S31联网BSSID查询改为真实AP；可选PTA查询明确返回不支持并在show中显示，不伪造优先级。含1427SCP初始化失败传播修复；构建/配对SHA/与1413配置一致性通过，未刷板、不加PASS。1425原始100轮重配网仍运行。**

**1428口径更新：依据竞赛导航，196项仅为全集盘点，不是比赛完成度分母。当前冲刺核验集通用35+品类53，完整通过65/88（73.9%），另须完成代码提交、多媒体Demo、介绍文档、视频、AI日志/Skill。完整工程目标与未选入阶段的P1/P2不删除。详见[contest1428-goals-progress.md](contest1428-goals-progress.md)。**

**1419定向扫描完成：100/100命令正常返回，370.20秒，无扫描报错/卡死；53轮3个同名AP、47轮2个，原文列表稳定要求尚未完全满足，状态为EXECUTION_COMPLETE_VARIATION_REVIEW_REQUIRED，不新增PASS。证据checkpoint1419-directed-scan。1425原始5.1.29断开/重新配网100轮正在运行，期间勿占用UART或复位。通用30/35，品类35项完整PASS。**

# S31 xTS execution status — 2026-09-16

**1423 LAN Demo上板验证通过：PC获取网页和status.json成功，板型/IP/请求计数正确；1424已清理服务。证据checkpoint1423-lan-demo。仅HTTP Demo通过，音频/BLE仍未整合，不增加xTS项数。正在1413镜像继续指定SSID原始100次扫描1419。**


**1422原TCP发送300秒完成，两端11.8MiB/329Kbit/s，板端自行返回；PC仍报告recv超时，不能认定正常FIN关闭。记录执行完成、告警与速率验收待定，不加PASS。证据checkpoint1422-original-tcp-tx。接着验证现有LAN Demo，再做连接稳定性。**


**1417原UDP发送300秒完成：板端300.01秒221MiB/6.17Mbit/s，收到服务端报告220MiB/6.16Mbit/s、丢包0.23%、乱序691，正常返回且1420 ps/free/ifconfig正常。PC独立日志本轮未追加，服务端数据以板端收到的原始Server Report为证据，不声称有独立PC原始日志。速率门槛原文未量化，记录实测、暂不加整项PASS。证据checkpoint1417-original-udp-tx。**


**1413最新进度：1412确认2986条UNKNOWN ERROR均为ESP_ERR_NO_MEM(0x101/-12)，现已仅对S31 STA发送暂满保留-ENOMEM排队重试、避免逐次错误刷屏；其他错误保留实际代码。临时诊断已移除，原iperf Client源码恢复。1413构建、配对校验、烧录校验完成；本镜像启动/配网及原始300秒复测尚未执行。通用30/35（85.7%）、品类35项完整通过，计数不增加。**


**1407短诊断已验证轮询修复有效改善：板端10.01秒6.11MiB/4361报文（原仅13报文），发送队列超时消失，shell正常。仍有UNKNOWN ERROR日志，1408补充实际错误码定位；短诊断不计xTS，原300秒待验证。证据checkpoint1407-udp-poll-recovery。通用30/35、品类35项不变。**


**1402短诊断确认UDP发送队列固定积压14980字节，每500ms超时，未见异常pacing。1403修复devif_poll_connections回调后清位覆盖新通知的问题，构建中待上板；证据checkpoint1402-udp-queue-diagnostic。1397原300秒复测仍仅13报文，未通过；短诊断不计xTS。通用30/35、品类35项不变。**


**1391原UDP发送300秒执行完成但性能未通过：301.05秒17.2KiB、13报文、469bit/s，PC报告2报文。原1386 PHY缺页未复现；1393 ps/free/ifconfig正常且无iperf遗留。证据checkpoint1391-udp-tx-stall。1392修复TCP/UDP发送缓冲释放通知的解锁到等待竞态，构建通过、正在上板验证；不声称已定位全部停滞根因。通用30/35、品类35项不变。**


**1386捕获CPU1 hr_timer MMU缺页，UDP300秒未开始（checkpoint1386-phy-mmu-fault）。已定位addrenv_switch切换内核线程时漏掉硬件页表deselect，1387补齐；构建/上板验证中，不声称扫描或DHCP停顿已解决。1379 NSH参数配置已上板，demo1385状态正常但音频/BLE节点缺失，完整Demo集成仍待完成。计数30/35、品类35项不变。**


**1378指定SSID扫描未通过：89/100次完成，第90次仅回显、60秒无提示符，已归档checkpoint1378-directed-scan-stall。正被动观察现场，未重启；1379仅NSH参数上限修复构建通过，尚未烧录。通用30/35、品类35项不变。**


**1376 UDP发送未完成：原始命令报NSH参数过多，随后PC ARP不可达，约4.64秒退出，保留FAIL（checkpoint1376-udp-tx-start-failure）。源码确认参数告警后仍执行，尚无截断证据。1379仅增加竞赛配置NSH参数上限至16，离线构建中；1354镜像并行执行1378指定SSID扫描100次，尚未完成。通用30/35、品类35项不变。**


**1372原始UDP接收实测完成：干净进程前置、原生Windows -t300/-b40M。主机300.01秒/32.4Mbit/s实际发送、827615报文；板端127MiB/3.48Mbit/s，报告88%丢包，最后ACK失败。仅记录实测，不计性能PASS；checkpoint1372-native-udp-rx-measured。继续UDP发送，通用30/35、品类35项不变。**


**1369/1372：发现旧iperf服务SIGTERM后未退出，1366同端口有多个监听，已主动停止该次发送，不作为有效300秒结果。1367/1368清理仍有35/44遗留，现场归档checkpoint1369-iperf-isolation后1370重启同一1354镜像，1371重新配网。1372增加开跑前无遗留iperf检查，执行原始原生Windows300秒/40M。不能据1365短诊断认定WSL为根因；计数不变。**


**1363/1365/1366：WSL路径UDP原始300秒未收到板端统计，未通过；短的原生Windows发送诊断已收到板端数据，证明需区分主机路径。现在1366执行原生Windows正式300秒/40M，尚未完成。短诊断不计PASS，实际收发总量和最后ACK告警保留。**


**1361接收实测完成：原始TCP Client -t300正常退出，两端约67.3MiB，主机304.11秒/1.86Mbit/s，板端289.82秒/1.95Mbit/s。时钟统计差异和socket选项告警保留；不追加接收端必须显示300秒的门槛，原脚本FAIL及复核记录均保存，速率验收待定。证据checkpoint1361-tcp-rx-measured。接着UDP RX1363，原始300秒/40M发送负载，不加整项PASS。**


**1360/1362：TCP TX持续发送原始300秒，未再出现1352的IOB错误；390秒未正常返回，手动关闭专用对端后才回到shell，故仍未通过。Linux总计8.79MiB包含收尾等待，不能作为300秒速率达标。证据checkpoint1360-tcp-tx-close-pending。按优先级继续另一方向TCP RX1361，暂不反复长跑单点。通用30/35、品类35项不变。**


**1360进行中：1354的16KiB发送背压已上板；1357首次ARP失败保留。电脑主动ping板卡确认MAC正确后，1360原始TCP TX300秒已建立连接并持续超过20秒，现使用Linux iperf经PC端口转发，双端逐秒日志持续。尚未完成、不计PASS；通用30/35、品类35项不变。**


**1352/1354：按用户全局优先级，先做TCP/UDP。原始TCP TX300秒未通过：板端15秒后IOB链分配失败且未收尾，PC16.67秒超时。1354仅启用已有16KiB发送背压配置，隔离构建待验证；不加PASS。Windows接收超时处理另有兼容问题，后续使用现有Linux iperf并记录PC转发拓扑。详见priority1352-network-and-demo.md。**


**1350/1351：SCP原始下载未通过，认证前用户进程EPC0页异常，shell存活。详细日志已确认TCP及SSH banner正常，接收KEXINIT后故障；根因未确认，未输入密码。证据checkpoint1351-scp-kex-fault。通用30/35、品类35项不变；1343重连DHCP阻塞仍未判修复。**


**1347新增PASS：5.1.26联网后周边AP扫描原始100次完成，345.494秒，100轮列表均返回且有变化，无扫描失败，结束仍RUNNING。1343配网前置renew阻塞保留于checkpoint1344，1345恢复后1346单次重连/DHCP正常，不代表间歇阻塞已修复。证据checkpoint1347-associated-scan；通用30/35、品类35项。**


**1342复核新增PASS：4.1.31原始公网ping断开前10/10成功，wapi disconnect后DNS发送ENETUNREACH(-101)、ping失败，后续ifconfig/free正常，无复位异常。1341脚本未识别不可达文本，原FAIL保留，未重跑。证据checkpoint1342-disconnect。通用30/35、品类34项；板端目前已断开Wi-Fi。**


**1337复核新增PASS：4.1.117原始FTP登录/list/get/put/quit通过，1336双向65536字节及上传回读SHA均一致。脚本误判原版QUIT主动返回-1产生的ERROR文字；源码及客户端221/三次226证实正常收尾，原FAIL和日志保留，无重跑。证据checkpoint1337-ftpd-review；通用30/35、品类33项。**


**1334新增PASS：4.1.114原始curl www.baidu.com返回HTML且退出0。1332补齐curl异步解析必需的AF_UNIX流套接字配置，保留SMP/MMU。证据checkpoint1334-curl-web；通用30/35、品类32项。继续FTP双向传输。**


**1331新增PASS：4.1.113原始HTTP下载通过，curl返回0，板端/data/x为65536字节，与主机源一致。1328修复原始curl/scp栈与TLS上限不匹配；1330公网网页仍报DNS线程创建失败，未通过。证据checkpoint1331-http-download；通用30/35、品类31项。**


**1326/1328：原始4.1.114 curl启动触发riscv_createstack断言，网络配置TLS上限8192小于curl/scp原始16384栈。失败证据已冻结checkpoint1326-curl-stack-failure；复用旧678经验，仅将网络配置TLS_LOG2_MAXSTACK改为14，1328隔离构建中，未计PASS。通用30/35、品类30项不变。**


**1325新增PASS：4.1.29原始保存/reboot/ifup/reconnect/renew/ping10/10通过，未重输密码。1322仅启用既有BOARDCTL_RESET，1321命令缺失失败保留。通用30/35、品类30项PASS；RSSI1324两次-71仍未通过。接着网络应用。**

**1320新增PASS：1318候选修正S31异步断开等待及重连配置缓存顺序，原4.1.30保存配置/重连/DHCP/网关ping10/10全部通过；未重输凭据、无自动重试。证据checkpoint1320-wapi-reconnect，1313失败保留。通用30/35，品类29项PASS；历史133项摘要全匹配。**

**1312新增PASS：4.1.28配置保存后SSID/PSK/mode/auth/cipher/alg逐项核对正确，秘密内容不归档。品类28项、通用30/35。1313重连关联成功但DHCP失败，1314接口仍有旧IP而路由选择报不可达，4.1.30未通过；1311 show的PTA查询报错、两次RSSI相同，均保留待补。先核验历史证据，已有PASS不重复执行。**

**1310新增PASS：4.1.21 WPA2关联、DHCP、网关ping10/10零丢包通过（1078配对镜像）。1309关联已成功，主机把panic=0统计误判为失败；1310同一连接续完DHCP/ping，无重连，原失败日志保留。品类27项PASS、通用30/35；用户已允许指定网络实测。**

**用户醒后调整优先级：允许网络适配。必须未配网的3.1.1待机12h（checkpoint864）和5.1.42扫描100次（checkpoint1112）已通过，无待跑前置。首批现有电脑/2.4GHz路由器覆盖19项：4.1.21/25/26/28/29/30/31，5.1.26/27/29，5.1.22/23/24/25，4.1.113/114/115/116/117。这些是待实测队列，不是新增PASS；其他加密/信道/路由器及外围条件仍待核对。音频1295再次180秒停滞，中断/锁记录正常；1297任务快照诊断仅构建完成，尚未上板，先保存转网络。**

**1271–1278：完整原AAC从Flash也复现180秒无完成回调、状态查询无响应，目标回读SHA正确；同文件此前TMPFS1249正常结束。1273短标记镜像的首个音频期间Flash读完整返回FLBCREMU，但1277仍无完成回调。已冻结checkpoint1272/1278，继续定位后续读取/调度，根因未确认，不加PASS。原3MiB已于1266恢复并读回校验，之后仅新增AAC测试文件；不配网、防休眠持续。**

**1265–1270定位中：独立Flash媒体镜像构建/烧录/启动成功；1266原3MiB测试区已恢复且全量读回一致，1267无格式化挂载原LittleFS成功。正在上传并回读完整原AAC，随后从Flash播放（session70695），用于定位1263 WAV阻塞，不替代WAV验收。防休眠1236继续，未配网。**

**1263/1264未通过：完整原WAV上传及目标回读SHA一致，媒体启动PREPARED/STARTED ret0；180.006秒未收到COMPLETED，随后position查询20秒无响应。已保留checkpoint1264-wav-stall；未复位、未重传、原3MiB备份尚待恢复。排查Flash读取与音频运行交互，根因未确认；不增加PASS，防休眠1236继续。**

**1262上传完成：完整原始WAV 17473937字节已上传，板端长度及传输返回值正常；现在自动回读目标文件验证SHA，随后启动媒体播放。上传结果仅含主机源SHA，不冒充板端SHA；尚未新增PASS。串口session78290、防休眠1236继续运行。**

**1254/1258/1262：WAV候选已烧录启动，组合卷挂载成功（17MiB，剩余堆约2MiB）。原3MiB测试区1255双备份一致，1256限定清理后双读确认空白；原备份保留待恢复。1262无人值守串行执行配置、完整17473937字节WAV上传/回读和播放（session78290）；当前尚未完成，不计PASS。1236防休眠进程确认有效，允许熄屏，不配网、不需人工操作。**

**1253自动部分完成：1243镜像原MP3 3603915字节运行92.354秒，COMPLETED/STOPPED ret0、close/q及线程退出正常。AAC1249、Opus1251、MP31253的自动解码/驱动执行均完成，三项仍待听音，不增加正式PASS。接着保全3MiB测试卷，为完整WAV准备。**

**1252/1253进行中：已清理本轮归档AAC的tmpfs副本，原始主机附件不变；原MP3上传后自动验证（session35580，1243镜像，独占UART）。AAC1249/Opus1251自动执行和归档已完成，均待听音；WAV1254构建完成尚未上板。1236防休眠持续有效，无自动到期。**

**1251自动部分完成：1243镜像原Opus完整运行92.351秒，COMPLETED/STOPPED ret0、close/q及线程退出正常；未报告XRUN/I2S错误。4.1.129仍待听音，证据checkpoint1251-media-opus；正式品类26项、通用30/35不变。接着原MP3。**

**1254离线构建完成：完整WAV组合卷候选已包含当前媒体修复，独立out/esp32s31-xts-flat-media-volume1254，镜像SHA8d4ea4b8…202fe7；仅BUILD PASS，未上板、未动原3MiB测试卷。板上1243继续1250/1251原Opus测试；AAC1249自动执行完成但听音待验收。1236持续防休眠有效。**

**1249自动部分完成：1243镜像原AAC 1474609字节运行92.330秒，COMPLETED/STOPPED ret0，close/q正常，播放器线程退出；未再观察I2S参数错误/XRUN/EOF误报。4.1.130仍待实际听音，不计完整PASS；证据checkpoint1249-media-aac。继续原Opus，通用30/35、品类26项不变，1236持续防休眠有效。**

**1246/1248：1243镜像启动及媒体服务初始化已成功，1248原AAC上传后自动运行1249收尾复测（session85273，独占UART）。夜间实时状态night-current-work.json。1236持续防休眠PID882843已确认存活，无自动到期；手动暂停/无人值守工作结束后释放。当前通用30/35、品类26项；音频仍待完整收尾和实际听音，不加PASS。**

**1243/1244：EOF结果分离修复已完成构建，实际函数四种状态检查通过（运行中EOF、短流启动、恢复真失败、排空完成）；旧函数对正常EOF误报已复现。1235无已观察到的I2S参数错误/XRUN，但EOF日志及close超时未验收，保留checkpoint1243-media-eof。1244新镜像/1245配置/1246服务初始化进行中，随后1249复测。1236持续防休眠有效。**

**1232/1234/1236：1229游标修复镜像启动、媒体服务初始化成功；1234上传原AAC后自动执行1235复测，结果待核查。Windows防自动休眠已切换为1236持续请求（PID882843/session91866，无12h自动到期），旧1182已请求释放；手动暂停或无人值守工作结束后通过stop文件释放。允许熄屏，不能保证外部断电/强制关机或系统崩溃。通用30/35、品类26项不变。**

**1227/1229：1220高优先级DMA候选播放时明确报告I2S -EINVAL。源码核对发现TX完成错误地把apb.curbyte设为nbytes，而音频上层复用mmap缓冲只重置nbytes，下一次整块提交被算为零长；与先前head/tail5时欠载相符。1229改为保留调用方游标，进度仍在请求内记录；未放宽512字节限制，未改原始AAC。构建中，尚未验收；证据checkpoint1227-media-cursor。**

**1224/1226：1220调度候选启动与媒体服务初始化通过，板上ps确认s31_duplex优先级253。1226原AAC上传后自动执行1227；只因1218失败且有具体调度变更才复测，不增加原始工作量。当前无配网、无人工听音；Windows1182防休眠存活，品类26项、通用30/35不变。**

**1218/1219：降低日志并修复回调返回值后，原AAC仍在STARTED后出现ALSA XRUN(-32)，close 0亦未及时返回；不计PASS，证据checkpoint1219-media-xrun。1220媒体候选正在提高DMA补环线程优先级（253，高于graph245/codec252），保留基准240，并启用有限ERROR/WARN诊断。尚未证明欠载根因；当前串口已释放，板上1212待新镜像恢复。**

**1215媒体服务启动通过：1212镜像（8dad0bb6…bb9fe92）与修正后的板级policy配置已上板，mediad初始化无错误、任务存活、资源正常；原配置与1206告警保留。1217重新导入原AAC、1218自动复测中；不计解码播放PASS，通用30/35、品类26项。Windows1182防休眠仍有效。**

**1211/1212：原AAC附件1474609字节上传/读回SHA一致，1209解码与44.1k浮点→48k S16转换初始化成功，但播放连续XRUN；1210普通close超时，1211记录硬复位恢复，未擦写Flash。未计PASS。已修复abufsrc回调负错误码被布尔比较覆盖的问题，并降低实时日志；1212构建中。板级静态路由已在graph.conf设置map=1，policy改为设置初始音量，避免无音源时重复触发格式协商。失败证据checkpoint1211-media-xrun。**

**1206/1207：1203启用mutex类型后，原ENOSYS消失（FFmpeg thread.h请求ERRORCHECK）；mediad保留未加载音源时rate/ch为0的格式协商告警，尚未消除。实际mediatool打开Music/关闭/q均成功、播放器线程退出，服务仍在；1208正在导入原AAC附件验证真实音源，不计媒体PASS。**

**1202/1203媒体适配：1199媒体镜像启动与内存检查成功，三份运行配置上传/读回SHA一致；mediad初始化因未启用CONFIG_PTHREAD_MUTEX_TYPES返回ENOSYS，失败日志及镜像已冻结checkpoint1202-media-init-failure。已补媒体候选所需配置，1203正在隔离构建，原985/987和已通过镜像不变。无新增媒体PASS，不配网。**

**1198新增PASS：逐条核对原文后，4.1.127的预期仅为nxlooper命令无报错；1197镜像完整执行原始启动/15.072秒观察/stop/q，无错误且线程退出，因此该项通过。4.2.14另要求回环正常，仍待实际听音，不计通过。扫描器EOF保护修复、禁用高频INFO保留ERROR后运行正常；先前1196失败完整保留。证据checkpoint1198-nxlooper-commands。品类26项PASS、通用30/35（85.7%）。**

**1196：扫描器EOF保护已上板，原始loopback命令不再卡在参数解析；随后RX传输返回-EIO，回环尚未通过。1195镜像/配置与原始失败日志冻结于checkpoint1196-scanf-eof。继续最小DMA故障定位；计数仍通用30/35、品类25项。防休眠1182进程已复核存活，用户休息期间不配网、不要求人工配合。**

**1191定位进展：1188音频日志候选复现启动停滞；1190临时入口标记证明停在原nxlooper参数 `sscanf` 内（已打印before，未打印after），尚未进入音频回环函数。应用源码已恢复，诊断镜像/代码/日志保留于checkpoint1191-nxlooper-parser；不计PASS，不归因DMA。当前板上1190诊断镜像，串口采集已退出，后续定位解析路径。**

**1186/1187：979独立全双工候选已刷写并上板；两个pcm节点和nxlooper设备选择成功，但 `loopback 2 16 48000` 在20秒内未返回提示符，追加30秒被动观察无输出。启动未验收、不计PASS；未因超时重跑/复位，现场待诊断。证据checkpoint1186-nxlooper-stall。通用30/35、品类25项PASS不变。**

**1184新增PASS：原始5.1.15 `fstest -m /data/fstest -n 1000` 完整结束，8833.597秒（约2小时27分），1000次填充/删除齐全，原程序2000 OK/0 FAILED，板上返回0、任务退出、目录清理完成。证据checkpoint1184-fstest1000摘要全部校验通过。1181主机脚本误判原程序目录项 . / .. 的Error显示，1183/1184仅排除这类显示文字后复核同一次运行；原日志与异常堆栈保留，未重跑。品类25项PASS，通用30/35（85.7%）；UART已释放。不配网，防休眠1182继续生效。**

**1181/1182/1184 夜间进行中：原始5.1.15 `fstest -m /data/fstest -n 1000` 独占串口，当前未计PASS；Windows防自动休眠请求已启用12小时（允许熄屏）。1184收尾进程只在1181退出且完整1000轮/2000 OK/0 FAILED核验通过后读取原始返回值、执行原文清理并归档至checkpoint1184-fstest1000；失败保留现场，不自动复位重试。实时收尾状态见fs1184-finish.json。通用30/35、品类24项PASS。用户休息期间不配网，不启动需接线/按键/听音的用例。**

**1176 RESET实测窗口超时：10分钟内未观察到用户实体RESET后的启动日志，未计通过；串口已释放，继续后续非外设测试。**

**1173新增PASS：原始5.1.1分区写满异常测试完成，主机126.708秒、程序报告103.52秒，原始TEST PASSED/返回0，测试文件自动清理，任务退出；保留原程序不报告终止写入errno的覆盖限制。证据checkpoint1173-fs-fill。品类24项PASS，通用30/35，串口已释放。**

**1173运行中：完整卷1170备份后，1171启动/1172无格式化sync挂载完成，执行原始5.1.1分区写满用例，独占UART/session20775，日志logs/xts1173-fs-fill.log。不得在运行时刷机或复位。品类23PASS、通用30/35。fstest1000次待结合外围实测时间安排。**

**9月17日用户已明确恢复。1167启动/1168无格式化挂载通过；1169复核5.1.16按原文观察标准通过：4579.232秒、首轮100次大文件操作及小文件阶段无错误，首轮OK；不宣称1000外层循环完成。kill收尾限制定位为1102未启用CONFIG_SIG_DEFAULT，保留失败脚本和复位记录，不追加非原文终止门槛。品类23项PASS，通用30/35。**

**9月17日已按用户要求暂停：1164碎片测试观察4579.232秒，100小文件/首轮100次大文件创建删除成功，原程序首轮OK；终止任务未退出，1165保留收尾异常，1166进入ROM并硬复位停止残留任务，无刷写、无新测试。暂不增加PASS，品类22项、通用30/35。证据checkpoint1164-fragment-observation。等待用户恢复。**

**用户最新指令（9月17日）：完成当前1164碎片测试后暂停。仅完成已启动观察范围、归档结果并汇报，不启动后续测试或适配，等待用户恢复。**

**1164运行中：原始5.1.16碎片测试，按资源设置-n100/-s1，未用-c/-N缩减程序循环；计划观察首个完整外层循环（100次大文件创建/删除）后明确停止，按原文“运行一段时间后无异常”记录实际观察范围，不宣称默认1000轮完成。当前1102镜像、独占UART/session76979，不配网，不复位。品类22项PASS，通用30/35。**

**1163新增PASS：5.1.17文件系统三线程一小时用例完成，实际3921.870秒，三线程正常退出，原始PASS及返回0，无错误。1102镜像、3MiB LittleFS显式sync挂载，保留已披露worker-join修复；耗时延长原因未证实，不作时钟精度结论。证据checkpoint1163-fs-one-hour。品类22项PASS，通用30/35（85.7%）；UART已释放。**

**1155新增PASS：1144候选在既有3MiB LittleFS上完成原始KVDB十轮5.1.68，程序/主机退出0，无提交/底层写入错误；最终DB405504B，可用591/768块。证据checkpoint1155-kv-large-stability。品类21项PASS，通用30/35。随后保存卷并执行文件系统一小时用例。**

**1147/1149：1078离线demo状态/RGB驱动/I2C/资源命令完成，BOOT真实按下1→松开0通过，RGB外观待用户确认，不增加xTS计数。1155：1144候选在已备份的3MiB卷上按原始前提重启后运行KVDB10轮，可用691块（约2.70MiB），尚未验收。通用30/35、品类20项PASS。**

**1143新增PASS：1131镜像原始16k/mono/16bit采集1/1及原样导出10.112秒，用户确认“讲话完整清晰，语速正常，无明显电流噪声”。品类累计20项PASS；通用30/35。1134错过讲话录音保留未验收；1139诊断中断未计PASS。接着验证1078离线demo，不配网。**

**1139中断说明：为响应用户麦克风重录请求，诊断在第4轮开始后主动中断，未观察到KV VFS错误但未完成10轮，不计PASS；checkpoint1139-kv-interrupted保留原始日志。接下来使用1131录音镜像，等待用户明确“开始”再录；通用30/35、品类19项不变。**

**1134：1131补齐NSH参数容量后，原始16k/mono/16bit采集1/1及原样导出完成（323584B，10.112秒），待用户试听，未增加PASS。1139 KVDB errno诊断镜像已按原文清库/重启后开始原始10轮；通用30/35、品类19项PASS。**

**1087试听验收：用户确认microphone1087-capture-original.wav“清晰流畅”，原4.2.12只录音完整通过。品类累计19项PASS，通用30/35。1129原16k/mono采集进行中，尚未验收。**

**1126新增PASS：5.1.19在1102显式sync挂载配置下，原双线程工作量/15秒复位后两个文件均229376字节，完整内容检查通过，无格式化。test04仅保留已披露的读取器安全修正，写入器不改；旧1083失败保留。品类18项PASS，通用30/35。1119 KVDB原始10轮FAIL，待底层errno诊断。**

**1112新增PASS：1078配对镜像上原始5.1.42未配网扫描100/100完成，扫描列表有变化，ifdown收尾正常。品类累计17项PASS；通用30/35。旧1073页异常未复现，根因仍未确定，保留原失败记录；1100 KVDB写入错误待容量复测。**

**1100：KVDB原始10轮已结束，程序打印PASS但记录90次数据库写入错误，复核不计通过。完整原始日志和旧执行器已归档checkpoint1100-kv-stability-errors；空间统计异常待排查。通用30/35、品类16项PASS不变。**

**1094：5.1.7原程序按文中允许Count完成1000读/10写，0.55s/43.21s，无空间错误；随机写性能暂未验收，不增加PASS。1091完整3MiB卷备份后仅清理已归档的1045文件。1087原始只录音4.2.12执行和原样导出完成，待用户听音。通用30/35、品类16项PASS不变。**

**1085新增PASS：原始BLE enable/state/disable/state完整通过，回调及状态2→0正确、退出清理正常。真实eFuse公共MAC、硬件熵源、控制器缓冲参数和关闭生命周期修复已上板验证。品类累计16项PASS、2项性能记录待社区验收；通用30/35。Wi-Fi1073第49轮页异常和FS1083未同步文件丢失仍未通过。**

**1081/1082新增PASS：5.1.18原始软件重启及崩溃两个分支完整通过，实际自动复位、无格式化重挂、非空整记录文件和原CRC检查均确认。品类累计15项PASS、2项性能记录待社区验收；通用30/35。Wi-Fi1073在第49轮页异常，BLE1076开启/state2成功但关闭崩溃，两项均未通过、修复中。**

**1065新增PASS：品类5.1.2原始1000×1024字节写入全部成功，21.899秒，TEST PASSED及后检查完成。品类累计14项PASS、2项性能记录待社区验收；通用30/35。1059麦克风用户试听通过，外接扬声器播放未验收；1061 BLE熵源成功后缓冲区配置失败，修复中。**

**1059试听确认：用户反馈“说话清晰，电流噪声已消失”。1057连续DMA/44.1k及零长FINAL修复已通过原-a3程序1/1和板载麦克风实际录音试听；原样导出10.031秒PCM/WAV，边界幅度比由15.36降至0.94。J9扬声器需外接，完整播放声音验收仍待实物，不增加品类整项PASS计数。证据checkpoint1059-audio-target。**

**19:15 更新：1045原1000/1000随机读写按竞赛时间安排中断，未计通过；已确认随机读1000次耗时0.54秒，随机写未返回。原文允许按情况调整Count，后续按允许次数复测；日志与中断说明保存在checkpoint1045-fs-random-incomplete，Flash卷未格式化，1053挂载证据因切换镜像已失效。1057音频修复镜像烧录/启动通过，1059原始录放复测进行中，声音尚未验收。**

**19:06 更新：1045文件系统原1000/1000仍占用串口，未返回终态。1057音频候选已补齐零长FINAL顺序收尾，连续DMA+44.1k独立构建/必要主机验证通过，待上板；用户已确认可配合听音，不重复询问。1049噪声录音缓冲边界RMS比值15.36，仅作为修复诊断，不替代试听。BLE1055已编译待上板。通用30/35、品类13项PASS及2项性能记录待社区验收。**

**1054/1055构建就绪：连续44.1k音频DMA修复已编译；BLE补齐硬件/dev/urandom熵源已编译。均未上板，不计通过。当前1045原随机读写独占串口，结果待返回；完成后优先音频用户重录、BLE原开关验证。**

**当前18:46：1045原1000/1000随机读写在备用3MiB卷执行中（尚未返回结果）；旧1MiB/KV数据保留。用户1049录音听到明显电流声，音频未通过；1054连续时钟/DMA修复构建中，准备好再通知用户重录，不重复询问可否配合。通用30/35，品类13项PASS及2项性能记录完成待社区验收。**

**音频1049听音反馈：用户报告很嘈杂的电流声，声音验收FAIL，不计xTS音频通过。原始cmocka/传输成功仅证明数据通路；正在定位I2S/codec输入配置，保留PCM/WAV。**

**音频1038补充：用户确认刚才未参与录音，因此PCM与cmocka通过仅为数据通路证据，声音验收未完成；按用户要求18:55:28以后再通知重测。板载麦克风，J9扬声器需外接。**

**最新1034/1035：Wi-Fi国家码US/CN读写、原100轮接口开关PASS，品类13项PASS，另2项性能记录完成待社区验收；通用30/35。音频硬件更正：板载麦克风+ES8311+功放，J9需外接喇叭，不能直接板上听音。1038原录放音执行/导出中，声音未验收。**

**最新：5.1.3原100轮PASS1022，5.1.4修复通用VFS路径组件边界后原用例PASS1028，品类11项、通用30/35。BLE1025停在enable状态1，未收到状态2，未通过，正在定位。**

**文件系统更新：5.1.5/6/8/9/10原始用例及后检查全部PASS，品类累计9项；5.1.4第二层目录chdir失败待定位，不能归因于硬件路径限制。通用30/35。日志编号因批处理命名分别1019/1535/1537/1538/6129，保留原名。**

**新增：文件系统 5.1.10 原始多线程读写通过；5.1.4 最大文件名/路径用例失败，原始程序报告无法进入生成路径并输出 TEST FAILED，证据 xts1533-fs-5.1.4.log，未改参数或伪造通过。**

**新增1013：品类KVDB4.1.11原始30/30通过，品类累计4项；通用30/35。**

**新增1016：999镜像原始Watchdog 0→1→2→3完整PASS，前三模式真实硬复位/堆栈/0x10，最后模式4/4通过；整包命令发送后本次无输入阻塞。通用正式30/35，品类3项。996真实失败及1004/1008传输中断保留。**

**新增1010：1002离线配对镜像上八个原始Crypto应用全部PASS，SHA/HMAC真实计数与status=0日志正常，支持的硬件AES/SHA/ECC路径验证完成；3DES/MD5/CRC32软件实现。通用正式29/35，品类3项。看门狗完整序列仍在定位串口输入阻塞。**

**新增1006：品类ROMFS4.1.2、FATUTF8 4.1.3原始用例PASS，品类累计3项。看门狗1004模式0/1真硬复位通过，模式2命令传输未完成，完整序列仍待验证。Crypto1001八个原始应用功能断言通过，SHA/HMAC日志格式修复中。通用暂28/35。**

**新增上板结果（992/994）：原始1.3.5 Flash块测试3/3通过（5.337秒）；品类5.1.13原始dd各256KiB读写通过，写275KiB/s、读2560KiB/s，前后内存稳定。隔离区0xc00000的两份初始空白备份一致。通用28/35，品类正式PASS 1；接着执行看门狗。**

**最新验收口径确认（989）：按用户要求，不额外要求连续日志或精确到秒的采样间隔。1.3.14四次阶段检查误差均≤2秒，最终实际24小时5.07秒、误差−1.684～−0.663秒，项目验收记PASS；前两次提前采样的实际时刻及日志缺口保留。通用更新为27/35，品类PASS 0；988原始证据与先前判断不改写。**

**16:43：864长测完成最终采样并正常退出，串口已释放。实际24小时5.07秒，最终板时相对PC误差−1.684～−0.663秒，满足24h/≤2s数值指标。前两次采样提前及06:33至08:12记录缺口保留，严格1.3.14暂不记PASS。每10分钟主机监测已跟踪到结束；正式通用26/35、品类PASS 0不变。最终证据988。**

**12:13：987完整原WAV候选最终干净构建通过（1,671,852字节）；17MiB临时卷原附件完整读回SHA一致、实际组合驱动边界与17项主机脚本保护检查通过。备用Flash尚未写入，连续PSRAM分配、真实传输/播放仍待上板。可并行的本轮离线准备已补齐，后续按长测收尾后的上板队列推进；不配网。通用26/35、品类正式PASS 0不变。**

**12:07：987完整原WAV临时卷准备进行中：现有LittleFS主机实测17MiB卷可完整写入17,473,937字节原附件，重挂读回SHA256一致，余300KiB。组合MTD/profile已实现，边界回归与构建尚在进行；备用Flash尚未写入，实板播放仍未验证。986原LZF压缩容量证据已归档。长测采集继续，通用26/35、品类正式PASS 0不变。**

**11:47：985原始mediatool音频候选最终干净构建通过，主机PCM能力/子图必要回归通过，未上板。MP3/AAC/Opus附件已核对；WAV原文件超单个16MiB存储容量，限制保留。长测仍在运行、不配网；通用26/35、品类正式PASS 0不变。**

**10:43：原定18小时采样已完成，实际64800.12秒，板/PC误差区间−0.730～+0.290秒；证据982已归档。24小时用例仍在运行，最终约16:43复核；通用26/35、品类PASS 0不变。**

**08:14 恢复取证：原showinfo任务PID7仍在、资源值一致、板时与PC相符；用户确认开发板持续供电。已恢复同次启动的采集，保留06:33至08:12日志缺口，预计16:43达到实际24h；尚未判定24h通过。恢复记录xts864-recovery.json。**

**08:10 中断更正：WSL于08:05重新启动，原864采集进程已不存在；最后串口日志06:33。12h待机归档PASS保留，24h时间一致性未完成，原JSON的RUNNING已过期。USB已重新挂接，正在核查板上连续性，尚未复位/刷机。**

Primary goal: applicable published xTS cases and actual usability. Submission
is September 19 (user confirmed September 15). Aim to finish board validation
and deliver fixture firmware September 16 evening; reserve September 17 for
physical peripheral tests and September 18 for fixes, retests and demo evidence. Existing diagnostics are supporting evidence only. No new
boundary diagnostics unless needed to unblock a standard case or avoid major
data/boot risk. Failed cases remain required and visible.

Authoritative case source: openvela-dev/docs/zh-cn/test_dev_guide/openvela_xts_test_cases.md.

Category coverage is tracked separately in `xts-category-status.md`:161 original
heading instances,29 with confirmed5GHz hardware mismatch, remaining132 requiring
product/fixture applicability and individual acceptance. Category cases are not
included in30/35 below. Storage/KVDB/OTA/NetApp detail: category-storage-audit.md.
No complete category acceptance or overall completion percentage is claimed.

Common checklist has35 headings:30 accepted against target evidence
(85.7% common-checklist coverage, NOT whole-project completion).
Case1.3.14 uses the clarified project acceptance decision989 and retains
actual early sampling times; no exact-six-hour-spacing claim is made.
Twenty are standard functional commands below, plus flash/NSH case1.3.1
and reboot cases1.2.1/2.1.4 plus footprint1.2.3/1.2.4, standby3.1.1
and time consistency1.3.14 under decision989, plus Flash block1.3.5, Crypto1.3.17 (1010) and Watchdog1.3.15 (1016).

Current common-checklist allocation (35 total):

| State | Headings | Scope |
|---|---:|---|
| Target/project acceptance PASS |30| Evidence below;1.3.14 clarified acceptance989 |
| Statistical output incomplete |1|Original10-stream RNG786; supplemental100-stream808 retained |
| Physical cooperation deferred |4|Reset button, cold-power timing, GPIO jumper, BMI160 |


| Common case | Command | Current result / evidence |
|---|---|---|
| 1.1.1 memory | cmocka_mm_test | PASS on686: all8/8subcases,3.630s, xts689-mm.log; earlier680 PASS also retained. |
| 1.1.2 scheduler | cmocka_sched_test | PASS on686: all9/9, xts688-sched.log. Earlier681/682 fatal and685 ENOMEM retained. Upstream selects9pthread cases in BUILD_KERNEL,7task cases are BUILD_FLAT-only. |
| 1.1.3 syscalls | cmocka_syscall_test | PASS on704:83/83,15.656s,normal NSH return,no assertion (xts709-syscall.log). Earlier692/698/702 failures retained. |
| 1.1.4 ostest | ostest | PASS on771/775,52.081s: full original configured loop reaches user_main:Exiting and status0, no ERROR. Fix retains recipient for syscall-deferred signals instead of allowing another thread to consume shared pending entry. Earlier764premature exit retained. FLAT-only spinlock source condition matches original entry point. |
| 1.1.5 getprime | getprime | PASS on690,1040ms,xts693-getprime.log |
| 1.1.6 heaptest | mm | PASS839 on837: original complete allocation/reallocation/alignment workload, TEST COMPLETE, no failed/skipped allocations,6.758s. Separate FLAT profile; full16MiB PSRAM included. |
| 1.1.7 scanf | scanftest | PASS on690,164OK/0FAILED including#25,xts694-scanftest.log |
| 1.1.8 C | hello | PASS on690,xts695-hello.log |
| 1.1.9 C++ hello | helloxx | PASS on716,all3dynamic/stack/static instances,xts718-helloxx.log |
| 1.1.10 popen | popen | PASS on730, Calling pclose() and clean shell return,733. Current source requires POSIX timers enabled, contrary to old checklist config snippet. |
| 1.1.11 pipe | pipe | PASS on730,FIFO/transfer/interlock/redirection complete,33.542s,732. Source removes its own FIFO paths; doc's duplicate rm is obsolete and not executed blindly. |
| 1.1.12 MD5 | md5_test -f /etc/1.txt -c 100 | PASS on730,100 identical correct hashes,2.339s,734. Fixture created only in fresh volatile /etc tmpfs, no persistent file changed. |
| 1.1.13 C++ functions | cxxtest | PASS797 on794,0.204s:vector/map/C++17/RTTI plus Catch Exception:runtime error, clean NSH return.790/793 capture libgcc unwinder abort and zero EH header:orphan sections cause32-byte loaded offset. RISC-V linker groups small-rodata/exception tables/small-data;795 layout goes from180mismatches to0. Explicit EH registration/terminator retained; temporary diagnostics removed. No original test edits. |
| 1.2.1 / 2.1.4 reboot | reboot10times | PASS on724:10/10 complete NSH/SMP startups, mean0.548s <=6s, no abnormal boot logs (726). Host/UART timing includes overhead; not cold-power-cycle evidence. Board reset fix committed fd5f113e1a0. |
| 1.2.2 Cold boot启动异常测试 | PASS1580：实体RESET按键 | 用户确认按键，1575配对镜像完整启动日志无异常，CPU1上线，ps/free正常；不作为真实断电计时证据。 |
| 2.1.3 cold boot timing | 10 actual USB power cycles / ROM-to-NSH timestamps | PASS1604: mean 0.265831s ≤4s; checkpoint1604-coldboot-pass. RESET-held enumeration wait excluded. |
| 1.2.3 / 1.2.4 footprint | free; df -h on each CPU | Complete on730,736log confirms CPU0(mask1)/CPU1(mask2), mask3 restored. Physical Flash map and shared-memory interpretation in xts736-footprint.md. |
| 1.3.1 flash | existing guarded flash procedure | Prior flash668 + demo669 establish flash/hash/NSH; not a substitute for other cases |
| 1.3.2 RAM file R/W | fstest -n 10 -m /tmp | PASS on765:10loops,20OK/0FAILED,no ERROR,64.948s (767). Original512-file/default-size workload unchanged. Linker reserves PSRAM0x50c00000..0x51000000 for4MiB FS heap, excludes it from12MiB page pool. Earlier703internal-SRAM ENOMEM retained. |
| 1.3.3 RAM patterns | ramtest -w/-h/-b -s 209256 | PASS on700 for all3widths; size from free maxfree, buffer allocated by original test in user heap. Logs705/706/707. |
| 1.3.4 RAM block R/W | mkrd;cmocka_driver_block | PASS840 on837: original stress/single-write/cache-write3/3,2.364s, fresh1MiB RAM disk /dev/ram10; no persistent storage writes. |
| 1.3.5 Flash block R/W | cmocka_driver_block -m /dev/xtsflash | PASS992 on946: original stress/single-write/cache-write3/3,5.337s. Fixed1MiB scratch at0xc00000; double initial blank backups990 identical. Used18256/free17218384 bytes unchanged before/after. |
| 1.3.6 GPIO | cmocka_driver_gpio | BUILD886 PASS, TARGET NOTRUN: J2.13 GPIO47 /dev/gpio0 input and J2.14 GPIO48 /dev/gpio1 output; original API/loop/IRQ runner requires actual IRQ count as well as cmocka PASS. External jumper still required. |
| 1.3.7 I2C/SPI sensor | cmocka_driver_i2c_spi | BUILD887 PASS, TARGET NOTRUN: external BMI160 I2C0 GPIO45/46 on J2.15/16, addr0x68, /dev/accel0. Actual-driver host checks PASS; C6 BMI160 protocol-equivalent fixture builds at `tools/esp32c6-bmi160-emulator`, but flashing, two-board wiring and original100 reads remain. |
| 1.3.10 UART | cmocka_driver_uart -d /dev/ttyS0 -n 0/1/2 | PASS on741:all3modes,official payload and10burst samples,744. Current source splits old combined test into three modes; no device test edits. |
| 1.3.11 UART files | rb -f /tmp/xts805;sb /tmp/xts805/{small,binary}.bin | PASS805 on801:129-byte and65573-byte binary files transferred both directions,byte-for-byte match,normal sb/rb exit. Existing host sbrb.py tail-length and final-ACK bugs reproduced802,fixed803(2host testsPASS); no protocol rewrite. Fresh tmpfs and host fixture directories only. |
| 1.3.12 RTC | cmocka_driver_rtc | PASS841 on837: original API/alarm/periodic3/3,37.343s; absolute/relative timing and alarm readback checked, real SIGEV_THREAD callback logged. Separate FLAT test profile; awake periodic service, not deep-sleep wakeup evidence. |
| 1.3.13 timer | cmocka_driver_timer -d /dev/timer0 | PASS on741,1/1,5.591s,743. Current registered timer driver is timer0; old doc's timer-command typo corrected to actual original program. |
| 1.3.14 24h time | documented date/host comparisons | PASS under clarified project acceptance989. Four recorded stage checks all within2s; elapsed86405.069s, final error[-1.684,-0.663]s. Early first-two sampling times and host capture gap disclosed; no exact-six-hour-spacing or continuous-log claim. Raw evidence988 unchanged. |
| 1.3.15 Watchdog | cmocka_driver_watchdog -r 0/1/2/3 | PASS1016 on999: original modes0→1→2→3 in order;0/1/2 watchdog panic+stack and real0x10 reboot, mode3 original4/4 PASS. Dedicated ROM delay measured500ms; fatal ISR clears without feeding. Original996 failure and1004/1008 partial UART sequences retained;1016 sends complete command frames. |
| 1.3.16 RNG | nist_sts 400000;all15tests;10streams | INCOMPLETE786 on781:162numeric P-values all>0.0001,zero starred rows;26 excursion rows unavailable (only1eligible stream).788 preserves stats:insufficient cycles,not heap exhaustion. Clock gating fix44795df21f4 resolves770's pervasive quality failure. Original algorithms/inputs unchanged; no full PASS inferred from unavailable statistics. |
| 1.3.17 Crypto | eight original cmocka_* crypto apps | Software backend PASS on754/759:3DES1,AES-CBC1/CTR1/XTS1,HMAC3,HASH4,CRC32 4 plusECDSA-P256keygen/sign/verify.757host-parser false negatives fixed and rerun759. 844 on810 also passes all eight original apps; AES-CBC executes28 hardware operations/640bytes/status0, other seven use software. Whole silicon crypto heading not counted as fully hardware verified. Offline998 target1001 passes all eight original functional apps, but SHA/HMAC completion logs show invalid status/byte values; runner rejects those hardware checks. Corrected offline1002 target1010 PASS all eight apps and supported hardware markers/status0. 3DES/MD5/CRC32 remain software. Original883 not flashed because automatic association was not disabled. |
| 3.1.1 12h standby | KASAN + resource monitor, unassociated12h | PASS864 on860:723 frozen records,43329.713s host REALTIME and43320s complete monitor periods; no faults/reset, free522728 bytes unchanged. See checkpoint864-standby12h/result.json and SHA256SUMS-xts864-standby12h. The early automatic label was withheld until actual12h completed. Kernel KASAN_GENERIC/INSTRUMENT_ALL and current SYSTEM_RESMONITOR/showinfo enabled; no NTP and wlan0DOWN. Uses internal FS allocator; process-private heaps excluded from kernel shadow registry. No user-process sanitizer or large-PSRAM-FS-heap KASAN coverage claimed. |

All35 common headings are now visible above. Later category-specific xTS cases
are outside this denominator; neither PASS nor N/A is inferred from custom
tests. BLE/Zigbee last, not removed from scope.

## Latest checkpoint

September15 19:52 +08:00, after the user's internet interruption: read-only
verification found longrun864 PID144782 alive, fresh UART resource samples,
191 resource records, exactly one initial ROM boot and no KASAN/assertion/
machine-trap/panic markers. Elapsed about3h9m; both timed cases remain RUNNING.
No reset or new serial connection was performed for this check. Next scheduled
time comparison remains22:43; do not infer24h accuracy from the initial sample.


Peripheral candidates886/887 built successfully while864 remained running.
GPIO exposes only J2 GPIO47/48, dynamic types, real IRQ counts and one-shot
level delivery; original poll timeout can otherwise falsely pass. Initial885
Kconfig dependency-loop failure retained, fixed in886. BMI160 binds external
I2C0 GPIO45/46 at0x68, legacy character interface matching the original test.
Necessary driver fixes propagate read/register errors and decode the complete
24-bit timestamp. Actual-function host checks pass ASAN/UBSAN; target NOTRUN.
`peripheral-test-guide.md` reserves September17 physical testing and documents
wiring plus guarded commands. `xts-peripheral.py` keeps original tests intact,
requires confirmed wiring, receipt/config validation and an unoccupied UART.
Common count stays25/35. Source/image archive: checkpoint888 and related sums.
Category-only network5.1.41/42 runners are prepared for100 up/down cycles and
100 unassociated scans after the longrun; old10-cycle/20-scan evidence is not
counted as these published cases. See network-case-next-steps.md. No network
source feature was added for this preparation.


883 completes the candidate's ECC-assisted keygen/sign path: optional P256
point hook, verify-then-multiply PIO, hardware constant-time flag, bounded1s
wait and full parameter/key register clearing. Scalar arithmetic remains in
the existing library; projective-blinded ECDH/other curves stay software.
Build883PASS; host884 actual adapter+library integration PASS in native and
no-int128 arithmetic,5point calls each, with signature/negative/ECDH checks.
OpenSSL substitutes point hardware only; no target PASS. Combined candidate
now expects CBC/CTR/XTS, SHA/HMAC, ECDSA verification and ECC point records in
the unchanged8-app batch. Use hardware-ecc runner after864 releases UART.
Archive883/SHA256SUMS-xts884/checkpoint884 retained; count remains25/35.


881 adds optional ECDSA P256 verification in separate ecdsa output. Ordinary
public coordinates/signature/digest are reversed into the locked LL's LE
format; no eFuse key selection/signing call. ECC/ECDSA clocks/lock, bounded
state waits and explicit invalid-signature status. Asymmetric selector now
prefers hardware over earlier-registered software and bounds the op index.
Build881PASS; host882 valid/invalid/length/timeout and actual-selector checks
PASS, ASAN/UBSAN clean. Keygen/sign remain corrected software. Target verification
still NOTRUN. Use hardware-ecdsa runner for the combined Crypto candidate after
864 releases UART. Archive881/SHA256SUMS-xts882/checkpoint882 retained.


879 adds optional HMAC-SHA1/256 via hardware SHA inner/outer digests, ordinary
volatile keys up to64bytes. Dedicated eFuse HMAC peripheral is not used.
Separate hmac output buildPASS; host880 imports6original HMAC vectors,
12single/split comparisonsPASS plus prior12SHA vectors/interleaving,26total
comparisons,63131 host compression calls. ASAN/UBSAN clean. Actual hardware
remains NOTRUN; MD5/3DES/ECDSA still software, long HMAC keys fall back.
Candidate includes878 ECC random-buffer fix; archive879/checkpoint880 and
SHA256SUMS-xts880 retained. Use hardware-hmac runner after864 releases UART.


878 rebuilds the SHA candidate with a necessary four-line ECC correctness
fix: arc4random_buf received NUM_ECC_DIGITS(4) rather than the full32-byte
private/nonce/blinding buffer. Host876 reproduces the incorrect4-byte request;
877 validates full buffers and P-256 operations with/without host int128.
Target original ECDSA remains pending on this corrected image. Use receipt878
for the current SHA output;873 archive retained. SHA and AES mode target
coverage still pending, count25/35 unchanged.


873 optional SHA1/SHA256/SHA512 PIO candidate buildPASS in separate
esp32s31-xts-sha output. Per-session aligned buffers and saved digest state
support the original streaming API; shared AES/SHA lock and bounded wait.
Host874 original9vectorsPASS;875 includes the configured600KiB vectors:
12original +2interleaved comparisonsPASS,63083 host compression operations,
ASAN/UBSAN clean. Host OpenSSL replaces peripheral compression only; target
SHA remains NOTRUN. Hardware runner requires original4/4HASH tests and each
SHA's million-byte/600KiB completion records. MD5/HMAC remain software.
Artifacts873 + SHA256SUMS-xts875 + checkpoint875 archived;864 undisturbed.
competition-demo.md now identifies paired810, entry commands and evidence.


871 adds optional hardware AES-CTR/XTS beside CBC in separate SMP/MMU demo
output.869 built mode logic; source review found crypto_newsession did not
retry parameter-incompatible drivers.871 retries remaining drivers only on
EINVAL/ENOTSUP, preserving hardware-only and resource errors. Host872 checks
the actual selector and actual mode code with original xTS vector tables:
CBC4/CTR6/XTS14,96 encrypt/decrypt single/stream comparisons,1568 host-ECB
calls;3AES192 vectors explicitly unsupported by hardware. Host ASAN/UBSAN
clean. OpenSSL replaces only the PIO primitive: not hardware evidence.
firmware871-aes-modes-build-only.tar.gz, SHA256SUMS-xts872 and checkpoint872
preserve candidate/config/source. Await864 before target verification.


868 watchdog candidate buildPASS and host868 verifies original mode1 uses
MINTTHRESH0xdf, preserving fatal level7.866 missed header declarations;
867 linked but disassembly found exported irq.h overwritten, leaving0xff.
868 fixes the canonical src/esp32s31/include/irq.h. No device flashed or
watchdog test run. Four-mode host runner syntaxPASS; no host reset allowed
between modes. Firmware868 and SHA256SUMS-xts868 archived as build-only.


865 separate xts-flat-flash buildPASS; no board access while864 runs.
/dev/xtsflash maps only0xc00000..0xcfffff. Registration performs no erase,
write or mount. backup-flash-scratch.py preserves two identical reads and
requires all0xff before the original test runner permits writes. Scripts
pass syntax checks. firmware865-flat-flash-build-only.tar.gz and its digest
are archived; hardware result still NOTRUN. No extra xTS PASS counted.


864 longrun is now actually RUNNING after860 build/861 paired flash and hash
verification.862 original memory8/8PASS,4.921s;863 original syscalls83/83PASS,
14.858s.864 started2026-09-15 16:43:12 +08:00, PID144782; host/raw UART and
live JSON use the xts864-longrun prefix. Preserve power, USB and this UART
session through Sep16 16:43. Original showinfo runs every60s; in BUILD_KERNEL
its mallinfo describes its own process heap, so explicit free/ps snapshots
also record kernel/page resources at the comparison checkpoints. Firmware860
is archived separately; SHA256SUMS-xts864 and checkpoint864 preserve hashes,
repository state, tracked delta and untracked port source. The common count
stays25/35 until actual long-duration results arrive.


844 hardware AES-CBC now verified after842 paired flash/hashPASS.843 attempted
while842 still owned UART and was rejected before any command;844 ran after
terminal flash success.28 hardware completions/640bytes, allstatus0. Seven
other crypto applications retain softwarePASS. Functional firmware837 and810
archived and hashed in SHA256SUMS-xts844; checkpoint844-xts.md gives commands.

845 flashes819 KASAN candidate;846 fails before Flash mapping because ASAN
hooks reside in Flash.847 moves runtime into IRAM and stops the retained KASAN
marker before BSS clear.849 now reaches allocator initialization but checker
recurses: CMake applies no-sanitize flags to mm but not the split kmm library.
850 hook-only exception builds; source review confirms allocator metadata
also requires the original Make/mm exceptions, so850 not flashed.851 applies
existing allocator/checker compile flags to kmm as well.853 reaches CPU1 but
fails while reading PSRAM FS-heap shadow metadata.854 changes only the
standby profile to its original SRAM filesystem allocator (FS_HEAPSIZE=0);
SMP/MMU/PSRAM process pages and full kernel instrumentation stay enabled.
856 then exposes syscall argument loss: explicit a0-a6 register variables
are clobbered by inserted checker calls.857 uses normal C ABI arguments;
host856/857 disassembly confirms preservation after the change.859 next
finds process-private heap shadows in the global kernel region list; another
CPU/address environment cannot access them.860 uses the existing nokasan
heap option only for BUILD_KERNEL process heaps. Kernel allocator checking
remains enabled; no claim of user-process sanitizer coverage. These attempts
did not count toward the longrun; all failed boot evidence remains.


Sep15 resumed with user priority: board-only adaptation first; external BMI160,
GPIO jumper and power-cycle fixtures deferred until later, no N/A inferred.
USB3-1 reattached and serial restored. NIST808 actually completed successfully:
100streams,188numeric rows, all threshold checks passed,5372.011s. This is
supplemental evidence; published10-stream786 stays incomplete.

820 flash816PASS but821 cannot boot;824/827/830/833 locate stall at the first
external heap metadata store. PMA candidate825 did not fix it. Datasheet
module table3 and existing production profile require Octal PSRAM; xts-flat
omitted ESPRESSIF_SPIRAM_USE_8LINE_MODE, selecting16-line HAL access.
834 adds8-line config;836 original mmPASS. All temporary boot/heap/trap
instrumentation and ineffective PMA change removed, preserved diagnostic836.
837 clean RTC/heap/block buildPASS;838 flash/hashPASS;839 mmPASS;840 RAM block
3/3PASS;841 RTC3/3PASS. Standard test sources unchanged. F0 dependency lock
stillPASS. Flash guard now supports explicit xts-flat-rtc and checks8-line
mode; both profiles retain the existing persistent partitions.


Sep15 WSL USB restored on BUSID3-1; device group access restored only for
/dev/ttyUSB0. Read-only755b confirms responsive NSH, Kmem225424free,
Page16080896free. Volatile NIST files no longer exist after intervening reset;
752 host log remains the evidence. No source reset or reference changes.

AppFS760 preflash guard blocked oversized3206144-byte image; no flash write.
Root cause: packaging stale applications from reused bin directory. Build762
uses generated current-target manifest, AppFS1343488bytes; stale source apps
preserved. Host tests761PASS, flash763and target764 bootPASS. NuttXe7a950e50d1
commits packaging fix and original NIST template asset integration only.

NIST770now complete and FAIL (see RNG row).774 raw128-byte sample has obvious
counter-like bytes. Source trace: S31 esp_perip_clk_init gates LP RNG after
bootloader_random_enable. Candidate devrandom/devurandom initialization now
reenables RNG once during later driver registration. Build781/flash783PASS;
784b short raw sample no longer shows the counter pattern.785 original syscall
regression83/83PASS.786 full original NIST finished:162numeric rows pass the
documented threshold;26 excursion rows unavailable,zero starred rows.788
captures original insufficient-cycle warnings before resetting. Clock fix
committed44795df21f4. No formal RNG PASS yet, no whitening or statistical test
parameter changes in786.787/791 C++ diagnostics captured790/793;794 fixes
orphan-section placement,795host layout0mismatches,797fullcxxtestPASS.
798helloxxPASS,799memory8/8PASS,800syscalls83/83PASS. Fix7e73da9864a committed
without temporary diagnostics.801/804build/flashPASS;805UART binary round-trip
PASS adds1.3.11. NuttX6c3a1973a61 addsprofile;appsac43243d4 fixeshostYMODEM.
806build/807flashPASS.808 is running an explicitly supplemental100-stream
NIST assessment since approximately12:07+08:00, exclusive UART, no reset
until completion/timeout. Published786 incomplete result remains unchanged.
809/810 hardware AES-CBC candidates buildPASS, not yet flashed or counted;
810 enables non-secret byte-count/status completion records for backend proof.
Separate FLAT original mm/block profile:811missingLIBC_EXECFUNCS dependency;
812hostguard incorrectly required absent BUILD_KERNEL negative line(fixed);
813rwbuffer needs workqueue(fixed by HPWORK);814linkPASS but post-link check
finds original test symbols absent. Cause: source-tree Make-generated builtin
headers override the CMake registry.815 tests compiling registry source beside
its build-local headers, preserving the old source-tree files.
815PASS verifies original mm/block are in linked image;appsfe038f6e4 commits
registry fix.816PASS adds actual HAL-reported free PSRAM interval to the FLAT
user heap using a target-only wrapper of the HAL's no-op registration hook.
Neither816 nor the RTC candidates have been flashed.817RTC buildPASS;
818 fixes32-bit time_t multiplication before converting alarm seconds to
microseconds and also buildsPASS. S31 alarm conversion/rdalarm now use fixed
UTC deadline; periodic support uses existing hardware HR timer service.
819 builds a separate SMP/MMU KASAN/resource-monitor candidate. No extra
headings counted until original target tests run; no reset of active808.

checkpoint744-xts.md records builds727-746, actual first errors, fixes,
commits and target results. Last verified driver firmware741 passes timer743
and UART744; firmware730 passes pipe/popen/MD5/footprint.750is the candidate
NIST stream-cap fix, not yet a verified RNG result. SHA256SUMS-xts730 covers
earlier recovery; newer incremental bundles nuttx/apps/tests-xts744 verified.

## Historical build sequence (superseded where noted above)

- build672: first actual blocker at configuration guard: TESTING_CMOCKA not
  enabled because LIBC_REGEX requires ALLOW_MIT_COMPONENTS. Added dependency
  in test-only profile, retaining local preexisting cmocka source; no fetch.
- build673: four-case batch reaches link; first real error ostest's
  rspinlock_t_test_thread undefined up_cpu_index. Do not just export privileged
  kernel CPU/IRQ operations to user ELF. Full log retained.
- build674: separate three-cmocka profile building. Required ostest profile
  retained as build-blocked; not excluded from acceptance denominator.
- build674 finishedFAIL: memory performance helper ELFs miss
  mmtest_get_rand_size. Added existing kernel/mm/common/test_mm_common.c to
  those two CMake targets, without altering test predicates.
- build675 completedexit0; allthreecmocka binaries verified inAppFS. flash676
  completedexit0 withhashchecks. xts677 actually invokedcmocka_mm_test and
  failedat riscv_createstack.c:161 before any subcase because TLS cap=8192.
- build678 raises CONFIG_TLS_LOG2_MAXSTACK from13to14 in testprofile only,
  preserving standard TESTS_TESTSUITES_STACKSIZE=16384. No assertion disabled.

All builds use `S31_DEMO_PROFILE=<profile> bash build-demo.sh <absolute-new-receipt>`;
full commands in logs/build672-xts-core.log, build673-xts-core.log,
build674-xts-core.log. Configuration and AppFS checks require real enabled
tests and installed binaries. Temporary filesystem /tmp is selected for
test file operations; exact case paths must be inspected before execution.

Historical checkpoint704: flash708exit0, syscall709exit0.
Memory710/scheduler711 original suites being rerun before saving fixes.
The remaining syscall exit assert is traced to task_uninit_info freeing
user-libc cache (getpwuid) via kernel lib_free/kmm_free. Candidate fix uses
group_free for these caches. Target validation709 nowPASS, no teardown assert.
700 increases NAME_MAX32to255, removes temporary scheduler error verbosity,
and adds original fstest/ramtest; assertions and all cases retained.
On690, getprime/scanf/hello PASS.
Firmware686 and690 archived before subsequent builds. New profile adds
documented syscall prerequisites and unmodified getprime, scanftest, hello.
Fixes committed separately: cee97921549 (failed pthread addrenv cleanup),
5a622b5838d (kernel-stack ABI alignment instead of user-TLS alignment).
Test profiles committed868da6f97f7. Bundle nuttx-xts690.bundle, WIP690 archive
and firmware678/683/686 all verified by SHA256SUMS-xts690.
Build696 tests a signal-context fix: restore S31 CLIC privilege/interrupt
return bits from the potentially edited saved xstatus, not a stale extension
slot. Not yet a target PASS. Another syscall blocker: close03 generates a
33-character filename but current NAME_MAX is32; test-only capacity needs
adjusting without shortening the original test's filenames.
Scheduler suites681/682 halted the target. Firmware678 is preserved in
firmware678-xts-core.tar.gz (SHA256 e93c1ebb2fa0ade0242105dbfe2da8aff090f9d1bfbe2ed5445aa884c79f0e80).
Complete previous
recovery image657 is archived in progress654-662.
Failed custom mqueue diagnostic663 remains uncommitted and disabled in normal
and xTS profiles. Keep its sources/logs, do not pursue it now. The interrupted
HTTP670 command produced no log/result and is not counted as PASS.

Test verdicts must inspect per-case output: cmocka_mm_test currently returns0
even if cmocka_run_group_tests reports failures. Exit status alone is not PASS.

## 2026-09-15 品类候选固件补齐（长测期间）

用户确认参赛对象为ESP32-S31 Function-CoreBoard-1开发板本身，按原计划/规格和板载能力核定品类范围。公共25/35不包含品类。

- 网络891：CountryCode getter修复，curl/FTP内核模式构建通过，待上板。
- 文件系统892：原始ROMFS/FATUTF8、压力/稳定性命令构建通过，初始使用RAM测试卷，待上板。
- KVDB895：服务端、UnQLite、完整30项cmocka配置及稳定性应用构建通过，隔离Flash测试卷尚未格式化。
- Sensor896：BMI160-uORB原始两topic订阅固件构建通过，外围实測待9月17日；普通字符接口887仍单独保留。
- 以上仅BUILD PASS，新增品类TARGET PASS为0。详细台账见xts-category-status.md，各编号checkpoint和哈希收据已保存。
- 原始长测864持续运行，20:50串口日志距离检查35秒，无异常复位或错误状态。

后续品类离线补齐：PWM898（单路GPIO48，待API+波形）；SCP901（原始双向传输应用）；
iperf2工具已在904合入网络应用候选（原始四方向300秒，未实测）；
文件系统905补独立Flash/LittleFS及三个原始掉电程序（待真实断电）。
以上均未新增TARGET PASS。各固件已独立归档并有SHA256收据。

September15 21:43: candidate907 filesystem throughput profile BUILD PASS ONLY; original dd statistics and priority255 enabled, isolated output preserves905. See checkpoint907-fs-perf-prepared.md. No target execution; longrun864 continues. Next unused908.

September15 21:53: candidate909 raw Flash dd mapping BUILD PASS ONLY, FTL+BCH /dev/xtsraw covers same1MiB scratch; no boot writes. Schedule after865 and before FS formatting. 908 rejected on BCH flag API mismatch, preserved. See checkpoint909-flash-raw-prepared.md. Next unused910.

September15 21:59: unified competition candidate911 BUILD/PACKAGING PASS ONLY; kernel1173468, AppFS2356224, existing904 network+demo with883 Crypto backends, no Crypto test apps. Isolated out/esp32s31-competition preserves810/883/904. No target run. Next unused912.

2026-09-15 22:47：864 首次 6h 单调计时采样已到；待机/时钟仍 RUNNING。发现 PC 单调计时与日历计时跨度不一致，最终时长和时钟验收需核查，详见 clock864-host-audit.md。原始日志不修改，板卡不重启。

September16 00:25 offline continuation: USB/ADB945 and BLE/bttool939 compiled and frozen, no category target PASS. Flash946 adds internal-stack dispatch for PSRAM callers; raw Flash/KVDB/persistent FS candidates are being refreshed before their target runs. Original864 still owns UART. Common PASS remains25/35.

September16 00:50: refreshed candidates946/947/948/949/950 all built and frozen with PSRAM-stack Flash fix. Audio951 includes original nxplayer/nxrecorder; ADC952 provides actual raw-sample lower half (no calibration), both built/frozen. Source checkpoint953 manifest56files PASS. Read-only observer PID832300/session14576 monitors864 logs and exits on new date sample/fault/process loss; it never opens UART. No active build; next unused954.

September16 independent RNG acceptance review:786 executed all requested
streams;162 numeric uniformity values passed (minimum0.000438), but26 excursion
rows have only1 eligible stream and undefined uniformity. The ten preserved
cycle counts in788 are398,248,10,47,139,109,1177,379,49,64; only1177 meets
the local J>=500 condition. assess.c filters the other streams and its integer
sampleSize/10 becomes0, producing ----. Do not convert this to whole-case PASS
or board-level N/A. NIST SP800-22 Rev1a section5.5 footnote11 also states the
uniformity value is undefined below10 effective sequences; section4.2.2 calls
for at least55 for meaningful analysis. Source: https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-22r1a.pdf .
The original400000-bit/10-stream xTS workload is unchanged;808 remains
explicit supplemental evidence. No extra rerun or driver change is warranted
solely to alter this statistical label. Counts remain25/35.

September16 04:13 timing checkpoint:864 sample2 error is[-0.085811,+0.933120]s,
still within2s. Its automatic standby PASS label occurred after only41401s
REALTIME/41400s complete monitor periods; formal standby stays RUNNING until
43200s actual observation. Original process and showinfo continue unchanged.
No common PASS added. Fresh read-only observer session10148 uses
clock864-observer-0413.jsonl and stops for review on actual12h readiness or the
next date sample.

September16 04:46 actual standby acceptance: original3.1.1 PASS; frozen723
records cover43329.713s host REALTIME and43320s monitor periods with zero
faults. Same864 process remains alive for1.3.14. Image/config hashes, frozen
manifest and standalone evidence archive verified. Common count now26/35;
category formal PASS remains0. Prior timestamped counts are historical.

### 2026-09-16 05:08 — audio968 BUILD ONLY

原始音频默认44.1kHz已完成时钟适配：ES8311按I2S采样率选择MCLK，S31新增11.2896MHz分频，旧48k路径保留。完整构建通过，442116字节；checkpoint968-flat-audio、firmware968-flat-audio-build-only.tar.gz及SHA256SUMS-xts968已保存并验证。963镜像冻结可回退。无上板新增PASS，通用26/35、品类0；864继续。USB965的现有UART刷写入口已补齐并做语法/摘要核验，未刷板。
