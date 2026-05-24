#include <Arduino.h>
#include <SPIFFS.h>
#include "wifi_image_manager.h"
#include "rtos_config.h"
#include "board_config.h"
#include "app_version.h"

// 创建全局 WiFi 图像管理器实例
WiFiImageManager wifiImageManager;

void setup() {
    // 初始化串口
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n╔════════════════════════════════════════╗");
    Serial.println("║   ESP32 WiFi 照片更换系统 (FreeRTOS)  ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.printf("当前固件版本: %d\n", FIRMWARE_VERSION);
    Serial.printf("芯片型号: %s\n", ESP.getChipModel());
    Serial.printf("核心数: %d\n", portNUM_PROCESSORS);
    Serial.printf("总堆内存: %u 字节\n", ESP.getHeapSize());
    Serial.printf("剩余堆内存: %u 字节\n\n", ESP.getFreeHeap());

    // ========== 初始化板级引脚 ==========
    boardPinInit();
    Serial.println("[初始化] 板级引脚初始化完成");
    Serial.printf("  TFT: SCLK=%d MOSI=%d CS=%d DC=%d RST=%d\n",
                  PIN_TFT_SCLK, PIN_TFT_MOSI, PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
    Serial.printf("  BLK=%d BOOT=%d\n\n", PIN_TFT_BLK, PIN_BOOT_KEY);
    
    // ========== 提前初始化 SPIFFS（格式化后保证后续挂载无竞争）==========
    Serial.println("[初始化] 挂载 SPIFFS 文件系统...");
    if (!SPIFFS.begin(true)) {
        Serial.println("[错误] SPIFFS 挂载失败！照片存储功能不可用");
    } else {
        Serial.printf("[✓] SPIFFS 已挂载，总空间: %u 字节，已用: %u 字节\n",
                      SPIFFS.totalBytes(), SPIFFS.usedBytes());
    }
    
    // ========== 初始化 WiFi 图像管理器 ==========
    Serial.println("[初始化] 启动 WiFi 图像管理器...");
    wifiImageManager.init("ESP32_Photo", "12345678");
    Serial.println("[✓] WiFi 图像管理器初始化完成\n");
    
    // ========== 初始化 FreeRTOS 系统 ==========
    rtosInit();
    
    Serial.println("[✓] 系统初始化完成! FreeRTOS调度器已启动...\n");
}

void loop() {
    // 在 FreeRTOS 环境中，loop() 函数由 Arduino 框架自动创建为一个最低优先级的 Idle 任务
    // 多任务已接管所有逻辑，loop() 空转即可，不需要延迟
}


