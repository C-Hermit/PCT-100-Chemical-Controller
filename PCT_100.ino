#include <Arduino.h>
#include <ArduinoJson.h> // 需要在库管理器中安装 ArduinoJson 库

// 引入所有底层驱动头文件
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"
#include "wifi_drv.h"
#include "mqtt_client_drv.h"
#include "oled_ui.h"
#include "serial_ctrl_drv.h"

// ==================== 业务状态全局变量 ====================
bool sys_power_on = false;     // 系统总闸状态:true-开启,false-关闭
bool is_auto_mode = true;      // 模式:true-自动,false-手动
uint8_t manual_state = 0;      // 手动模式下的 4 种状态 (0~3 对应 00, 01, 10, 11)

// 阈值默认值
float temp_threshold = 30.0;   // 温度高值阈值
unsigned int light_threshold = 300; // 光照阈值

// 数据缓存变量
float gl_current_temp = 25.0;       // 温度全局缓存
unsigned int gl_current_lux = 500; // 光照全局缓存

// WiFi 信息
String input_ssid = "";
String input_pwd  = "";
bool wifi_configured = false;
// ==================== 时间片内核变量 ====================
unsigned long timer_key_logic = 0;
unsigned long timer_oled_ui   = 0;
unsigned long timer_ldr = 0; 
unsigned long timer_sensor= 0;
volatile bool oled_need_refresh = false;
// ==================== 核心功能函数声明 ====================
void handle_hardware_logic(void);
void handle_auto_control(void);
void report_device_status(void);
void mqtt_callback_handler(char* topic, byte* payload, unsigned int length);
void handle_serial_config(void);
// ==================== Arduino 核心接口 ====================
void setup() {
    Serial.begin(115200);
    
    // 初始化所有硬件外设
    key_init();
    relay_init();
    ldr_init();
    ds18b20_init();
    oled_ui_init();

    // 初始化网络与MQTT
    mqtt_client_init();
    mqtt_client_set_callback(mqtt_callback_handler);

    if (wifi_drv_auto_connect(input_ssid, input_pwd)) {
        wifi_configured = true; 
    } 
    else {
      // 串口交互扫描配网
      Serial.println(">> [WiFi]: 未满足自动联网条件，启动串口交互配网...");
      wifi_drv_interactive_config(input_ssid, input_pwd);

      if (wifi_drv_is_connected()) {
        wifi_configured = true;
      }
    }
    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // ----------------------------------------------------
    // 网络层任务：高频非阻塞轮询
    // ----------------------------------------------------
    serial_ctrl_loop();

    if (wifi_configured) {
        // 记住当前网络名，用于判断网络是否被换了
        String old_ssid = input_ssid; 
        
        // 传入引用，内部修改会直接同步到外层的全局变量中
        wifi_drv_loop(input_ssid, input_pwd, wifi_configured);
        
        // 状态交由外层解耦处理：如果 SSID 变了，说明内部触发了换网
        if (input_ssid != old_ssid) {
            oled_need_refresh = true; 
        }
        
        mqtt_client_loop();
    }
    
    // ----------------------------------------------------
    // 核心任务 1:高频实时任务（无条件满速轮询，微秒级响应）
    // ----------------------------------------------------
    key_scan();

    unsigned long current_time = millis();

    // ----------------------------------------------------
    // 核心任务 2:按键约束与逻辑状态机（每 20ms 调度一次）
    // ----------------------------------------------------
    if (current_time - timer_key_logic >= 20) {
        timer_key_logic = current_time;
        handle_hardware_logic();
    }

    // ----------------------------------------------------
    // 核心任务 3:光敏采集与光控响应（每 50ms 调度一次）
    // ----------------------------------------------------
    if (current_time - timer_ldr >= 50) {
        timer_ldr = current_time;
        
        if (sys_power_on) {
            // 光敏电阻 ADC 读取极快，50ms 频率让手遮挡时立刻能做出硬件响应
            gl_current_lux = ldr_get_lux(); 
            
            if (is_auto_mode) {
                handle_ldr_auto_control(); // 毫秒级闭环灯光
            }
        }
    }

    // ----------------------------------------------------
    // 核心任务 4:温度采集与风机控制（每 2000ms 调度一次）
    // ----------------------------------------------------
    if (current_time - timer_sensor >= 2000) {
        timer_sensor = current_time;
        
        if (sys_power_on) {
            // DS18B20 转换慢，保持 2 秒慢速读取，防止卡死单片机
            gl_current_temp = ds18b20_get_temp(); 
            
            if (is_auto_mode) {
                handle_sensor_auto_control(); // 闭环控制风机
            }
        }
    }

    // ----------------------------------------------------
    // 核心任务 4:OLED 屏幕数据异步刷新（每 300ms 调度一次）
    // ----------------------------------------------------
    if ((current_time - timer_oled_ui >= 3000)|| oled_need_refresh) {
        timer_oled_ui = current_time;
        oled_need_refresh = false;
        
        oled_ui_refresh(
            sys_power_on, 
            is_auto_mode, 
            gl_current_lux, 
            light_threshold, 
            gl_current_temp, 
            temp_threshold, 
            (get_led_status() == RELAY_ON), 
            (get_fun_status() == RELAY_ON)
        );
    }
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

            oled_need_refresh = true;
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
        }
    } else {
        if (sys_power_on) {
            sys_power_on = false;
            // 总闸关闭，强制关闭所有继电器 (模式归00)
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            manual_state = 0;

            oled_need_refresh = true;
            Serial.println(">> [总闸]: 关闭！所有功能切断，继电器归 00。");
        }
        return; // 总闸关闭状态下，不执行后续任何操作
    }

    // ---- KEY2 模式切换与手动控制 ----
    // 长按 KEY2:切换 自动/手动 模式
    if (key_check(1, KEY_LONG)) {
        is_auto_mode = !is_auto_mode;
        oled_need_refresh = true;

        Serial.print(">> [KEY2长按]: 切换模式为 -> ");
        Serial.println(is_auto_mode ? "【自动模式】" : "【手动模式】");
        return;
    }

    // 处于手动状态时
    if (!is_auto_mode) {
        // 短按 KEY2:在 00, 01, 10, 11 之间循环
        if (key_check(1, KEY_SIGNED)) {
            manual_state = (manual_state + 1) % 4;
            oled_need_refresh = true;
            
            Serial.print(">> [KEY2短按]: 手动状态切换 -> ");
            Serial.println(manual_state, BIN);
        }

        // 根据状态码控制继电器 (低位控制风扇FUN_PIN，高位控制灯LED_PIN)
        set_led_status((manual_state & 0x02) ? RELAY_ON : RELAY_OFF); // 6号引脚
        set_fun_status((manual_state & 0x01) ? RELAY_ON : RELAY_OFF); // 7号引脚
    } 
}

