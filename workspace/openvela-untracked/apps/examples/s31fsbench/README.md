# S31提交型文件系统数字基准

使用标准stdio/POSIX接口，补充原xTS随机读写程序的数据校验和持久化计时。它是独立诊断应用，原xTS程序保持自己的命令和证据身份。

```sh
mkdir /apps/fscheck
s31fsbench run /apps/fscheck
s31fsbench verify /apps/fscheck
```

`run`只在指定的现存绝对目录中新建`safe-random.bin`，文件存在时拒绝覆盖。文件固定800000字节（200×200×20字节），setup执行200次`fflush+fsync`；随后1000次随机读并比较数据、10次20字节随机写，单独计时最后`fflush+fsync`，全文件40000条记录校验，检查close结果。时间采用`CLOCK_MONOTONIC`。原始xTS setup直接调用`fsync`而没有显式stdio flush，原始随机写计时也未单独纳入最后提交，不能把两套计时混为一项。

`verify`只读，使用同一目标libc的确定性位置序列，检查整个文件尺寸与每个记录。完成后可以退出应用、卸载/重新挂载存储再调用它，区分进程内回读和重挂持久化。程序保留合成文件供复核，不自动删除。目录选择、原始测试、重挂和部署/回退由外部工具协调。

KERNEL配置需同时满足ELF声明栈大小和aligned-TLS上限；当前资格栈32KiB。主机测试和[Korvo资格工具](../../../../tools/board-tests/README.md)分别记录接口/坏数据拒绝与实板结果。运行水位采样不是完整峰值；真实掉电、产品共存与长期稳定另验。
