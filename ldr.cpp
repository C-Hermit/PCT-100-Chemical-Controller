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
 * @brief 将 ADC 值转换为实际的 Lux 勒克斯值（针对 10k在上VCC，LDR在下GND的电路）
 * @return 光照强度 (Lux)
 */
unsigned int ldr_get_lux(void)
{
    int adc_val = ldr_read_average();
    
    // 边界安全防御：防止分母为 0 或由于死区导致计算异常
    // 当 adc_val 极高（接近4095）时，代表全黑
    if (adc_val >= 4090) return 0;       
    // 当 adc_val 极低（接近0）时，代表光线极强
    if (adc_val <= 5) return 2000;       

    // 1. 根据严密的代数推导计算 LDR 的实际阻值 (单位：欧姆)
    // 物理规律：光越亮，adc_val越小，r_ldr越小
    double r_ldr = ((double)adc_val * 10000.0) / (4095.0 - (double)adc_val);

    // 2. 兜底保护
    if (r_ldr <= 0) return 0;
    
    // 3. 根据 GL5516 的特性曲线计算 Lux
    // 照度公式：Lux = (常数 / R_ldr) ^ (1 / γ)
    double lux = pow(3251652.0 / r_ldr, 1.4); 
    

    return (unsigned int)lux;
}
#endif
