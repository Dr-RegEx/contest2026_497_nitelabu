# 1690 专用Flash线程候选：构建完成，未上板

1684将LittleFS后缀复制由单字节改为按读缓存边界分块；当前1689在该镜像上执行原始随机读写，不能抢占UART。

发现另一条独立耗时路径：esp_flash_dispatch对PSRAM栈调用使用work_queue(LPWORK,...,0)，当前clock_delay2abstick(0)=clock()+1，工作通过watchdog在后续系统tick唤醒。当前USEC_PER_TICK=10000，重复小块MTD操作可能累积延迟；尚无逐请求上板计时证据，不直接把全部耗时归因于此。

候选增加默认关闭的ESP32S31_FLASH_DIRECT_WORKER，仅在专用xts-flat-category-fs-worker启用。4KiB静态内部SRAM栈地址0x2f009e90、16字节对齐；请求经互斥锁串行发布，信号量立即唤醒，调用者关闭取消直到完成；内部栈调用继续走直接执行路径。SPI/cache保护保留，工作回调在恢复cache后才发布完成，发布后不再解引用调用者请求。未修改全局工作队列或原始用例。

构建1690完成，receipt校验通过；非目标板通过。新配置增加4KiB静态SRAM及一个工作线程，需上板核对实际工作线程、完整原始负载、资源及临时文件恢复。

准备：flash1692-fs-worker.sh要求1689结果中三份文件已恢复；boot1692-fs-worker.py输出保存logs/boot1692-fs-worker.log；mount1692-fs-worker.py输出保存logs/mount1692-fs-worker.log；之后run1694-fs-worker-original.py比较相同800000字节文件、读1000/写10。只在前置工作终止、恢复确认后顺序执行。
