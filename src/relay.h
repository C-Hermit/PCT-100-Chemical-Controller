#ifndef _RELAY_H
#define _RELAY_H
#include <Arduino.h>
#define LED_PIN 6
#define FAN_PIN 7

// 开关状态宏定义（提高代码可读性）
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// 初始化继电器引脚
void relay_init(void);

// LED 继电器控制
void relay_set_led(uint8_t status);
void relay_toggle_led(void);
uint8_t relay_get_led(void);

// 风扇继电器控制
void relay_set_fan(uint8_t status);
void relay_toggle_fan(void);
uint8_t relay_get_fan(void);
#endif