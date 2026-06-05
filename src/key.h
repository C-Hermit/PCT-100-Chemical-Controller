#ifndef _KEY_H
#define _KEY_H

#include <Arduino.h>

#define KEY_COUNT 2

// 引脚定义
#define KEY1_PIN 20  // 锁死按键
#define KEY2_PIN 21  // 回弹按键

//按键状态
#define KEY_HOLD   0x01     //按键按住不放时置1
#define KEY_SIGNED  0x02     //按键单击
#define KEY_LONG   0x04     //按键长按时

#define KEY_TIME_LONG (1000/20)
// 初始化按键
void key_init(void);
// 按键检查
uint8_t key_check(uint8_t n, uint8_t Flag);
// 读取按键1状态（核心处理逻辑）
void key_scan(void);

// 获取按键当前的实际输出状态
uint8_t key_getstate(uint8_t n);

#endif