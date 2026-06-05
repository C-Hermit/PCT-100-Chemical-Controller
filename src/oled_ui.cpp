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
    u8g2.setFontPosTop(); // 将坐标原点设在字体的左上角 (y = 0, 16, 32, 48)
}

void oled_ui_refresh(void) {
    u8g2.clearBuffer();
    char buf[32];

    // 1. 第一行：模式 + 总闸 + WiFi (y = 0)
    snprintf(buf, sizeof(buf), "模式:%s", sys.is_auto ? "自动" : "手动");
    u8g2.drawUTF8(0, 0, buf);
    snprintf(buf, sizeof(buf), "总闸:%s", sys.power_on ? "ON" : "OFF");
    u8g2.drawUTF8(56, 0, buf);
    snprintf(buf, sizeof(buf), "W:%s", sys.wifi_connected ? "Y" : "N");
    u8g2.drawUTF8(100, 0, buf);

    // 2. 第二行：LIGHT 实时值 / 阈值 (y = 16)
    snprintf(buf, sizeof(buf), "光照: %u/%u", sys.current_lux, sys.light_th);
    u8g2.drawUTF8(0, 16, buf);

    // 3. 第三行：TEMP 实时值 / 阈值 (y = 32)
    snprintf(buf, sizeof(buf), "温度: %.1f/%.1f", sys.current_temp, sys.temp_th);
    u8g2.drawUTF8(0, 32, buf);

    // 4. 第四行：LED 与 FAN 状态 (y = 48)
    snprintf(buf, sizeof(buf), "灯光: %s", sys.led_relay_on ? "ON" : "OFF");
    u8g2.drawUTF8(0, 48, buf);
    snprintf(buf, sizeof(buf), "风扇: %s", sys.fan_relay_on ? "ON" : "OFF");
    u8g2.drawUTF8(70, 48, buf);

    u8g2.sendBuffer();
}