#include <Arduino.h>

// 底层驱动
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"
#include "wifi_drv.h"
#include "mqtt_client_drv.h"
#include "oled_ui.h"
#include "ws2812_drv.h"

// 基础设施
#include "scheduler.h"
#include "serial_cmd.h"
#include "device.h"

// 业务模块
#include "controller.h"
#include "mqtt_handler.h"

// ==================== 全局状态实例 ====================
SystemState sys = {
    .power_on    = false,
    .is_auto     = true,
    .manual_code = 0,
    .current_temp = 25.0,
    .current_lux = 500,
    .temp_th     = 30.0,
    .light_th    = 300,
};

// 任务 ID，供其他模块唤醒
int oled_task_id;

// ==================== 定时任务包装器 ====================
static void task_key_logic(void)  { ctrl_key_logic(); }
static void task_ldr_sense(void)  { ctrl_auto_light(); }
static void task_temp_sense(void) { ctrl_auto_fan(); }

static void task_ws2812_indicator(void) {
    static bool toggle = false;
    if (wifi_drv_is_connected()) {
        ws2812_drv_set_color(0, 0, 40, 0);     // 绿色常亮
    } else {
        toggle = !toggle;
        if (toggle) {
            ws2812_drv_set_color(0, 0, 0, 60); // 蓝色亮
        } else {
            ws2812_drv_clear();                  // 灭
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

// ==================== Arduino 核心接口 ====================

void setup() {
    Serial.begin(115200);

    // 硬件初始化
    key_init();
    relay_init();
    ldr_init();
    ds18b20_init();
    oled_ui_init();
    wifi_drv_init();
    ws2812_drv_init();
    ctrl_init();

    // MQTT 传输层 + 协议层
    mqtt_client_init();
    mqtt_handler_init();

    // 注册定时任务
    sched_init();
    sched_add(task_key_logic,    20);
    sched_add(task_ldr_sense,    50);
    sched_add(task_temp_sense,   2000);
    oled_task_id = sched_add(task_oled_refresh, 3000);
    sched_add(task_ws2812_indicator, 500);

    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // 高频轮询
    serial_cmd_loop();
    wifi_drv_loop();
    if (wifi_drv_is_connected()) {
        mqtt_client_loop();
    }
    key_scan();

    // 定时调度
    sched_run();
}
