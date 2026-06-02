#include "wifi_drv.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;
/**
 * @brief 内部私有函数：将 WiFi 信息保存到 NVS 闪存中
 */
static void save_wifi_credentials(const char* ssid, const char* password) {
    // 打开命名空间 "wifi_space"，false 表示可读写
    prefs.begin("wifi_space", false);
    
    // 写入键值对
    prefs.putString("ssid", ssid);
    prefs.putString("pwd", password);
    
    prefs.end(); // 关闭命名空间，保存生效
    Serial.println(">> [WiFi NVS]: 新的 WiFi 凭据已固化到闪存中。");
}

/**
 * @brief 初始化并连接指定的 WiFi（常用于串口配置新网络）
 */
void wifi_drv_init(const char* ssid, const char* password) {
    if (ssid == NULL || strlen(ssid) == 0) {
        Serial.println(">> [WiFi]: SSID 为空，取消连接。");
        return;
    }

    Serial.println();
    Serial.print(">> [WiFi]: 正在连接网络 -> ");
    Serial.println(ssid);

    // 显式断开之前可能存在的连接
    WiFi.disconnect(true);
    delay(100);

    WiFi.begin(ssid, password);
    
    // 这里采用非阻塞形式，由外部 loop 判定或在初始化时有限等待
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) { // 最多死等 10 秒
        delay(500);
        Serial.print(".");
        timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n>> [WiFi]: 连接成功！");
        Serial.print(">> [WiFi]: IP 地址: ");
        Serial.println(WiFi.localIP());
        save_wifi_credentials(ssid, password);
    } else {
        Serial.println("\n>> [WiFi]: 连接超时，等待稍后重试或重新配置。");
    }

}

/**
 * @brief 系统开机时尝试从 NVS 自动读取并回连旧 WiFi
 * @param out_ssid 传出参数，用于将读取到的 SSID 刷回 main.cpp 的全局变量
 * @param out_pwd  传出参数，用于将读取到的 PWD 刷回 main.cpp 的全局变量
 * @return true 闪存中有历史记录且发起了连接；false 闪存为空
 */
bool wifi_drv_auto_connect(String &out_ssid, String &out_pwd) {
    // 以只读方式（true）打开命名空间
    prefs.begin("wifi_space", true);
    
    // 读取数据，如果不存在则返回空字符串 ""
    String saved_ssid = prefs.getString("ssid", "");
    String saved_pwd  = prefs.getString("pwd", "");
    
    prefs.end();

    if (saved_ssid.length() > 0) {
        Serial.println("\n>> [WiFi NVS]: 发现历史网络记录，正在尝试自动回连...");
        
        // 将读取到的值赋给传出引用，同步 main.cpp 中的状态
        out_ssid = saved_ssid;
        out_pwd  = saved_pwd;

        // 启动连接（这里直接调用本库的初始化，如果连不上会进入断线重连状态机）
        // 为了防止开机 setup 死等太久影响别的硬件，可以在后台依靠 loop 重连，也可以直接 begin
        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(out_ssid.c_str(), out_pwd.c_str());
        
        return true;
    }
    
    Serial.println("\n>> [WiFi NVS]: 未检测到历史网络记录，请通过串口配网。");
    return false;
}

bool wifi_drv_is_connected(void) {
    return (WiFi.status() == WL_CONNECTED);
}

void wifi_drv_loop(const char* ssid, const char* password) {
    static unsigned long last_wifi_check = 0;
    // 每 5 秒检查一次网络状态，防止频繁网络轮询阻塞系统
    if (millis() - last_wifi_check >= 5000) {
        last_wifi_check = millis();
        if (WiFi.status() != WL_CONNECTED && strlen(ssid) > 0) {
            Serial.println(">> [WiFi]: 网络断开，正在尝试后台重连...");
            WiFi.begin(ssid, password);
        }
    }
}