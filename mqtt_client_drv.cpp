#include "mqtt_client_drv.h"
#include "relay.h"  // 联动你的继电器驱动
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient espClient;
static PubSubClient client(espClient);

// 内部私有函数：处理收到的 MQTT 消息（下发控制）
static void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("[MQTT Recv] Topic: ");
    Serial.print(topic);
    Serial.print(" | Payload: ");
    Serial.println(message);

    // 联动继电器逻辑
    if (String(topic) == TOPIC_SUBSCRIBE) {
        if (message == "1") {
            // relay_on(); // 调用你 relay 驱动里的函数
            Serial.println("Action -> 开启继电器");
        } else if (message == "0") {
            // relay_off(); 
            Serial.println("Action -> 关闭继电器");
        }
    }
}

// 内部私有函数：处理断线重连（带用户名密码验证）
static void mqtt_reconnect() {
    while (!client.connected()) {
        Serial.print("正在尝试连接有密 MQTT 服务器...");
        
        // 随机生成一个唯一 ClientID
        String clientId = "ESP32C3-" + String(random(0, 0xffff), HEX);
        
        // ✨ 核心修改点：连接时传入用户名和密码
        // 判定逻辑：如果连用户名都没有设置(为空)，则尝试匿名连接，否则带密连接
        bool connect_status = false;
        if (strlen(MQTT_USER) > 0) {
            connect_status = client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
        } else {
            connect_status = client.connect(clientId.c_str());
        }

        if (connect_status) {
            Serial.println("认证成功，已连接!");
            // 连接成功后，重新订阅控制主题
            client.subscribe(TOPIC_SUBSCRIBE);
        } else {
            Serial.print("认证/连接失败, 状态码=");
            Serial.print(client.state());
            Serial.println(" 5秒后重试...");
            delay(5000);
        }
    }
}

void mqtt_client_init(void) {
    // 1. 连接 WiFi
    Serial.println();
    Serial.print("正在连接 WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 连接成功！");

    // 2. 配置 MQTT 服务器及回调
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(mqtt_callback);
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