#ifndef RTOS_CONFIG_H
#define RTOS_CONFIG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// ============ RTOS 任务配置 ============

// 任务优先级定义 (FreeRTOS优先级: 0最低, 24最高, idleTask为0)
#define TASK_PRIORITY_WIFI       3       // WiFi HTTP 服务器任务 - 最高优先级
#define TASK_PRIORITY_MONITOR    2       // 监控状态任务
#define TASK_PRIORITY_DISPLAY    2       // 显示刷新任务
#define TASK_PRIORITY_APP        1       // 应用业务逻辑任务

// 任务堆栈大小 (字节)
#define STACK_SIZE_WIFI         (8 * 1024)      // WiFi任务: 8KB
#define STACK_SIZE_MONITOR      (4 * 1024)      // 监控任务: 4KB
#define STACK_SIZE_DISPLAY      (4 * 1024)      // 显示任务: 4KB
#define STACK_SIZE_APP          (4 * 1024)      // 应用任务: 4KB

// 任务运行核心 (ESP32有双核)
#define CORE_WIFI                1        // 核心1 - 网络通信任务
#define CORE_MONITOR             0        // 核心0 - UI/监控任务
#define CORE_DISPLAY             0        // 核心0 - 显示任务
#define CORE_APP                 1        // 核心1 - 应用业务逻辑

// ============ 状态结构和信号量配置 ============

// 系统状态结构
typedef struct {
    uint32_t uptime;        // 运行时间
    uint32_t freeHeap;      // 剩余堆内存
    uint8_t coreLoad[2];    // 核心负载
} System_Status_t;

// ============ 全局句柄 ============

extern TaskHandle_t wifiTaskHandle;
extern TaskHandle_t monitorTaskHandle;
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t appTaskHandle;
extern TaskHandle_t exampleTaskHandle;

extern SemaphoreHandle_t wifiMutex;
extern SemaphoreHandle_t sysMutex;

// ============ 任务函数声明 ============

/**
 * WiFi HTTP 服务器任务
 * 优先级最高，处理 HTTP 请求接收图像
 */
void wifiTask(void *pvParameters);

/**
 * 系统监控任务
 * 定期显示系统状态、WiFi状态、内存使用
 */
void monitorTask(void *pvParameters);

/**
 * 显示任务
 * 负责屏幕初始化与周期刷新（全部在RTOS中执行）
 */
void displayTask(void *pvParameters);

/**
 * 应用业务逻辑任务
 * 执行主要应用逻辑
 */
void appTask(void *pvParameters);

/**
 * 示例任务
 * 每2秒打印一次心跳信息，演示如何编写独立任务
 */
void exampleTask(void *pvParameters);

/**
 * 系统初始化
 * 创建任务、队列、信号量
 */
void rtosInit();

#endif // RTOS_CONFIG_H
