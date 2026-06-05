#ifndef _DEVICE_H
#define _DEVICE_H

#include <Arduino.h>

// 系统全局业务状态（唯一 extern 引用点）
struct SystemState {
    bool power_on;             // 总闸: true-开启
    bool is_auto;              // 模式: true-自动, false-手动
    uint8_t manual_code;       // 手动状态码 00~11

    float current_temp;        // 温度缓存
    unsigned int current_lux;  // 光照缓存

    float temp_th;             // 温度阈值
    unsigned int light_th;     // 光照阈值

    // 指示灯模块状态（由编排层 + 业务层在运行中刷新）
    bool wifi_connected;       // WiFi 连接状态
    bool mqtt_connected;       // MQTT 连接状态
    bool led_relay_on;         // LED 继电器状态
    bool fan_relay_on;         // 风扇继电器状态
};

extern SystemState sys;

#endif
