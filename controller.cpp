#include "controller.h"
#include "device.h"
#include "scheduler.h"
#include "mqtt_handler.h"
#include "key.h"
#include "relay.h"

// scheduler 的 OLED 任务 ID，由 PCT_100.ino 定义
extern int oled_task_id;

// ==================== 初始化 ====================

void ctrl_init(void) {
    // 继电器初始化已在 relay_init() 中完成
}

// ==================== 按键状态机 ====================

void ctrl_key_logic(void) {
    // ---- KEY1 总闸 ----
    if (key_getstate(0) == KEY_HOLD) {
        if (!sys.power_on) {
            sys.power_on = true;
            sys.is_auto = true;
            Serial.println(">> [总闸]: 开启！切回默认自动模式。");
            sched_wake(oled_task_id);
            mqtt_handler_send_status();
        }
    } else {
        if (sys.power_on) {
            sys.power_on = false;
            set_led_status(RELAY_OFF);
            set_fun_status(RELAY_OFF);
            sys.manual_code = 0;
            Serial.println(">> [总闸]: 关闭！所有功能切断。");
            sched_wake(oled_task_id);
            mqtt_handler_send_status();
        }
        return;
    }

    // ---- KEY2 模式切换 ----
    if (key_check(1, KEY_LONG)) {
        sys.is_auto = !sys.is_auto;
        Serial.printf(">> [KEY2长按]: 模式 -> %s\n", sys.is_auto ? "自动" : "手动");
        sched_wake(oled_task_id);
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
        sched_wake(oled_task_id);
        mqtt_handler_send_status();
    }
}

// ==================== 自动控制 ====================

void ctrl_auto_light(void) {
    if (!sys.power_on) return;
    sys.current_lux = ldr_get_lux();

    if (sys.is_auto) {
        bool on = (sys.current_lux < sys.light_th);
        if (on != (get_led_status() == RELAY_ON)) {
            set_led_status(on ? RELAY_ON : RELAY_OFF);
            sched_wake(oled_task_id);
            mqtt_handler_send_status();
        }
    }
}

void ctrl_auto_fan(void) {
    if (!sys.power_on) return;
    sys.current_temp = ds18b20_get_temp();

    if (sys.is_auto) {
        bool on = (sys.current_temp > sys.temp_th);
        if (on != (get_fun_status() == RELAY_ON)) {
            set_fun_status(on ? RELAY_ON : RELAY_OFF);
            sched_wake(oled_task_id);
            mqtt_handler_send_status();
        }
    }
}

// ==================== 外部命令 ====================

void ctrl_set_relay(int id, bool on) {
    if (id == 3) {
        set_led_status(on ? RELAY_ON : RELAY_OFF);
        if (on) sys.manual_code |= 0x02; else sys.manual_code &= ~0x02;
    } else if (id == 4) {
        set_fun_status(on ? RELAY_ON : RELAY_OFF);
        if (on) sys.manual_code |= 0x01; else sys.manual_code &= ~0x01;
    }
    sched_wake(oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_mode(bool auto_mode) {
    sys.is_auto = auto_mode;
    sched_wake(oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_temp_threshold(float t) {
    sys.temp_th = t;
    sched_wake(oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_set_light_threshold(unsigned int l) {
    sys.light_th = l;
    sched_wake(oled_task_id);
    mqtt_handler_send_status();
}

void ctrl_power_off(void) {
    sys.power_on = false;
    set_led_status(RELAY_OFF);
    set_fun_status(RELAY_OFF);
    sys.manual_code = 0;
    sched_wake(oled_task_id);
    mqtt_handler_send_status();
}
