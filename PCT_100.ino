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
#include "serial_cmd.h"
#include "ws2812_drv.h"


// ==================== 业务状态全局变量 ====================
struct SystemState {
    bool power_on;             // 系统总闸状态: true-开启, false-关闭
    bool is_auto;              // 模式: true-自动, false-手动
    uint8_t manual_code;       // 手动模式下的状态码 (00~11)
    
    float current_temp;        // 温度全局缓存
    unsigned int current_lux;  // 光照全局缓存
    
    float temp_th;             // 温度高值阈值
    unsigned int light_th;     // 光照阈值

    bool wifi_connected;       // 🌟 新增：WiFi 连接状态 (true-已连接, false-未连接)
};

// 实例化全局状态并初始化
SystemState sys = {
    .power_on       = false,
    .is_auto        = true,
    .manual_code    = 0,
    .current_temp   = 25.0,
    .current_lux    = 500,
    .temp_th        = 30.0,
    .light_th       = 300,
    .wifi_connected = false    // 🌟 默认未连接
};
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
// ==================== Arduino 核心接口 ====================
void setup() {
    Serial.begin(115200);
    
    // 初始化所有硬件外设
    key_init();
    relay_init();
    ldr_init();
    ds18b20_init();
    oled_ui_init();
    wifi_drv_init();
    ws2812_drv_init();

    // 初始化MQTT
    mqtt_client_init();
    mqtt_client_set_callback(mqtt_callback_handler);
    
    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // ----------------------------------------------------
    // 网络层任务：高频非阻塞轮询
    // ----------------------------------------------------
    // 串口命令处理
    serial_cmd_loop();

    // 驱动库自己管内部状态，main.cpp 只需要机械性地调用 loop
    wifi_drv_loop();
    ws2812_drv_loop();

    // 只要硬件网络层是通的，就维持 MQTT 客户端底层轮询
    if (wifi_drv_is_connected()) {
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
        
        if (sys.power_on) {
            // 光敏电阻 ADC 读取极快，50ms 频率让手遮挡时立刻能做出硬件响应
            sys.current_lux = ldr_get_lux(); 
            
            if (sys.is_auto) {
                handle_ldr_auto_control(); // 毫秒级闭环灯光
            }
        }
    }

    // ----------------------------------------------------
    // 核心任务 4:温度采集与风机控制（每 2000ms 调度一次）
    // ----------------------------------------------------
    if (current_time - timer_sensor >= 2000) {
        timer_sensor = current_time;
        
        if (sys.power_on) {
            // DS18B20 转换慢，保持 2 秒慢速读取，防止卡死单片机
            sys.current_temp = ds18b20_get_temp(); 
            
            if (sys.is_auto) {
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
            sys.power_on, 
            sys.is_auto, 
            sys.current_lux, 
            sys.light_th, 
            sys.current_temp, 
            sys.temp_th, 
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
        if (!sys.power_on) {
            sys.power_on = true;
            sys.is_auto = true; // 开启时默认进入自动模式

            oled_need_refresh = true;
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
            report_device_status();
        }
    } else {
        if (sys.power_on) {
            sys.power_on = false;
            // 总闸关闭，强制关闭所有继电器 (模式归00)
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            sys.manual_code = 0;

            oled_need_refresh = true;
            Serial.println(">> [总闸]: 关闭！所有功能切断，继电器归 00。");
        }
        return; // 总闸关闭状态下，不执行后续任何操作
    }

    // ---- KEY2 模式切换与手动控制 ----
    // 长按 KEY2:切换 自动/手动 模式
    if (key_check(1, KEY_LONG)) {
        sys.is_auto = !sys.is_auto;
        oled_need_refresh = true;

        Serial.print(">> [KEY2长按]: 切换模式为 -> ");
        Serial.println(sys.is_auto ? "【自动模式】" : "【手动模式】");
        report_device_status();
        return;
    }

    // 处于手动状态时
    if (!sys.is_auto) {
        // 短按 KEY2:在 00, 01, 10, 11 之间循环
        if (key_check(1, KEY_SIGNED)) {
            sys.manual_code = (sys.manual_code + 1) % 4;
            oled_need_refresh = true;
            
            Serial.print(">> [KEY2短按]: 手动状态切换 -> ");
            Serial.println(sys.manual_code, BIN);
            // 根据状态码控制继电器 (低位控制风扇FUN_PIN，高位控制灯LED_PIN)
            set_led_status((sys.manual_code & 0x02) ? RELAY_ON : RELAY_OFF); // 6号引脚
            set_fun_status((sys.manual_code & 0x01) ? RELAY_ON : RELAY_OFF); // 7号引脚
            report_device_status();
        }
    } 
}

/**
 * @brief 自动模式控制逻辑：灯光控制
 */
void handle_ldr_auto_control(void) {
    bool next_state = (sys.current_lux < sys.light_th) ? RELAY_ON : RELAY_OFF;
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
    bool next_state = (sys.current_temp > sys.temp_th) ? RELAY_ON : RELAY_OFF;
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

    doc["temperature"] = sys.current_temp;
    doc["light"] = sys.current_lux;
    doc["mode"] = sys.is_auto ? "auto" : "manual";
    doc["key1_lock"] = sys.power_on;
    doc["relay3"] = (get_led_status() == RELAY_ON);
    doc["relay4"] = (get_fun_status() == RELAY_ON);
    doc["sys.temp_th"] = sys.temp_th;
    doc["sys.light_th"] = sys.light_th;

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
    if (!sys.power_on) {
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

    //Serial.print(">> [MQTT 收到命令]: ");
    //Serial.println(cmd);

    // 1. 控制继电器命令 (set_relay)
    if (strcmp(cmd, "set_relay") == 0) {
        int relay_id = doc["relay"];
        bool val = doc["value"];

        if (sys.is_auto) {
            Serial.println(">> [MQTT 拒绝]: 当前是自动模式，无法手动控制继电器！");
            return;
        }

        if (relay_id == 3) { // 对应系统里的 LED 灯光
            set_led_status(val ? RELAY_ON : RELAY_OFF);
            // 同步手动状态码
            if (val) sys.manual_code |= 0x02; else sys.manual_code &= ~0x02;
        } 
        else if (relay_id == 4) { // 对应系统里的 风机
            set_fun_status(val ? RELAY_ON : RELAY_OFF);
            // 同步手动状态码
            if (val) sys.manual_code |= 0x01; else sys.manual_code &= ~0x01;
        }
        report_device_status(); // 状态改变后立即主动上报一次
    }
    // 2. 切换工作模式命令 (set_mode)
    else if (strcmp(cmd, "set_mode") == 0) {
        const char* mode = doc["mode"];
        if (strcmp(mode, "auto") == 0) {
            sys.is_auto = true;
        } else if (strcmp(mode, "manual") == 0) {
            sys.is_auto = false;
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
            sys.temp_th = doc["temp"];
        }
        if (doc.containsKey("light")) {
            sys.light_th = doc["light"];
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