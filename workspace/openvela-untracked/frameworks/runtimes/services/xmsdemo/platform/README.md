# 原生产品演示候选

Device活动通过AM/WM/PM启动、切换与退出；展示实际存储、网络地址、内存与运行时间，保存外观设置，并进入标准Widgets及HOME。使用UIKit字体管理器，暂停时停止刷新，销毁时按控件→字体→路径顺序释放自有资源。没有直接驱动录音/录像，也不启动自动配网。

使用 `korvo-openvela-engineering-demo` 配置，manifest及字体进入ROMFS。E154已完成目标构建和Korvo上创建/恢复/暂停/销毁、HOME切换及实际FreeType字体初始化；SystemServer需36 KiB配置栈，已观察到23576 B使用量。需要 `/etc/fonts/Montserrat-Regular.ttf` 及持久 `/data`。页面先使用英文字体；中文资源、触摸人工体验、主题按钮保存、网络连接、服务异常与长期运行仍需验证。首次退出后31664 B增量未完整归因；KVDB正常提交不等于物理掉电安全。
