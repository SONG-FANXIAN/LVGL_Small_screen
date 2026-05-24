# ESP32 WiFi 照片更换系统

## 功能介绍

通过 WiFi 无线更换 ESP32 TFT 屏幕上显示的照片。

### 主要特性

- ✅ WiFi AP 模式 - 无需路由器，ESP32 自建热点
- ✅ HTTP 上传 - 浏览器或 Python 工具上传照片
- ✅ 自动转换 - JPG/PNG 自动转为 RGB565
- ✅ 实时刷新 - 上传后屏幕自动更新
- ✅ SPIFFS 存储 - 照片持久化保存
- ✅ FreeRTOS 多任务 - WiFi/显示/监控分离

## 系统架构

### 工作流程

```
[手机/电脑] ──WiFi──→ [ESP32 AP] ──HTTP──→ [接收照片]
                                              ↓
                                         [SPIFFS 存储]
                                              ↓
[屏幕显示] ←──── LVGL刷新 ←──── [Display_Task]
```

### WiFi 配置

| 参数       | 默认值      |
| ---------- | ----------- |
| AP SSID    | ESP32_Photo |
| AP 密码    | 12345678    |
| IP 地址    | 192.168.4.1 |
| HTTP 端口  | 80          |
| 最大客户端 | 2           |

## 使用方法

### 1. 烧写固件

```bash
platformio run --target upload
platformio device monitor
```

### 2. 连接 WiFi

连接到 `ESP32_Photo`（密码 `12345678`）

### 3. 上传照片

**浏览器方式**：访问 `http://192.168.4.1`

**Python 工具**：

```bash
pip install Pillow
python tools/wifi_image_sender.py photo.jpg
```

## HTTP API

### POST /upload

上传 RGB565 原始图像数据。

- **Content-Type**: `application/octet-stream`
- **Body**: 115200 字节 RGB565 数据
- **响应 200**: `照片上传成功！屏幕将自动更新。`

### GET /

返回 HTML 上传页面，可直接在浏览器中选择文件上传。

## 图像格式

| 参数   | 值                   |
| ------ | -------------------- |
| 分辨率 | 240 × 240 像素       |
| 格式   | RGB565               |
| 大小   | 115,200 字节         |
| 字节序 | 小端 (Little Endian) |
| R 分量 | 5 bits (高5位)       |
| G 分量 | 6 bits (中6位)       |
| B 分量 | 5 bits (低5位)       |

## 多任务架构

| 任务         | 优先级 | 核心 | 功能          |
| ------------ | ------ | ---- | ------------- |
| WiFi_Task    | 3      | 1    | HTTP 服务器   |
| Monitor_Task | 2      | 0    | 系统监控      |
| Display_Task | 2      | 0    | LVGL 屏幕刷新 |
| App_Task     | 1      | 1    | 应用业务      |

## 故障排查

| 问题                 | 解决方案                             |
| -------------------- | ------------------------------------ |
| 找不到 WiFi 热点     | 检查 ESP32 串口输出，确认 AP 已启动  |
| 无法连接 192.168.4.1 | 确认电脑/手机已连接 ESP32_Photo WiFi |
| 上传后屏幕无变化     | 检查图像是否为 240×240，查看串口日志 |
| 颜色显示异常         | 检查 RGB565 字节序（小端）           |
