# 1688 同一卷镜像空间复现

只在电脑内存中读取fs1685-scratch-before.bin，不修改原始镜像、不访问板端串口。

只移出音频和persist.db后，原版lfs.c与分块复制候选都在随机seek报告No more free space，说明该空间条件不能用于评价优化。额外移出备份中的frag1164/Fragment_test_largefile（599040字节）后，候选完成10次覆盖及全文件重挂校验。此为离线定位，不替代板端xTS。

镜像还含s19/stability_test04_file_1（237568字节），不需要触碰。板端1687进程尚在等待原始程序返回；不得抢串口、复位或丢弃RAM中的临时音频/KVDB副本。待收尾恢复后，后续可将上述599040字节碎片测试副本也临时搬到RAM并校验，完成原用例后全部恢复。

证据：inventory1688.log、existing1688.log、existing1688-baseline.log、space1688.log。
