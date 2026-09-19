# S31 检查点（2026-09-11 00:05 后持续更新）

主工程 `openvela-dev/nuttx`，分支 `codex/esp32s31-port`。
最新提交 `428252a08db`（S31内核Wi-Fi锁/共存内存释放回调配对，附主机测试）。
前一提交 `60affe12dad`（按HAL配置收集三组Wi-Fi IRAM段，附地图检查）。
前一提交 `5040fcd1f7c`（S31非lazy CLEAN浮点上下文保存，附4项主机测试）。
前一提交 `19415d5f28e`（修复 CMake 配置往返切换后旧config.h残留，附回归测试）。
前一提交 `66a0aa00a31`（记录 IDF 实板隔离对照及未解决边界，仅文档）。
最新功能提交 `6e1020517c2`（S31 Wi-Fi 时钟管理初始化状态同步及生命周期测试）。

最新备份已完成并通过 `SHA256SUMS-idf180` 全部校验：
`idf-isolation170-183-checkpoint.tar.gz`（26 MiB，诊断源码、IDF ELF/镜像、
完整读回备份及日志）、`firmware180-restored-baseline.tar.gz`、
`esp32s31-port-6e102-to-66a0.bundle` 和 `s31-candidate180-diagnostics.patch`。
bundle 已验证，依赖已保存的 6e102 基线链。首次用裸提交范围创建 bundle
被 Git 拒绝为 empty bundle，没有覆盖文件；改为检查固定 HEAD 后通过
命名引用创建，已有诊断补丁先比较一致再复用。失败/重试日志都保留。
`e170f61efa5`：S31 STA IPv4 地址通知及生命周期测试。
`2b025058fc6`：默认开启 HE 的 S31 STA 协议兼容开关。
`a55bd00931c`：ping 验收必须匹配完整收发数量和精确零丢包。
`a36279b0f07`：RX 聚合窗口使用 RX 配置并支持关闭聚合。
`467b19c840f`：接收队列释放节点期间禁止抢占，解锁后恢复。
`67a039da9e0`：接收入队和出队使用同一把锁。
`04de89b5e50`：动态 TX 模式不向 HAL 请求静态 TX 缓冲池。
`c98b8d47184`：避免 IP 清零隐式触发 DHCP，并拒绝残留 IP 假阳性。
`6239374d8df`：按逻辑使能状态重新使能 S31 Wi-Fi 输入 1。
`37c5b65ec70`：PHY 校准前恢复 S31 模拟总线时钟。
`d1935091989`：跳过无名称用户堆的 procfs 注册，NULL 注销安全，附主机测试。
`4acc64709fd`：板级 Make 操作说明。
`172ecc974e3` 修复 ELF 查找无名称符号时的空指针异常；实板 Make 启动通过。
`6867fb3d7f1`：S31 Make 无线源码、头文件优先级、
Flash 私有头文件路径与现有 CMake 对齐；本地锁定 HAL 不再运行
submodule update/reset/重复补丁步骤。其他芯片继续使用旧 Wireless.mk。

此前本轮提交：

- `b214fe469e9`：慢扫描返回 EAGAIN，允许 WAPI 重试。
- `d877b922dda`：STA 配置失败释放锁，连接超时报 ETIMEDOUT，断开日志只记录原因/RSSI。
- `3a2a6817aae`：S31 异常入口先加载芯片能力，确保选择 CLIC 12 位中断掩码。

## 实板状态与尚未通过项

### 2026-09-14：持续完整适配，291 起的新验证

用户确认蓝牙/Zigbee仅后置，不删除范围。持续目标已建立。
只读核验N HEAD42267bfd793，原有6份诊断修改及3个隔离配置保留，未重置。

更正此前过粗HE摘要：267三轮首组ping分别0/4、1/4、0/4，但第二组均4/4；
因此严格网关验收三轮失败不等于HE关联后始终完全不通。三轮RX统计仅legacy，
没有HE数据帧，仍未达到HE验收。不能把第二组成功当作Wi-Fi6收发通过。

291离线HE CMake编译退出0，配置/头文件与协议71检查通过；292配对烧录
退出0、哈希验证通过。烧录保护升级为核验273双份[0,0x500000)备份，
允许只读AppFS种子完整3MiB范围，仍不写0x500000起分区。
293三轮tcp-diag：整体[1,1,0]；TCP结果失败/成功/成功，RX只有legacy。
294同固件锁定授权SSID的较强AP 60:ce:41:ab:02:d0（约-63dBm、ch11），
三轮均TCP连接超时、整体[1,1,1]，仍无HE数据帧。未改路由器或Clash。
295配对恢复284 demo退出0且写入hash校验通过；296重新连接并启动网页，
网址192.168.1.60:8080。284固件可从290检查点恢复。

开始GPIO/BOOT小组：官方板卡指南确认BOOT GPIO61，保持输入、不驱动绑带。
审计发现公共GPIO IRQ位图右移时丢失位偏移，并有bit31右移32风险；仅低
32位被处理，无法接收GPIO61。297旧代码负例失败，298修正后的真实ISR
模型在22/31/32/62引脚规模通过空/单点/全部双点组合/全位测试，UBSan通过。
S31明确选择锁定HAL的GPIO_INTR0输出，新增高32位分发；按键主机测试300
通过极性、输入配置、非法ID、attach失败和detach，不替代实板按键验收。
299第一次编译退出0，但漏INPUT总开关导致lower/buttons应用没有纳入，未烧录；
补CONFIG_INPUT=y，并为构建脚本增加选项与AppFS应用存在性保护。
299b首个真实编译错误是Apps buttons_main.c调用用户ELF不可用的task_create；
内核构建改为在自身进程运行（NSH可用buttons &），平坦模式保留原行为。
299c编译及1635布尔/协议7核验通过，302配对烧录退出0、哈希验证通过。
301主机入口测试覆盖kernel/flat行为，300板级测试通过；旧HAL头有HYS
capability未定义警告，不修改参考仓库。自有新增宏参数括号警告已修正。

303实板首次BOOT输入测试失败：应用卡在read-drain循环，没有状态输出；
当前button_upper.c的read永远返回当前状态而不是EAGAIN，所以旧示例会
忙循环。修正为每次poll通知仅读一次，处理EINTR/EAGAIN及短读/错误事件。
304 CMake通过，305主机poll测试通过；306成对烧录和hash验证通过。
307实板通过：按官方原理图第2页auto-download真值表，pyserial RTS=False
保持EN高，只切换DTR以产生BOOT GPIO61物理电平变化；5次按下/释放共10
边沿全部poll通知正确，无复位。最终释放DTR，kill本次buttons进程。没有
驱动SoC绑带GPIO输出；此结果不覆盖手动按键机械触点或其他GPIO引脚。
308仅调整Apps helper结构体初始化的nxstyle格式，CMake退出0；309配对
烧录及hash通过，310再次通过BOOT物理电平5周期/10边沿测试。311既有
主机回归与F0依赖核验通过。GPIO/按键提交N f839d5b7dbe、7336ef1a060，
Apps 59b05cd92；旧诊断修改保留。

312网络回归失败：bringup运行在CPU1，wapi scan在30秒窗口未完成，
清理后复位，网页没有保持运行。因此308不标记为稳定demo，284仍是
归档恢复基线。新线索：esp_route_intr配置当前CPU的CLIC，却固定将
矩阵路由到CPU0；包括可能在CPU1初始化的HR timer。这是源码缺陷，
尚不能直接认定为所有HE失败的根因（既往CPU0-only也有HE失败）。

313小组：S31矩阵路由使用this_cpu；S-mode teardown清除两个核的
该源路由，保留其他寄存器字段。新增实际函数提取测试覆盖双核、32个
输入、两种触发和7级优先级；7336旧代码负例在CPU1断言失败，新代码
通过。双矩阵169源unmap及其他芯片路径测试通过。测试初版辅助函数
priority名称与实参冲突导致编译失败，改名set_priority后通过。
313离线CMake编译退出0，完整命令见build313-demo-irq-route.log；
这组修正不代表跨核enable/disable所有权问题已经解决，实板回归待验。

314烧录313配对镜像退出0，两段hash通过；315在CPU0初始化的实板扫描、
DHCP、网关4/4通过，316网页10次及边界通过。317三轮独立双扫描均通过，
DHCP和两组网关ping共24/24通过；TCP4096结果失败/成功/失败，失败均为
Windows客户端3秒connect超时，板端已经listen。未把此结果当作全部网络
通过，未改客户端超时或网关验收阈值。各轮初始化均CPU0。

318新增demo-cpu1-init诊断配置（include demo，仅默认CPUSET=0x2；
Wi-Fi worker仍显式CPU0），离线CMake通过；构建313配对镜像已保存到
firmware313-demo-irq-route.tar.gz。319配对烧录退出0、hash通过。
320日志确认AppFS/板级初始化实际CPU1，扫描、DHCP和网关4/4通过，
网页启动成功。这是CPU1场景的首次定向通过，不等于HE或长期稳定验收。
321同一CPU1固件网页连续50次及边界通过；322按键5周期/10边沿通过。
323三轮CPU1初始化：每轮主动/被动扫描、DHCP、两组4包网关ping全部
通过（24/24），整体[0,0,0]；这一批不含TCP echo，不能覆盖317的失败。
IRQ路由和定向配置已单独提交d468c494ff8。324恢复普通demo默认双核
CPUSET=0x3离线编译通过；318 CPU1诊断配对固件另存归档。325检查点
保存新提交bundle、未提交诊断补丁、313/318/324镜像及全部本轮日志。

325归档SHA全部通过。326恢复324配对镜像hash通过；328普通demo重新
扫描、DHCP、网关和启动网页通过。327修复公共I2C频率请求被默认值
替代的错误，主机实际helper通过、旧版本负例失败，提交9e8cae06cf5。

I2C组：新增demo-i2c隔离配置，GPIO50/51连接板载ES8311控制总线。
329首次CMake失败，首错esp_i2c.c中旧i2c_ll_write_cmd_reg不存在；另有
XTAL_CLK_FREQ、filter、clr_bus签名差异。按锁定HAL逐项接入新命令/
滤波接口、esp_clk_xtal_freq、RCC原子时钟/复位、新中断源和pin matrix，
补齐S31 Make/CMake HAL I2C源文件。330编译退出0，1651布尔/协议7通过。
332配对烧录退出0/hash通过；333在100k/400k/100k请求下，0x18的FD/FE
共6次均读出83/11。原理图CE下拉与ES8311资料确认地址，Linux codec
头文件对照ID；没有写Codec配置、驱动PA或开启音频。实际SCL波形未测。
Windows已有pypdfium2成功渲染官方原理图第三页为schematic331-audio.png；
Linux无PDF库，Windows无fitz但有PDFium，未安装任何依赖。

