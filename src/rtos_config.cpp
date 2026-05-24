#include "rtos_config.h"
#include "wifi_image_manager.h"
#include "board_config.h"
#include "display_manager.h"
#include "app_version.h"

// ============ 全局句柄定义 ============
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t monitorTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t appTaskHandle = NULL;
TaskHandle_t exampleTaskHandle = NULL;

SemaphoreHandle_t wifiMutex = NULL;
SemaphoreHandle_t sysMutex = NULL;

// WiFi状态共享快照：WiFi任务写，监控/显示任务读
static WiFiImageStatus_t gWifiStatusSnapshot = {WIFI_STATE_IDLE, 0, 0, 0, 0, 0, 0};

// 外部全局 WiFi 图像管理器
extern WiFiImageManager wifiImageManager;

// 系统启动时间
static uint32_t systemStartTime = 0;

// ============ 任务实现 ============

/**
 * WiFi HTTP 服务器任务
 * 持续处理 HTTP 请求，接收图像上传
 */
void wifiTask(void *pvParameters) {
    Serial.println("[TASK-WIFI] WiFi任务已启动");
    Serial.printf("[TASK-WIFI] 运行在核心: %d\n", xPortGetCoreID());
    Serial.printf("[TASK-WIFI] 优先级: %d\n", uxTaskPriorityGet(NULL));
    
    WiFiImageStatus_t wifiStatus;
    wifiStatus.receivedBytes = 0;
    wifiStatus.totalBytes = 0;
    wifiStatus.eventCode = 0;
    wifiStatus.clientCount = 0;
    
    while (1) {
        // 处理 HTTP 请求
        wifiImageManager.handle();
        
        // 更新 WiFi 状态快照
        wifiStatus.state = wifiImageManager.getState();
        wifiStatus.progress = wifiImageManager.getProgress();
        wifiStatus.errorCode = wifiImageManager.getError();
        wifiStatus.receivedBytes = wifiImageManager.getReceivedBytes();
        wifiStatus.totalBytes = wifiImageManager.getTotalBytes();
        wifiStatus.eventCode = wifiImageManager.getLastEventCode();
        wifiStatus.clientCount = wifiImageManager.getClientCount();
        
        // 写入共享快照（单写端）
        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            gWifiStatusSnapshot = wifiStatus;
            xSemaphoreGive(wifiMutex);
        }
        
        // 让出 CPU 时间
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * 系统监控任务
 * 显示 WiFi 状态、系统状态、内存使用
 */
void monitorTask(void *pvParameters) {
    Serial.println("[TASK-MON] 监控任务已启动");
    Serial.printf("[TASK-MON] 运行在核心: %d\n", xPortGetCoreID());
    Serial.printf("[TASK-MON] 优先级: %d\n", uxTaskPriorityGet(NULL));
    
    WiFiImageStatus_t wifiStatus;
    wifiStatus.state = WIFI_STATE_IDLE;
    wifiStatus.progress = 0;
    wifiStatus.errorCode = 0;
    wifiStatus.receivedBytes = 0;
    wifiStatus.totalBytes = 0;
    wifiStatus.eventCode = 0;
    wifiStatus.clientCount = 0;
    System_Status_t sysStatus;
    uint32_t lastUpdate = 0;
    uint32_t lastWifiState = WIFI_STATE_IDLE;
    
    while (1) {
        // 每500ms更新一次
        vTaskDelay(pdMS_TO_TICKS(500));
        
        uint32_t now = millis();
        
        // 读取共享快照（多读端）
        if (xSemaphoreTake(wifiMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            wifiStatus = gWifiStatusSnapshot;
            xSemaphoreGive(wifiMutex);
        }

        // WiFi 状态改变时打印
        if (wifiStatus.state != lastWifiState) {
            lastWifiState = wifiStatus.state;
            
            Serial.println("\n[WiFi状态变更]");
            switch (wifiStatus.state) {
                case WIFI_STATE_IDLE:
                    Serial.println("  状态: 空闲");
                    break;
                case WIFI_STATE_AP_STARTING:
                    Serial.println("  状态: AP 启动中...");
                    break;
                case WIFI_STATE_AP_READY:
                    Serial.println("  状态: AP 就绪 - 等待客户端连接");
                    Serial.printf("  客户端数: %d\n", wifiStatus.clientCount);
                    break;
                case WIFI_STATE_CLIENT_CONNECTED:
                    Serial.println("  状态: 客户端已连接");
                    break;
                case WIFI_STATE_RECEIVING:
                    Serial.println("  状态: 正在接收图像...");
                    break;
                case WIFI_STATE_IMAGE_READY:
                    Serial.println("  状态: 新图像就绪 - 等待显示刷新");
                    break;
                case WIFI_STATE_ERROR:
                    Serial.printf("  状态: 错误 - 错误代码: %d\n", wifiStatus.errorCode);
                    break;
            }
        }
        
        // 接收中时定期显示进度
        if (wifiStatus.state == WIFI_STATE_RECEIVING && now - lastUpdate > 1000) {
            lastUpdate = now;
            Serial.printf("[图像接收] %d%% (%u/%u 字节)\n",
                        wifiStatus.progress,
                        wifiStatus.receivedBytes,
                        wifiStatus.totalBytes);
        }
        
        // 每5秒显示系统状态
        static uint32_t lastSysStatus = 0;
        if (now - lastSysStatus > 5000) {
            lastSysStatus = now;
            
            sysStatus.uptime = (now - systemStartTime) / 1000;
            sysStatus.freeHeap = ESP.getFreeHeap();
            
            Serial.println("\n[系统状态]");
            Serial.printf("  运行时间: %u秒\n", sysStatus.uptime);
            Serial.printf("  剩余内存: %u 字节\n", sysStatus.freeHeap);
            Serial.printf("  WiFi客户端: %d\n", wifiStatus.clientCount);
            Serial.printf("  IP地址: %s\n", wifiImageManager.getIPAddress().c_str());
            Serial.printf("  任务数: %u\n", uxTaskGetNumberOfTasks());
            
            Serial.println("\n[任务信息]");
            if (wifiTaskHandle) {
                Serial.printf("  WiFi任务 - 优先级: %d, 状态: %d\n",
                            uxTaskPriorityGet(wifiTaskHandle),
                            eTaskGetState(wifiTaskHandle));
            }
            if (monitorTaskHandle) {
                Serial.printf("  监控任务 - 优先级: %d, 状态: %d\n",
                            uxTaskPriorityGet(monitorTaskHandle),
                            eTaskGetState(monitorTaskHandle));
            }
            if (displayTaskHandle) {
                Serial.printf("  显示任务 - 优先级: %d, 状态: %d\n",
                            uxTaskPriorityGet(displayTaskHandle),
                            eTaskGetState(displayTaskHandle));
            }
            if (appTaskHandle) {
                Serial.printf("  应用任务 - 优先级: %d, 状态: %d\n",
                            uxTaskPriorityGet(appTaskHandle),
                            eTaskGetState(appTaskHandle));
            }
            if (exampleTaskHandle) {
                Serial.printf("  示例任务 - 优先级: %d, 状态: %d\n",
                            uxTaskPriorityGet(exampleTaskHandle),
                            eTaskGetState(exampleTaskHandle));
            }
            
            Serial.println();
        }
        
        // 图像就绪时刷新显示
        if (wifiStatus.state == WIFI_STATE_IMAGE_READY) {
            Serial.println("[系统] 新照片已接收，屏幕即将刷新");
            // 显示任务会自动检测 imageReady 标志
        }
    }
}

/**
 * 显示任务
 * 屏幕初始化与状态刷新全部在RTOS任务中完成
 */
void displayTask(void *pvParameters) {
    Serial.println("[TASK-DISPLAY] 显示任务已启动");
    Serial.printf("[TASK-DISPLAY] 运行在核心: %d\n", xPortGetCoreID());

    if (displayInit()) {
        Serial.println("[TASK-DISPLAY] TFT 初始化成功");
    } else {
        Serial.println("[TASK-DISPLAY] TFT 初始化失败");
    }

    // 启动时尝试恢复上次保存的照片
    displayShowBoot();

    uint32_t heartbeat = 0;
    while (1) {
        // 每 5 秒打印心跳，确认 displayTask 在运行
        heartbeat++;
        if (heartbeat % 50 == 0) {  // 50 * 100ms = 5秒
            Serial.printf("[TASK-DISPLAY] 心跳 #%u, isImageReady=%d, uploading=%d\n",
                          heartbeat / 50, (int)wifiImageManager.isImageReady(),
                          (int)g_isUploading);
        }

        // 上传中：更新进度条
        if (g_isUploading) {
            displayShowProgressBar();
        }
        // 内存屏障 + 直接 volatile 检查（不经过类方法）
        __sync_synchronize();
        if (g_newImageReady) {
            Serial.println("[TASK-DISPLAY] 🔔 新图像就绪!");
            g_newImageReady = false;
            displayReloadImage();
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms 轮询一次
    }
}

/**
 * 应用业务逻辑任务
 * 执行主要应用功能
 */
void appTask(void *pvParameters) {
    Serial.println("[TASK-APP] 应用任务已启动");
    Serial.printf("[TASK-APP] 运行在核心: %d\n", xPortGetCoreID());
    Serial.printf("[TASK-APP] 优先级: %d\n", uxTaskPriorityGet(NULL));
    
    uint32_t appCounter = 0;
    
    while (1) {
        // 应用业务逻辑 (每10秒执行一次)
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        appCounter++;
        Serial.printf("[应用] 业务逻辑执行次数: %u\n", appCounter);
        
        // 在这里添加你的应用逻辑
        // 例如: 传感器数据采集、数据处理等
    }
}

/**
 * 示例任务
 * 演示任务结构：初始化 -> 周期执行 -> vTaskDelay让出CPU
 */
void exampleTask(void *pvParameters) {
    Serial.println("[TASK-EXAMPLE] 示例任务已启动");
    Serial.printf("[TASK-EXAMPLE] 运行在核心: %d\n", xPortGetCoreID());
    Serial.printf("[TASK-EXAMPLE] 优先级: %d\n", uxTaskPriorityGet(NULL));

    uint32_t heartbeat = 0;
    bool lastBootPressed = boardBootKeyPressed();
    Serial.println("[TASK-EXAMPLE] 示例任务仅做按键与心跳演示，不再控制背光");

    while (1) {
        heartbeat++;

        const bool bootPressed = boardBootKeyPressed();
        if (bootPressed != lastBootPressed) {
            lastBootPressed = bootPressed;
            Serial.printf("[示例任务] BOOT按键: %s\n", bootPressed ? "按下" : "释放");
        }

        Serial.printf("[示例任务] 心跳=%u\n", heartbeat);

        // 任务中必须使用vTaskDelay，不要用delay()
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * RTOS系统初始化
 * 创建所有任务、队列和信号量
 */
void rtosInit() {
    Serial.println("\n========== RTOS 系统初始化 ==========");
    
    systemStartTime = millis();
    
    // ========== 创建互斥锁 ==========
    Serial.println("[初始化] 创建互斥锁...");
    
    wifiMutex = xSemaphoreCreateMutex();
    if (wifiMutex == NULL) {
        Serial.println("[错误] WiFi互斥锁创建失败!");
        return;
    }
    Serial.println("[✓] WiFi互斥锁已创建");

    // 初始化共享快照默认值
    gWifiStatusSnapshot.state = WIFI_STATE_IDLE;
    gWifiStatusSnapshot.progress = 0;
    gWifiStatusSnapshot.receivedBytes = 0;
    gWifiStatusSnapshot.totalBytes = 0;
    gWifiStatusSnapshot.errorCode = 0;
    gWifiStatusSnapshot.eventCode = 0;
    gWifiStatusSnapshot.clientCount = 0;
    
    sysMutex = xSemaphoreCreateMutex();
    if (sysMutex == NULL) {
        Serial.println("[错误] 系统互斥锁创建失败!");
        return;
    }
    Serial.println("[✓] 系统互斥锁已创建");
    
    // ========== 创建任务 ==========
    Serial.println("[初始化] 创建FreeRTOS任务...");
    
    // WiFi任务 - 核心1, 优先级3
    if (xTaskCreatePinnedToCore(
        wifiTask,                   // 任务函数
        "WiFi_Task",                // 任务名称
        STACK_SIZE_WIFI,            // 堆栈大小
        NULL,                       // 参数
        TASK_PRIORITY_WIFI,         // 优先级
        &wifiTaskHandle,            // 任务句柄
        CORE_WIFI                   // 运行核心
    ) != pdPASS) {
        Serial.println("[错误] WiFi任务创建失败!");
        return;
    }
    Serial.println("[✓] WiFi任务已创建 (核心1, 优先级3)");
    
    // 监控任务 - 核心0, 优先级2
    if (xTaskCreatePinnedToCore(
        monitorTask,                // 任务函数
        "Monitor_Task",             // 任务名称
        STACK_SIZE_MONITOR,         // 堆栈大小
        NULL,                       // 参数
        TASK_PRIORITY_MONITOR,      // 优先级
        &monitorTaskHandle,         // 任务句柄
        CORE_MONITOR                // 运行核心
    ) != pdPASS) {
        Serial.println("[错误] 监控任务创建失败!");
        return;
    }
    Serial.println("[✓] 监控任务已创建 (核心0, 优先级2)");

    // 显示任务 - 核心0, 优先级2
    if (xTaskCreatePinnedToCore(
        displayTask,                // 任务函数
        "Display_Task",            // 任务名称
        STACK_SIZE_DISPLAY,         // 堆栈大小
        NULL,                       // 参数
        TASK_PRIORITY_DISPLAY,      // 优先级
        &displayTaskHandle,         // 任务句柄
        CORE_DISPLAY                // 运行核心
    ) != pdPASS) {
        Serial.println("[错误] 显示任务创建失败!");
        return;
    }
    Serial.println("[✓] 显示任务已创建 (核心0, 优先级2)");
    
    // 应用任务 - 核心1, 优先级1
    if (xTaskCreatePinnedToCore(
        appTask,                    // 任务函数
        "App_Task",                 // 任务名称
        STACK_SIZE_APP,             // 堆栈大小
        NULL,                       // 参数
        TASK_PRIORITY_APP,          // 优先级
        &appTaskHandle,             // 任务句柄
        CORE_APP                    // 运行核心
    ) != pdPASS) {
        Serial.println("[错误] 应用任务创建失败!");
        return;
    }
    Serial.println("[✓] 应用任务已创建 (核心1, 优先级1)");

    // 示例任务 - 核心0, 优先级1
    if (xTaskCreatePinnedToCore(
        exampleTask,                // 任务函数
        "Example_Task",            // 任务名称
        3 * 1024,                   // 堆栈大小
        NULL,                       // 参数
        1,                          // 优先级
        &exampleTaskHandle,         // 任务句柄
        0                           // 运行核心
    ) != pdPASS) {
        Serial.println("[错误] 示例任务创建失败!");
        return;
    }
    Serial.println("[✓] 示例任务已创建 (核心0, 优先级1)");
    
    Serial.println("\n========== RTOS 初始化完成 ==========\n");
    Serial.println("任务分配:");
    Serial.println("  核心0: 监控任务(优先级2) + 显示任务(优先级2) + 示例任务(优先级1) + Idle任务");
    Serial.println("  核心1: WiFi任务(优先级3) + 应用任务(优先级1)");
    Serial.println();
}
