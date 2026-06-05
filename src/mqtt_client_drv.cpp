#include "mqtt_client_drv.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>

#include "version.h"     // 默认 DEVICE_ID
#include "wifi_drv.h"

// ==================== 静态对象 ====================
static WiFiClient espClient;
static PubSubClient client(espClient);

static mqtt_config_t _config;
static char _topic_publish[64];
static char _topic_subscribe[64];

// ==================== 默认配置（NVS 为空时的回退值） ====================
// 警告：此处不可填写真实凭据，否则 push 到 GitHub 会泄露。
// 首次烧录后通过串口命令 "mqtt set <key>=<value>" 配置，
// 设置的值会自动持久化到 NVS，不受源码更新影响。
#define DFLT_SERVER   ""
#define DFLT_PORT     0
#define DFLT_USER     ""
#define DFLT_PASSWORD ""

// ==================== 内部函数声明 ====================
static void build_topics(void);
static void load_config(void);
static void save_config(void);
static void mqtt_reconnect(void);

// ==================== 内部实现 ====================

static void build_topics(void) {
    snprintf(_topic_publish,  sizeof(_topic_publish),  "chemctrl/%s/status",  _config.device_id);
    snprintf(_topic_subscribe, sizeof(_topic_subscribe), "chemctrl/%s/command", _config.device_id);
}

static void load_config(void) {
    Preferences prefs;
    prefs.begin("mqtt_space", true);

    String s = prefs.getString("server", DFLT_SERVER);
    strncpy(_config.server, s.c_str(), sizeof(_config.server) - 1);
    _config.server[sizeof(_config.server) - 1] = '\0';

    _config.port = (uint16_t)prefs.getUInt("port", DFLT_PORT);

    s = prefs.getString("user", DFLT_USER);
    strncpy(_config.user, s.c_str(), sizeof(_config.user) - 1);
    _config.user[sizeof(_config.user) - 1] = '\0';

    s = prefs.getString("pass", DFLT_PASSWORD);
    strncpy(_config.password, s.c_str(), sizeof(_config.password) - 1);
    _config.password[sizeof(_config.password) - 1] = '\0';

    s = prefs.getString("dev_id", DEVICE_ID);
    strncpy(_config.device_id, s.c_str(), sizeof(_config.device_id) - 1);
    _config.device_id[sizeof(_config.device_id) - 1] = '\0';

    prefs.end();
    build_topics();
}

static void save_config(void) {
    Preferences prefs;
    prefs.begin("mqtt_space", false);
    prefs.putString("server", _config.server);
    prefs.putUInt("port", _config.port);
    prefs.putString("user", _config.user);
    prefs.putString("pass", _config.password);
    prefs.putString("dev_id", _config.device_id);
    prefs.end();
    build_topics();
}

static void mqtt_reconnect(void) {
    if (!wifi_drv_is_connected()) return;

    static unsigned long last_reconnect = 0;
    if (millis() - last_reconnect < 5000) return;
    last_reconnect = millis();

    client.setServer(_config.server, _config.port);

    Serial.print(">> [MQTT]: 尝试连接 ");
    Serial.print(_config.server);
    Serial.print(":");
    Serial.print(_config.port);
    Serial.print(" ... ");

    String clientId = String(_config.device_id) + "-" + String(random(0, 0xffff), HEX);

    bool ok = false;
    if (strlen(_config.user) > 0) {
        ok = client.connect(clientId.c_str(), _config.user, _config.password);
    } else {
        ok = client.connect(clientId.c_str());
    }

    if (ok) {
        Serial.println("已连接!");
        client.subscribe(_topic_subscribe);
    } else {
        Serial.print("失败, rc=");
        Serial.println(client.state());
    }
}

// ==================== 对外接口 ====================

void mqtt_client_init(void) {
    load_config();
    client.setServer(_config.server, _config.port);
}

void mqtt_client_loop(void) {
    if (!client.connected()) {
        mqtt_reconnect();
    }
    client.loop();
}

bool mqtt_client_publish(const char* payload) {
    if (client.connected()) {
        return client.publish(_topic_publish, payload);
    }
    return false;
}

void mqtt_client_set_callback(void (*callback)(char*, byte*, unsigned int)) {
    client.setCallback(callback);
}

void mqtt_client_update_config(const mqtt_config_t* cfg) {
    _config = *cfg;
    save_config();
    client.setServer(_config.server, _config.port);
    if (client.connected()) {
        client.disconnect();
    }
    Serial.println(">> [MQTT]: 配置已更新，自动重连中...");
}

const mqtt_config_t* mqtt_client_get_config(void) {
    return &_config;
}

bool mqtt_client_is_connected(void) {
    return client.connected();
}

void mqtt_client_force_reconnect(void) {
    if (!wifi_drv_is_connected()) {
        Serial.println(">> [MQTT]: WiFi 未连接，无法重连。");
        return;
    }
    if (client.connected()) {
        client.disconnect();
    }
    client.setServer(_config.server, _config.port);

    String clientId = String(_config.device_id) + "-" + String(random(0, 0xffff), HEX);
    bool ok = false;
    if (strlen(_config.user) > 0) {
        ok = client.connect(clientId.c_str(), _config.user, _config.password);
    } else {
        ok = client.connect(clientId.c_str());
    }

    if (ok) {
        Serial.println(">> [MQTT]: 重连成功！");
        client.subscribe(_topic_subscribe);
    } else {
        Serial.print(">> [MQTT]: 重连失败, rc=");
        Serial.println(client.state());
    }
}
