# ESP32 WiFi 照片更换 - 快速开始指南

## 🚀 项目概览

ESP32 WiFi 照片更换系统：通过 WiFi 将照片无线传输到 ESP32 并显示在 240×240 TFT 屏幕上。

## ⚡ 3分钟快速开始

### 第1步：编译并烧写

```bash
platformio run --target upload
platformio device monitor
```

启动后串口会显示：

```
╔════════════════════════════════════════╗
║   ESP32 WiFi 照片更换系统 (FreeRTOS)  ║
╚════════════════════════════════════════╝
  WiFi AP 已就绪
  SSID:     ESP32_Photo
  密码:     12345678
  IP地址:   192.168.4.1
```

### 第2步：连接 WiFi

在手机/电脑上连接 WiFi 热点：

- **SSID**: `ESP32_Photo`
- **密码**: `12345678`

### 第3步：上传照片

**方式A - 浏览器（推荐）**：

打开浏览器访问 `http://192.168.4.1`，选择照片上传。

**方式B - Python 工具**：

```bash
# 安装依赖
pip install Pillow

# 上传照片（自动转换 JPG/PNG → RGB565）
python tools/wifi_image_sender.py your_photo.jpg
```

### 第4步：查看效果

上传完成后屏幕自动刷新显示新照片！

## 📁 文件结构

```
├── src/
│   ├── main.cpp                  ← 主程序入口
│   ├── wifi_image_manager.cpp    ← WiFi HTTP 服务器
│   ├── rtos_config.cpp           ← FreeRTOS 多任务
│   └── display_manager.cpp       ← LVGL 显示管理
├── include/
│   ├── wifi_image_manager.h
│   ├── rtos_config.h
│   ├── display_manager.h
│   └── board_config.h            ← 引脚配置
├── tools/
│   ├── ota_tool.py               ← 图像转换工具
│   └── wifi_image_sender.py      ← 照片上传工具
└── platformio.ini                ← 项目配置
```

## 🛠️ 工具说明

### ota_tool.py - 图像转换

```bash
# JPG → RGB565 转换
python tools/ota_tool.py --convert photo.jpg --output photo.rgb565

# 打包为带校验头的图像包
python tools/ota_tool.py --pack photo.rgb565 --output photo.imgpkg

# 验证图像包
python tools/ota_tool.py --verify photo.imgpkg
```

### wifi_image_sender.py - 照片上传

```bash
# 自动转换并上传
python tools/wifi_image_sender.py photo.jpg

# 上传 RGB565 原始文件
python tools/wifi_image_sender.py --raw photo.rgb565

# 从图像包上传
python tools/wifi_image_sender.py --from-ota photo.imgpkg
```

## 🔧 自定义配置

### 修改 WiFi 名称/密码

在 `src/main.cpp` 中：

```cpp
wifiImageManager.init("ESP32_Photo", "12345678");
//                     ^^^^^^^^^^^^  ^^^^^^^^^
//                     SSID          密码（至少8位）
```

### 修改屏幕引脚

在 `include/board_config.h` 中修改引脚定义。

## 📊 图像规格

- 分辨率: 240 × 240 像素
- 格式: RGB565 (每像素 2 字节)
- 大小: 115,200 字节
- 字节序: 小端 (Little Endian)
