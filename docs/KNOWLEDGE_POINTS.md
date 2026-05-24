# ESP32 WiFi 照片更换系统 — 知识点总结

> 基于项目全部源码和文档的全面知识梳理

---

## 一、嵌入式硬件与接口

| 知识点           | 具体内容                                                          |
| ---------------- | ----------------------------------------------------------------- |
| **MCU 平台**     | ESP32，双核 Xtensa LX6 处理器，`portNUM_PROCESSORS = 2`           |
| **显示屏**       | GC9A01 圆形/方形 TFT 屏，240×240 分辨率，SPI 接口                 |
| **SPI 引脚映射** | `SCLK=14, MOSI=15, CS=5, DC=27, RST=33, BLK=22`                   |
| **SPI 频率**     | 40 MHz（`SPI_FREQUENCY=40000000`）                                |
| **背光控制**     | 通过 GPIO22 控制，支持高低电平极性可配置（`TFT_BLK_ACTIVE_HIGH`） |
| **按键输入**     | BOOT 按键（GPIO0），外接上拉，按下接地                            |
| **内存资源**     | 总堆内存约 300KB+，多任务按需分配（WiFi 8KB + 其他各 4KB）        |

---

## 二、FreeRTOS 多任务调度

| 知识点         | 具体内容                                                                |
| -------------- | ----------------------------------------------------------------------- |
| **任务划分**   | 4 个用户任务：`WiFi_Task`、`Monitor_Task`、`Display_Task`、`App_Task`   |
| **优先级设计** | WiFi 任务最高（3），显示/监控中等（2），应用最低（1），空闲任务（0）    |
| **核心绑定**   | `xTaskCreatePinnedToCore()` — 核心0 运行显示+监控，核心1 运行 WiFi+应用 |
| **堆栈分配**   | WiFi 8KB，其余各 4KB                                                    |
| **任务同步**   | 互斥锁（`xSemaphoreCreateMutex`）保护 WiFi 状态快照的「单写多读」       |
| **时间管理**   | `vTaskDelay(pdMS_TO_TICKS(...))` 让出 CPU，`millis()` 跟踪超时          |

### 任务架构图

```
优先级 3  ┌──────────────────────┐ 核心1
(最高)   │  WiFi_Task           │ HTTP 请求处理，保证不丢数据
         └──────────────────────┘

优先级 2  ┌──────────────────────┐ 核心0
         │  Monitor_Task        │ 状态显示 + 系统监控
         │  Display_Task        │ LVGL 屏幕刷新
         └──────────────────────┘

优先级 1  ┌──────────────────────┐ 核心1
         │  App_Task            │ 应用业务，低优先级
         └──────────────────────┘

优先级 0  ┌──────────────────────┐ 核心0,1
(最低)   │  Idle_Task           │ 空闲回收 + Arduino loop()
         └──────────────────────┘
```

### 常用 FreeRTOS API

| 函数                                    | 功能                   |
| --------------------------------------- | ---------------------- |
| `xTaskCreatePinnedToCore()`             | 创建任务并绑定指定核心 |
| `vTaskDelay(pdMS_TO_TICKS(ms))`         | 延迟 N 毫秒并让出 CPU  |
| `xSemaphoreCreateMutex()`               | 创建互斥锁             |
| `xSemaphoreTake()` / `xSemaphoreGive()` | 获取/释放互斥锁        |
| `xPortGetCoreID()`                      | 获取当前运行核心 ID    |
| `uxTaskPriorityGet()`                   | 获取任务优先级         |
| `eTaskGetState()`                       | 获取任务运行状态       |
| `uxTaskGetNumberOfTasks()`              | 获取系统任务总数       |

---

## 三、WiFi 与 HTTP 服务器

| 知识点          | 具体内容                                                                                     |
| --------------- | -------------------------------------------------------------------------------------------- |
| **WiFi 模式**   | AP 模式（`WiFi.softAP()`），无需路由器，ESP32 自建热点                                       |
| **热点参数**    | SSID: `ESP32_Photo`，密码: `12345678`，信道: 6，最大客户端: 2                                |
| **IP 地址**     | 固定 `192.168.4.1`                                                                           |
| **HTTP 服务器** | 基于 `WiFiServer`/`WiFiClient` 手动解析 HTTP 协议                                            |
| **请求解析**    | 逐行读取请求头，`parseContentLength()` 提取 `Content-Length`                                 |
| **POST 接收**   | 接收 `Content-Type: application/octet-stream`，直接写入 SPIFFS                               |
| **GET 响应**    | 返回内嵌 HTML5 上传页面（含 Canvas 客户端缩放）                                              |
| **诊断端点**    | `GET /verify` 返回 SPIFFS 文件系统状态                                                       |
| **超时保护**    | 接收超过 30 秒无数据则复位状态                                                               |
| **状态机**      | 7 个状态：IDLE → AP_STARTING → AP_READY → CLIENT_CONNECTED → RECEIVING → IMAGE_READY → ERROR |