334主机命令位测试初版使用GCC11不支持的自动变量填充值选项而失败，
该失败不算旧代码负例。334b改为在测试fixture内仅对命令变量声明注入
确定性初值，实际helper旧代码在restart寄存器值断言失败，新代码通过；
验证ACK预期位、DONE/保留位清零及最后一字节NACK。335将S31恢复总线
等待放在全局IRQ临界区之外、用传输mutex互斥并传播超时，CMake退出0；
336实际reset/clear helper模型通过完成、10ms上限、失败关闭和锁失败。

337配对烧录通过；338显示96次ID正确及3次恢复，但复审发现333/338脚本
漏判NSH参数数量超限提示，因此不算无错误验收。342与网页并行时还出现
一次400k传输失败。已改用紧凑选项、限制argc<=7并拒绝nsh错误；343在
不复位且网页进程存活时，3次恢复/96次读取无提示无错误通过。339扫描/
DHCP/网关及网页启动通过，340网页50次和边界通过；341既有主机/F0通过。
board bringup全文件nxstyle仍有旧节标题/旧注释/for格式问题，不宣称全文件
风格零错误；本组esp_i2c.c风格与diff检查通过。

344增加I2C错误日志、修正Apps ioctl返回-1丢失真实errno；CMake退出0。
345实际Apps helper测试通过成功/陈旧errno/ETIMEDOUT/EIO。Apps本地未配
Git身份导致首次提交失败；不改全局设置，后续使用既有提交身份Codex
<codex@local>作为单次命令参数重试。346烧录用于确认342具体中断错误位。
Apps错误码修正提交e498e0ec6。346配对烧录退出0/hash通过；347重新
扫描、DHCP、网关、网页启动通过；348网页100次/边界通过。349紧凑命令
并行测试首轮在100k读取FE时复现Message 0 timed out，真实errno110
（软件等待500ms），因此不能直接把先前400k失败归因为SCL硬件超时。
350实际ISR主机测试发现status=0仍推进状态机；旧代码负例失败，新防护
通过。只改零状态中断处理并增加S31超时raw/ena/state日志，未改超时
阈值；350CMake退出0，351计划配对烧录作单项对照。

后续源码审计：锁定I2C HAL默认timing会设置约10个总线周期的硬件SCL
等待阈值，IDF master另外设置2500us默认值；当前NuttX尚未接入该配置。
这是待确认接口差异，不把它当作349软件超时已证实的根因。超时之后的
IRQ/信号量清理和初始化/enable之间的跨核迁移也需进一步验证。

### 2026-09-14：基础 demo 优先，暂缓 Bluetooth/Zigbee

最新N HEAD为42267bfd793（仅S31保留ELF运行时元数据），Apps HEAD为
118d5ff0e（网页请求边界实板测试），前一Apps提交39e68c334为demo本体。
下文272e已被284替代为当前板上固件，旧配对镜像保存在281检查点。

281检查点progress272-281-demo-checkpoint.tar.gz（4.8MiB）及配对固件、
两个Git增量bundle SHA256验证通过。282旧272e+显式8KiB栈连续100次
状态读取、POST405、路径404、1024字节浏览器头、超长头拒绝、5秒空闲
连接超时后恢复全部通过。Apps边界测试单独提交118d5ff0e。

283元数据负例：旧bin_debug/s31demo含stack8192/heap65536/priority100，
bin/s31demo缺全部元数据，测试按预期退出1。修正S31 Make/CMake strip
保留6个nx_*运行时符号，未更改其他芯片或ELF加载器。284离线CMake编译
退出0，1602布尔/协议7核验通过，新AppFS605184字节，元数据对照通过。
285读取真实Make规则、对真实ELF执行strip并验证保留值，非S31分支参数
不变。这里不是完整demo Make实板验收，不扩大结论。
286配对烧录退出0、双镜像写入hash验证通过，不触碰0x500000起可写分区。
287不带prlimit启动s31demo，实板默认栈8128字节、优先级100，确认根因
修正生效；DHCP及网关4/4通过。当前网址仍为http://192.168.1.60:8080/。
288新284固件连续50次HTTP状态读取及全部请求边界/超时恢复通过，测试
退出0后保持demo运行，没有再打开串口或复位板子。
289原主机汇总及F0锁定依赖验证通过（协议单测仍显式选6e102基线，不
覆盖未提交的统计诊断）。Wi-Fi6/HE仍失败，未把demo通过当作完整认证。

本轮开始时主分支/HEAD为428252a08db，无新合并操作；用户要求优先交付基础demo。
新增未提交的Apps examples/s31demo和N demo配置/启动工具，网页只读：
IPv4、运行时长、HTTP请求数，无外部资源/凭据存储/命令或文件写接口。
demo明确关闭HE，保留SMP/MMU/PSRAM/AppFS和NSH；蓝牙/Zigbee未启用。
272编译本身成功，但构建脚本误查apps/bin/s31demo而退出1；实际路径
是bin/s31demo且AppFS已包含应用。修正路径后272b通过。272c扩大HTTP头
到2047字节并给网页应用8KiB栈，以覆盖普通浏览器请求头；272d/e仅规范
C格式，最终272e编译/1602布尔配置及ELF协议实参7检查通过。
主机网页回归、nxstyle和diff检查通过。274成对烧录272e退出0，两镜像
写入哈希验证通过。275启动/DHCP/网关4/4通过，276 Windows直连HTTP
10次JSON和首页全部通过（期间没有调整Clash）。

随后发现AppFS strip移除了nx_stacksize，实际ps栈1984字节而非配置8192。
bin_debug中nx_stacksize=0x2000，bin中无符号。采用应用级显式启动
`prlimit -s 8192 s31demo &`，不扩展修改公共ELF加载器。277尝试仅重启
已知PID时读到POWERON启动输出，未找到目标任务，按保护条件退出1，
没有执行kill；不能确认是串口打开复位还是此前缓存输出。278用修正工具
重新启动、网关4/4通过，ps确认s31demo可用栈8128字节。启动策略/凭据
脱敏3项单测通过。279 Windows直连首页及50次HTTP连续读取全部通过，
运行时长递增，无复位；280主机HTTP及3项启动策略测试通过。
当前固件仍为272e，网址http://192.168.1.60:8080/，重启后需重新联网，
不保存密码。N demo配置/工具/文档已提交71ecca5e283，保留未提交诊断。
Apps首次提交因本地Git身份缺失失败，沿用历史Codex <codex@local>，仅
对单次提交传递配置，不修改用户全局Git配置。

273读回Flash [0,0x500000) 两份，各5242880字节，cmp一致，SHA256均为
7eabae1c66fef40d165b627d6869f1f2bc70f618e7baae3e7dc9376d8db577e1。
保存demo-before272/{before-a.bin,before-b.bin,SHA256SUMS}。因为demo
AppFS为601088字节，旧544768字节备份上限不够，此次先完整备份再写，
新版烧录保护允许已备份的3MiB只读种子区域，不触碰0x500000起的分区。
USB CP2102N身份与MAC匹配，BUSID2-1 Shared恢复WSL附加；新节点root:
dialout导致一次Permission denied，只修改该临时节点属主regex以恢复访问。

Clash核查：Windows存在Meta 198.18.0.1/30、默认路由到198.18.0.2，
但Find-NetRoute到192.168.1.60选择WLAN/192.168.1.0/24直连，源地址
192.168.1.29；开发板到网关192.168.1.1的ping不经过电脑。暂未关闭或
修改TUN/代理/路由器，不能把HE故障归因于TUN，也不泛化为代理绝不影响
电脑侧测试。276直连HTTP已通过，未改变TUN状态。

267 HE计数器三轮均网关失败，清理完成。第一轮最大延后17704075周期，
第二/三轮分别120602/196133周期，均无ROM浮点异常；只能作为时延线索，
并非HE根因证明。264 bgn计数器三轮完整联网均通过。
269原主机汇总/F0通过，270最新最小NSH Make通过，未烧录最小镜像。
268完整检查点已补保存并SHA256校验通过，覆盖demo新增代码之前。

### 最新：ROM 浮点非法指令复现（优先于下方历史状态）

S31 CLEAN保存修复已提交5040，IRAM布局已提交60af。
260在257上三轮完整联网[0,0,0]，261三启动/三radio烟测通过（不写存储）。
堆回调修正已单独提交4282，bundle `esp32s31-port-60af-to-4282.bundle`
已验证。提交只包含两个释放回调、主机测试和英文说明，没有提交诊断统计。
当前板上262（bgn+只读中断延后计数）及配对AppFS，263写入退出0、
哈希验证通过；264三轮完整联网进行中。257和262均已成对归档。
265同源码HE构建进行中，不得与板上262混淆。
245写入退出0，内核/AppFS写入哈希验证通过；246三轮HE结果[1,1,1]。
每轮协议71/PHY6及DHCP通过，但网关0/8；未见ROM浮点异常。FPU修复
不能称为HE修复，且这些轮次未进入TCP/UDP步骤。
241编译和烧录前均通过1601布尔配置/头文件检查、ELF协议实参检查，值7。
242三轮联网[0,0,0]，每轮均确认实板协议7/PHY4，DHCP、网关8/8、DNS、
TCP4096字节和UDP256x96字节内容/序列全过，未见ROM浮点异常。
244同板241长ping三轮[0,0,0]，174/174网关ping，无ROM浮点异常。
243同源码仅启用HE编译已通过1601布尔及ELF协议71检查，镜像/头文件
已成对归档；245已烧录243，246作HE完整联网对照。
248原主机汇总/F0依赖锁验证通过；新FPU4项与CMake配置往返测试通过。
FPU负例明确选择19415，CLEAN新帧及迁移帧两项按预期失败；不把诊断
统计代码计入原协议单测（该项仍明确选择6e102）。
19415提交已保存并验证 `esp32s31-port-66a0-to-19415.bundle`。
5040提交已保存并验证 `esp32s31-port-19415-to-5040.bundle`。
249重新引入215的三组Wi-Fi IRAM收集（N/M），这次基于5040 FPU修复；
仅链接布局变化，HE配置不变。243旧地图负例在Flash的wifislprxiram处
按预期失败；249编译和新地图检查通过，250成对写入校验退出0。
251 HE三轮[1,1,1]，DHCP通过/网关丢包，无ROM浮点异常；不是HE修复。
252 Make编译和地图检查通过。253同源码回切bgn构建成功，协议实参7，
254烧录退出0，255三轮完整联网[0,0,0]，无浮点异常；布局已提交60af，
bundle `esp32s31-port-5040-to-60af.bundle` 已验证。
247检查点已保存该未提交候选及243/241已归档基线，SHA256校验通过，
包含248/249主机结果但不包含249固件与250以后的日志。

