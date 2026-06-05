#include "controller.h"
#include "device.h"
#include "scheduler.h"
#include "mqtt_handler.h"
#include "mqtt_client_drv.h"
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"
#include "oled_ui.h"

// scheduler 的 OLED 任务 ID（内部使用，由 ctrl_init 注册）
static int ctrl_oled_task_id;

static void task_oled_refresh(void) { oled_ui_refresh(sys); }

// ==================== 初始化 ====================

void ctrl_init(void) {
    // 注册定时任务（周期 ms, 栈大小字节）
    sched_add(ctrl_key_logic,  20,   2048);
    sched_add(ctrl_auto_light, 50,   2048);
    sched_add(ctrl_auto_fan,   2000, 4096);
    ctrl_oled_task_id = sched_add(task_oled_refresh, 3000, 4096);
}

// ==================== 按键状态机 ====================

void ctrl_key_logic(void) {
    // ---- KEY1 总闸 ----
    if (key_getstate(0) == KEY_HOLD) {
        if (!sys.power_on) {
            sys.power_on = true;
            sys.is_auto = true;
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
            sched_wake(ctrl_oled_task_id);
            mqtt_handler_send_status();
        }
    } else {
        if (sys.power_on) {
            sys.power_on = false;
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            sys.led_relay_on = false;
            sys.fan_relay_on = false;
            sys.manual_code = 0;
            Serial.println(">> [总闸]: 关闭！所有功能切断。");
            sched_wake(ctrl_oled_task_id);
            mqtt_handler_send_status();
        }
        return;
    }

    // ---- KEY2 模式切换 ----
    if (key_check(1, KEY_LONG)) {
        sys.is_auto = !sys.is_auto;
        Serial.printf(">> [KEY2长按]: 模式 -> %s\n", sys.is_auto ? "自动" : "手动");
        sched_wake(ctrl_oled_task_id);
        mqtt_handler_send_status();
        return;
    }

    // ---- KEY2 手动编码循环 ----
    if (!sys.is_auto && key_check(1, KEY_SIGNED)) {
        sys.manual_code = (sys.manual_code + 1) % 4;
        Serial.printf(">> [KEY2短按]: 手动状态 -> %d%d\n",
                      (sys.manual_code & 0x02) ? 1 : 0,
                      (sys.manual_code & 0x01) ? 1 : 0);
        set_led_status((sys.manual_code & 0x02) ? RELAY_ON : RELAY_OFF);
        set_fun_status((sys.manual_code & 0x01) ? RELAY_ON : RELAY_OFF);
        sys.led_relay_on = (sys.manual_code & 0x02);
        sys.fan_relay_on = (sys.manual_code & 0x01);
        sched_wake(ctrl_oled_task_id);
        mqtt_handler_send_status();
    }
}

// ==================== 自动控制 ====================

void ctrl_auto_light(void) {
    if (!sys.power_on) return;
    sys.current_lux = ldr_get_lux();

    if (sys.is_auto) {
        bool on = (sys.current_lux < sys.light_th);
        if (on != sys.led_relay_on) {
            set_led_status(on ? RELAY_ON : RELAY_OFF);
            sys.led_relay_on = on;
            sched_wake(ctrl_oled_task_id);
            mqtt_handler_send_status();
        }
    }
}

void ctrl_auto_fan(void) {
    if (!sys.power_on) return;
    sys.current_temp = ds18b20_get_temp();

    if (sys.is_auto) {
        bool on = (sys.current_temp > sys.temp_th);
        if (on != sys.fan_relay_on) {
            set_fun_status(on ? RELAY_ON : RELAY_OFF);
            sys.fan_relay_on = on;
            sched_wake(ctrl_oled_task_id);
            mqtt_handler_send_status();
        }
    }
}

// ==================== 外部命令 ====================

void ctrl_set_relay(int id, bool on) {
    if (id == 3) {
        set_led_status(on ? RELAY_ON : RELAY_OFF);
        sys.led_relay_on = on;
        if (on) sys.manual_code |= 0x02; else sys.manual_code &= ~0x02;
    } else if (id == 4) {
        set_fun_status(on ? RELAY_ON : RELAY_OFF);
        sys.fan_relay_on = on;
        if (on) sys.manual_code |= 0x01; else sys.manual_code &= ~0x01;
    }
    sched_wake(ctrl_oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_mode(bool auto_mode) {
    sys.is_auto = auto_mode;
    sched_wake(ctrl_oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_temp_threshold(float t) {
    sys.temp_th = t;
    sched_wake(ctrl_oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_light_threshold(unsigned int l) {
    sys.light_th = l;
    sched_wake(ctrl_oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_power_off(void) {
    sys.power_on = false;
    set_led_status(RELAY_OFF);
    set_fun_status(RELAY_OFF);
    sys.led_relay_on = false;
    sys.fan_relay_on = false;
    sys.manual_code = 0;
    sched_wake(ctrl_oled_task_id);
    mqtt_handler_send_status();
}

// ==================== MQTT 配置管理（业务层封装） ====================

void ctrl_set_mqtt_server(const char* ip, uint16_t port) {
    mqtt_config_t cfg = *mqtt_client_get_config();
    if (ip && strlen(ip) > 0) {
        strncpy(cfg.server, ip, sizeof(cfg.server) - 1);
        cfg.server[sizeof(cfg.server) - 1] = '\0';
    }
    if (port > 0) cfg.port = port;
    mqtt_client_update_config(&cfg);
}

void ctrl_set_mqtt_auth(const char* user, const char* pass) {
    mqtt_config_t cfg = *mqtt_client_get_config();
    if (user) {
        strncpy(cfg.user, user, sizeof(cfg.user) - 1);
        cfg.user[sizeof(cfg.user) - 1] = '\0';
    }
    if (pass) {
        strncpy(cfg.password, pass, sizeof(cfg.password) - 1);
        cfg.password[sizeof(cfg.password) - 1] = '\0';
    }
    mqtt_client_update_config(&cfg);
}

void ctrl_set_mqtt_device_id(const char* id) {
    if (!id || strlen(id) == 0) return;
    mqtt_config_t cfg = *mqtt_client_get_config();
    strncpy(cfg.device_id, id, sizeof(cfg.device_id) - 1);
    cfg.device_id[sizeof(cfg.device_id) - 1] = '\0';
    mqtt_client_update_config(&cfg);
}

void ctrl_mqtt_print_status(void) {
    const mqtt_config_t* cfg = mqtt_client_get_config();
    Serial.println("\n============ [MQTT 配置] ============");
    Serial.printf("  服务器:   %s:%u\n", cfg->server, cfg->port);
    Serial.printf("  用户名:   %s\n", cfg->user);
    Serial.printf("  DeviceID: %s\n", cfg->device_id);
    Serial.printf("  连接状态: %s\n", mqtt_client_is_connected() ? "已连接" : "未连接");
    Serial.println("======================================");
}

void ctrl_mqtt_reconnect(void) {
    mqtt_client_force_reconnect();
}
