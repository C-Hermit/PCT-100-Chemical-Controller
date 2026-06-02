#ifndef __WS2812_DRV_H__
#define __WS2812_DRV_H__

#include <Arduino.h>

// 硬件引脚与灯珠数量配置
#define WS2812_PIN        0   // 你的万色灯引脚
#define WS2812_NUM_LEDS   1    // 灯珠数量

// 初始化万色灯（含开机闪烁提示）
void ws2812_drv_init(void);

// 设置指定灯珠的 RGB 颜色 (r, g, b 范围 0~255)
void ws2812_drv_set_color(uint16_t index, uint8_t r, uint8_t g, uint8_t b);

// 关闭所有灯珠
void ws2812_drv_clear(void);

#endif // __WS2812_DRV_H__