256静态接口检查找到两处内核分配/任务堆free错配：Wi-Fi的
_spin_lock_delete和coex的_free，均对应kmm_malloc。当前free通过
umm_getheap使用task_get_info()->ta_heap；不能假设所有调用来自kthread。
参考R两项均经wrapper调用kmm_free。本次仅S31 BUILD_KERNEL选用kmm_free，
保持其他chip/flat绑定不变。主机实际表预处理4项：旧代码2项失败、候选
全过。257 CMake、258 Make通过；不能称此接口修正已解释HE丢包。

262只在N加入监视器延后计数，M保持已验证功能代码。仅测CLIC输入1
在M-mode被延后到重新交给S-mode的周期，不是外设引脚到ISR的总时延。
读取mcycle低32位，固定320MHz；样本/总和为32位会回绕，直达中断不计。
三个SMP合成重放路径均采样，宏只在t0-t2已无活跃值处插入，不改返回
状态/使能策略。262编译成功，计数器64字节，诊断补丁未提交。

重要纠正：236日志虽命名bgn，但三轮实际bitmap71、PHY6（HE），结果
[1,1,1]为HE网关丢包，不能算bgn回归，未见FPU异常。234的.config是bgn，
但当时使用了旧HE头文件/目标文件。240定位CMake的.config.prev只初建
而不在生成新头后更新；A->B->A会错误跳过生成。主机真实CMake复现
旧代码保留旧头/重复重写，修正后off/on/off/on及无变化快路径全过。
resetconfig现在像menuconfig一样失效生成头；仅删除可再生成的构建头，
不删除配置、源码或仓库。241已完成真实HE->bgn切换并验证ELF实参7。
232/234以及235的协议分类不得仅靠归档.config推断；224的扫描回退
与234扫描恢复不能当作已控制相同协议的对比。242/244已重测真实bgn。

历史build234及其成对AppFS：只将S31非lazy保存阈值从DIRTY降为
CLEAN，保留INITIAL/OFF跳过、原有标记CLEAN和全部恢复路径。
234编译、烧录校验退出0；主机4项通过，HEAD负例在CLEAN新帧和迁移帧
失败，unused/其他芯片/lazy策略保持不变。scan235主动2.7秒/被动5.44秒
及定时器/ifdown全部完成。236三轮bgn完整联网进行中；237生产Make、
238最小NSH Make编译退出0，无真实编译错误。M已同步为234仅CLEAN阈值。
239仅打开HE的候选编译退出0，尚未烧录；234原bgn镜像已归档。
224主机测试曾要求初始化时恢复全零寄存器；现在不再宣称修复初始化策略，
该用例改为确认保持原行为。缩小范围不是把234当成完整FPU验收。

232仅保留完整保存、恢复回原路径，编译和烧录通过，但233扫描仍超时，
说明仅撤回INITIAL恢复不足。当时M仍是224完整候选（227编译成功）；
现已在237同步234，没有烧录M。各二进制具体配置必须以receipt/归档为准。

224及其成对AppFS：原180配置/链接布局，新增仅针对S31
非lazy FPU的完整保存/恢复候选；保留220故障寄存器诊断。CMake224和
Make227编译退出0；主机控制流测试4项通过，HEAD负例前三项失败。
223延长对照结果[1,0,0]：第1轮复现ROM故障，hart=0，
mstatus=000448a0（FS=CLEAN而非关闭），FCSR=000000c8（FRM=6保留值）。
第2/3轮各50+8网关ping通过，不得合并称为稳定。

当前openvela的riscv_savefpu会跳过CLEAN非lazy帧，但该模式的FPU存储
位于新建的栈帧，不是lazy模式的持久TCB区域。新帧未填充就被恢复，
可读入垃圾FCSR/浮点寄存器；restore还跳过已清零的INITIAL帧。
参考实现的两种跳过优化只用于lazy，非lazy始终复制。224只对S31非lazy
采用完整保存/恢复，不改其他芯片和lazy行为。主机脚本解释真实汇编的
整数分支和数组复制，覆盖CLEAN新帧、INITIAL恢复、迁移帧和旧路径隔离；
它不是硬件FPU模拟器。ELF反汇编也确认224不再跳过复制。
225三启动/三radio烟测通过，228原主机汇总/F0通过，229最小NSH Make
编译通过。但226三轮bgn全部在主动扫描阶段超时[1,1,1]，没有有效关联，
不得称为浮点修复完成。231后台扫描诊断：NSH/sleep/ps仍可运行，wifi
线程在MQ empty，lpwork/hr_timer在正常等待，wapi等待g_scan_priv+4的
scan_signal；不是全核挂死，没有观察到FPU异常。此候选保存
`fpu224-eager-both-candidate.patch`、`firmware224-eager-fpu-bgn.tar.gz`。
230同候选仅打开HE编译通过，但因扫描回退未烧录，保存
`firmware230-eager-fpu-he-unflashed.tar.gz`。HE仍待后续验证。
最新已校验完整检查点为 `progress184-222-checkpoint.tar.gz`（9.9MiB），
仅覆盖到220/221及之前；223以后的FPU候选/日志尚需下一次检查点。

历史220及其成对AppFS：原180配置/链接布局，仅增强致命异常
日志，记录hart、mstatus、fcsr（FS关闭时不读取FCSR，以ffffffff占位）。
CMake220编译、flash220写入校验退出0；network221三轮[0,0,0]，全部
DHCP/GW/DNS/TCP/UDP通过，未复现故障。220只有故障日志增强，不得称为
修复；继续223五组十包ping/每轮，共三轮的较长对照以捕获异常现场。

219 IRAM候选联网结果[1,1,0]。前两轮异常与原180的214第3轮、169第2轮、
211第2/3轮完全相同：mepc=2f83a752、mtval=d01676d3、ra=2f83a8dc，
mcause低位2。真实失败是ROM非法指令，不只是丢包/应用超时。
本地工具链反汇编 `trapped-instruction.S` 确认该字为
`fcvt.s.wu fa3,a2`（动态舍入）。当前已启用ARCH_FPU、未启用LAZYFPU，
两核均调用riscv_fpuconfig；FPU状态/上下文是待验证线索，不能先断言根因。
215链接候选已归档 `iram215-candidate.patch` 和
`firmware215-iram-candidate.tar.gz`，从N/M撤回，不与FPU诊断混测。
新链接检查脚本仍保留未跟踪；目前旧布局运行它会失败，这是预期。
218原主机测试汇总/F0锁验证均通过，协议测试明确使用6e102。

### 历史：IRAM 布局单项对照

当前板上为 build215 内核及其成对 AppFS，b/g/n、SMP cpuset=3，配置与180
逐字节相同。CMake215/Make217 编译退出0，无首个真实编译错误；链接测试
确认 RX/sleep 共享段3796字节、extra段676字节进入内部SRAM。旧180地图
负例被正确拒绝（0x400b17d8处仍在Flash）；解析器3项测试通过。
production smoke216 三次启动、三轮radio启停通过。network219已结束，见上。
该变化尚未证明解决HE，尚未提交。生产输出目录B仍保留180，不要覆盖。

- 208（CPU0/HE + 三项休眠默认值）结果[1,1,1]，未修复HE。
- 209（原SMP/bgn配置 + 三项休眠默认值）编译成功，210烟测通过；
  211联网[0,1,1]，后两轮ping命令超时。212 Make编译成功。
- 已归档 `firmware209-sleep-defaults-candidate.tar.gz` 和
  `sleep-defaults205-candidate.patch`，撤回N/M该15行候选及N测试修改。
- 213恢复180内核及原AppFS全部受影响扇区，退出0，读回逐字节一致；
  `appfs-after213-restore.bin` SHA256同下方初始备份。
- 214原180同环境bgn联网结果[0,0,1]，前两轮DHCP/GW/DNS/TCP/UDP全过，
  第三轮DHCP后ping命令及ifdown超时，硬复位清理通过。不能把211失败
  独断归咎于休眠默认值，也不能把214前两轮当作长期稳定。
- 215仅修改S31链接脚本，收集sdkconfig桥接已启用IRAM/RX_IRAM/EXTRA_IRAM
  所需的wifiorslpiram/wifislprxiram/wifiextrairam；sleep-only段仍在Flash。
  两种构建布局测试通过；`test_esp32s31_wifi_iram.py`不是无线功能测试。
- 完整构建命令分别为 `bash build-regression-wifi.sh <build215绝对receipt>`
  和 `bash build-production-make.sh`；输出在 `logs/build215-iram.log`、
  `logs/build217-make-iram.log`。烧录使用 `flash-regression-pair.sh` 检查
  receipt及写入长度，只写已备份的内核/AppFS区域。0x500000起未写。

### 历史：M-mode / CPU0 隔离诊断

生产构建目录仍保存 build180；平坦模式对照已结束，flash196 恢复180，
smoke197 通过。随后CPU0对照使用成对198b内核/AppFS，202已恢复180和
原AppFS全部受影响扇区，读回逐字节一致，smoke204通过。其后临时运行
CPU0/HE build206（仅增加三项HAL时序初始化），network208已结束，见上。
0x500000 起的可写分区始终未写入。
恢复必须使用 `bash restore-production180-pair.sh <new-absolute-readback>`，
恢复内核及AppFS，不能只恢复内核；确认无串口进程后运行并等待退出0，
再重跑 production smoke。原AppFS备份 `appfs-before198.bin` 覆盖
0x200000..0x284fff，与初始5MiB备份对应范围逐字节一致；SHA256为
`0d9ac9f69bc7687814f7a2374744236b89b9539e334a4017b8e1e90c8733cd5c`。

- 新建独立输出 `openvela-dev/out/esp32s31-cmake-wifi-flat184`，未初始化或
  下载仓库。临时 `wifi-flat` 配置不启用 MMU/SMP/PSRAM/AppFS，开启 HE。
