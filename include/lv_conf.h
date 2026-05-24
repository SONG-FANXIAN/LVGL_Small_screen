#ifndef LV_CONF_H
#define LV_CONF_H

/* Minimal LVGL config for ESP32 + GC9A01 */

#define LV_COLOR_DEPTH 16   // RGB565，匹配GC9A01和TFT_eSPI默认16位色
#define LV_COLOR_16_SWAP 0  // 字节序不交换（若颜色异常可尝试改为1）

// 允许LVGL解码JPEG（配合LV_IMG_CF_RAW图片描述符）
#define LV_USE_SJPG 1

#endif /* LV_CONF_H */
