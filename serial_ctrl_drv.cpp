#include "serial_ctrl_drv.h"
#include "wifi_drv.h"
#include <ArduinoJson.h>

// 外部全局变量引用声明（与 main.cpp 共享状态）
extern bool sys_power_on;
extern bool is_auto_mode;
extern uint8_t manual_state;
extern float temp_threshold;
extern unsigned int light_threshold;
extern String input_ssid;
extern String input_pwd;
extern bool wifi_configured;
extern volatile bool oled_need_refresh;

// 内部声明：状态主动上报函数（解耦调用）
extern void report_device_status(void);

void serial_ctrl_loop(void) {
    if (Serial.available() <= 0) {
        return;
    }

    // 读取一行数据，去掉首尾空白字符
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    // ----------------------------------------------------
    // 兼容逻辑 1：标准 JSON 格式调参（推荐，适合上位机/透传）
    // ----------------------------------------------------
    if (input.startsWith("{") && input.endsWith("}")) {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, input);
        if (error) {
            Serial.println(">> [串口错误]: JSON 格式解析失败！");
            return;
        }

        bool changed = false;
        if (doc.containsKey("temp_th")) {
            temp_threshold = doc["temp_th"];
            Serial.printf(">> [串口调参]: 温度阈值修改为 -> %.2f ℃\n", temp_threshold);
            changed = true;
        }
        if (doc.containsKey("light_th")) {
            light_threshold = doc["light_th"];
            Serial.printf(">> [串口调参]: 光照阈值修改为 -> %u Lux\n", light_threshold);
            changed = true;
        }
        if (doc.containsKey("mode")) {
            String m = doc["mode"].as<String>();
            if (m == "auto") { is_auto_mode = true; changed = true; }
            else if (m == "manual") { is_auto_mode = false; changed = true; }
        }

        if (changed) {
            oled_need_refresh = true;
            report_device_status(); // 修改成功后，同步向 MQTT 平台更新状态
        }
        return;
    }

    // ----------------------------------------------------
    // 兼容逻辑 2：K-V 键值对文本指令（适合串口助手手动敲击）
    // 格式例如：SET:TEMP=32.5 或 SET:LIGHT=250 或 GET:STATUS
    // ----------------------------------------------------
    input.toUpperCase(); // 转大写，防止大小写敏感引发误判
    
    if (input.startsWith("SET:")) {
        String cmd = input.substring(4);
        int eq_index = cmd.indexOf('=');
        if (eq_index != -1) {
            String key = cmd.substring(0, eq_index);
            String val = cmd.substring(eq_index + 1);
            bool changed = false;

            if (key == "TEMP") {
                temp_threshold = val.toFloat();
                Serial.printf(">> [串口调参]: 温度阈值设置为 -> %.1f ℃\n", temp_threshold);
                changed = true;
            } 
            else if (key == "LIGHT") {
                light_threshold = val.toInt();
                Serial.printf(">> [串口调参]: 光照阈值设置为 -> %u Lux\n", light_threshold);
                changed = true;
            }

            if (changed) {
                oled_need_refresh = true;
                report_device_status();
            }
        } else {
            Serial.println(">> [串口错误]: 语法错误。示例: SET:TEMP=30.5");
        }
    } 
    else if (input == "GET:STATUS") {
        // 本地打印当前参数
        Serial.println("\n============ [当前系统参数] ============");
        Serial.printf(">> 总闸状态: %s\n", sys_power_on ? "ON" : "OFF");
        Serial.printf(">> 运行模式: %s\n", is_auto_mode ? "AUTO" : "MANUAL");
        Serial.printf(">> 温度阈值: %.1f ℃\n", temp_threshold);
        Serial.printf(">> 光照阈值: %u Lux\n", light_threshold);
        Serial.println("=======================================");
    } 
    else {
        Serial.println(">> [串口错误]: 未知指令！可用格式：");
        Serial.println("   1. WiFi配置: SSID,PASSWORD");
        Serial.println("   2. 文本调参: SET:TEMP=30.5  SET:LIGHT=200");
        Serial.println("   3. 参数查询: GET:STATUS");
        Serial.println("   4. JSON调参: {\"temp_th\":30.5,\"light_th\":200}");
    }
}