### HTTP API

| 方法           | 路径   | 说明                                    |
| -------------- | ------ | --------------------------------------- |
| `GET /`        | 根路径 | 返回 HTML 上传页面（Canvas 客户端缩放） |
| `POST /upload` | 上传   | 接收 115,200 字节 RGB565 原始图像数据   |
| `GET /verify`  | 诊断   | 返回 SPIFFS 文件系统状态信息            |

---

## 四、图像处理

| 知识点           | 具体内容                                                           |
| ---------------- | ------------------------------------------------------------------ | --------- | --- |
| **RGB565 格式**  | 16 位色深：R(5bit) G(6bit) B(5bit)，小端字节序                     |
| **图像尺寸**     | 固定 240×240，`115,200 字节`（= 240 × 240 × 2 BPP）                |
| **色彩转换公式** | `R5 = R8 >> 3`, `G6 = G8 >> 2`, `B5 = B8 >> 3`                     |
| **像素合成**     | `rgb565 = (R5 << 11)                                               | (G6 << 5) | B5` |
| **客户端转换**   | HTML5 Canvas + JavaScript `getImageData` 在浏览器端完成 JPG→RGB565 |
| **Python 转换**  | Pillow 库 `Image.resize(LANCZOS)` + 像素级 RGB565 转换             |
| **分块推送**     | 4KB 分块从 SPIFFS 读取 → `tft.pushColors()` 推送，避免大内存分配   |
| **CRC32 校验**   | `ota_tool.py` 支持对图像包添加 CRC32 校验头（魔数 `0xAABB5650`）   |

### RGB565 数据布局

```
字节0   字节1   字节2   字节3   ...
┌────────────┬────────────┬───
│ RRRRRGGG  │ GGGBBBBB  │ ...   (小端序: 低字节在前)
└────────────┴────────────┴───
 低8位                 高8位
```

---

## 五、SPIFFS 文件系统

| 知识点       | 具体内容                                                        |
| ------------ | --------------------------------------------------------------- |
| **挂载**     | `SPIFFS.begin(true)` — `true` 表示不存在时自动格式化            |
| **存储路径** | `/img.rgb565` — 单文件持久化存储当前照片                        |
| **文件写入** | 边接收边写入（流式），配合 `g_uploadProgress` 进度追踪          |
| **文件校验** | 读取后校验大小是否为 115,200 字节，避免显示残缺图像             |
| **分区表**   | `huge_app.csv` — 最大化应用程序空间                             |
| **诊断能力** | `/verify` 端点返回文件是否存在、大小、前 8 字节内容、数据多样性 |

---

## 六、显示驱动（TFT_eSPI + LVGL）

| 知识点         | 具体内容                                                     |
| -------------- | ------------------------------------------------------------ |
| **TFT_eSPI**   | Bodmer 的 TFT 驱动库，通过 `build_flags` 宏配置引脚/分辨率   |
| **LVGL**       | v8.3.11 轻量 GUI 库，配置 `LV_COLOR_DEPTH=16`（RGB565）      |
| **直接写屏**   | 绕过 LVGL 控件层，直接使用 `tft.pushColors()` 实现最高帧率   |
| **增量进度条** | 仅擦除/重绘变化区域（旧百分比文字 + 新增绿色填充），减少闪烁 |
| **SPI 事务**   | `tft.startWrite()` / `tft.endWrite()` 包裹 SPI 批量操作      |
| **启动恢复**   | 开机时自动从 SPIFFS 恢复上次保存的照片                       |

### PlatformIO build_flags 配置

```ini
-D GC9A01_DRIVER=1          # 驱动芯片型号
-D TFT_WIDTH=240            # 屏幕宽度
-D TFT_HEIGHT=240           # 屏幕高度
-D TFT_SCLK=14              # SPI 时钟引脚
-D TFT_MOSI=15              # SPI 数据引脚
-D TFT_CS=5                 # SPI 片选引脚
-D TFT_DC=27                # 数据/命令引脚
-D TFT_RST=33               # 复位引脚
-D SPI_FREQUENCY=40000000   # SPI 时钟频率 40MHz
-D LV_COLOR_DEPTH=16        # LVGL 16 位色深
-D LV_USE_SJPG=1            # 启用 JPEG 解码
```

