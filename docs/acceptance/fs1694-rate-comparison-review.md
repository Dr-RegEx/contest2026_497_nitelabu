# 随机写速率比较核验（1694）

- 来源：`fs1694-result.json`、`logs/fs1694-worker-original.log`、`fs1174-random-write-review.md`。
- 原测试源码：`openvela-dev/tests/testcases/vela_fs_test/fs/stress/random_read_and_write_test.c`。
- 计时段执行10次，每次一条20字节记录，1694耗时45.66秒：有效应用负载约4.38 B/s，平均每次4.566秒。此换算不是物理Flash带宽，也不包含最终flush的独立测量。
- 1094同数量随机写43.21秒，约4.63 B/s；1694耗时增加约5.67%，并未超过这个早期记录。
- 1689同数量随机写1060.82秒；1694相对该退化配置加快约23.23倍。这不能表述成相对历史最佳提升。
- 原始xTS允许调整Count，但预期含“合理范围（毫秒级）”。保留自测完成、待验收状态，不计整项PASS。
