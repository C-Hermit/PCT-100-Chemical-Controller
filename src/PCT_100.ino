#include <Arduino.h>

// ==================== 驱动层：硬件抽象 ====================
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"
#include "oled_ui.h"
#include "ws2812_drv.h"

// ==================== 传输层：通信协议栈 ====================
#include "wifi_drv.h"
#include "mqtt_client_drv.h"

// ==================== 业务层：领域逻辑 ====================
#include "controller.h"
#include "mqtt_handler.h"
#include "ws2812_indicator.h"

// ==================== 编排层：调度 / 命令 / 状态 ====================
#include "scheduler.h"
#include "serial_cmd.h"
#include "device.h"

// ==================== 全局状态实例 ====================
SystemState sys = {
    .power_on    = false,
    .is_auto     = true,
    .manual_code = 0,
    .current_temp = 25.0,
    .current_lux = 500,
    .temp_th     = 30.0,
    .light_th    = 300,
    .wifi_connected = false,
    .mqtt_connected = false,
    .led_relay_on = false,
    .fan_relay_on = false,
};

// 任务 ID，供其他模块唤醒
int oled_task_id;

// ==================== 定时任务包装器 ====================
static void task_key_logic(void)  { ctrl_key_logic(); }
static void task_ldr_sense(void)  { ctrl_auto_light(); }
static void task_temp_sense(void) { ctrl_auto_fan(); }

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

    // ── 驱动层初始化 ──
    key_init();
    relay_init();
    ldr_init();
    ds18b20_init();
    oled_ui_init();
    ws2812_drv_init();

    // ── 传输层初始化 ──
    wifi_drv_init();
    mqtt_client_init();

    // ── 业务层初始化 ──
    ctrl_init();
    mqtt_handler_init();
    ws2812_indicator_init();

    // ── 编排层：注册定时任务（含 FreeRTOS 任务栈大小）──
    sched_init();
    sched_add(task_key_logic,         20,   2048);  // 按键状态机，栈需求小
    sched_add(task_ldr_sense,         50,   2048);  // 光照采样，栈需求小
    sched_add(task_temp_sense,        2000, 4096);  // DS18B20 1-Wire 协议，栈需求较大
    oled_task_id = sched_add(task_oled_refresh, 3000, 4096);  // U8g2 字库渲染，栈需求大
    sched_add(ws2812_indicator_loop,  20,   2048);  // 指示灯动画，栈需求小

    Serial.println(">> 系统初始化完成，等待总闸 KEY1 闭合...");
}

void loop() {
    // ── 传输层轮询 ──
    serial_cmd_loop();
    wifi_drv_loop();
    if (wifi_drv_is_connected()) {
        mqtt_client_loop();
    }

    // ── 驱动层轮询 ──
    key_scan();

    // ── 刷新系统状态快照（供业务层指示灯等模块使用）──
    sys.wifi_connected = wifi_drv_is_connected();
    sys.mqtt_connected = sys.wifi_connected && mqtt_client_is_connected();

    // ── 编排层：定时调度 ──
    sched_run();
}
