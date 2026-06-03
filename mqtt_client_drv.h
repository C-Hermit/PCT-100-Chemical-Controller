#ifndef _MQTT_CLIENT_DRV_H
#define _MQTT_CLIENT_DRV_H

#include <Arduino.h>

// ==================== MQTT 可配置参数限制 ====================
#define MQTT_CONFIG_SERVER_MAX  64
#define MQTT_CONFIG_USER_MAX    32
#define MQTT_CONFIG_PASS_MAX    32
#define MQTT_CONFIG_ID_MAX      32

typedef struct {
    char     server[MQTT_CONFIG_SERVER_MAX];
    uint16_t port;
    char     user[MQTT_CONFIG_USER_MAX];
    char     password[MQTT_CONFIG_PASS_MAX];
    char     device_id[MQTT_CONFIG_ID_MAX];
} mqtt_config_t;

/**
 * @brief 初始化 MQTT 客户端：从 NVS 加载配置，设置服务器地址
 */
void mqtt_client_init(void);

/**
 * @brief 核心任务句柄：内部实现网络感知重连，必须放在主 loop() 中
 */
void mqtt_client_loop(void);

/**
 * @brief 供外部调用的发布接口（向当前 device_id 对应的 topic 发布）
 */
bool mqtt_client_publish(const char* payload);

/**
 * @brief 允许外部传入自定义的 MQTT 消息解析回调函数
 */
void mqtt_client_set_callback(void (*callback)(char*, byte*, unsigned int));

/**
 * @brief 更新 MQTT 完整配置（保存到 NVS，自动触发断线重连）
 */
void mqtt_client_update_config(const mqtt_config_t* cfg);

/**
 * @brief 获取当前 MQTT 配置（const 指针，只读）
 */
const mqtt_config_t* mqtt_client_get_config(void);

/**
 * @brief 查询 MQTT 客户端当前连接状态
 */
bool mqtt_client_is_connected(void);

/**
 * @brief 强制断线并立即重连
 */
void mqtt_client_force_reconnect(void);

#endif