---

## 七、Python 工具链

| 知识点                                | 具体内容                                                                  |
| ------------------------------------- | ------------------------------------------------------------------------- |
| **图像转换** (`ota_tool.py`)          | JPG/PNG → RGB565 原始文件、带 CRC32 校验的 `.imgpkg` 打包/验证            |
| **照片上传** (`wifi_image_sender.py`) | `http.client.HTTPConnection` POST 二进制数据到 ESP32                      |
| **CLI 设计**                          | `argparse` 子命令（`--convert`/`--pack`/`--verify`/`--raw`/`--from-ota`） |
| **错误处理**                          | 连接失败时给出排查提示（检查 WiFi 连接、IP 地址、设备状态）               |
| **依赖**                              | Pillow（图像处理）+ 标准库（http.client, argparse, zlib, struct）         |

---

## 八、软件架构与设计模式

| 知识点         | 具体内容                                                        |
| -------------- | --------------------------------------------------------------- |
| **PlatformIO** | `espressif32` 平台 + `arduino` 框架，统一依赖管理（`lib_deps`） |
| **状态机模式** | `WiFiImageState_t` 枚举 7 个状态，驱动 WiFi 管理器行为          |
| **快照模式**   | WiFi 任务写入 `WiFiImageStatus_t` 快照，其他任务通过互斥锁读取  |
| **关注点分离** | 4 个独立 `.h/.cpp` 模块：显示、WiFi、RTOS、板级配置             |
| **版本管理**   | `FIRMWARE_VERSION` 宏统一管理固件版本                           |
| **setup/loop** | `setup()` 做全部初始化并启动调度器，`loop()` 保持空转           |

---

## 九、值得注意的细节技巧

### 9.1 `static` 局部大数组

`display_manager.cpp` 中的 4KB 缓冲区用 `static` 修饰，避免占用宝贵的任务栈空间（Display_Task 只有 4KB 栈）。

```cpp
static uint8_t chunk[4096];  // static 避免占用 4KB 任务栈
```

### 9.2 进度条增量刷新

不重绘整条进度条，只填充新增的绿色区域并擦除旧百分比文字，大幅减少 SPI 传输量。

### 9.3 `volatile` 全局标志

`g_newImageReady` 等跨任务变量用 `volatile` 声明，防止编译器优化导致读取缓存值。

```cpp
volatile bool g_newImageReady = false;
volatile uint8_t g_uploadProgress = 0;
volatile bool g_isUploading = false;
```

### 9.4 零堆分配推送

`spiffsToTft()` 全程无动态内存分配，适合嵌入式环境。

### 9.5 固件版本统一

通过 `app_version.h` 统一定义 `FIRMWARE_VERSION`，避免串口和屏幕显示不一致。

### 9.6 接收超时保护

```cpp
if (millis() - lastDataMillis > 30000) {
    // 接收超过 30 秒无数据则复位状态
    finishReceiving(false);
}
```

### 9.7 文件大小校验

```cpp
if (fsize != IMAGE_SIZE_240) {
    // 防止显示残缺/错误图像
    return false;
}
```

---

## 十、完整工作流程

```
┌──────────┐     WiFi AP      ┌──────────┐     HTTP      ┌──────────────┐
│ 手机/电脑 │ ◄──────────────► │  ESP32   │ ◄─────────── │ WiFi_Task    │
│ 浏览器   │   192.168.4.1    │  AP 热点  │              │ (优先级3)     │
└──────────┘                  └──────────┘              └──────┬───────┘
                                                              │
                              ┌───────────────────────────────┤
                              │ SPIFFS 写入                   │ 状态快照更新
                              ▼                               ▼
                     ┌──────────────┐              ┌──────────────────┐
                     │ /img.rgb565  │              │ WiFiImageStatus  │
                     │ (115200 字节) │              │ (互斥锁保护)      │
                     └──────┬───────┘              └────────┬─────────┘
                            │                               │
                            ▼                               ▼
                     ┌──────────────┐              ┌──────────────────┐
                     │ Display_Task │ ◄─────────── │ Monitor_Task     │
                     │ (优先级2)     │  imageReady  │ (优先级2)         │
                     └──────┬───────┘              └──────────────────┘
                            │
                    tft.pushColors()
                            │
                            ▼
                     ┌──────────────┐
                     │ GC9A01 TFT   │
                     │ 240×240 屏幕  │
                     └──────────────┘
```

---

_文档生成日期：2026-05-25_
