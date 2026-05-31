#include <Arduino.h>
#include "key.h"

// 引入 key.c 中定义的全局变量
extern uint8_t key_flag[KEY_COUNT]; 

void setup() {
    // 初始化串口，波特率 115200
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待串口连接（部分开发板如 Leonardo/Micro 需要）
    }
    Serial.println("--- 按键状态测试程序启动 ---");

    // 初始化按键引脚
    key_init();
}

void loop() {
    // 核心：必须在主循环中不断调用扫描函数
    key_scan();

    // 1. 检测单击 (KEY_SIGNED)
        if (key_check(1, KEY_SIGNED)) {
            Serial.print("【按键 ");
            Serial.print(1+ 1);
            Serial.println("】触发：单击 (Single Click)");
            
            // 调用 key_check 清除该标志位
            
        }

        // 2. 检测长按 (KEY_LONG)
        if (key_check(1, KEY_LONG)) {
            Serial.print("【按键 ");
            Serial.print(1+ 1);
            Serial.println("】触发：长按 2 秒 (Long Press)");
            
            // 调用 key_check 清除该标志位
        }
    // 轮询检测所有按键
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        
        

        // 3. 检测保持按下 (KEY_HOLD)
        // 注意：根据你的 key_check 逻辑，KEY_HOLD 不会被清除，只要按着就一直为 1
        // 为了防止串口被“按下”信息刷屏，这里配合 static 变量做了一个电平变化打印
        static uint8_t last_hold_state[KEY_COUNT] = {0};
        uint8_t current_hold = (key_flag[i] & KEY_HOLD) ? 1 : 0;
        
        if (current_hold != last_hold_state[i]) {
            if (current_hold) {
                Serial.print("【按键 ");
                Serial.print(i + 1);
                Serial.println("】状态：正在被按下... (Holding)");
            } else {
                Serial.print("【按键 ");
                Serial.print(i + 1);
                Serial.println("】状态：已松开 (Released)");
            }
            last_hold_state[i] = current_hold;
        }
    }

    // 适当小延时，避免 loop 空转过快消耗 CPU（不影响 key_scan 的 20ms 定时）
    delay(1); 
}