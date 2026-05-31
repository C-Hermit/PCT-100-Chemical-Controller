#ifndef _LDR_H
#define _LDR_H

#include <Arduino.h>

#define LDR_PIN 1

void ldr_init(void);
int ldr_read_raw(void);
int ldr_read_average(void);

// 修改：直接获取光照 Lux 值
unsigned int ldr_get_lux(void);

#endif