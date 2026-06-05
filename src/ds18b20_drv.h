#ifndef _DS18B20_DRV_H
#define _DS18B20_DRV_H

#include <Arduino.h>

// 定义 DS18B20 数据线连接的引脚（ESP32-C3 的 GPIO 4）
#define DS18B20_PIN 10

// 初始化 DS18B20 模块
void ds18b20_init(void);

// 获取单总线上第一个传感器的摄氏温度
float ds18b20_get_temp(void);

// 打印总线上所有 DS18B20 的温度（用于多探头测试）
void ds18b20_print_all(void);

#endif