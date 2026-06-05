#ifndef _CONTROLLER_H
#define _CONTROLLER_H

#include <Arduino.h>

// 初始化继电器状态
void ctrl_init(void);

// 按键状态机（20ms 调度）：总闸切换、模式切换、手动编码循环
void ctrl_key_logic(void);

// 自动灯光控制（50ms 调度）
void ctrl_auto_light(void);

// 自动风机控制（2000ms 调度）
void ctrl_auto_fan(void);

// == 外部命令接口（MQTT / 串口通过以下函数触发，内含 side effect）==
void ctrl_set_relay(int id, bool on);
void ctrl_set_mode(bool auto_mode);
void ctrl_set_temp_threshold(float t);
void ctrl_set_light_threshold(unsigned int l);
void ctrl_power_off(void);

// ========== MQTT 配置管理（业务层封装，不暴露传输层细节）==========
void ctrl_set_mqtt_server(const char* ip, uint16_t port);
void ctrl_set_mqtt_auth(const char* user, const char* pass);
void ctrl_set_mqtt_device_id(const char* id);
void ctrl_mqtt_print_status(void);
void ctrl_mqtt_reconnect(void);

#endif
