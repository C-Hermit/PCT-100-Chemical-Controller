#ifndef _KEY_C
#define _KEY_C
#include "key.h"


// 按键状态宏定义
#define KEY_RELEASE 0
#define KEY_PRESS   1

uint8_t key_flag[KEY_COUNT]; //按键状态标志


/**
 * @brief 初始化按键引脚
 */
void key_init(void)
{
    // 1. 配置引脚为输入、默认下拉
    pinMode(KEY1_PIN, INPUT_PULLDOWN);
    pinMode(KEY2_PIN, INPUT_PULLDOWN);
}
/**
 * @brief 检查按键引脚
 */
uint8_t key_check(uint8_t n, uint8_t flag)
{
    if(key_flag[n]&flag)
    {
        if(flag!=KEY_HOLD)
        {
            key_flag[n]&=~flag;
        }
        return 1;
    }
    return 0;
}
/**
 * @brief 获取按键状态
 */
uint8_t key_getstate(uint8_t n)
{
    if(n==0)
    {
        if(digitalRead(KEY1_PIN)==1)
        {
            return KEY_PRESS;
        }
    }
    else if(n==1)
    {
        if(digitalRead(KEY2_PIN)==1)
        {
            return KEY_PRESS;
        }
    }
    return KEY_RELEASE;
}
/**
 * @brief 按键扫描函数（由 FreeRTOS 任务每 20ms 调用一次）
 * 包含 20ms 的软件消抖逻辑
 */
void key_scan(void)
{
    static uint8_t i;
    static uint8_t cur_state[KEY_COUNT],pre_state[KEY_COUNT];
    static uint8_t state[KEY_COUNT];
    static uint16_t time_cnt[KEY_COUNT];

    for (i = 0; i < KEY_COUNT; i++)
    {
        pre_state[i]=cur_state[i];
        cur_state[i]=key_getstate(i);

        if(time_cnt[i]>0)
        {
            time_cnt[i]--;
        }

        if(cur_state[i]==KEY_PRESS)
        {
            key_flag[i]|=KEY_HOLD;
        }
        else
        {
            key_flag[i]&=~KEY_HOLD;
        }

        if(state[i]==0)
        {
            if(cur_state[i]==KEY_PRESS)
            {
                time_cnt[i]=KEY_TIME_LONG;
                state[i]=1;
            }
        }
        else if(state[i]==1)
        {
            if(cur_state[i]==KEY_RELEASE)
            {
                key_flag[i]|=KEY_SIGNED;
                state[i]=0;
            }
            else if(time_cnt[i]==0)
            {
                key_flag[i]|=KEY_LONG;
                state[i]=2;
            }
        }
        else if(state[i]==2)
        {
            if(cur_state[i]==KEY_RELEASE)
            {
                state[i]=0;
            }
        }
    }
}

#endif