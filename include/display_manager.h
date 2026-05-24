#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>

// 图像大小
#define IMAGE_SIZE_240 115200

// SPIFFS 图像路径
#define SPIFFS_IMAGE_PATH    "/img.rgb565"

// 上传进度（0-100），由 WiFi 任务写入，显示任务读取
extern volatile uint8_t g_uploadProgress;
extern volatile bool    g_isUploading;

// 新图像就绪标志（直接 volatile，不经过类方法避免缓存问题）
extern volatile bool    g_newImageReady;

// 初始化 TFT 硬件
bool displayInit();

// 启动时显示 SPIFFS 中已保存的照片
void displayShowBoot();

// 从 SPIFFS 读取图像并推送到 TFT
void displayReloadImage();

// 上传过程中显示进度条
void displayShowProgressBar();

#endif
