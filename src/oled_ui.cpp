#include "oled_ui.h"
#include "device.h"

// 参数含义：旋转角度, 时钟引脚, 数据引脚, 复位引脚
U8G2_SH1106_128X64_VCOMH0_F_SW_I2C u8g2(U8G2_R0, 
                                         OLED_SCL_PIN, 
                                         OLED_SDA_PIN, 
                                         OLED_RST_PIN);

void oled_ui_init(void) {
    u8g2.begin();
    u8g2.setContrast(0x60);
    u8g2.setPowerSave(0);
    
    // 设置等宽英文字体，清晰度高
    u8g2.setFont(u8g2_font_wqy12_t_gb2312); 
    u8g2.setFontRefHeightExtendedText();
    u8g2.setFontPosTop(); // 坐标原点在字体顶部 (y = 0, 13, 26, 39, 52)
}

void oled_ui_refresh(const SystemState& state) {
    u8g2.clearBuffer();
    char buf[32];

    // 第0行：WiFi / MQTT 连接状态 (y = 0)
    snprintf(buf, sizeof(buf), "WiFi: %s",state.wifi_connected ? "ON" : "OFF");
    u8g2.drawUTF8(0, 0, buf);
    snprintf(buf, sizeof(buf), "MQTT: %s", state.mqtt_connected ? "ON" : "OFF");
    u8g2.drawUTF8(70, 0, buf);
    // 1. 第一行：MODE 与 MAIN (y = 13)
    snprintf(buf, sizeof(buf), "模式: %s", state.is_auto ? "自动" : "手动");
    u8g2.drawUTF8(0, 13, buf);
    snprintf(buf, sizeof(buf), "总闸: %s", state.power_on ? "ON" : "OFF");
    u8g2.drawUTF8(70, 13, buf);

    // 2. 第二行：LIGHT 实时值 / 阈值 (y = 26)
    snprintf(buf, sizeof(buf), "光照: %u/%u", state.current_lux, state.light_th);
    u8g2.drawUTF8(0, 26, buf);

    // 3. 第三行：TEMP 实时值 / 阈值 (y = 39)
    snprintf(buf, sizeof(buf), "温度: %.1f/%.1f", state.current_temp, state.temp_th);
    u8g2.drawUTF8(0, 39, buf);

    // 4. 第四行：LED 与 FAN 状态 (y = 52)
    snprintf(buf, sizeof(buf), "灯光: %s", state.led_relay_on ? "ON" : "OFF");
    u8g2.drawUTF8(0, 52, buf);
    snprintf(buf, sizeof(buf), "风扇: %s", state.fan_relay_on ? "ON" : "OFF");
    u8g2.drawUTF8(70, 52, buf);

    u8g2.sendBuffer();
}