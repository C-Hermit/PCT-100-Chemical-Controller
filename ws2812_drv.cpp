#include "ws2812_drv.h"
#include <Adafruit_NeoPixel.h>

// 静态实例化底层驱动对象
// 参数：灯珠数量，物理引脚号，灯珠像素类型（WS2812 通常是 NEO_GRB + NEO_KHZ800）
static Adafruit_NeoPixel _pixels(WS2812_NUM_LEDS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

void ws2812_drv_init(void) {
    // 初始化 Adafruit 库
    _pixels.begin();
    _pixels.setBrightness(40); // 💡 设置适度亮度（0~255），防止大半夜刺眼
    _pixels.clear();
    _pixels.show(); // 先强制刷一次全灭，白光瞬间消散
    
    Serial.println(">> [WS2812]: Adafruit 标准驱动加载成功，时序恢复正常。");

    // ✨ 增加开机万色灯闪烁提示 (白光快闪 3 次，提示系统成功上电苏醒)
    for (int i = 0; i < 3; i++) {
        ws2812_drv_set_color(0, 120, 120, 120); // 适度白光亮
        delay(80);
        ws2812_drv_clear(); // 灭
        delay(80);
    }
}

void ws2812_drv_set_color(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= WS2812_NUM_LEDS) return;
    
    // 调用 Adafruit 标准接口写入颜色缓存并推向硬件
    _pixels.setPixelColor(index, _pixels.Color(r, g, b));
    _pixels.show(); 
}

void ws2812_drv_clear(void) {
    _pixels.clear();
    _pixels.show();
}

