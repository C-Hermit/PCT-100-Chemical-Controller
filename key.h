#ifndef __KEY_H
#define __KEY_H
#include "Arduino.h"
/* 引脚定义 */
#define KEY_PIN 21 
/* 宏函数定义 */
#define KEY digitalRead(KEY_PIN)  
/* 函数声明 */
void key_init(void); 
#endif
