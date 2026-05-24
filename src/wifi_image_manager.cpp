#include "wifi_image_manager.h"
#include "display_manager.h"

// ============ 辅助函数 ============

static const char* wifiStateToStr(WiFiImageState_t s) {
    switch (s) {
        case WIFI_STATE_IDLE:             return "IDLE";
        case WIFI_STATE_AP_STARTING:      return "AP_START";
        case WIFI_STATE_AP_READY:         return "AP_READY";
        case WIFI_STATE_CLIENT_CONNECTED: return "CLIENT";
        case WIFI_STATE_RECEIVING:        return "RECV";
        case WIFI_STATE_IMAGE_READY:      return "IMG_OK";
        case WIFI_STATE_ERROR:            return "ERROR";
        default:                          return "UNK";
    }
}

// 从 HTTP 请求头中解析 Content-Length
static int parseContentLength(const String& headers) {
    int idx = headers.indexOf("Content-Length:");
    if (idx < 0) return -1;
    idx += 15;
    while (idx < (int)headers.length() && headers[idx] == ' ') idx++;
    int end = idx;
    while (end < (int)headers.length() && isDigit(headers[end])) end++;
    return headers.substring(idx, end).toInt();
}

// ============ WiFiImageManager 实现 ============

WiFiImageManager::WiFiImageManager()
    : server(nullptr),
      state(WIFI_STATE_IDLE),
      lastError(0),
      receivedBytes(0),
      totalBytes(0),
      lastEventCode(0),
      imageReady(false),
      lastDataMillis(0) {
    memset(rxBuffer, 0, sizeof(rxBuffer));
}

WiFiImageManager::~WiFiImageManager() {
    if (server) {
        server->end();
        delete server;
        server = nullptr;
    }
    WiFi.softAPdisconnect(true);
}

void WiFiImageManager::logEvent(uint16_t code, const char* tag, const char* msg) {
    lastEventCode = code;
    Serial.printf("[WiFi][%s][N=%u][S=%s][RX=%u/%u] %s\n",
                  tag, (unsigned)code, wifiStateToStr(state),
                  receivedBytes, totalBytes, msg ? msg : "");
}

