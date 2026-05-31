#include <Arduino.h>
#include <ArduinoJson.h> // 需要在库管理器中安装 ArduinoJson 库

// 引入所有底层驱动头文件
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"
#include "mqtt_client_drv.h"

// ==================== 业务状态全局变量 ====================
bool sys_power_on = false;     // 系统总闸状态：true-开启，false-关闭
bool is_auto_mode = true;      // 模式：true-自动，false-手动
uint8_t manual_state = 0;      // 手动模式下的 4 种状态 (0~3 对应 00, 01, 10, 11)

// 阈值默认值（根据图片初始化）
float temp_threshold = 40.0;   // 温度高值阈值
unsigned int light_threshold = 5000; // 光照阈值

// 定时数据上报变量
unsigned long last_report_time = 0;
const unsigned long report_interval = 5000; // 默认每 5 秒自动上报一次状态

// ==================== 核心功能函数声明 ====================
void handle_hardware_logic(void);
void handle_auto_control(void);
void report_device_status(void);
void mqtt_callback_handler(char* topic, byte* payload, unsigned int length);

// ==================== Arduino 核心接口 ====================
void setup() {
    Serial.begin(115200);
    
    // 初始化所有硬件外设
    key_init();
    relay_init();
    ldr_init();
    ds18b20_init();
    
    // 初始化网络与MQTT
    mqtt_client_init();
    
    mqtt_client_set_callback(mqtt_callback_handler);

    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // 1. 驱动内核轮询
    key_scan();
    mqtt_client_loop();

    // 2. 处理硬件按键与闸门逻辑
    handle_hardware_logic();
    
    delay(10); // 稍微延时交出 CPU
}

// ==================== 业务逻辑细分实现 ====================

/**
 * @brief 硬件按键与总闸约束逻辑
 */
void handle_hardware_logic(void) {
    // ---- KEY1 总闸状态判断 ----
    // 只要 KEY1 处于 HOLD 状态，总闸打开；否则关闭
    if (key_getstate(0) == KEY_HOLD) {
        if (!sys_power_on) {
            sys_power_on = true;
            is_auto_mode = true; // 开启时默认进入自动模式
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
        }
    } else {
        if (sys_power_on) {
            sys_power_on = false;
            // 总闸关闭，强制关闭所有继电器 (模式归00)
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            manual_state = 0;
            Serial.println(">> [总闸]: 关闭！所有功能切断，继电器归 00。");
        }
        return; // 总闸关闭状态下，不执行后续任何操作
    }

    // ---- KEY2 模式切换与手动控制 ----
    // 长按 KEY2：切换 自动/手动 模式
    if (key_check(1, KEY_LONG)) {
        is_auto_mode = !is_auto_mode;
        Serial.print(">> [KEY2长按]: 切换模式为 -> ");
        Serial.println(is_auto_mode ? "【自动模式】" : "【手动模式】");
    }

    // 处于手动状态时
    if (!is_auto_mode) {
        // 短按 KEY2：在 00, 01, 10, 11 之间循环
        if (key_check(1, KEY_SIGNED)) {
            manual_state = (manual_state + 1) % 4;
            Serial.print(">> [KEY2短按]: 手动状态切换 -> ");
            Serial.println(manual_state, BIN);
        }

        // 根据状态码控制继电器 (低位控制风扇FUN_PIN，高位控制灯LED_PIN)
        set_led_status((manual_state & 0x02) ? RELAY_ON : RELAY_OFF); // 6号引脚
        set_fun_status((manual_state & 0x01) ? RELAY_ON : RELAY_OFF); // 7号引脚
    } 
    // 处于自动状态时
    else {
        static unsigned long last_sensor_time = 0;
        if (millis() < last_sensor_time) last_sensor_time = millis();
        
        if (millis() - last_sensor_time >= 1000) { // ⏳ 每 1000ms 才允许读一次传感器
            last_sensor_time = millis();
            // 只有时间到了，才在自动模式下触发传感器读取
            handle_auto_control(); 
        }
    }
}

/**
 * @brief 自动模式控制逻辑
 */
