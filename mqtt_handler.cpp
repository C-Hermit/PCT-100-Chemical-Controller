#include "mqtt_handler.h"
#include "device.h"
#include "controller.h"
#include "relay.h"
#include "mqtt_client_drv.h"
#include <ArduinoJson.h>

// ==================== 初始化 ====================

void mqtt_handler_init(void) {
    // MQTT 客户端初始化已在外部完成
    // 只负责注册回调
    mqtt_client_set_callback(mqtt_callback_handler);
}

// ==================== 状态上报 ====================

void mqtt_handler_send_status(void) {
    StaticJsonDocument<256> doc;

    doc["temperature"] = sys.current_temp;
    doc["light"]       = sys.current_lux;
    doc["mode"]        = sys.is_auto ? "auto" : "manual";
    doc["key1_lock"]   = sys.power_on;
    doc["relay3"]      = (get_led_status() == RELAY_ON);
    doc["relay4"]      = (get_fun_status() == RELAY_ON);
    doc["temp_th"]     = sys.temp_th;
    doc["light_th"]    = sys.light_th;

    char output[256];
    serializeJson(doc, output);
    mqtt_client_publish(output);
}

// ==================== 下行命令解析 ====================

/*
  支持的 JSON 命令格式:
    {"cmd":"set_relay",    "relay":3, "value":true}
    {"cmd":"set_mode",     "mode":"auto"}
    {"cmd":"set_threshold","temp":30.5, "light":200}
    {"cmd":"get_status"}
    {"cmd":"reboot"}
*/
static void handle_json_command(JsonDocument& doc) {
    const char* cmd = doc["cmd"];
    if (cmd == NULL) return;

    if (strcmp(cmd, "set_relay") == 0) {
        if (sys.is_auto) {
            Serial.println(">> [MQTT 拒绝]: 自动模式下不可手动控制继电器。");
            return;
        }
        ctrl_set_relay(doc["relay"], doc["value"]);

    } else if (strcmp(cmd, "set_mode") == 0) {
        const char* mode = doc["mode"];
        ctrl_set_mode(strcmp(mode, "auto") == 0);

    } else if (strcmp(cmd, "set_threshold") == 0) {
        if (doc.containsKey("temp"))  ctrl_set_temp_threshold(doc["temp"]);
        if (doc.containsKey("light")) ctrl_set_light_threshold(doc["light"]);

    } else if (strcmp(cmd, "get_status") == 0) {
        mqtt_handler_send_status();

    } else if (strcmp(cmd, "reboot") == 0) {
        Serial.println(">> [MQTT 重启]: 正在重启系统...");
        delay(500);
        ESP.restart();
    }
}

// ==================== PubSubClient 回调 ====================

void mqtt_callback_handler(char* topic, byte* payload, unsigned int length) {
    if (!sys.power_on) {
        Serial.println(">> [MQTT 拒绝]: 总闸关闭，忽略远程命令。");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.println(">> [MQTT 解析错误]: JSON 格式非法。");
        return;
    }

    handle_json_command(doc);
}