- 每次构建命令：`bash build-flat-wifi.sh <absolute-new-receipt>`，完整展开
  命令及输出在 `logs/flat184*-build.log`、`flat186-build.log`、`flat188-build.log`。
- 184 首个真实错误为 task TLS 三接口隐式声明；配置补 `TLS_TASK_NELEM=4`，
  与当前 C6 Wi-Fi 配置一致。184b 因 CMake 未刷新现有配置仍同样失败；
  仅对独立诊断输出执行 resetconfig 后，184c 编译/镜像生成退出 0。
- 185 自动联网在 Wi-Fi start 后无 NSH；186 禁用 NSH 自动联网，编译退出0，
  187 NSH/ps/free/ifconfig/sleep 通过，ifup 返回后控制台停滞，未开始关联。
- 188 为 M-mode 增加 CLIC mcause 上下文保存候选（S-mode 分支不变），
  编译退出0；189 仍在启用网卡附近失去响应，不能称为修复。
- 190 的 CLIC 阈值临界区候选编译退出0，191 仍在 ifup 附近停滞；
  已保存 `s31-flat190-clic-candidate.patch` 并撤回全部4文件候选。
- 192 仅对照使用现有逐线程 Wi-Fi IPC 信号量，编译退出0，193 未恢复
  网卡启停，已撤回2处候选。194 仅对照使用现有 IRAM libc 替代 ROM，
  编译退出0，195 仍停滞，已撤回 CMake 候选。上述均未进行有效 HE 关联，
  不能用于排除/确认生产 HE 的 SMP/MMU 根因，也不能视作候选修复。
- 跟踪源码重新只剩原4处诊断修改；另保留临时 wifi-flat/production-cpu0
  配置供复现。主源码最小配置、生产构建目录和参考仓库未覆盖。
- 新独立输出 `esp32s31-cmake-wifi-cpu0198` 保留生产 S-mode/MMU/SMP，
  仅将默认任务 cpuset 改1、协议开启HE；初次198在 CMake 重生成时误用
  配置中的相对 `APPS_DIR=../apps`，报“Application directory ../apps is not
  found”。不是缺仓库；从诊断输入移除生成路径，让 CMake 按现有主工程
  自动定位 apps 后重编198b。命令 `bash build-cpu0-wifi.sh <absolute-receipt>`。
- 198b编译退出0；生成配置与180仅有HE开关和任务默认cpuset两处差异。
  成对内核/AppFS已校验烧录，旧镜像保存 `firmware198b-cpu0-he.tar.gz`。
  199因测试器误用只返回用户任务的PID函数而提前失败，未开始关联；
  200修正并逐个读取lpwork/hr_timer/wifi/NSH affinity=1后，HE20/DHCP
  成功，首组ping1/4、第二组4/4、TCP4096字节通过，整轮仍判失败。
  201三轮全部affinity=1/HE20/DHCP，但网关/TCP全失败，结果[1,1,1]。
  不得把CPU0限制或单次TCP成功称为HE修复。AP仍为60:ce:41:46:0e:ec，
  当前频道6；早前IDF173同AP在频道1，时间/信道条件不同需保留。
- 编译器真实参数预处理对照工具 `compare-wifi-init.py`：203首次误选
  源码树最小配置，203b加入配置检查后拒绝未展开宏；203c把构建config.h
  放在所有预包含之前，非修改方式获得正确结果。RX/TX数量、聚合窗口、
  队列等相同，feature_caps为NuttX0x80/IDF0x5a1。旧g_wifi_feature_caps
  被链接器丢弃，未据此改代码。无效203/203b日志不可当作固件参数。
- 新候选补齐S31已有三个时序默认值的HAL设置调用（50ms/10s/15ms转us），
  不启用站点省电。205首个主机错误是夹具把电源累计次数当成布尔状态，
  修正后205b C6/S31生命周期测试通过，HEAD负例在sleep_defaults断言失败。
  206 CMake编译退出0；207主机汇总/F0锁验证通过，协议测试明确取自6e102。
  209独立生产SMP/bgn构建和验证已结束，候选已归档撤回，见最新记录。

2026-09-11 IDF 对照已完成并恢复：用户明确授权先双份 Flash 读回校验，
再临时切换 IDF 并恢复。细节和安全恢复入口在 `../../diagnostics/idf-baseline/README.md`。
network169 b/g/n 三轮 `[0,1,0]`：第 2 轮 DHCP/网关/DNS 通过，TCP
connect 超时，预复位 ifdown 超时但硬复位清理成功；不能宣称全部稳定。
IDF171 原始库、172 使用 HAL PHY、173 使用 HAL PHY+Wi-Fi 库，
每组 HE20/HT20 各 3 轮，全部 DHCP/网关 8/8/TCP 4096 字节通过。
每组均恢复原始扇区并完整读回 5 MiB，与初始双份备份一致。
170 的 TCP 收尾时序测试缺陷已在 171 修正；不计入稳定通过组。
主工程 build174 为 Wi-Fi-only 软件共存条件修正候选（HAL/IDF 无修改）。
CMake174 与隔离 Make176 编译均退出 0，首个真实错误无；仍有 HAL 原有
shadow/GPIO 格式/PHY endif 警告。network175 HE 三轮 DHCP 成功但网关
失败；network177 跳过额外 scan/pscan 仍 HE 网关失败、TCP connect 超时。
因此共存条件调整与去掉额外扫描均不是 HE 修复，不能宣称根因已解决。
network179 b/g/n 控制结果 `[1,0,0]`：第一轮关联后 sleep/ifdown 等待超时，
硬复位清理成功；后两轮 DHCP/网关/DNS/TCP/UDP 全通过。共存候选不提交，
已保存 `coexistence-candidate174-rejected.patch` 并从主源码和隔离 Make
逐项撤回。主源码重新只剩本轮开始时的 4 个诊断文件修改。
build180 用恢复后的基线源码重建，退出 0、首个真实错误无，专属收据
验证后 kernel-only 烧录；`.config` 与已归档 build168 完全相同。
smoke181 三次启动、三次无线启停、双核 NSH/挂载/定时器检查全部通过，
没有启用存储写入测试。隔离 Make182 已恢复基线构建产物，编译退出 0，
首个真实错误无；未烧录。host183 汇总及参考依赖锁复验通过，其中协议
单测明确取自 6e1020517c2，不覆盖工作区中的临时 RX/TX 统计调用。
`idf171-173-summary.json` 已程序化复核 18 轮网关 144/144、TCP 18/18，
6 份初始/恢复 Flash 快照哈希均一致。恢复后的源码与原有 4 个诊断修改
均保留，无 merge/rebase/cherry-pick/sequencer 残留。

开发板当前为 **CMake production build180 b/g/n 基线内核 + build150 TCP/UDP AppFS**，
build136c 的 b/g/n TCP 通过镜像已独立归档，未用主源码最小镜像覆盖。
此前 flash99/101 哈希校验通过。build81 加等待对象/LP 工作线程回溯地址诊断；
build87 加 DHCP 收发传输头部诊断，build89 加发送返回值和关联 AP 信息；
build93 加 ARP/ICMP 头部及 TX 完成状态，不输出载荷或凭据。

最新结论优先于以下历史排查记录：

- build140 恢复 STA_11AX=y，并读取实际协商 PHY，编译和 kernel-only
  烧录成功。network141 确认 mode=6（锁定 HAL 的 HE20），DHCP 成功但
  网关失败，Windows TCP 在 connect 阶段超时；正常关闭及复位清凭据成功。
- build142 仅增加关联/停止时的只读 RX 硬件计数，编译退出 0、首个真实
  错误无，专属 build142.sha256 核对后 flash142 成功。没有清计数、改 RF
  参数或关闭基带看门狗。network143 同样 HE20、DHCP 成功、网关 0/8、
  TCP connect 超时；stop 时 full/fifo/rxhung/txhung/panic/wdg 全零，
  bf_report_err 从 0 到 27、bf_ndp_timeout 从 0 到 1。这只是诊断线索，
  不是已确认根因；独立 RX error getter 返回 -1，不能把其零输出当作正常。
- build144 仅关闭隔离配置 STA_11AX 做相同硬件计数的 b/g/n 对照，编译
  退出 0、首个真实错误无，flash144 哈希通过。network145 实际 HT20
  （mode=4）、DHCP/网关 8/8/TCP 4096 字节精确往返通过，正常退出及
  ifdown/复位通过，bf_timeout/bf_error 始终零。未更改板级默认 defconfig、
  路由器或 Windows 防火墙。
- build146 重新启用隔离配置 HE，仅在 S31 station connect 临时设置
  wifi_sta_config_t.he_su_beamformee_disabled=1，并在关联后读取此位；
  锁定 HAL 头文件给出了该能力位语义。编译退出 0、首个真实错误无。
  临时隔离实验尚非永久修复；不得宣称完整 HE 能力通过。
- network147 确认实际 HE20、SU 禁用位读回 1，bf_timeout/bf_error 归零，
  但仍 DHCP 成功、网关 0/8、TCP connect 超时，关闭/复位通过。因此 SU
  反馈错误不是数据不通的充分解释，已经撤回该临时限制。
- build148 增加 RX 类型/TX 完成统计，在站点启动启用、停止读取后释放，
  不改 RF 参数。首个真实错误是缺少 esp_wifi_he.h 导致统计启用 API 隐式
  声明；无成功凭据、未烧录。补入锁定 HAL 现有头文件后重编 build148b。
- build148b 编译退出 0、flash148b 哈希通过。network149 扩展 RX/TX 统计
  启用返回 0，但关联等待及正常关闭超时；硬件复位后的 ifdown 在输出统计
  后触发 `S31SM:M-TRAP mcause=0x18000001 mepc=0 ra=0`。属于新增诊断
  路径的运行异常，不据此推断原 HE 故障。已经保存失败镜像/ELF/配置至
  firmware148b-statistics-failed.tar.gz 及源码 s31-adapter148b-statistics.patch，
  并撤回所有扩展统计启用/查询/关闭调用，保留此前通过的低频硬件计数。
- build150 选择隔离配置 b/g/n，加入现有 UDP server（5471、IPv4、NETINIT
  禁用，不改 DHCP 地址），编译退出 0、首个真实错误无。新增 UDP 发送及
  串口验收工具：256 个 96 字节单播包，40ms 间隔，连续排空串口；严格
  要求全部序号、大小、单一发送端及无内容错误，不能以发送完成代替接收。
  主机工具与实际 apps fill_buffer/check_buffer 的 256 种内容逐项比对通过，
  丢包/重复/乱序/错误大小/变更发送端等反例均拒绝。
