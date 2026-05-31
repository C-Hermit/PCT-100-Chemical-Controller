#ifndef _KEY_C
#define _KEY_C
#include "key.h"

// --- 轮询相关变量 (KEY1) ---
static unsigned long pre_cnt = 0;
static uint8_t key1_state = KEY_RELEASE;
static uint8_t last_key1_physical = LOW;

// --- 中断相关变量 (KEY2) ---
// volatile 关键字确保多核/中断环境下变量的可见性，防止被编译器优化
static volatile uint8_t key2_state = KEY_RELEASE; 
static volatile unsigned long last_interrupt_time = 0;

// KEY2 的中断服务函数 (ISR)
// 当 KEY2 引脚电平发生改变时自动调用
void key2_isr(void)
{
    unsigned long interrupt_time = millis();
    
    // 1. 软件消抖：限制两次中断触发的间隔必须大于 150ms
    if (interrupt_time - last_interrupt_time > 150) 
    {
        // 2. 核心：只有当确定是“按下”（LOW）时才触发状态翻转
        // 松开（HIGH）时虽然也会进中断，但不满足这个条件，什么都不做
        if (digitalRead(KEY2_PIN) == HIGH)
        {
            key2_state = !key2_state; // 状态翻转
        }
    }
    
    // 注意：时间戳的更新建议放在条件外面，或者只在有效触发时更新。
    // 这里保持每次中断都更新，以确保滤除后续的连续抖动
    last_interrupt_time = interrupt_time; 
}

/**
 * @brief 初始化按键引脚及中断
 */
void key_init(void)
{
    // 1. 配置引脚为输入、默认下拉
    pinMode(KEY1_PIN, INPUT_PULLDOWN);
    pinMode(KEY2_PIN, INPUT_PULLDOWN);

    // 2. 绑定 KEY2 的外部中断
    // CHANGE 模式：电平由低变高、或由高变低时都会触发中断
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), key2_isr, CHANGE);
}

/**
 * @brief 按键扫描函数（仅轮询 KEY1）
 * 包含 20ms 的软件消抖逻辑
 */
void key1_scan(void)
{
    unsigned long cur_cnt = millis();
    
    // 20ms 定时轮询 KEY1
    if (cur_cnt - pre_cnt >= 10) 
    {
        pre_cnt = cur_cnt;

        // --- KEY1 逻辑：锁死按键（自锁开关 / 状态翻转） ---
        uint8_t cur_key1_physical = digitalRead(KEY1_PIN);

        // 检测上升沿：从低电平变为高电平（代表刚刚被按下）
        if (cur_key1_physical == HIGH && last_key1_physical == LOW)
        {
            key1_state = !key1_state; // 状态取反（0变1，1变0）
        }
        if (cur_key1_physical == LOW && last_key1_physical == HIGH)
        {
            key1_state = !key1_state; // 状态取反（0变1，1变0）
        }
        last_key1_physical = cur_key1_physical; // 更新物理状态
    }
}

/**
 * @brief 获取 KEY1 (锁死按键) 的状态
 */
uint8_t get_key1_state(void)
{
    return key1_state;
}

/**
 * @brief 获取 KEY2 (回弹按键) 的状态
 */
uint8_t get_key2_state(void)
{
    return key2_state;
}

#endif