#ifndef _MQTT_CLIENT_DRV_H
#define _MQTT_CLIENT_DRV_H

#include <Arduino.h>

// ==================== 配置区 ====================
#define WIFI_SSID         "hermit"
#define WIFI_PASSWORD     "cai986532"

#define MQTT_SERVER       "47.98.170.180" // 服务器地址
#define MQTT_PORT         8081

// ✨ 新增：MQTT 安全认证用户名和密码
#define MQTT_USER         "dzdx_emqx"       
#define MQTT_PASSWORD     "Jp4!sQ7$"       

// MQTT 主题定义
#define TOPIC_PUBLISH     "chemctrl/PCT_100_013/status"  
#define TOPIC_SUBSCRIBE   "chemctrl/PCT_100_013/command" 
// ===============================================

// 初始化 WiFi 和 MQTT 客户端
void mqtt_client_init(void);

// 核心任务句柄：必须放在主 loop() 中
void mqtt_client_loop(void);

// 供外部调用的发布接口
bool mqtt_client_publish(const char* payload);

#endif