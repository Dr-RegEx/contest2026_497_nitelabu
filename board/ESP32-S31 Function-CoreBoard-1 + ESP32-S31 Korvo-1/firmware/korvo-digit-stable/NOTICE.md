# 许可与来源

NuttX/openvela 及工程代码保留 Apache-2.0 与原文件版权；许可及 NuttX/apps NOTICE 见 `licenses/`。OV3660 初始化派生自 Espressif esp32-camera `3fb41a99d61a853313d1cd5543ebf2c109ef7c0e`，保留其 MIT LICENSE/NOTICE。

MNIST 由 Yann LeCun、Corinna Cortes 提供，源于 NIST。模型为训练后量化的 INT8 784→64→10 MLP。模型权重及内置 MNIST 自检样本按原工程声明保留 CC BY-SA 3.0 归属。模型数据集 SHA256：`731c5ac602752760c8e48fbffcf8c3b850d9dc2a2aedcf2cc48468fc17b673d1`。

许可：https://creativecommons.org/licenses/by-sa/3.0/ 。原始数据集归属说明：https://github.com/keras-team/keras/blob/master/keras/src/datasets/mnist.py 。推理、预处理与汇编代码遵循 Apache-2.0。
