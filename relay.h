#ifndef _RELAY_H
#define _RELAY_H
#include <Arduino.h>
#define LED_PIN 6
#define FUN_PIN 7

// 开关状态宏定义（提高代码可读性）
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// 初始化继电器引脚
void relay_init(void);

// LED 继电器控制
void set_led_status(uint8_t status);
void toggle_led(void);// 翻转LED状态
uint8_t get_led_status(void); 

// 风扇继电器控制
void set_fun_status(uint8_t status);
void toggle_fun(void);// 翻转风扇状态
uint8_t get_fun_status(void);
#endif