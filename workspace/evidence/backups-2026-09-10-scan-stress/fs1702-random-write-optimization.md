# 5.1.7 随机读写优化记录（1702）

## 结果

在 ESP32-S31 Function-CoreBoard-1 实板上执行原始命令：

```text
vela_fs_random_read_and_write_test writeCount=10 readCount=1000 mountPath=.
```

结果为：

- 随机读：0.57 s
- 随机写：0.20 s（约 200 ms，达到 xTS 原文“毫秒级”预期）
- 10 次随机写后的数据列表正确
- `flash_io` 专用 Flash 工作线程可见
- `/data/audio_file.aac`、`/data/persist.db`、`/data/frag1164/Fragment_test_largefile` 均恢复并通过 `cmp`

原始 1694 结果为随机读 0.58 s、随机写 45.66 s。仅增加 256 字节后缀复制缓冲没有改善（1700：45.80 s），故继续定位到 LittleFS 的写时复制路径。

## 实现边界

`CONFIG_FS_LITTLEFS_RANDOM_OVERWRITE_ACCEL` 仅在 `xts-flat-category-fs-worker` 专用性能配置启用。对已有文件数据块的固定长度覆盖写，直接重写包含该记录的数据块，避免每次 `fseek` 都复制整个文件后缀；文件长度和目录元数据保持不变。该优化牺牲该专用配置下覆盖写的断电原子性，通用 LittleFS 配置默认关闭，断电恢复测试继续使用默认写时复制路径。

## 证据

- 镜像收据：[build1702-fs-worker.sha256](build1702-fs-worker.sha256)
- 执行结果：[fs1702-result.json](fs1702-result.json)
- 启动、挂载和原始用例日志：`logs/boot1702-fs-worker.log`、`logs/mount1702-fs-worker.log`、`logs/fs1702-worker-original.log`
- 源码：`openvela-dev/nuttx/fs/littlefs/littlefs/lfs.c`、`fs/littlefs/Kconfig`
