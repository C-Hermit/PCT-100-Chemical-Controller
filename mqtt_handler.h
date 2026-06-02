#ifndef _MQTT_HANDLER_H
#define _MQTT_HANDLER_H

#include <Arduino.h>

// 初始化：注册 MQTT 回调
void mqtt_handler_init(void);

// 构建 JSON 状态并发布到 MQTT
void mqtt_handler_send_status(void);

#endif
