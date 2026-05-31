#include <Arduino.h>

// 1. 引入你写好的所有底层硬件驱动库
#include "key.h"
#include "relay.h"
#include "ldr.h"
#include "ds18b20_drv.h"

// 2. 引入你写好的 MQTT 通信协议驱动库
#include "mqtt_client_drv.h"

// 定时发送传感器数据的变量
unsigned long last_report_time = 0;
const unsigned long report_interval = 5000; // 每 5 秒上传一次数据（可自行调节）

void setup() {
    // 初始化硬件串口（用于电脑查看测试结果）
    Serial.begin(115200);
    while (!Serial) {
        ; // 等待串口准备就绪
    }
    
    Serial.println("\n==============================================");
    Serial.println("         ESP32-C3 综合外设与 MQTT 测试          ");
    Serial.println("==============================================");
    
    // 3. 初始化所有本地硬件外设
    // key_init();     // 初始化按键
    // relay_init();   // 初始化继电器
    ldr_init();        // 初始化光敏电阻（已确保在安全的 GPIO 1）
    ds18b20_init();    // 初始化温度传感器
    
    Serial.println("[System] 本地硬件驱动初始化完成。");
    
    // 4. 初始化网络并连接有密 MQTT 服务器
    // 注意：请确保在 mqtt_client_drv.h 中已配置好 WiFi 账号密码及 MQTT 账号密码
    mqtt_client_init();
}

void loop() {
    // 5. 核心：必须在 loop 中不停轮询，用于维持心跳、重连以及接收远程 MQTT 消息
    mqtt_client_loop();

    // 6. 本地按键联动继电器逻辑（如果需要测试本地控制）
    /*
    if (key_scan() == 1) { // 假设按键按下返回 1
        relay_toggle();    // 翻转继电器状态
        Serial.println("[Local] 检测到本地按键按下，翻转继电器状态！");
    }
    */

    // 7. 定时采集传感器数据并打包上传至 MQTT 服务器
    if (millis() - last_report_time > report_interval) {
        last_report_time = millis();

        // 从驱动库获取最新的光照和温度数据
        unsigned int lux_val = ldr_get_lux();
        float temp_val = ds18b20_get_temp();

        // 串口本地打印查看
        Serial.println("\n--- [本地采样数据] ---");
        Serial.print("光照强度: ");
        Serial.print(lux_val);
        Serial.println(" Lux");
        
        Serial.print("当前温度: ");
        if (temp_val == 85.0 || temp_val == 127.0 || temp_val < -50.0) {
            Serial.println("读取中或通信错误...");
        } else {
            Serial.print(temp_val, 1);
            Serial.println(" °C");
        }

        // 将数据组织成标准的 JSON 字符串格式
        // 样例：{"temp":25.5,"lux":450}
        String json_payload = "{\"temp\":" + String(temp_val, 1) + 
                             ",\"lux\":" + String(lux_val) + "}";

        // 调用 MQTT 驱动层的发布接口，将数据发送到云端
        if (mqtt_client_publish(json_payload.c_str())) {
            Serial.print("[MQTT] 数据上报成功 -> ");
            Serial.println(json_payload);
        } else {
            Serial.println("[MQTT] 数据上报失败（可能网络未连接）");
        }
        Serial.println("----------------------");
    }
}