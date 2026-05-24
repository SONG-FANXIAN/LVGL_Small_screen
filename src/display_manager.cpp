#include "display_manager.h"

#include "board_config.h"
#include "wifi_image_manager.h"

#include <TFT_eSPI.h>
#include <SPIFFS.h>

// ============ 全局状态 ============

static TFT_eSPI tft = TFT_eSPI();
static bool     g_displayReady = false;

// 上传进度（WiFi 任务写入，显示任务读取）
volatile uint8_t g_uploadProgress = 0;
volatile bool    g_isUploading    = false;

// 新图像就绪（WiFi 任务写入，显示任务读取）
volatile bool    g_newImageReady = false;

// ============ SPIFFS → TFT 分块推送（零堆分配）============

// 从 SPIFFS 分块读取并推送到 TFT，成功返回 true
static bool spiffsToTft() {
    if (!SPIFFS.exists(SPIFFS_IMAGE_PATH)) {
        Serial.println("[DISPLAY] SPIFFS 无图像文件");
        return false;
    }
    File f = SPIFFS.open(SPIFFS_IMAGE_PATH, FILE_READ);
    if (!f) {
        Serial.println("[DISPLAY] SPIFFS 打开失败");
        return false;
    }
    size_t fsize = f.size();
    if (fsize != IMAGE_SIZE_240) {
        Serial.printf("[DISPLAY] SPIFFS 大小异常: %u\n", fsize);
        f.close();
        return false;
    }

    Serial.println("[DISPLAY] SPIFFS→TFT 分块推送中...");
    tft.startWrite();
    tft.setAddrWindow(0, 0, 240, 240);
    static uint8_t chunk[4096];  // static 避免占用 4KB 任务栈
    size_t rem = IMAGE_SIZE_240;
    while (rem > 0) {
        size_t n = f.read(chunk, rem > 4096 ? 4096 : rem);
        if (n == 0) break;
        if (rem == IMAGE_SIZE_240) {
            Serial.printf("[DISPLAY] 前4B: %02X %02X %02X %02X\n",
                          chunk[0], chunk[1], chunk[2], chunk[3]);
        }
        tft.pushColors((uint16_t*)chunk, n / 2, true);
        rem -= n;
    }
    f.close();
    tft.endWrite();
    Serial.println("[DISPLAY] ✅ 推送完成");
    return true;
}

// ============ 公开接口 ============

bool displayInit() {
    if (g_displayReady) return true;

    tft.init();
    tft.setRotation(0);
    boardBacklightSet(true);
    tft.fillScreen(TFT_BLACK);

    g_displayReady = true;
    Serial.println("[DISPLAY] TFT 初始化完成");
    return true;
}

// 启动时恢复上次保存的照片
void displayShowBoot() {
    if (!g_displayReady) return;
    if (SPIFFS.exists(SPIFFS_IMAGE_PATH)) {
        Serial.println("[DISPLAY] 启动：恢复上次照片...");
        spiffsToTft();
    } else {
        Serial.println("[DISPLAY] 启动：无已保存照片");
    }
}

// 照片上传完成后由 displayTask 调用
void displayReloadImage() {
    Serial.println("=== displayReloadImage() ===");
    if (!g_displayReady) return;

    // 短暂白色闪屏证明函数被调用
    tft.fillScreen(TFT_WHITE);
    delay(100);

    if (!spiffsToTft()) {
        tft.fillScreen(TFT_RED);
    }
}

// ============ 上传进度条 ============

void displayShowProgressBar() {
    if (!g_displayReady) return;

    uint8_t pct = g_uploadProgress;
    if (pct > 100) pct = 100;

    // ---- 布局常量（240×240 屏幕） ----
    const int BAR_X      = 30;
    const int BAR_Y      = 90;
    const int BAR_W      = 180;
    const int BAR_H      = 24;
    const int BAR_RADIUS = 4;
    const int TEXT_Y     = 50;
    const int PCT_Y      = 130;

    tft.startWrite();

    // 第一次调用时清屏并绘制标题
    static uint8_t lastPct = 255;  // 非法初始值，强制首次全刷
    if (pct == 0 && lastPct != 0) {
        tft.fillScreen(TFT_BLACK);
        // 标题
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString("上传中...", 120, TEXT_Y, 2);
        tft.drawString("0%", 120, PCT_Y, 2);
        // 外框
        tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, BAR_RADIUS, TFT_WHITE);
    } else if (pct != lastPct) {
        // 增量更新：只擦除/重绘进度条填充部分 + 百分比文字

        // 擦除百分比文字
        char oldPctStr[8];
        snprintf(oldPctStr, sizeof(oldPctStr), "%d%%", lastPct);
        tft.setTextColor(TFT_BLACK, TFT_BLACK);
        tft.setTextDatum(TC_DATUM);
        tft.drawString(oldPctStr, 120, PCT_Y, 2);

        // 计算新旧填充宽度
        int oldFillW = (lastPct * BAR_W) / 100;
        int newFillW = (pct * BAR_W) / 100;
        if (newFillW > BAR_W) newFillW = BAR_W;

        if (newFillW > oldFillW) {
            // 进度增长：用绿色填充增量部分
            int fillX = BAR_X + oldFillW;
            int fillW = newFillW - oldFillW;
            if (fillW > 0) {
                tft.fillRect(fillX, BAR_Y, fillW, BAR_H, TFT_GREEN);
            }
        }
        // 注意：进度不会回退，所以不处理 newFillW < oldFillW

        // 绘制新的百分比
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
        tft.drawString(pctStr, 120, PCT_Y, 2);
    }

    tft.endWrite();
    lastPct = pct;
}

