# LVGL_Small_screen

> ESP32 WiFi 照片更换系统 (FreeRTOS版) — 通过 WiFi 无线更换屏幕上显示的照片

## 📋 项目介绍

基于 **ESP32 双核 + FreeRTOS + LVGL** 的无线照片更换系统。ESP32 自建 WiFi 热点，手机或电脑连接后可通过浏览器或 Python 工具上传照片，自动转换为 RGB565 格式并实时显示在 240×240 TFT 屏幕上。

- ✅ WiFi AP 模式 — 无需路由器，ESP32 自建热点
- ✅ HTTP 照片上传 — 浏览器/Python 工具上传
- ✅ 自动格式转换 — JPG/PNG → RGB565 自动转换
- ✅ 实时显示刷新 — 上传后自动更新屏幕
- ✅ SPIFFS 持久存储 — 断电后照片不丢失
- ✅ FreeRTOS 多任务 — WiFi/显示/监控分离

## 🏗️ 软件架构

```
┌─────────────────────────────────────────────────┐
│                 FreeRTOS 任务调度器               │
├──────────────────────┬──────────────────────────┤
│   核心0 (PRO_CPU)    │     核心1 (APP_CPU)       │
├──────────────────────┼──────────────────────────┤
│   Display_Task (pri2)│     WiFi_Task (pri3)      │
│   LVGL 屏幕刷新      │     HTTP 服务器接收照片    │
├──────────────────────┼──────────────────────────┤
│   Monitor_Task (pri2)│     App_Task (pri1)       │
│   系统状态监控       │     应用业务逻辑           │
└──────────────────────┴──────────────────────────┘
```

**任务详情：**

| 任务         | 优先级   | 核心 | 堆栈 | 功能                      |
| ------------ | -------- | ---- | ---- | ------------------------- |
| WiFi_Task    | 3 (最高) | 1    | 8KB  | HTTP 服务器，接收照片上传 |
| Monitor_Task | 2        | 0    | 4KB  | 系统状态监控，串口输出    |
| Display_Task | 2        | 0    | 4KB  | LVGL 屏幕刷新，图像显示   |
| App_Task     | 1        | 1    | 4KB  | 应用业务逻辑              |

**工作流程：**

```
[手机/电脑] ──WiFi──→ [ESP32 AP] ──HTTP──→ [接收照片] → [SPIFFS] → [TFT 屏幕]
```

## 🛠️ 安装教程

### 1. 克隆仓库

```bash
git clone https://gitee.com/asdsongfanxianjiazu/LVGL_Small_screen.git
```

### 2. 安装 PlatformIO

推荐使用 VS Code + PlatformIO 插件，或命令行安装：

```bash
pip install platformio
```

### 3. 编译烧写

```bash
# 编译并烧写到 ESP32
platformio run --target upload

# 查看串口输出
platformio device monitor
```

### 4. 安装 Python 工具依赖（可选）

```bash
pip install Pillow
```

## 📖 使用说明

### 方式一：浏览器上传（推荐）

1. ESP32 上电后，在手机/电脑上连接 WiFi 热点：**ESP32_Photo**（密码 `12345678`）
2. 打开浏览器访问 **http://192.168.4.1**
3. 选择照片文件，点击「上传照片」
4. 屏幕自动刷新显示新照片

### 方式二：Python 工具上传

```bash
# 自动转换 JPG/PNG 并上传
python tools/wifi_image_sender.py your_photo.jpg

# 上传 RGB565 原始文件
python tools/wifi_image_sender.py --raw photo.rgb565
```

### 图像转换工具

```bash
# JPG → RGB565 转换
python tools/ota_tool.py --convert photo.jpg --output photo.rgb565

# 打包为带校验头的图像包
python tools/ota_tool.py --pack photo.rgb565 --output photo.imgpkg

# 验证图像包
python tools/ota_tool.py --verify photo.imgpkg
```

### WiFi 配置

| 参数      | 默认值      |
| --------- | ----------- |
| AP SSID   | ESP32_Photo |
| AP 密码   | 12345678    |
| IP 地址   | 192.168.4.1 |
| HTTP 端口 | 80          |

### 硬件引脚

| 引脚   | 功能               |
| ------ | ------------------ |
| GPIO14 | TFT SCLK (SPI时钟) |
| GPIO15 | TFT MOSI (SPI数据) |
| GPIO5  | TFT CS (片选)      |
| GPIO27 | TFT DC (数据/命令) |
| GPIO33 | TFT RST (复位)     |
| GPIO22 | TFT BLK (背光)     |
| GPIO0  | BOOT 按键          |

## 📁 项目结构

```
├── src/
│   ├── main.cpp                  # 主程序入口
│   ├── wifi_image_manager.cpp    # WiFi HTTP 服务器
│   ├── rtos_config.cpp           # FreeRTOS 多任务
│   └── display_manager.cpp       # LVGL 显示管理
├── include/
│   ├── wifi_image_manager.h
│   ├── rtos_config.h
│   ├── display_manager.h
│   ├── board_config.h            # 引脚配置
│   ├── lv_conf.h                 # LVGL 配置
│   └── app_version.h             # 固件版本
├── tools/
│   ├── ota_tool.py               # 图像转换工具
│   └── wifi_image_sender.py      # 照片上传工具
├── docs/
│   ├── KNOWLEDGE_POINTS.md       # 知识点总结
│   ├── RTOS_ARCHITECTURE.md      # RTOS 架构说明
│   ├── FREERTOS_QUICK_REFERENCE.md # FreeRTOS 快速参考
│   └── OTA_README.md             # OTA 说明
├── platformio.ini                # PlatformIO 配置
├── PROJECT_SUMMARY.md            # 项目总结
└── QUICK_START.md                # 快速开始
```

## 🤝 参与贡献

1. Fork 本仓库
2. 新建 `Feat_xxx` 分支
3. 提交代码
4. 新建 Pull Request

## 📄 许可证

本项目仅供学习交流使用。
