#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>

// ================================
// 板级引脚映射（基于 SCH_ESP32时钟_2026-05-08）
// ================================

// GC9A01 屏幕 SPI 引脚
static const int PIN_TFT_SCLK = 14;
static const int PIN_TFT_MOSI = 15;
static const int PIN_TFT_CS   = 5;
static const int PIN_TFT_DC   = 27;
static const int PIN_TFT_RST  = 33;

// 屏幕背光控制（BLK）
static const int PIN_TFT_BLK  = 22;

// BOOT 按键（原理图标注 ESP32/IO0）
static const int PIN_BOOT_KEY = 0;

// 背光有效电平（如发现亮灭相反，将 true 改为 false）
static const bool TFT_BLK_ACTIVE_HIGH = true;

inline void boardPinInit() {
    pinMode(PIN_TFT_BLK, OUTPUT);
    digitalWrite(PIN_TFT_BLK, TFT_BLK_ACTIVE_HIGH ? HIGH : LOW);

    // BOOT按键：原理图有上拉到3V3，按下接地
    pinMode(PIN_BOOT_KEY, INPUT_PULLUP);
}

inline void boardBacklightSet(bool on) {
    const int level = (on == TFT_BLK_ACTIVE_HIGH) ? HIGH : LOW;
    digitalWrite(PIN_TFT_BLK, level);
}

inline bool boardBootKeyPressed() {
    return digitalRead(PIN_BOOT_KEY) == LOW;
}

#endif // BOARD_CONFIG_H
