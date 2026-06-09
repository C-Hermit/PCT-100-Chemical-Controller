#ifndef _RELAY_C
#define _RELAY_C
#include "relay.h"

// 记录当前继电器的内部状态
static uint8_t led_status = RELAY_OFF;
static uint8_t fan_status = RELAY_OFF;

/**
 * @brief 初始化继电器引脚为输出模式，并默认关闭
 */
void relay_init(void)
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);

    // 默认上电关闭
    relay_set_led(RELAY_OFF);
    relay_set_fan(RELAY_OFF);
}

/**
 * @brief 设置 LED 继电器的状态
 */
void relay_set_led(uint8_t status)
{
    led_status = status;
    digitalWrite(LED_PIN, led_status);
}

/**
 * @brief 翻转 LED 继电器的状态
 */
void relay_toggle_led(void)
{
    if (led_status == RELAY_ON) {
        relay_set_led(RELAY_OFF);
    } else {
        relay_set_led(RELAY_ON);
    }
}

/**
 * @brief 获取当前 LED 继电器的内部记录状态
 * @return RELAY_ON (1) 或 RELAY_OFF (0)
 */
uint8_t relay_get_led(void)
{
    return led_status;
}

/**
 * @brief 设置 风扇继电器的状态
 */
void relay_set_fan(uint8_t status)
{
    fan_status = status;
    digitalWrite(FAN_PIN, fan_status);
}

/**
 * @brief 翻转 风扇继电器的状态
 */
void relay_toggle_fan(void)
{
    if (fan_status == RELAY_ON) {
        relay_set_fan(RELAY_OFF);
    } else {
        relay_set_fan(RELAY_ON);
    }
}

/**
 * @brief 获取当前 风扇继电器的内部记录状态
 * @return RELAY_ON (1) 或 RELAY_OFF (0)
 */
uint8_t relay_get_fan(void)
{
    return fan_status;
}

#endif