- network151 三轮结果 [0,1,0]：三轮 DHCP/网关 8/8/DNS/正常 ifdown 均通过，
  第 1、3 轮 TCP 4096 字节和 UDP 256 包完整内容通过；第 2 轮 TCP connect
  超时，尚未进入 UDP。整组失败，不得用 2/3 表述为连续稳定通过；三轮
  均为 HT20，同名 AP 的跨设备 TCP 路径仍需继续回归。
- GOT_IP 候选只把成功日志改为 wlinfo，新增板级说明；主机生命周期测试
  通过，HEAD 基线确实没有 wlan_sta_ip_poll。host152 的 18 个脚本、依赖锁
  和单独 nettest 主机测试通过。build152 编译退出 0，flash152 内核哈希通过，
  production-smoke153 三启动/存储/无线启停进行中；未提交临时逐包/硬件诊断。
- 已反汇编锁定 HAL libpp.a 的 esp_test_get_tx_statistics：可选第三参数
  非空时复制 984 字节（6 * 164），而公开指针类型只有单元素 164 字节。
  这说明 build148b 的栈对象会被越界写；证据 tx-statistics-abi153.dis，
  不能把该新增诊断错误归因于 HE。本次 build154 传 NULL 跳过可选数组，
  保留 136 字节 TX 摘要/164 字节 RX 类型，重新启用 HE 做安全统计对照。
- build152 的 production-smoke153 已全部通过（含测试文件删除及不存在校验），
  GOT_IP 三文件单独提交 e170f61efa5；临时诊断均未进入提交。
  esp32s31-port-2b025-to-e170.bundle 已 verify。仅将该提交的功能改动同步
  到隔离 Make 工作区，保留其他既有修改，production-make156 正在回归。
- build154 编译退出 0、首个真实错误无，flash154 哈希通过。网络诊断启动
  一度被安全检查误认作烧录仍占串口；再次独立确认日志校验完成、会话退出0、
  fuser/pgrep 无串口用户及烧录/测试进程后才获准开始 network155，没有并发
  操作同一串口或绕过拒绝。
- network155 已完整结束：HE20，DHCP 成功、网关 0/8、TCP connect 超时，
  正常 ifdown 和复位后 ifdown 均通过，无再次 M-TRAP。RX legacy=69、
  HT/SU/ERSU/MU 全零，TX enable/complete=78、succ/ack=31、retry=61、
  no-memory=0。说明安全统计已可用，但尚不能由此断言 HE TX 或 RX 根因。
- production-make156（同步 GOT_IP）和 minimal-make157 均编译/镜像生成成功，
  退出 0、首个真实错误无，均未烧录。仍保留 esptool.py 弃用提示。
- 锁定 HAL esp_private/wifi.h 明确 fixed-rate 对管理和数据帧生效，但要求
  TX AMPDU 关闭。因此 build158 先只关闭 TX AMPDU、保留 HE/RX AMPDU/安全
  统计，作为下一次固定速率实验的独立对照；不更改板级 defconfig。
- build158 编译及烧录校验成功；network159 HE20、TX AMPDU 关闭时 DHCP
  成功、网关 0/8、TCP connect 超时，正常关闭及复位通过。停止统计 RX
  legacy=45、HT/HE 各项为 0，TX 55 完成/8 成功/30 重试，无内存不足。
  保存 firmware158-he-tx-control.tar.gz 和 s31-candidate158.patch。
- build160 只增加 esp_wifi_internal_set_fix_rate(STA,true,WIFI_PHY_RATE_6M)，
  不改变 HE 协商或接收能力；编译退出 0、首个真实错误无，flash160 内核
  哈希通过。network161 正在检查调用结果及实际 HE 模式下的联网表现。
- network161 固定 6M 返回 0、仍协商 HE20；DHCP 成功、网关 0/8、TCP
  connect 超时，正常关闭和复位通过。RX legacy=44、HT/HE 均 0；TX
  enable/complete=21、succ/ack=8、retry=4。固定 legacy 发送未恢复链路，
  已撤回此限制，不能据此声称完成 HE TX/RX 根因定位。
- build162 恢复正常 TX AMPDU，保留 HE，增加 MODEM_SYSCON 只读时钟回读；
  使用锁定 S31 LL 接口检查 BB/80x1/44M/MAC/APB/FE160/SEC，只读不复位
  或改写寄存器，准备与同代码 b/g/n 对照。
- build162 编译及 flash162 校验通过；network163 HE20/DHCP 成功、网关
  0/8/TCP connect 超时，正常关闭及复位通过。关联和停止前 conf1=0038e7ff，
  BB/80x1/44M/MAC/APB/FE160 均开启，SEC=0；这些采样未证明故障期间
  基带时钟缺失。SEC 在 S31 HAL 属于 BT peripheral 时钟，不能凭其为0
  推断 Wi-Fi 加密故障或盲目打开。
- 源码差异：锁定 HAL wifi_init.c 初始化成功后/卸载后分别调用
  modem_clock_configure_wifi_status(true/false)，当前公共移植代码缺失。
  S31 modem_clock_impl.c 利用此状态保护已初始化 Wi-Fi 的共享时钟释放。
  build164 补入 S31 专用成功路径调用，编译退出0、首个真实错误无，
  flash164 内核哈希通过；正在 network165 实测，并非已经确认 HE 修复。
  新主机测试覆盖 S31/C6、注册/预初始化/驱动/认证组件失败、busy、卸载失败
  和重试：新版通过，HEAD 基线 S31 状态断言预期失败；只改当前移植代码，
  没有改动参考 HAL。SHA256SUMS-e170-control158 已全部核验通过。
- network165 HE/DHCP 通过、网关和 TCP 仍失败，正常关闭及复位通过；
  该时钟接口补齐未消除 HE 故障。production-smoke166 三启动/SMP/计时/
  存储写读重启删除/无线启停全部通过；主机新增重新初始化用例也通过，
  单独提交 6e1020517c2，仅含公共 init 文件及测试，临时诊断未提交。
- build168 同代码关闭 HE 做 b/g/n 对照，编译退出0、首个真实错误无。
  Make167 同步生命周期改动、保留隔离工作区其他现有修改，构建进行中。
- TCP 测试现在明确输出失败阶段 connect/send/receive/server-close；不再
  用“字节比较失败”笼统描述连接超时。未延长超时或增加超时重试，验收
  条件不放宽。
- build115/flash115 已成功；network116 初始地址为 0，三轮 DHCP 成功/失败/成功，
  GOT_IP 通知在获得实际地址后发生，网关仍不稳定，尚未更改板级 defconfig。
- build117 接收队列加锁编译/烧录通过，smoke118 三启动、SMP、计时、存储和
  无线启停通过，提交 67a039da9e0。network119 一轮关联命令超时、正常 ifdown
  超时（复位恢复），另两轮 DHCP 成功但网关失败；不能认定该单次超时的根因。
- 后续锁审查：iob_remove_queue -> iob_free_qentry -> nxsem_post 可能唤醒
  分配者，而普通 spin_lock_irqsave 不阻止任务抢占。改用已有 nopreempt
  配对接口；主机新增唤醒场景通过，67a0 基线触发预期断言失败。
  build120 编译退出 0，首个真实错误无；专属哈希匹配后 kernel-only 烧录成功。
  network121 三轮重连回归进行中，不把主机锁测试通过算作 Wi-Fi 稳定性通过。
- 67a0 增量 bundle 已 verify；candidate120 补丁、独立 GOT_IP 主机测试、
  内核/AppFS/配置已归档，SHA256SUMS-67a0-candidate120 全部核验通过。
  首次用两个裸提交作 bundle 范围被 Git 拒绝为空，改为分支引用加排除基线后
  成功；这是备份命令修正，未修改 Git 历史，也不是工程编译错误。
- network121 三轮 DHCP、正常 ifdown 均通过，无命令超时，但网关均失败；
  smoke122 三启动、SMP、计时、存储写读重启删除和无线启停通过。
  因此提交锁抢占修正 467b19c840f，不能视为 Wi-Fi 间歇问题已解决。
- RX BA 窗口错用 TX 配置已修正并提交 a36279b0f07；12 个主机组合通过，
  旧代码在 TX 关闭配置下引用不存在的 TX_BA_WIN，主机编译预期失败。
  production-cmake123 编译退出 0，首个真实错误无。当前两个窗口均 6，
  默认运行值不变，未单独烧录 build123，不冒称独立实板验证。
- build124 增加关联后读取固件实际 STA MAC 和省电模式的只读诊断，
  编译退出 0、flash124 哈希验证通过；network125 实测 MAC 与 NuttX
  网卡一致，省电模式 0（NONE），DHCP 通过、网关两组 0/4。
- host124 共 16 个脚本通过，依赖锁定验证通过。a362 增量 bundle、
  candidate124 补丁及完整镜像已保存，SHA256SUMS-a362-diag124 核验通过。
- build126 仅把临时逐包诊断用 WLAN_PACKET_DIAG=0 编译排除，保留原始
  收发流程；编译退出 0、烧录哈希通过。自动权限审查最初认为烧录仍占用
  串口而拒绝网络测试；独立核实烧录已退出 0、日志哈希完成且无占用进程后
  再获准启动测试，没有并发使用串口或绕过拒绝。
- network127 无逐包日志三轮：DHCP 失败/成功/成功，网关未通过，全部
  正常 ifdown 成功。因此关闭日志不是已验证的解决办法。
- build128 仅另用锁定 HAL 提供的 esp_wifi_set_protocol 临时选择 b/g/n，
  初始化前后读取协议位图并记录设置返回值，不改路由器、不做永久降级。
  编译退出 0，首个真实错误无；烧录哈希通过。
- network129 b/g/n 三轮全部 DHCP/网关/正常关闭通过，24/24 ping；分别连接
  三个同名 AP，覆盖两个信道 11 AP 和较弱信道 6 AP。镜像、补丁和哈希已保存
  firmware128-bgn-control.tar.gz、s31-candidate128-bgn.patch、SHA256SUMS-bgn128。
- build131 仅在相同协议设置调用中加回 11AX 位，协议读回 71（b/g/n/ax），
  编译/烧录通过。network132 三轮 DHCP 成功但全部网关 0/8；第一轮 AP 与
  network129 第一轮相同。由此缩小到 HE 相关路径，但未确认内部根因。
