#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "model_data.h"

// TensorFlow Lite Micro 核心库
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// 实例化传感器对象
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// --- 全局变量定义 ---
namespace {
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;

    // 分配“算盘”内存（Tensor Arena）
    const int kTensorArenaSize = 60 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
}

void setup() {
    // 1. 初始化串口通讯
    Serial.begin(115200);
    
    // 针对 Native USB 的等待逻辑
    // 如果开机没看到输出，请在打开监视器后按一下板子上的 RST
    while (!Serial) {
        delay(10);
    }

    Serial.println("\n--- 工业级 PdM 边缘诊断引擎启动 ---");

    // 2. 设置错误报告器（这是编译器要求的第 5 个关键参数）
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    // 1. 初始化 I2C（SDA=8, SCL=9）
    Wire.begin(8, 9); 

    // 2. 初始化传感器检测
    if(!accel.begin()) {
        Serial.println("❌ 未检测到 ADXL345，请检查接线！");
        while(1);
    }
    accel.setRange(ADXL345_RANGE_16_G); // 设置量程，工业监测建议用 16G

    // 3. 加载 AI 模型大脑
    model = tflite::GetModel(g_pdm_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        TF_LITE_REPORT_ERROR(error_reporter, "❌ 错误：模型版本不匹配！");
        return;
    }

    // 4. 准备运算解析器（翻译官）
    static tflite::AllOpsResolver resolver;

    // 5. 实例化解释器（核心步骤：这里补齐了 7 个参数）
    static tflite::MicroInterpreter static_interpreter(
        model,              // 1. 模型大脑
        resolver,           // 2. 算子解析器
        tensor_arena,       // 3. 运算空间（草稿纸）
        kTensorArenaSize,   // 4. 空间大小
        error_reporter,     // 5. 报错员 (必填)
        nullptr,            // 6. 资源变量 (传空)
        nullptr             // 7. 性能分析器 (传空)
    );
    interpreter = &static_interpreter;

    // 6. 为张量分配内存
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(error_reporter, "❌ 错误：分配张量内存失败！");
        return;
    }

    // 7. 关联输入端口（模型接收 128 个数据点的地方）
    input = interpreter->input(0);

    Serial.println("✅ AI 引擎初始化成功，准备进行推理！");
    Serial.print("📊 期望输入点数: ");
    Serial.println(input->dims->data[1]); // 验证是否为你训练时的 128
}

void loop() {
    // --- 模拟传感器数据输入 ---
    // 目前我们先用 0.5f 填满 128 个点
    // 等下一阶段接好 ADXL345 传感器，我们就把真实数据塞进这里
    //for (int i = 0; i < 128; i++) {
    //    input->data.f[i] = 0.5f; 
    //}

    // 3. 实时采样：填满 128 个数据点
    Serial.println("📡 正在采集震动特征...");
    for (int i = 0; i < 128; i++) {
        // 1. 获取 Adafruit 库的 m/s² 数据
        sensors_event_t event;
        accel.getEvent(&event);
        
        // 2. 换算为 g，并 💥减去同样的基准线💥
        // 确保这里的 1.1168f 和你在 Python 里减去的数字一模一样！
        float raw_z = (event.acceleration.z / 9.80665f) - 1.1168f; 
        
        // 💥 关键调试：只打印前 10 个点，看看数据的真实波动
        if (i < 10) {
            Serial.print(raw_z, 4); 
            Serial.print(" ");
        }   

        // 3. 喂给 AI
        input->data.f[i] = raw_z;

        delay(10); // 采样频率约为 100Hz
    }
    Serial.println(); // 换行

    // --- 执行 AI 推理 ---
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        Serial.println("❌ 警告：推理过程出错！");
        return;
    }

    // --- 读取诊断结果 ---
    // 假设输出是一个 0~1 之间的故障概率
    float result = interpreter->output(0)->data.f[0];

    Serial.print("🤖 诊断报告 -> 故障概率: ");
    Serial.print(result * 100, 2); // 转换为百分比
    Serial.println("%");

    if(result > 0.8f) {
        Serial.println("⚠️ 警告：检测到异常震动趋势！");
    }

    // 工业采样间隔，暂时设为 2 秒一次
    delay(2000);
}