# Camera V4L2 用户缓冲与延迟回调生命周期修复

状态：仅主机代码修复、生产函数并发回归、严格交叉编译与完整镜像链接完成。未刷板、未接摄像头，不增加 Camera 实物通过项。

## 已修复

- 原 DVP HPWORK 向 V4L2 用户缓冲复制时，STREAMOFF、REQBUFS、最后 CLOSE 可改变缓冲/回调。给 `imgdata_ops_s` 增加默认零的可选 `deferred_copy`，仅 S31 选择启用；upper 提供本实例 `cmng->mutex`，worker **只 trylock**，锁忙丢帧，不等待正持锁关闭设备的线程。
- 任务锁覆盖复制和完成回调，长帧 memcpy 不持 IRQ spinlock。STREAMON、QBUF 恢复捕获、拍照切换补齐同一任务锁；已有 STREAMOFF/REQBUFS/CLOSE 沿用原锁。锁序为已有 lock_state → cmng mutex → 短 spinlock；worker 不拿 lock_state，完成回调内 stop 不执行 work_cancel_sync。
- 在短 spinlock 内快照 target/callback/arg，设置当前 copy 线程；复制中外部直接改 target 返回 `-EBUSY`，同 worker 完成回调可正常换 buffer 或 stop。generation 拒绝停止/换流前排队的旧帧，避免投递给新缓冲。
- 可选路径 complete_capture 拒绝非 CAPTURE 状态或不存在 vbuf_next 的迟到完成。
- data.uninit 脱离 upper mutex 前，先在 frame_done 排队所用同一 spinlock 中清 target/callback 并递增 generation，禁止 ISR 新排队，然后同步 drain，再清 mutex 指针。即使底层 stop 失败，迟到 DMA 回调也不能触及即将销毁的 upper mutex；DMA 资源仍遵守已有失败保留规则，不伪造 stop 成功。

## 生产代码验证

`python3 tools/tests/camera-v4l2-lifecycle/run.py`：抽取当前实际 DVP 回调/set_target/detach/stop/uninitialize 和实际 upper `capture_streamoff`、`capture_reqbufs`、`capture_close`；pthread mutex/spinlock、受控 memcpy 屏障、ASan/UBSan。外围 HAL、分配器及 upper 状态转换/资源清理由可观察替身提供，不是完整 NuttX 运行。

通过情形：

1. 复制中 STREAMOFF 等待 copy+callback 完成；并发外部 clear 返回 EBUSY。
2. 非零 count=2 的 REQBUFS 容器重分配分支成功；CAPTURE 时并发 REQBUFS 等待后正确 EPERM。**未把原有 count=0 早退当作释放验证，也未测试真实 MMAP 堆重分配。**
3. 最后 CLOSE 且 unlinked 分支等待复制完成，执行清理、data detach，并实际销毁 pthread mutex；内存释放由测试替身记录契约，非真实 VFS inode 释放。
4. upper 锁忙时 worker 丢帧；旧 generation 排队帧不写新缓冲。
5. 同一完成回调内换 buffer、stop、clear 无递归锁死。
6. stop 故障后 detach 关闭回调入口；销毁 upper mutex 后注入迟到 DMA 完成，不再排队；重复 detach 安全。

原 `tools/tests/camera-dvp-ownership/run.py` 同时回归通过，覆盖双 DMA 缓冲所有权、100 帧、排队失败及 HAL teardown 失败重试。日志 `camera-v4l2-lifecycle-host-test.log`。

## 构建交付

- 前一候选保留：`openvela-dev/out/esp32s31-camera-v4l2-lifecycle`。
- 最终完整构建：`build-camera-v4l2-lifecycle-final.sh`，输出 `openvela-dev/out/esp32s31-camera-v4l2-lifecycle-final`，退出 0；原 source `.config` / `config.h` 已恢复。
- 三个生产 C 文件按最终配置显式先 include 生成 config.h，额外 `-Werror` 严格编译通过，记录 `camera-v4l2-lifecycle-strict.log`。
- 最终 `nuttx.bin`：614676 字节；SHA-256 `dc827b9240c3a9cec8fa22b77422ea2756b1cb9cb900db0b45fa278e319fb299`。
- 完整日志 `build-camera-v4l2-lifecycle-final.log`；哈希收据 `camera-v4l2-lifecycle-final.sha256`。

通用影响边界：新增字段改变 imgdata 内部结构 ABI，相关驱动需一起重编译；静态 ops 未选择 deferred_copy 的驱动字段自动为 false，保留其原回调模型。没有执行其它摄像头驱动的板上回归，没有修改原始 SDK。既有 Camera DMA 双缓冲与 teardown 失败保留修复未覆盖回退。