void handle_auto_control(void) {
    // 读取传感器数据
    unsigned int current_lux = ldr_get_lux();
    float current_temp = ds18b20_get_temp();

    // 1. 光敏控制灯光 (继电器6)
    // 低于阈值开灯，高于/等于阈值关灯
    if (current_lux < light_threshold) {
        set_led_status(RELAY_ON);
    } else {
        set_led_status(RELAY_OFF);
    }

    // 2. 温度控制风扇 (继电器7)
    // 高于阈值开风扇，低于/等于阈值关风扇
    if (current_temp > temp_threshold) {
        set_fun_status(RELAY_ON);
    } else {
        set_fun_status(RELAY_OFF);
    }
}

/**
 * @brief 构造并上报当前的设备状态 (JSON 格式)
 */
void report_device_status(void) {
    StaticJsonDocument<256> doc;

    // 填充 JSON 数据
    doc["temperature"] = ds18b20_get_temp();
    doc["light"] = ldr_get_lux();
    doc["mode"] = is_auto_mode ? "auto" : "manual";
    doc["key1_lock"] = sys_power_on;
    doc["relay3"] = (get_led_status() == RELAY_ON);
    doc["relay4"] = (get_fun_status() == RELAY_ON);
    doc["temp_threshold"] = temp_threshold;
    doc["light_threshold"] = light_threshold;

    // 序列化为字符串并通过 MQTT 发布
    char output[256];
    serializeJson(doc, output);
    mqtt_client_publish(output);
    
    Serial.print(">> [MQTT 上报]: ");
    Serial.println(output);
}

/**
 * @brief 接收并解析来自服务器的控制命令 (下行 JSON)
 */
void mqtt_callback_handler(char* topic, byte* payload, unsigned int length) {
    // 如果总闸关闭，直接丢弃拒绝执行服务器远程控制
    if (!sys_power_on) {
        Serial.println(">> [MQTT 拒绝]: 总闸关闭中，无视远程下行命令。");
        return;
    }

    // 解析 JSON 字符串
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.println(">> [MQTT 解析错误]: JSON 格式非法！");
        return;
    }

    // 获取命令类型
    const char* cmd = doc["cmd"];
    if (cmd == NULL) return;

    Serial.print(">> [MQTT 收到命令]: ");
    Serial.println(cmd);

    // 1. 控制继电器命令 (set_relay)
    if (strcmp(cmd, "set_relay") == 0) {
        int relay_id = doc["relay"];
        bool val = doc["value"];

        if (is_auto_mode) {
            Serial.println(">> [MQTT 拒绝]: 当前是自动模式，无法手动控制继电器！");
            return;
        }

        if (relay_id == 3) { // 对应系统里的 LED 灯光
            set_led_status(val ? RELAY_ON : RELAY_OFF);
            // 同步手动状态码
            if (val) manual_state |= 0x02; else manual_state &= ~0x02;
        } 
        else if (relay_id == 4) { // 对应系统里的 风机
            set_fun_status(val ? RELAY_ON : RELAY_OFF);
            // 同步手动状态码
            if (val) manual_state |= 0x01; else manual_state &= ~0x01;
        }
        report_device_status(); // 状态改变后立即主动上报一次
    }
    // 2. 切换工作模式命令 (set_mode)
    else if (strcmp(cmd, "set_mode") == 0) {
        const char* mode = doc["mode"];
        if (strcmp(mode, "auto") == 0) {
            is_auto_mode = true;
        } else if (strcmp(mode, "manual") == 0) {
            is_auto_mode = false;
        }
        report_device_status();
    }
    // 3. 查询设备状态命令 (get_status)
    else if (strcmp(cmd, "get_status") == 0) {
        report_device_status(); // 立即上报一次
    }
    // 4. 设置阈值命令 (set_threshold)
    else if (strcmp(cmd, "set_threshold") == 0) {
        if (doc.containsKey("temp")) {
            temp_threshold = doc["temp"];
        }
        if (doc.containsKey("light")) {
            light_threshold = doc["light"];
        }
        report_device_status();
    }
    // 5. 远程重启设备命令 (reboot)
    else if (strcmp(cmd, "reboot") == 0) {
        Serial.println(">> [MQTT 重启]: 正在通过软件重启系统...");
        delay(500);
        ESP.restart(); // ESP32/ESP8266 软件重启指令
    }
}