- 新增候选 ESPRESSIF_WIFI_STA_11AX 开关，默认 y，仅 S31 STA/APSTA 可见，
  关闭才显式选择 b/g/n；非 S31 路径不变。主机覆盖两种 S31 配置、C6、
  STA/APSTA、设置模式/协议/启动失败释放锁，全部通过；基线缺设置调用预期失败。
- build133 编译成功，但原 CMake reconfigure 不刷新既有 .config 的新默认项，
  因此未将 build133 作为默认配置验证或烧录。脚本新增 olddefconfig 模式，
  使用当前 CMake 的精确 Kconfig 环境且不重置到 defconfig。build133b 成功，
  刷新差异仅新增 STA_11AX=y；IP=0、广播 DHCP 和其他既有设置全部保留。
- build134 显式关闭隔离配置中的 STA_11AX 后刷新和全量编译成功，flash134
  kernel/AppFS 均哈希验证；未改变 production defconfig 的默认 HE 能力。
  network135 三次 58 ping 长连接、主机双向诊断和 DNS 测试进行中。
- minimal-make130 和隔离 production-make134 均成功。后者仅同步已提交 RX 锁
  和 RX BA 窗口修正，尚未同步 GOT_IP 候选或协议开关，也未烧录该 Make 版本。
- 强化 ping 验收：旧脚本的子串 0% 可能匹配 50% 等部分丢包，新增精确解析
  收发数/丢包率并拒绝空/重复结果，主机回归通过，提交 a55bd00931c。
  已逐条核对 network129 的实际 4/4 和 network132 的 0/4 汇总，上述对照结论
  不受该判定缺陷影响；network135 起使用新判定。
- network135 三轮全部成功：每轮 58/58 网关 ping、DNS 解析通过，合计
  174/174；Windows 到开发板合计 12/12 ping 成功。开发板到 Windows 仍失败，
  未改防火墙，不能据此证明反向 TCP/ICMP 入站策略。smoke137 基础回归通过。
  协议开关、README 和主机测试已单独提交 2b025058fc6，默认 y 未降级。
- 为复用现有 TCP 校验程序，仅在隔离配置启用 EXAMPLES_NETTEST、SERVER、
  IPv4、4096 字节和端口 5471；禁用 NETTEST_INIT，不覆盖已获 DHCP 的网卡。
  build136 首个真实错误：主机 tcpclient 链接缺 g_nettestserver_ipv4。
  build136b 补入 nettest_cmdline.c 后首个真实错误：htonl 不能作全局常量
  初始化，另有 FAR 未定义、命令行重复定义地址配置警告。两次均未烧录，
  没有生成成功凭据。
- apps 在原 3847d5dda、无操作残留的工作区新建 codex/esp32s31-nettest 分支，
  保留 nist-sts 未跟踪文件。修复主机源码列表、运行期地址初始化、调用命令行
  解析、非法地址判断及多余宏定义。IPv4/IPv6、client/server、默认/loopback/
  参数覆盖/非法参数主机测试通过；旧版预期失败。build136c 退出 0，内核和
  包含 nettest 的种子镜像哈希验证后烧录成功。
- network139 三次独立连接 DHCP/网关/DNS/TCP 全通过。Windows 客户端向板上
  单连接 nettest 发送 4096 字节确定性数据并完整接回，逐字节相同，SHA256
  三轮一致；板上均收到/发出 4096 字节并正常退出。没有开启 Windows 监听
  端口、抓包或修改防火墙。此为基础 TCP 功能，不是吞吐量或 xTS 全项通过。
- apps 修正提交 c5fba9b03。首次提交因 apps 未配置身份而失败，使用已经存在
  的 NuttX 提交身份 Codex <codex@local> 作为本次命令参数后成功，未修改全局
  Git 配置。apps 增量 bundle、NuttX a362-to-2b025 bundle、136c 镜像和候选
  补丁均保存；SHA256SUMS-tcp139 全部核验通过。host138 全部通过且依赖锁通过。
- 已只读找到参考 production/validation 镜像及检查配置、挂载代码，尚未切换
  参考固件，未修改参考仓库或既有资料。
- build140 恢复当前隔离配置的 HE 开关，并加入实际协商 PHY 模式只读查询。
  tcp-diag 测试模式即使网关失败也继续记录本机 TCP 结果，最终仍保留失败状态，
  不把部分成功计为整轮成功；编译进行中。
- network80/82 将设置 IP 放后台，发现任务在等待 Semaphore，但 LP worker
  等待的是自己的工作队列，不是已确认的网络锁死。
- 当前 NSH cmd_ifconfig 的 `if (!gip) netlib_obtain_ipv4addr(ifname)` 表明
  `ifconfig wlan0 0.0.0.0` 会隐式发起 DHCP；原脚本 8 秒超时不足以证明卡死，
  随后再 renew 还会重复 DHCP。已修正为查询现有网关、显式保留非零网关清零
  IP，再单独 renew；失败时不接受残留静态 IP。主机回归 dhcp-probe84 通过。
- network83 去掉清零、直接 renew 时仍带 10.0.0.2 静态地址，DHCP 失败；
  network84 使用正确纯清零流程，DHCP=192.168.1.60，网关 ping 4/4 成功。
  所以原“网关不通”已突破，但 network85 再次 DHCP 失败，尚未连续稳定通过。
- network84 附带的 `arp -a` 是无效查询（当前 NSH -a 要求指定 IP，不是列出
  全表，也不是添加）；已改为 `arp -i wlan0 -a <gateway>`。添加条目是 -s。
- network88/90：DHCP 三次广播发出，没有 DHCP RX 回调；network90
  发送提交返回 0，关联到同名较弱 AP（信道 6，扫描约 -77 dBm）。
- network91 脚本先设置 BSSID，因 SIOCSIWAP 立即连接而缺少 SSID/密码，
  返回 12298；这是脚本顺序错误，不能作为 AP 故障证据。已改为密码、
  延迟 ESSID（2）、BSSID 触发连接的顺序。
- network92 临时选择同名较强 AP（信道 11，约 -65 dBm），关联、DHCP
  成功；收到两次目的为 192.168.1.60 的单播 DHCP RX，网关 ping 0/4。
  有两次 ARP 等待超时，ping 后已有网关 ARP 映射；不能宣称网络稳定。
  测试结束重启并 ifdown，不保存 BSSID/凭据，不改变路由器。
- build93 编译成功（首个真实错误：无）；flash93 哈希校验成功。
  network94 DHCP 成功、ARP 回复收到，八个 ICMP 请求发送完成 status=1，
  但无 ICMP RX，两轮 ping 0/4。未修改公共网络锁。
- network95 选择另一个同名信道 11 AP：关联成功，DHCP 三次提交成功，
  只见网关对候选地址发 ARP 请求，没有 DHCP RX，未获租约。
- build96/flash96 成功；network97 较强 AP 再次 DHCP 成功，验证全部八个
  ICMP TX 的源/目的 IPv4、源/目的 MAC、长度及两个校验和正确（ffff），
  发送完成成功，但仍无 ICMP 回复。
- Windows 本机 192.168.1.29 对网关两次 ping 均 3 ms 成功。network98
  同一固件 DHCP 成功，本机到开发板四次超时，板到本机和网关均不通。
  本机测试期间串口异步输出被下条命令 reset_input 丢弃，因此不能用
  network98 证明当时板上完全没有收到本机请求；脚本已补保留这段输出。
- build99 仅在隔离 CMake .config 中将 DHCPC_BOOTP_FLAGS 改为 0x8000，
  用于验证初始未配置 IP 时请求广播 DHCP 回复；尚未修改 production
  defconfig。全量 3282 步编译通过，首个真实错误无；存在锁定 HAL 的
  shadow/format/endif-labels 警告，未修改参考 HAL。
- 原 build96 内核、AppFS、配置已保存 firmware96-unicast-diagnostic.tar.gz。
- network100 默认较弱信道 6 AP：广播 DHCP 成功（回复确为广播），主机、
  网关 ping 失败。Windows ARP 缓存 MAC 正确；pktmon 只读状态查询被
  Windows 拒绝访问，没有启动抓包或改变过滤器。
- build101/flash101 加 ARP MAC/长度诊断并通过；network102 广播 DHCP
  成功，本机双向 ping 失败，但网关两轮 ping 均 4/4，WPA2_DHCP_GATEWAY=PASS。
  第一次本机请求 ICMP RX 已见，触发 ARP 请求但未见其回复发送；仍不等于
  双向通信或长期稳定性通过。ARP 包 SHA/THA 与设备/对端地址一致。
- 确认参考 esp_wlan_netdev.c 的 wlan_sta_ip_poll 和锁定 HAL wifi_default.c
  在取得 IP 后调用 esp_wifi_internal_set_sta_ip，当前 esp_wlan.c 完全缺失。
  已以 S31+STA+IPv4 条件补接关联后轮询、断开/ifdown 取消，尚待实板验证。
  不宣称该遗漏是目前间歇失败的唯一根因。
- production-cmake103 首个真实错误：netdev_lock 宏展开 nxrmutex_lock 缺声明。
  补包含 nuttx/mutex.h 后 production-cmake103b 退出 0，镜像生成成功。
  注意 flash103 在发现编译失败之前已启动，实际重写旧 build101 镜像，
  不能计为 GOT_IP 修复测试；随后确认 103b 编译成功再烧录 flash103b。
  主机 gotip-host103 测试通过：未启用、无 IP、失败重试、成功停止、断开、重连。
- network104 三次独立启动自动选择 AP：全部 DHCP 成功，通知 IP 日志均出现，
  网关通过情况为失败/成功/失败，本机双向均失败。因此 GOT_IP 漏接属实，
  但不能单独解释或消除当前故障；默认 10.0.0.2 静态地址令通知在 DHCP 前
  即发生，后续正式 DHCP 配置应从 INADDR_ANY 开始验证。
- build105 仅额外在 S31 初始化时临时把 ampdu_rx_enable 设 0，TX 聚合不变，
  用于验证普通数据帧接收/聚合路径；不作为永久降级修复。编译退出 0。
- 构建脚本新增 S31_BUILD_RECEIPT 可选专属哈希凭据，只在成功构建后生成，
  拒绝复用路径；flash-verified-cmake.sh 要求当前内核/AppFS 与该凭据一致
  才烧录，避免把编译失败后残留镜像误当新版本。
