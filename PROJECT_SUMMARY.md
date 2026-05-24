# 项目总结：ESP32 WiFi 照片更换系统 (FreeRTOS版)

## 📋 项目概况

**项目名称**: ESP32 WiFi 照片更换系统
**框架**: Arduino + FreeRTOS
**芯片**: ESP32 (双核)
**功能**: 通过 WiFi 无线更换屏幕上显示的照片

## 🎯 系统功能

- ✅ **WiFi AP 模式** - ESP32 自建热点，无需路由器
- ✅ **HTTP 照片上传** - 浏览器/Python 工具上传照片
- ✅ **自动格式转换** - JPG/PNG → RGB565 自动转换
- ✅ **实时显示刷新** - 上传后自动更新屏幕
- ✅ **网页上传界面** - 手机/电脑浏览器直接访问

## 📁 项目结构

```
├── src/
│   ├── main.cpp                  # 主程序
│   ├── wifi_image_manager.cpp    # WiFi 图像管理器
│   ├── rtos_config.cpp           # RTOS 多任务
│   └── display_manager.cpp       # 显示管理器 (LVGL)
├── include/
│   ├── wifi_image_manager.h
│   ├── rtos_config.h
│   ├── display_manager.h
│   ├── board_config.h
│   ├── lv_conf.h
│   └── app_version.h
├── tools/
│   ├── ota_tool.py               # 图像转换工具
│   └── wifi_image_sender.py      # WiFi 上传工具
├── platformio.ini
└── docs/
```

## 🏗️ 多任务架构

| 任务         | 优先级 | 核心 | 功能                |
| ------------ | ------ | ---- | ------------------- |
| WiFi_Task    | 3      | 1    | HTTP 服务器接收照片 |
| Monitor_Task | 2      | 0    | 显示状态、系统信息  |
| Display_Task | 2      | 0    | LVGL 屏幕刷新       |
| App_Task     | 1      | 1    | 应用业务逻辑        |

## 🔄 照片更换流程

```
1. ESP32 启动 WiFi AP (ESP32_Photo)
2. 手机/电脑连接 WiFi
3. 浏览器访问 http://192.168.4.1 上传照片
4. ESP32 接收 → 保存 SPIFFS → 自动刷新屏幕
```

## 🚀 快速开始

```bash
# 编译烧写
platformio run --target upload

# 连接 WiFi: ESP32_Photo / 12345678
# 浏览器打开: http://192.168.4.1

# 或使用 Python 工具上传
python tools/wifi_image_sender.py photo.jpg
```
