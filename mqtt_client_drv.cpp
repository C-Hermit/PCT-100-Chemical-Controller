#include "mqtt_client_drv.h"
#include <WiFi.h>
#include <PubSubClient.h>

#include "wifi_drv.h" 

static WiFiClient espClient;
static PubSubClient client(espClient);

// 内部私有：断线带密重连机制
static void mqtt_reconnect() {
    // 核心防御：如果底层 WiFi 还没连上，绝对不去尝试连接 MQTT，防止内核产生硬死锁
    if (!wifi_drv_is_connected()) {
        return;
    }

    static unsigned long last_reconnect_time = 0;
    // 限制重连频率：每 5 秒最多尝试连接一次，决不无限制死循环卡死 main loop
    if (millis() - last_reconnect_time < 5000) {
        return;
    }
    last_reconnect_time = millis();

    Serial.print(">> [MQTT]: 正在尝试连接有密 MQTT 服务器...");
    String clientId = "ESP32C3-" + String(random(0, 0xffff), HEX);
    
    bool connect_status = false;
    if (strlen(MQTT_USER) > 0) {
        connect_status = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    } else {
        connect_status = client.connect(clientId.c_str());
    }

    if (connect_status) {
        Serial.println(" 认证成功，已连接!");
        client.subscribe(TOPIC_SUBSCRIBE);
    } else {
        Serial.print(" 认证失败, 错误状态码码=");
        Serial.println(client.state());
    }
}
void mqtt_client_init(void) {
    // 仅配置服务器地址和端口，真正的连接交由 loop 中的逻辑处理
    client.setServer(MQTT_SERVER, MQTT_PORT);
}
void mqtt_client_loop(void) {
    if (!client.connected()) {
        mqtt_reconnect();
    }
    client.loop();
}

bool mqtt_client_publish(const char* payload) {
    if (client.connected()) {
        return client.publish(TOPIC_PUBLISH, payload);
    }
    return false;
}
// 提供给外部注册回调的接口
void mqtt_client_set_callback(void (*callback)(char*, byte*, unsigned int)) {
    client.setCallback(callback); // 直接把 PubSubClient 的回调指向传进来的函数
}