- network106 关闭 RX 聚合三轮：DHCP 全通过，网关三轮均失败，未改善普通
  IP 通信。build107 恢复 RX 聚合，仅临时关闭 TX 聚合；编译、专属镜像
  哈希匹配、烧录校验均通过，network108 连续三轮对照进行中。
- 已确认但尚未修改的初始化差异：当前 adapter 的 rx_ba_win 错用 TX_BA_WIN
  （当前都为 6，不能解释本次故障）；S31 sdkconfig 在动态 TX 模式仍定义
  HAL STATIC_TX_BUFFER_NUM=16，参考驱动为动态模式显式设静态预算为 0。
  此外 RX 队列生产者持 priv->lock，而 wlan_recvframe 出队未持同一锁，
  IOB 队列原语本身不保护头尾更新；这些是后续需要独立验证的差异，未归因。
- network108 关闭 TX 聚合三轮也均未通过；已恢复全部正常 RX/TX 聚合配置。
- 静态缓冲映射差异已修正并提交 04de89b5e50，6 个主机配置组合通过，旧版
  在动态模式触发预期错误。首次主机夹具漏 Flash 频率设置（非工程错误），
  补 80 MHz 夹具后通过。build109、flash109、host110、minimal-make110
  均成功；production-smoke110 三启动/SMP/定时器/存储/无线启停通过。
  存储仅删除本轮唯一 .s31probe 临时文件，无用户文件删除。
- network111 三轮 DHCP 成功，网关均未全通过；第三轮首组 1/4，第二组 4/4。
- network112 同一连接持续测试：五组十次网关 ping 全部通过，再加两组四次
  全通过，共 58/58。不能把单轮持续成功等同重连或全网络通过。
- network-probe.py 清理改为先有限等待 ifdown，再复位兜底并 ifdown；避免
  每次都直接硬复位而没有尝试正常关闭无线。network112 正常关闭通过。
  network113 使用同一 build109 做三轮正常关闭/复位/重连对照，进行中。
- 04de 已增量 bundle 保存并 verify。GOT_IP 轮询及网络诊断仍未提交。
- network113 正常关闭后复位重连三轮：DHCP 通过，网关均未通过；不能把
  正常关闭当作完整修复。Make 隔离工作区小范围同步 04de 映射，
  production-make114 编译通过，尚未烧录这一 Make 版本。
- build115 仅另将 CMake .config 的 NETINIT_IPADDR 从 10.0.0.2 改为
  INADDR_ANY，保持广播 DHCP；因此 Wi-Fi IP 通知不再因占位静态地址过早发生。
  编译退出 0、专属哈希通过，正在更新内核及匹配只读 AppFS。板级 defconfig
  尚未更改，等待实际验证。
- PDF 原件均可读且未修改。当前 WSL 无 pdftotext/PDFium，旧 pdftext 中文
  映射乱码，不能用其中文作证。找到现有 Windows PDFium 运行环境后，
  render-spec-review.py 只读渲染模块 PDF 第 2/3/39/40/54/55 页至 pdf-review/。
  已目视核实：2.4 GHz 1T1R b/g/n/ax、PCB 天线、320 MHz 双核；模组建议
  供电 3.0–3.6 V、典型 3.3 V，外部电源供电能力最小 0.8 A；Wi-Fi 发射
  峰值表最高 375 mA。这些不是本板供电故障的实测证据，未要求更改接线。
- 已异步询问用户是否授权 Windows 管理员限定开发板的协议头抓包，等待答复。
  不在答复前绕过权限、启动抓包或修改防火墙；其他板端工作继续。
- host92 包含新增 PHY 时钟、DHCP 脚本回归及原有测试，共 12 个脚本，
  全部通过，依赖锁定验证通过。提交 c98b 已增量 bundle 保存并 verify。
- 隔离 Make 工作区已小范围同步 PHY/IRQ 提交内容，production-make86 成功，
  尚未烧录它做新的 Make 网络对照。

build63 增加 S31 扫描参数/完成事件/AP 数量脱敏诊断，build69 加 ISR 计数，
build71 加 CPU 输入映射/使能日志。诊断代码未提交；build73 输入 1
重新使能修正经扫描、DHCP、基础硬件回归后已提交。不能把当前树视为干净，
也不能把扫描恢复称为完整联网通过。
production-make-smoke52：三次启动（约 3.30 秒）、NSH/SMP、定时器、
存储写读重启删除、三次无线启停全部通过。
此前 CMake production build33：三次启动、存储写读重启删除、
三次无线启停通过。随后十轮启停、二十次主动/被动扫描通过，无崩溃/卡死。
扫描仅见少量弱信号 AP，目标测试 AP 未发现。连接日志记录 reason=201
（锁定 HAL 定义 NO_AP_FOUND），不是已确认的密码认证失败。
当时尚未通过关联、DHCP、网关 ping；后续 network84 的突破见上文，仍不等于 xTS 验收完成。
此前每轮约 64 字节的增长已定位到每次用户程序启动注册无名称 procfs 堆记录，
不是 Wi-Fi 数据缓冲本身。修复跳过 NULL 名称并使注销 NULL 安全；主机三个
backtrace 配置通过，板上无名称记录消失。完整系统仍不能宣称无内存泄漏。
radio-heap50 在 CMake build33 上做十次纯无线启停、每次静置 3 秒，
全部为 used=71188 / blocks=152；因此纯启停没有复现增长。
network54 在 Make 固件上主动/被动扫描命令通过，但仍 reason=201，
未关联、未取得 DHCP；已经重启清理临时联网设置并关无线。
scan-heap55 因 wapi show 的两个未支持查询而停止，非板卡崩溃。
修复后 Make scan-heap57 在第四次 wapi help 无响应；scan-heap58 在第二次
被动扫描无响应。没有异常堆栈，根因未确认，不能归因于密码或单纯射频问题。
CMake 对照 scan-heap60 完成 help/show/主动/被动各五次及最终 ifdown；help
五次 used=76860、blocks=160 完全一致，unnamed=0。单轮通过不等于长期稳定。
network62 使用用户授权的另一 2.4 GHz、信道 6 手机热点，仍空扫描和 reason=201，
没有获得 DHCP。正常重启并 ifdown 清理通过。network61 尚未到 Wi-Fi 测试：
旧 USB 节点阻塞，Windows 显示 Shared 而非 Attached；重新 attach 后同一设备
枚举为 ttyUSB1，network62 的启动恢复正常。不得将 network61 归类为 AP 失败。
network64 诊断复测完成：active/passive 的扫描完成事件均 status=0、aps=0，
scan_get_ap_num 均 ret=0、aps=0，country 查询 ret=0、first=1、count=11。
因此不是扫描列表仅在 WAPI 展示层丢失，也不是信道 6/11 被区域范围排除。
关联仍 reason=201，未进入 DHCP，最终重启和 ifdown 清理通过。
无线接收链路/PHY/底层中断等根因仍待定位；手机到开发板的实际距离尚未确认。
用户随后要求切回原先授权的 2.4 GHz 家庭网络，不再使用手机热点。
network65（原诊断固件）和 network67（PHY 时钟修复）均空扫描、reason=201。
PHY 时钟差异来自锁定 HAL 的 bootloader_esp32s31.c 中保持 XTAL 的启动路径，
及参考 NuttX esp_phy_enable_wrapper 校准前恢复 160 MHz 的代码；当前适配漏了
后半步。已加 HAL 模拟总线锁保护，S31/C6 主机顺序用例通过，旧版 S31 失败；
build66 编译/烧录通过，smoke68 三启动/定时器/存储/无线启停全部通过。
该修复并未解决空扫描，因此不把它写成 Wi-Fi 已联网。
network70/72 进一步显示 Wi-Fi source=122/120 均路由到 CPU input=1，启动
仅调用一次使能 mask=2。主动扫描约 2.53 秒、被动约 5.18 秒，适配层 ISR
累计停在 CPU0=1/CPU1=0。此前完成策略硬编码 input=1 drop（不重新使能），
但实测后续 worker 并未调用使能 hook。build73 去除这个例外，仍尊重驱动
显式 disable；更新主机 65 用例后新实现通过、旧版 input1 失败。
network74 实测主动/被动扫描各 8 AP，ISR 累计 40/69，成功关联并 DHCP 获得
192.168.1.60（网关 192.168.1.1），这是空扫描阻塞的明确突破；网关 ping
四次均失败，仍不能视为完整联网通过。Windows 主机 ping 网关 2/2 成功。
network75/76 再测扫描/关联成功，但随后 ifconfig wlan0 0.0.0.0 无响应；
重启清理均通过。network77 增加连接后 sleep/ps/monstat，以区分整体调度
停顿与网络相关阻塞。network77 的 sleep/ps/monstat 均正常返回，随后设置
IP 仍超时；整体调度在设置前正常，lpwork 等待 Semaphore、wifi 等待 MQ，
monitor pending=0，addrenv live=1。下一步检查网络锁和收发路径；尚无充分
证据认定具体锁死因，不要直接批量替换公共驱动的 net_lock。
host76（十项已有测试及依赖锁）全部通过；smoke78 三次启动（约 0.60 秒）、
计时、无线启停三次、存储写读重启删除全通过；最小 Make build79 成功，
保留 esptool.py 弃用提示，最小固件未烧录。
本轮测试结束后板卡已复位清理并 ifdown，无凭据持久化。

新增备份 `esp32s31-port-4acc-to-37c5.bundle` 已验证，需先有 4acc 基线。
`s31-scan-diagnostics69.patch` 保存当时未提交诊断；
`firmware69-phy-isr-diagnostic.tar.gz` 保存诊断恢复镜像，并非联网通过镜像。
`SHA256SUMS-37c5` 在工作区根目录执行 `sha256sum -c` 已通过。
随后保存 `esp32s31-port-37c5-to-6239.bundle`（需 37c5 基线）、
`s31-scan-diagnostics-final79.patch`（仅未提交诊断）及
`firmware73-dhcp-verified.tar.gz`（DHCP 曾通过、ping 未通过）。
`s31-irq-rearm-diagnostics73.patch` 是提交前保存的完整候选补丁，已有 6239
提交后不能重复应用其中 IRQ 修正。
已向用户询问板卡靠近 AP、PCB 天线周边遮挡情况，尚无答复。
用户凭据只用于交互式内存输入，未写入配置、脚本或本目录日志。

## 构建记录

