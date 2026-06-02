#include "ws2812_drv.h"
#include <Adafruit_NeoPixel.h> // 🌟 拥抱正规军，引入行业标准库

// 外部联动：获取 WiFi 驱动的联网状态
extern bool wifi_drv_is_connected(void); 

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

/**
 * @brief 🌟 核心非阻塞状态机 loop
 * 挂载在主 loop 中，无网络蓝灯交替闪烁，联网成功绿灯常亮，绝不卡主时序
 */
void ws2812_drv_loop(void) {
    static unsigned long last_blink_time = 0;
    static bool toggle = false;

    // 500ms 刷新周期，每一次执行仅需数微秒，完全不影响你的按键、继电器和总闸逻辑
    if (millis() - last_blink_time >= 500) {
        last_blink_time = millis();
        
        if (wifi_drv_is_connected()) {
            // 🟢 状态 A：网络已连通 -> 纯净绿灯常亮
            ws2812_drv_set_color(0, 0, 40, 0); 
        } 
        else {
            // 🔵 状态 B：未联网 / 异步配网中 -> 蓝灯交替闪烁提示
            toggle = !toggle;
            if (toggle) {
                ws2812_drv_set_color(0, 0, 0, 60); // 纯蓝色亮
            } else {
                ws2812_drv_clear(); // 灭
            }
        }
    }
}