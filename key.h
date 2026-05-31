#ifndef _KEY_H
#define _KEY_H

#include <Arduino.h>

// 引脚定义
#define KEY1_PIN 20  // 锁死按键
#define KEY2_PIN 21  // 回弹按键

// 按键状态宏定义
#define KEY_RELEASE 0
#define KEY_PRESS   1

// 初始化按键
void key_init(void);

// 读取按键1状态（核心处理逻辑）
void key1_scan(void);

// 获取按键当前的实际输出状态
uint8_t get_key1_state(void);
uint8_t get_key2_state(void);

#endif