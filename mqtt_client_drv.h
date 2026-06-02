#ifndef _MQTT_CLIENT_DRV_H
#define _MQTT_CLIENT_DRV_H

#include <Arduino.h>

// ==================== MQTT 服务器配置区 ====================
#define MQTT_SERVER         "47.98.170.180" 
#define MQTT_PORT           8081

#define MQTT_USER           "dzdx_emqx"       
#define MQTT_PASSWORD       "Jp4!sQ7$"       

#define TOPIC_PUBLISH       "chemctrl/PCT_100_013/status"  
#define TOPIC_SUBSCRIBE      "chemctrl/PCT_100_013/command" 
// =========================================================

/**
 * @brief 初始化 MQTT 配置（不再包含 WiFi 连接逻辑）
 */
void mqtt_client_init(void);

/**
 * @brief 核心任务句柄：内部实现网络感知重连，必须放在主 loop() 中
 */
void mqtt_client_loop(void);

/**
 * @brief 供外部调用的发布接口
 */
bool mqtt_client_publish(const char* payload);

/**
 * @brief 允许外部传入自定义的 MQTT 消息解析回调函数
 */
void mqtt_client_set_callback(void (*callback)(char*, byte*, unsigned int));

#endif