#include "ds18b20_drv.h"
#include <DS18B20.h>

// 在类的内部或全局实例化库对象，传入指定的引脚
static DS18B20 ds(DS18B20_PIN);

/**
 * @brief 初始化 DS18B20
 */
void ds18b20_init(void)
{
    // matmunk 库内部会自动处理 OneWire 的初始化，这里可以留空
    // 或者做一些基础的引脚状态确认
    pinMode(DS18B20_PIN, INPUT); 
}

/**
 * @brief 获取第一个（或唯一一个）DS18B20 的温度值
 * @return 摄氏度 (float)
 */
float ds18b20_get_temp(void)
{
    // 重置总线搜索器
    ds.selectNext(); 
    
    // 读取温度
    float temp = ds.getTempC();
    
    // matmunk 库在读取失败或未连接时会返回特殊错误标志或 85.0 / 127.0
    // 如果需要过滤异常值，可以在这里处理
    return temp;
}

/**
 * @brief 遍历并打印单总线上挂载的所有 DS18B20 传感器的温度
 */
void ds18b20_print_all(void)
{
    int sensor_count = 0;
    
    // 重置搜索指针到第一个设备
    // selectNext() 会自动寻找下一个设备，直到找完返回 false
    while (ds.selectNext()) {
        sensor_count++;
        Serial.print("Sensor #");
        Serial.print(sensor_count);
        Serial.print(" Temp: ");
        Serial.print(ds.getTempC());
        Serial.println(" °C");
    }
    
    if (sensor_count == 0) {
        Serial.println("Error: 未检测到任何 DS18B20 传感器！");
    }
}