#ifndef _LDR_C
#define _LDR_C
#include "ldr.h"
#include <math.h> // 引入数学库计算对数

#define FILTER_SAMPLES 8

void ldr_init(void)
{
  pinMode(LDR_PIN, INPUT);
  analogReadResolution(12);
}

int ldr_read_raw(void)
{
    return analogRead(LDR_PIN);
}

int ldr_read_average(void)
{
    long sum = 0;
    for (int i = 0; i < FILTER_SAMPLES; i++) {
        sum += analogRead(LDR_PIN);
        delayMicroseconds(50);
    }
    return (int)(sum / FILTER_SAMPLES);
}

/**
 * @brief 将 ADC 值转换为实际的 Lux 勒克斯值
 * @return 光照强度 (Lux)
 */
unsigned int ldr_get_lux(void)
{
    int raw_adc = ldr_read_average();
    
    // 1. 安全边界兜底
    if (raw_adc > 4095) raw_adc = 4095;
    if (raw_adc < 0)    raw_adc = 0;

    // 2. 依照换算公式进行反相处理 (暗 -> 0, 亮 -> 4095)
    long inverse_adc = 4095 - raw_adc;

    // 3. 计算 (反相ADC)^2 / 30000 
    // 使用 long 类型防止平方运算时数据溢出
    unsigned int lux = (inverse_adc * inverse_adc) / 30000;
    
    return lux;
}
#endif