void WiFiImageManager::init(const char* ap_ssid, const char* ap_password) {
    logEvent(100, "INIT", "启动WiFi AP模式...");
    state = WIFI_STATE_AP_STARTING;

    // 配置并启动 AP
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(ap_ssid, ap_password, WIFI_AP_CHANNEL, 0, WIFI_AP_MAX_CLIENTS);
    if (!ok) {
        lastError = 1;
        state = WIFI_STATE_ERROR;
        logEvent(101, "INIT_FAIL", "WiFi AP启动失败");
        return;
    }

    // 启动 HTTP 服务器
    server = new WiFiServer(HTTP_SERVER_PORT);
    server->begin();

    state = WIFI_STATE_AP_READY;
    Serial.println("═══════════════════════════════════════");
    Serial.printf("  WiFi AP 已就绪\n");
    Serial.printf("  SSID:     %s\n", ap_ssid);
    Serial.printf("  密码:     %s\n", ap_password);
    Serial.printf("  IP地址:   %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("  HTTP端口: %d\n", HTTP_SERVER_PORT);
    Serial.println("  用浏览器或工具上传 240x240 RGB565 图像");
    Serial.println("═══════════════════════════════════════");
    logEvent(102, "AP_READY", "等待客户端连接...");
}

void WiFiImageManager::resetState() {
    state = WIFI_STATE_AP_READY;
    receivedBytes = 0;
    totalBytes = 0;
    lastError = 0;
    imageReady = false;
    if (spiffsFile) spiffsFile.close();
}

void WiFiImageManager::handle() {
    // 超时保护：接收中超过 30 秒无数据则复位
    if (state == WIFI_STATE_RECEIVING) {
        if (millis() - lastDataMillis > 30000) {
            logEvent(201, "TIMEOUT", "接收超时，复位");
            finishReceiving(false);
            return;
        }
    }

    if (!server) return;

    // 检测是否有新客户端连接
    WiFiClient client = server->accept();  // 非阻塞调用，无客户端返回空
    if (client) {
        state = WIFI_STATE_CLIENT_CONNECTED;
        logEvent(200, "CLIENT_CONN", "新客户端已连接");

        // 读取 HTTP 请求头（设置超时）
        client.setTimeout(5000);
        String firstLine = client.readStringUntil('\n');
        // 继续读取所有头部直到空行（用阻塞读取，不用 available() 避免竞态）
        String allHeaders = firstLine + "\n";
        {
            uint32_t headerStart = millis();
            while (client.connected() && (millis() - headerStart < 5000)) {
                String line = client.readStringUntil('\n');
                allHeaders += line + "\n";
                // 空行 = 头部结束（\r 或 \r\n 去掉后长度 <= 1）
                if (line.length() <= 2) break;
            }
        }

        int contentLen = parseContentLength(allHeaders);
        if (contentLen <= 0 || contentLen > IMAGE_SIZE_240 + 1024) {
            // 不是图像上传请求，可能是浏览器访问首页或 /verify
            if (firstLine.indexOf("GET") >= 0) {
                // /verify 诊断端点
                if (firstLine.indexOf("/verify") >= 0) {
                    String diag = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n\r\n";
                    diag += "=== SPIFFS 诊断 ===\n";
                    diag += "SPIFFS 挂载: " + String(SPIFFS.begin(false) ? "OK" : "失败") + "\n";
                    if (SPIFFS.exists(SPIFFS_IMAGE_PATH)) {
                        File f = SPIFFS.open(SPIFFS_IMAGE_PATH, "r");
                        diag += "文件存在: 是\n";
                        diag += "文件大小: " + String(f.size()) + " (期望 " + String(IMAGE_SIZE_240) + ")\n";
                        uint8_t head[8];
                        size_t n = f.read(head, 8);
                        diag += "前8字节: ";
                        for (size_t i = 0; i < n; i++) {
                            if (head[i] < 0x10) diag += "0";
                            diag += String(head[i], HEX) + " ";
                        }
                        diag += "\n";
                        // 检查数据是否全相同（可能是错误数据）
                        bool allSame = true;
                        for (size_t i = 1; i < n; i++) if (head[i] != head[0]) allSame = false;
                        diag += "数据多样: " + String(allSame ? "否(全相同!)" : "是") + "\n";
                        f.close();
                    } else {
                        diag += "文件存在: 否\n";
                    }
                    diag += "imageReady: " + String(imageReady ? "true" : "false") + "\n";
                    diag += "state: " + String(state) + "\n";
                    client.print(diag);
                    client.stop();
                    state = WIFI_STATE_AP_READY;
                    return;
                }
                // 返回简单的 HTML 上传页面（使用 fetch API 发送纯二进制）
                String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
                    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 Photo Upload</title>"
                    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                    "<style>body{font-family:sans-serif;text-align:center;padding:20px;background:#1a1a2e;color:#eee}"
                    "h2{color:#e94560}input,button{padding:12px;margin:10px;font-size:16px}"
                    "button{background:#e94560;color:#fff;border:none;border-radius:8px;cursor:pointer}"
                    "#msg{margin-top:15px;font-size:14px}"
                    ".info{font-size:14px;color:#aaa;margin-top:20px}"
                    "</style></head><body>"
                    "<h2>📷 ESP32 照片上传</h2>"
                    "<p>选择照片文件（自动缩放到 240x240）</p>"
                    "<input type='file' id='fileInput' accept='image/*'><br>"
                    "<button onclick='uploadPhoto()'>上传照片</button>"
                    "<button onclick='verifySpiffs()' style='background:#444'>诊断 SPIFFS</button>"
                    "<div id='msg'></div>"
                    "<pre id='diag' style='text-align:left;font-size:12px;margin-top:10px;color:#0f0'></pre>"
                    "<p class='info'>WiFi AP: " WIFI_AP_SSID " | 图像格式: RGB565 240×240</p>"
                    "<script>"
                    "function uploadPhoto(){"
                    "var f=document.getElementById('fileInput').files[0];"
                    "if(!f){document.getElementById('msg').innerHTML='请先选择文件';return;}"
                    "var c=document.createElement('canvas');c.width=240;c.height=240;"
                    "var ctx=c.getContext('2d');"
                    "var img=new Image();img.onload=function(){"
                    "ctx.drawImage(img,0,0,240,240);"
                    "var imageData=ctx.getImageData(0,0,240,240);"
                    "var buf=new ArrayBuffer(115200);var view=new DataView(buf);var off=0;"
                    "for(var y=0;y<240;y++)for(var x=0;x<240;x++){"
                    "var i=(y*240+x)*4;"
                    "var r=imageData.data[i]>>3;var g=imageData.data[i+1]>>2;var b=imageData.data[i+2]>>3;"
                    "var rgb=(r<<11)|(g<<5)|b;"
                    "view.setUint8(off++,rgb&0xFF);view.setUint8(off++,rgb>>8);"
                    "}"
                    "document.getElementById('msg').innerHTML='上传中...';"
                    "fetch('/upload',{method:'POST',body:buf}).then(r=>r.text()).then(t=>{"
                    "document.getElementById('msg').innerHTML=t;"
                    "}).catch(e=>{document.getElementById('msg').innerHTML='上传失败: '+e});"
                    "};img.src=URL.createObjectURL(f);"
                    "}"
                    "function verifySpiffs(){fetch('/verify').then(r=>r.text()).then(t=>{document.getElementById('diag').innerText=t})}"
                    "</script></body></html>";
                client.print(html);
            } else {
                client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
            }
            client.stop();
            state = WIFI_STATE_AP_READY;
            return;
        }

        // 上传图像请求
        logEvent(210, "UPLOAD_START", "开始接收图像上传...");
        startReceiving(contentLen);

        // ========== 接收图像数据（只写 SPIFFS，不用共享缓冲区）==========
        uint32_t totalRx = 0;
        uint32_t lastProgress = 0;
        uint32_t lastData = millis();
        Serial.printf("[WiFi] 开始接收, totalBytes=%u\n", totalBytes);

        while (totalRx < IMAGE_SIZE_240) {
            if (millis() - lastData > 10000) {
                Serial.printf("[WiFi] 超时! totalRx=%u\n", totalRx);
                break;
            }
            yield();

            if (client.available()) {
                size_t len = client.read(rxBuffer, RX_BUFFER_SIZE);
                if (len > 0) {
                    if (spiffsFile) spiffsFile.write(rxBuffer, len);
                    totalRx += len;
                    receivedBytes = totalRx;
                    lastData = millis();

                    uint32_t pct = (totalRx * 100) / IMAGE_SIZE_240;
                    if (pct > 100) pct = 100;
                    g_uploadProgress = (uint8_t)pct;
                    if (pct - lastProgress >= 10) {
                        lastProgress = pct;
                        Serial.printf("[WiFi] %u%% (%u/%u)\n", pct, totalRx, IMAGE_SIZE_240);
                    }
                }
            } else {
                vTaskDelay(1);
            }
        }
        Serial.printf("[WiFi] 接收结束: totalRx=%u\n", totalRx);

        // ========== 关闭 SPIFFS 文件 ==========
        if (spiffsFile) {
            spiffsFile.close();
            spiffsFile = (File){};
        }

        // ========== 设标志（先设显示标志，再清上传状态）==========
        if (totalRx >= IMAGE_SIZE_240) {
            g_uploadProgress = 100;
            __sync_synchronize();
            g_newImageReady = true;  // 直接 volatile，displayTask 100ms 内可见
            state = WIFI_STATE_IMAGE_READY;
            Serial.println("[WiFi] ✅ g_newImageReady=true");
            g_isUploading = false;
        } else {
            g_isUploading = false;
            SPIFFS.remove(SPIFFS_IMAGE_PATH);
            state = WIFI_STATE_AP_READY;
            Serial.printf("[WiFi] ❌ 失败 totalRx=%u\n", totalRx);
        }

        // ========== 发 HTTP 响应（用 write 直接写原始字节最可靠）==========
        client.write("HTTP/1.1 200 OK\r\n", 17);
        client.write("Content-Type: text/plain\r\n", 26);
        client.write("Connection: close\r\n", 19);
        client.write("\r\n", 2);
        if (totalRx >= IMAGE_SIZE_240) {
            client.write("OK\r\n", 4);
        } else {
            client.write("ERROR\r\n", 7);
        }
        client.flush();
        delay(2000);  // 等 lwIP 把数据发出去
        client.stop();
        Serial.println("[WiFi] 客户端已断开");
    }
}

void WiFiImageManager::startReceiving(uint32_t size) {
    receivedBytes = 0;
    totalBytes = size;
    lastDataMillis = millis();
    state = WIFI_STATE_RECEIVING;

    g_isUploading = true;
    g_uploadProgress = 0;

    // 打开 SPIFFS 文件
    if (SPIFFS.exists(SPIFFS_IMAGE_PATH)) SPIFFS.remove(SPIFFS_IMAGE_PATH);
    spiffsFile = SPIFFS.open(SPIFFS_IMAGE_PATH, FILE_WRITE);
    if (!spiffsFile) {
        lastError = 2;
        state = WIFI_STATE_ERROR;
        Serial.println("[WiFi] ❌ SPIFFS 文件创建失败!");
    } else {
        Serial.println("[WiFi] ✅ SPIFFS 文件已创建");
    }
}

void WiFiImageManager::finishReceiving(bool success) {
    g_isUploading = false;
    if (spiffsFile) { spiffsFile.close(); spiffsFile = (File){}; }
    if (!success) {
        SPIFFS.remove(SPIFFS_IMAGE_PATH);
        state = WIFI_STATE_AP_READY;
        Serial.println("[WiFi] finishReceiving: 失败清理");
    }
}

uint8_t WiFiImageManager::getProgress() const {
    if (totalBytes == 0) return 0;
    return (uint8_t)((receivedBytes * 100U) / totalBytes);
}

uint8_t WiFiImageManager::getClientCount() const {
    return (uint8_t)WiFi.softAPgetStationNum();
}

String WiFiImageManager::getIPAddress() const {
    return WiFi.softAPIP().toString();
}