/**
 * @brief 自动模式控制逻辑：灯光控制
 */
void handle_ldr_auto_control(void) {
    bool next_state = (gl_current_lux < light_threshold) ? RELAY_ON : RELAY_OFF;
    if (get_led_status() != next_state) {
        set_led_status(next_state);
        oled_need_refresh = true;
        report_device_status(); // 仅在状态改变时上报，防闪烁刷屏
    }
}

/**
 * @brief 自动模式控制逻辑：风机控制
 */
void handle_sensor_auto_control(void) {
    bool next_state = (gl_current_temp > temp_threshold) ? RELAY_ON : RELAY_OFF;
    if (get_fun_status() != next_state) {
        set_fun_status(next_state);
        oled_need_refresh = true;
        report_device_status();
    }
}

/**
 * @brief 构造并上报当前的设备状态 (JSON 格式)
 */
void report_device_status(void) {
    StaticJsonDocument<256> doc;

    doc["temperature"] = gl_current_temp;
    doc["light"] = gl_current_lux;
    doc["mode"] = is_auto_mode ? "auto" : "manual";
    doc["key1_lock"] = sys_power_on;
    doc["relay3"] = (get_led_status() == RELAY_ON);
    doc["relay4"] = (get_fun_status() == RELAY_ON);
    doc["temp_threshold"] = temp_threshold;
    doc["light_threshold"] = light_threshold;

    char output[256];
    serializeJson(doc, output);
    mqtt_client_publish(output);
    
    // Serial.print(">> [MQTT 上报]: ");
    // Serial.println(output);
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