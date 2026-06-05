#ifndef _OLED_UI_H
#define _OLED_UI_H

#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// ==================== OLED 硬件引脚配置宏 ====================
#define OLED_SCL_PIN   5    // 时钟引脚
#define OLED_SDA_PIN   4    // 数据引脚
#define OLED_RST_PIN   U8X8_PIN_NONE  // 复位引脚（若屏幕没有RST引脚，保持 U8X8_PIN_NONE）

// ============================================================

// 初始化 OLED 屏幕
void oled_ui_init(void);

struct SystemState;  // 前向声明，避免驱动层反向依赖

// 核心刷新函数：传入系统状态结构体，不依赖全局 sys
void oled_ui_refresh(const SystemState& state);

#endif