#include <Arduino.h>
#include <ArduinoJson.h>

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
#include "scheduler.h"

// ==================== 业务状态全局变量 ====================
struct SystemState {
    bool power_on;             // 系统总闸状态: true-开启, false-关闭
    bool is_auto;              // 模式: true-自动, false-手动
    uint8_t manual_code;       // 手动模式下的状态码 (00~11)

    float current_temp;        // 温度全局缓存
    unsigned int current_lux;  // 光照全局缓存

    float temp_th;             // 温度高值阈值
    unsigned int light_th;     // 光照阈值
};

SystemState sys = {
    .power_on       = false,
    .is_auto        = true,
    .manual_code    = 0,
    .current_temp   = 25.0,
    .current_lux    = 500,
    .temp_th        = 30.0,
    .light_th       = 300,
};

// ==================== 任务函数声明（调度器回调） ====================
static void task_key_logic(void);
static void task_ldr_sense(void);
static void task_temp_sense(void);
static void task_oled_refresh(void);

// ==================== 调度器任务 ID（对外暴露，供 serial_cmd 等模块 wake） ====================
int oled_task_id;

// ==================== 核心函数声明 ====================
void handle_hardware_logic(void);
void handle_ldr_auto_control(void);
void handle_sensor_auto_control(void);
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

    // 初始化 MQTT
    mqtt_client_init();
    mqtt_client_set_callback(mqtt_callback_handler);

    // 注册定时任务（调度器接管所有定时业务）
    sched_init();
    sched_add(task_key_logic,    20);
    sched_add(task_ldr_sense,    50);
    sched_add(task_temp_sense,   2000);
    oled_task_id = sched_add(task_oled_refresh, 3000);

    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // ---- 高频轮询（每次 loop 必须执行） ----
    serial_cmd_loop();
    wifi_drv_loop();
    ws2812_drv_loop();
    if (wifi_drv_is_connected()) {
        mqtt_client_loop();
    }
    key_scan();

    // ---- 定时任务调度 ----
    sched_run();
}

// ==================== 定时任务实现 ====================

static void task_key_logic(void) {
    handle_hardware_logic();
}

static void task_ldr_sense(void) {
    if (sys.power_on) {
        sys.current_lux = ldr_get_lux();

        if (sys.is_auto) {
            handle_ldr_auto_control();
        }
    }
}

static void task_temp_sense(void) {
    if (sys.power_on) {
        sys.current_temp = ds18b20_get_temp();

        if (sys.is_auto) {
            handle_sensor_auto_control();
        }
    }
}

static void task_oled_refresh(void) {
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

// ==================== 业务逻辑实现 ====================

void handle_hardware_logic(void) {
    // ---- KEY1 总闸状态判断 ----
    if (key_getstate(0) == KEY_HOLD) {
        if (!sys.power_on) {
            sys.power_on = true;
            sys.is_auto = true;

            sched_wake(oled_task_id);
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
            report_device_status();
        }
    } else {
        if (sys.power_on) {
            sys.power_on = false;
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            sys.manual_code = 0;

            sched_wake(oled_task_id);
            Serial.println(">> [总闸]: 关闭！所有功能切断，继电器归 00。");
        }
        return;
    }

    // ---- KEY2 模式切换与手动控制 ----
    if (key_check(1, KEY_LONG)) {
        sys.is_auto = !sys.is_auto;
        sched_wake(oled_task_id);

        Serial.print(">> [KEY2长按]: 切换模式为 -> ");
        Serial.println(sys.is_auto ? "【自动模式】" : "【手动模式】");
        report_device_status();
        return;
    }

    if (!sys.is_auto) {
        if (key_check(1, KEY_SIGNED)) {
            sys.manual_code = (sys.manual_code + 1) % 4;
            sched_wake(oled_task_id);

            Serial.print(">> [KEY2短按]: 手动状态切换 -> ");
            Serial.println(sys.manual_code, BIN);
            set_led_status((sys.manual_code & 0x02) ? RELAY_ON : RELAY_OFF);
            set_fun_status((sys.manual_code & 0x01) ? RELAY_ON : RELAY_OFF);
            report_device_status();
        }
    }
}

void handle_ldr_auto_control(void) {
    bool next_state = (sys.current_lux < sys.light_th) ? RELAY_ON : RELAY_OFF;
    if (get_led_status() != next_state) {
        set_led_status(next_state);
        sched_wake(oled_task_id);
        report_device_status();
    }
}

void handle_sensor_auto_control(void) {
    bool next_state = (sys.current_temp > sys.temp_th) ? RELAY_ON : RELAY_OFF;
    if (get_fun_status() != next_state) {
        set_fun_status(next_state);
        sched_wake(oled_task_id);
        report_device_status();
    }
}

void report_device_status(void) {
    StaticJsonDocument<256> doc;

    doc["temperature"] = sys.current_temp;
    doc["light"] = sys.current_lux;
    doc["mode"] = sys.is_auto ? "auto" : "manual";
    doc["key1_lock"] = sys.power_on;
    doc["relay3"] = (get_led_status() == RELAY_ON);
    doc["relay4"] = (get_fun_status() == RELAY_ON);
    doc["temp_th"] = sys.temp_th;
    doc["light_th"] = sys.light_th;

    char output[256];
    serializeJson(doc, output);
    mqtt_client_publish(output);
}

// ==================== MQTT 下行命令回调 ====================

void mqtt_callback_handler(char* topic, byte* payload, unsigned int length) {
    if (!sys.power_on) {
        Serial.println(">> [MQTT 拒绝]: 总闸关闭中，无视远程下行命令。");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.println(">> [MQTT 解析错误]: JSON 格式非法！");
        return;
    }

    const char* cmd = doc["cmd"];
    if (cmd == NULL) return;

    if (strcmp(cmd, "set_relay") == 0) {
        int relay_id = doc["relay"];
        bool val = doc["value"];

        if (sys.is_auto) {
            Serial.println(">> [MQTT 拒绝]: 当前是自动模式，无法手动控制继电器！");
            return;
        }

        if (relay_id == 3) {
            set_led_status(val ? RELAY_ON : RELAY_OFF);
            if (val) sys.manual_code |= 0x02; else sys.manual_code &= ~0x02;
        } else if (relay_id == 4) {
            set_fun_status(val ? RELAY_ON : RELAY_OFF);
            if (val) sys.manual_code |= 0x01; else sys.manual_code &= ~0x01;
        }
        report_device_status();
    } else if (strcmp(cmd, "set_mode") == 0) {
        const char* mode = doc["mode"];
        if (strcmp(mode, "auto") == 0) {
            sys.is_auto = true;
        } else if (strcmp(mode, "manual") == 0) {
            sys.is_auto = false;
        }
        report_device_status();
    } else if (strcmp(cmd, "get_status") == 0) {
        report_device_status();
    } else if (strcmp(cmd, "set_threshold") == 0) {
        if (doc.containsKey("temp"))  sys.temp_th  = doc["temp"];
        if (doc.containsKey("light")) sys.light_th = doc["light"];
        report_device_status();
    } else if (strcmp(cmd, "reboot") == 0) {
        Serial.println(">> [MQTT 重启]: 正在通过软件重启系统...");
        delay(500);
        ESP.restart();
    }
}