所有日志在本目录 `logs/`；命令由对应 shell 脚本的 `set -x` 记录。
每次编译没有修改原始 PDF、参考 IDF/HAL；未重新下载仓库。

| 编号/日志 | 完整命令入口 | 首个真实错误与结果 |
| --- | --- | --- |
| 28 passive-build28 | `bash ../2026-09-09-wifi/s31-build-current.sh` | CMake 成功；passive-flash28 烧录成功；network28 扫描通过、关联失败 |
| 29 connect-build29 | 同上 | CMake 成功；connect-flash29 成功；network29 确认 NO_AP_FOUND/ETIMEDOUT |
| 30 minimal-make30 | `bash build-minimal-make.sh` | 编译链接成功，Python 环境缺 esptool，镜像阶段失败 |
| 31 minimal-make31 | 同上，修正 PATH | 镜像生成成功；仍有向量能力宏未定义警告 |
| 32 minimal-make32 | 同上，包含 soc_caps.h | 成功，相关警告消失，未烧录最小固件 |
| 33 production-build33 | CMake 入口同 28 | 成功；production-flash33 / production-smoke33 / scan-stress33-redacted 均通过 |
| 34 production-make-configure34 | 配置包装命令 | Windows PATH 空格未引用，env 报错，未运行 configure |
| 35 production-make-configure35 | `bash build-production-make.sh configure` | PATH 缺已有 kconfig-tweak；部分配置保存为 production-config35.partial |
| 36 production-make-configure36 | 同上，加入已有 PREBUILT | S31 production 配置成功，Kernel/SMP2 |
| 37 production-make37 | `bash build-production-make.sh` | Make 尝试隐式下载 LittleFS/应用 mbedTLS，DNS 失败，无下载成功；已告知用户并加入离线保护 |
| 38 production-make38 | 同上，复用 manifest 已有子仓库 worktree | 旧 Wireless.mk 找不到 aes.c；随后增加 S31 专用源码清单 |
| 39 production-make39 | 同上 | 新 INCLUDES 过早展开，rom/ets_sys.h 找不到；修正为延迟展开 |
| 40 production-make40 | 同上 | 强制兼容头 CFLAGS 过早展开为 /chip；改用已定义 TOPDIR |
| 41 production-make41 | 同上 | memspi_host_driver.h 找不到；补上与 CMake 相同的 Flash 私有头文件目录 |
| 42 production-make42 | 同上 | 应用 mbedTLS3 头抢先匹配，触发 MBEDTLS_CTR_DRBG_C prerequisite 等错误；只在 S31 arch 编译提前 HAL crypto 头路径 |
| 43 production-make43 | 同上 | 内核 Make 和带 digest 的 nuttx.bin 成功；尚未构建应用/AppFS |
| 44 production-cmake44 | CMake 入口同 28 | 成功；host44 中原六项、STA 锁测试、新 Make/CMake 清单测试、依赖锁检查均通过 |
| 45 minimal-make45 | `bash build-minimal-make.sh` | 最小 Make 编译/镜像成功，保留 esptool.py 弃用提示 |
| 46 production-export46 | `bash build-production-make.sh export` | Make SDK 导出成功，nuttx-export-0.0.0.tar.gz |
| 47 production-import47 | apps 目录 `bash tools/mkimport.sh -z -x ../nuttx/nuttx-export-0.0.0.tar.gz` | 成功，仅生成隔离 apps/import SDK |
| 48 production-apps48 | `bash build-production-make.sh apps` | Make 用户态 init/sh/ping/renew/wapi 编译链接成功 |
| 49 production-appfs49 | `bash build-production-make.sh appfs` | AppFS 成功；check-make-images.sh 检查分区边界、五个 ELF 大小与 SHA256，通过 |
| 51 production-make-flash51 / smoke51 | esptool 写 0x2000 内核 + 0x200000 AppFS；标准 production_smoke.py 三启动/存储/radio3 | 烧录校验成功；运行在进入 NSH 前出现 strcmp(NULL,...) 访问异常，libelf_findsymbol 遇 -ESRCH 仍比较空 iobuffer |
| 52 production-make52 / flash52 / smoke52 | `bash build-production-make.sh`，仅烧录 0x2000 内核，标准 smoke 命令 | 跳过无名称 ELF 符号；主机旧版失败、新版通过；实板三启动/存储/radio3 全通过 |
| 53 production-cmake53 | CMake 入口同 28 | ELF 修复后 CMake 重建成功，尚未烧录这份 CMake 产物 |
| 56 production-make56 | `bash build-production-make.sh` | procfs 修复后成功；主机测试 56 的第一次是测试夹具编译错误，56b 修正夹具后新实现通过、旧实现断言失败 |
| 57 production-cmake57 / host57 | CMake 入口同 28；`bash verify-host-current.sh` | 构建、十项主机测试与锁定依赖验证通过 |
| 59 minimal-make59 | `bash build-minimal-make.sh` | 最小镜像成功，未烧录 |
| 60 production-cmake-flash60 / scan-heap60-redacted | CMake57 内核与 AppFS 烧录；`run-redacted.py scan-heap-probe.py LOG` | 镜像校验通过，整组诊断完成，无名称堆记录为零 |
| 63 production-cmake63-scan-diagnostic / production-cmake-flash63 | CMake 入口同 28，仅烧录 0x2000 内核 | 编译及烧录校验成功；扫描/STA 连接主机测试通过；测试命令曾误写不存在的 test_esp_wifi_sta_connect_lock.py，已改为现有 test_esp_wifi_connect.py |
| 66 production-cmake66-phy-clock / flash66 | CMake 入口同 28；同 flash63 命令但串口 ttyUSB0 | 构建和烧录通过；PHY 主机 C6/S31 通过，旧版 S31 预期断言失败 |
| 68 production-smoke68-phy-clock / host68 | 标准 smoke 三启动/存储/radio3；verify-host-current.sh | 均通过，包括依赖锁检查；PHY 测试另见 fixed66 |
| 69 production-cmake69-isr-diagnostic / flash69 | 同 66 | 构建和烧录通过，ISR 计数用于 network70 |
| 71 production-cmake71-irq-route / flash71 | 同 66 | 构建和烧录通过，路由诊断用于 network72 |
| 73 production-cmake73-irq-rearm / flash73 | 同 66 | 构建、烧录通过；65 主机用例通过，旧策略 input1 用例预期失败；network74 扫描/关联/DHCP 通过，ping 失败；后续复测见上文 |
| 78 production-smoke78-irq-rearm | 标准 smoke 三启动/存储/radio3，串口 ttyUSB0 | 全部通过；独立于尚未通过的关联后 IP 修改稳定性与 ping |
| 79 minimal-make79 | `bash build-minimal-make.sh` | 最小 Make 编译/镜像生成通过，未烧录 |

家庭网络测试 65/67/70/72/74/75/76/77 的命令入口相同，仅日志名不同：

```sh
/home/regex/work/esp32s31-openvela/s31-reference/.venv-nuttx/bin/python -u /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/network-probe.py /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/logs/network77-home-settle-redacted.log 192.168.1.1 /dev/ttyUSB0
```

手机热点对照的完整命令（凭据隐藏交互输入，不在命令行/文件中）：

```sh
/home/regex/work/esp32s31-openvela/s31-reference/.venv-nuttx/bin/python -u /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/network-probe.py /home/regex/work/esp32s31-openvela/backups/2026-09-10-scan-stress/logs/network62-hotspot-redacted.log 192.168.43.1 /dev/ttyUSB1
```

network64 命令相同，仅日志名为 `network64-hotspot-diagnostic-redacted.log`。
build63 只烧录内核，完整命令：

```sh
/home/regex/.espressif/python_env/idf6.1_py3.10_env/bin/esptool --chip esp32s31 --port /dev/ttyUSB1 --baud 460800 write-flash --flash-size 16MB --flash-mode dio --flash-freq 80m 0x2000 /home/regex/work/esp32s31-openvela/openvela-dev/out/esp32s31-cmake-production6/nuttx.bin
```

完整实板 smoke 命令（在主 nuttx 目录、用已有带 pyserial 的 Python）：

```sh
python -u tools/espressif/esp32s31_production_smoke.py --port /dev/ttyUSB0 --baud 115200 --boots 3 --storage --radio-cycles 3
```

build51 的 mepc=0x2f000f8c 位于 strcmp；RA=0x400aa7a8 位于
libelf_findsymbol，a0=0。主机基线复现日志 elf-unnamed-baseline52，
修复日志 elf-unnamed-fixed52。没有通过删除 ELF 符号表规避错误。

完整 clean production Make 编译还出现锁定 HAL 自带的 shadow、GPIO 日志
格式、PHY endif-label 警告；尚未处理，不要把此次结果描述成零警告。

## 隔离 Make 工作区

`openvela-dev/out/esp32s31-make-production-verify/{nuttx,apps}` 是从已有本地
仓库创建的 detached worktree，不是新下载。nuttx 基于 3a2a6817aae 并保留
与 6867fb3d7f1 对应的构建修改及 172ecc974e3 的 ELF 修复，未强行清理
或切换。apps 基于 3847d5dda4e。
两个嵌套依赖同样来自已有、manifest 锁定的本地仓库：

- nuttx/fs/littlefs/littlefs：9d31e6df950d7140ed17502022be91186164c160。
- apps/crypto/mbedtls/mbedtls：9fdfb2e5a49c5c650ec7ce0b87c4d1b6aed3ad4c。

`offline-bin/` 只用于本目录构建脚本，拒绝 curl/wget 及 git 网络/reset 类
动作。不用这些保护脚本替代依赖锁验证。

## 恢复注意

主源码 `.config` 是最小配置，CMake production 配置在独立 build 目录。
主源码 nuttx.bin 是最小镜像，不能误当 production 烧录。
当前 production 板上镜像来自 `out/esp32s31-cmake-production6`。
`firmware6867-verified.tar.gz` 包含修复前的已验证 CMake 恢复基线。
USB CP2102N 序列号仍为 f65e4ef67f71f011975a049f1045c30f；Windows BUSID
从 4-7 变为 2-1，已核对身份后恢复转发，临时节点属主设为 regex。
所有烧录只涉及 0x2000 内核和 0x200000 只读 AppFS 种子；不整片擦除，
不覆盖 0x500000 的可写应用分区。
apps 的 nist-sts 未跟踪内容和参考 HAL/IDF 已知嵌套修改均保留。
旧 /tmp 构建日志曾丢失，以本目录持久日志和此前检查点为准。
