# Korvo 快应用底层配置候选

继承 `korvo-openvela-product`，接入 QuickJS、protobuf-c、字体/PNG/Yoga、curl、Feature框架和IP/Wi-Fi依赖。状态与证据统一见 `backups/quickapp-foundation-readiness/` 及平台能力矩阵；目标依赖构建已通过；板上接口/资源验证仍待完成。

`QUICKAPP`关闭：本地缺少匹配S31的core，不能宣称可运行RPK。Feature设备模块分别资格验证后再启用；上游强制exchange选择保留C++实现，尚未验证，不用默认选择声明支持。JS命令仅用于基础引擎诊断，不是快应用替身。字体目录 `/data/font/` 当前没有预装字体，字体回退不能当成完整字体交付；CA/系统时间/HTTPS亦需独立准备与验证。

Wi-Fi按现有BGN/PS_NONE和256×400B IOB设置，无自动配网，不保存凭据；这些参数有内存/功耗成本，先验证共存，不重启速率优化。单核FLAT/XMS Lite及1MiB数据分区约束沿用父配置；禁止混入KERNEL/AppFS分区覆盖。

使用已有隔离构建器及HMI的AIDL工具环境；不得升级SDK、下载未锁定QuickJS master或复用其他配置的运行结论。
