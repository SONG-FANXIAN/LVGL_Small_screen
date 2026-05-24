#ifndef WIFI_IMAGE_MANAGER_H
#define WIFI_IMAGE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "display_manager.h"  // IMAGE_SIZE_240, SPIFFS_IMAGE_PATH

// ================================
// WiFi 图像管理器
// 通过 WiFi AP + HTTP 接收照片并更新屏幕显示
// ================================

// WiFi AP 默认配置
#define WIFI_AP_SSID         "ESP32_Photo"
#define WIFI_AP_PASSWORD     "12345678"
#define WIFI_AP_CHANNEL      6
#define WIFI_AP_MAX_CLIENTS  2

// HTTP 服务器端口
#define HTTP_SERVER_PORT     80

// SPIFFS 图像存储路径（定义在 display_manager.h）
// #define SPIFFS_IMAGE_PATH    "/img.rgb565"

// IMAGE_SIZE_240 定义在 display_manager.h 中

// ============ 状态定义 ============

typedef enum {
    WIFI_STATE_IDLE = 0,          // 空闲
    WIFI_STATE_AP_STARTING = 1,   // AP 启动中
    WIFI_STATE_AP_READY = 2,      // AP 就绪，等待连接
    WIFI_STATE_CLIENT_CONNECTED = 3, // 有客户端连接
    WIFI_STATE_RECEIVING = 4,     // 正在接收图像
    WIFI_STATE_IMAGE_READY = 5,   // 新图像就绪（等待显示刷新）
    WIFI_STATE_ERROR = 6          // 错误
} WiFiImageState_t;

// ============ 状态快照（RTOS 任务间共享） ============

typedef struct {
    WiFiImageState_t state;
    uint8_t progress;           // 接收进度 0-100%
    uint32_t receivedBytes;
    uint32_t totalBytes;
    uint8_t errorCode;
    uint16_t eventCode;         // 最近事件码
    uint8_t clientCount;        // 当前连接的客户端数
} WiFiImageStatus_t;

class WiFiImageManager {
public:
    WiFiImageManager();
    ~WiFiImageManager();

    // 初始化 WiFi AP + HTTP 服务器
    void init(const char* ap_ssid = WIFI_AP_SSID,
              const char* ap_password = WIFI_AP_PASSWORD);

    // 处理 HTTP 请求（由 RTOS 任务周期性调用）
    void handle();

    // 获取当前状态
    WiFiImageState_t getState() const { return state; }

    // 获取接收进度（0-100%）
    uint8_t getProgress() const;

    // 获取错误代码
    uint8_t getError() const { return lastError; }

    // 获取最近事件码
    uint16_t getLastEventCode() const { return lastEventCode; }

    // 获取已接收字节数
    uint32_t getReceivedBytes() const { return receivedBytes; }

    // 获取图像总字节数
    uint32_t getTotalBytes() const { return totalBytes; }

    // 获取连接的客户端数量
    uint8_t getClientCount() const;

    // 判断是否有新图像就绪
    bool isImageReady() const { return imageReady; }

    // 清除图像就绪标志（由显示任务调用）
    void clearImageFlag() { imageReady = false; state = WIFI_STATE_AP_READY; }

    // 获取本地 IP 地址字符串
    String getIPAddress() const;

private:
    WiFiServer* server;
    WiFiClient  currentClient;
    WiFiImageState_t state;
    uint8_t lastError;
    uint32_t receivedBytes;
    uint32_t totalBytes;
    uint16_t lastEventCode;
    volatile bool imageReady;       // 多核共享，必须 volatile
    uint32_t lastDataMillis;

    // 图像接收缓冲区
    static const size_t RX_BUFFER_SIZE = 4096;
    uint8_t rxBuffer[RX_BUFFER_SIZE];

    // SPIFFS 文件句柄
    File spiffsFile;

    // 内部方法
    void logEvent(uint16_t eventCode, const char* tag, const char* msg);
    void startReceiving(uint32_t size);
    void finishReceiving(bool success);
    void resetState();
};

#endif // WIFI_IMAGE_MANAGER_H
