# TinyML 降维打击 —— 电机故障实时诊断 Demo

![GitHub top language](https://img.shields.io/github/languages/top/cheruidonglu
TinyML-Real-time-diagnosis-of-motor-faults/)
![Model Size](https://img.shields.io/badge/Model_Size-10.73_KB-brightgreen)

## 📺 Demo 展示
本项目展示了如何将一个庞大的深度学习模型压缩 100 倍，并部署在 ESP32-S3 上进行实时电机震动分析。

**点击下方链接查看实验演示：**
[Bilibili 实验演示视频](https://www.bilibili.com/video/BV1EE5s6MEJ4/?vd_source=52c17eaae57ab39fcec5c80e054aff62)

---

## 🚀 项目亮点：为什么叫“降维打击”？
我们的原始模型接近 1MB，无法在资源受限的单片机上流畅运行。
通过 **Global Average Pooling (GAP)** 技术替换全连接层，我们实现了：
* **模型压缩：** 从 ~1,000,000 参数减少到约 1,600 个参数。
* **极致体积：** TFLite 二进制模型仅 **10.73 KB**。
* **实时性：** 在 ESP32-S3 上推理延迟几乎可忽略不计，实现真正的边缘实时诊断。

---

## 🛠 实验工具
### 硬件栈
* **MCU:** ESP32-S3 (DevKitC-1)
* **传感器:** ADXL345 三轴加速度计 (I2C 通讯)
* **动力源:** 小型直流电机 + 偏心负载（螺钉模拟故障）

### 软件栈
* **训练:** TensorFlow 2.x + Keras (Jupyter Notebook)
* **部署:** TensorFlow Lite Micro
* **开发环境:** VS Code + PlatformIO

---

## 📐 实验步骤

### 1. 数据采集与特征工程
利用 `tools/main_record.bak` 采集电机在正常与故障（绑螺钉）状态下的 Z 轴加速度数据。通过 FFT 观察到故障状态下的频谱能量明显向低频偏移。

### 2. 模型手术 (Model Surgery)
在 `notebooks/Phase6_retrain.ipynb` 中，我们将模型结构调整为：
`Conv1D -> MaxPooling -> GlobalAveragePooling -> Dense(1, Softmax)`
GAP 层直接对特征图求平均，砍掉了 99% 的全连接层参数。

### 3. 边缘部署
将生成的 `model_data.h` 放入工程，通过 `main.cpp` 调用 TFLite Micro 解释器。

---

## 💻 如何运行
1. 安装 [PlatformIO](https://platformio.org/) 插件。
2. 克隆本仓库。
3. 将 ADXL345 的 SDA 连至 GPIO 8，SCL 连至 GPIO 9。
4. 点击 PlatformIO: Upload 烧录程序。
5. 打开串口监视器 (115200 baud)，观察 AI 预测概率。

```cpp
// 核心推理逻辑片段
interpreter->Invoke();
float fault_probability = output->data.f[0];
Serial.printf("诊断报告 > 故障概率: %.2f%%\n", fault_probability * 100);