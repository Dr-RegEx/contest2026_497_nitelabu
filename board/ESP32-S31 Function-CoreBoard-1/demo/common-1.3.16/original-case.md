##### 1.3.16 RNG功能测试


**测试目的：** 一、通用自测用例 > 1、功能测试 > 1.3 驱动BSP

**前提条件：**

1、打开如下配置：

```
CONFIG_TESTING_NIST_STS=y
CONFIG_TESTING_DRIVER_TEST=y
CONFIG_CMOCKA=y
```

2、打开上述配置后进行编译
3、烧录刚刚编译的bin包
4、打开nsh窗口

**步骤：**

1、按下述方法执行：
cd /tmp
mkdir -p experiments/AlgorithmTesting/ApproximateEntropy
mkdir -p experiments/AlgorithmTesting/CumulativeSums
mkdir -p experiments/AlgorithmTesting/Frequency
mkdir -p experiments/AlgorithmTesting/LongestRun
mkdir -p experiments/AlgorithmTesting/OverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursionsVariant
mkdir -p experiments/AlgorithmTesting/Runs
mkdir -p experiments/AlgorithmTesting/Universal
mkdir -p experiments/AlgorithmTesting/BlockFrequency
mkdir -p experiments/AlgorithmTesting/FFT
mkdir -p experiments/AlgorithmTesting/LinearComplexity
mkdir -p experiments/AlgorithmTesting/NonOverlappingTemplate
mkdir -p experiments/AlgorithmTesting/RandomExcursions
mkdir -p experiments/AlgorithmTesting/Rank
mkdir -p experiments/AlgorithmTesting/Serial
nist_sts 400000
2、进入测试程序后依次输入：
0
/dev/urandom/
1
0
10
1
3、查看结果：cat /tmp/experiments/AlgorithmTesting/finalAnalysisReport.txt

**预期结果：**

P-Value是全部测试结果的卡方分布的累计值，其值大于0.0001即可认为该随机数样本具有足够的均匀性与独立性，当值不够随机时在值后以及测试项名称前会有/*标记该项

---
