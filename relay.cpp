#ifndef _RELAY_C
#define _RELAY_C
#include "relay.h"

// 记录当前继电器的内部状态
static uint8_t led_status = RELAY_OFF;
static uint8_t fun_status = RELAY_OFF;

/**
 * @brief 初始化继电器引脚为输出模式，并默认关闭
 */
void relay_init(void)
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(FUN_PIN, OUTPUT);
    
    // 默认上电关闭
    set_led_status(RELAY_OFF);
    set_fun_status(RELAY_OFF);
}

/**
 * @brief 设置 LED 继电器的状态
 */
void set_led_status(uint8_t status)
{
    led_status = status;
    digitalWrite(LED_PIN, led_status);
}

/**
 * @brief 翻转 LED 继电器的状态
 */
void toggle_led(void)
{
    if (led_status == RELAY_ON) {
        set_led_status(RELAY_OFF);
    } else {
        set_led_status(RELAY_ON);
    }
}

/**
 * @brief 获取当前 LED 继电器的内部记录状态
 * @return RELAY_ON (1) 或 RELAY_OFF (0)
 */
uint8_t get_led_status(void)
{
    return led_status;
    // 调试进阶提示：也可以直接用 digitalRead(LED_PIN) 读取引脚真实物理电平
}

/**
 * @brief 设置 风扇继电器的状态
 */
void set_fun_status(uint8_t status)
{
    fun_status = status;
    digitalWrite(FUN_PIN, fun_status);
}

/**
 * @brief 翻转 风扇继电器的状态
 */
void toggle_fun(void)
{
    if (fun_status == RELAY_ON) {
        set_fun_status(RELAY_OFF);
    } else {
        set_fun_status(RELAY_ON);
    }
}

/**
 * @brief 获取当前 风扇继电器的内部记录状态
 * @return RELAY_ON (1) 或 RELAY_OFF (0)
 */
uint8_t get_fun_status(void)
{
    return fun_status;
}

#endif