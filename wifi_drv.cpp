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
 * @brief 🛠️ 独立出来的 WiFi 扫描与列表打印函数
 * @return int 扫描到的有效网络数量
 */
int wifi_drv_scan_and_print(void) {
    Serial.println("\n>> [WiFi]: 正在扫描附近无线网络...");
    
    // 每次扫描前确保处于 STA 模式
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(200);

    // 阻塞式主动扫描
    int n = WiFi.scanNetworks(false, false, false);
    Serial.println(">> [WiFi]: 扫描完成！");
    
    if (n <= 0) {
        Serial.println(">> [错误]: 未找到任何公开的 WiFi 网络！");
        return 0;
    }

    // 打印 WiFi 列表
    Serial.println("----------------------------------------------");
    Serial.printf("%-5s | %-20s | %-4s | %s\n", "编号", "WiFi 名称 (SSID)", "信号", "加密");
    Serial.println("----------------------------------------------");
    for (int i = 0; i < n; ++i) {
        Serial.printf("%-5d | %-20.20s | %-4d | %s\n", 
                      i + 1, 
                      WiFi.SSID(i).c_str(), 
                      WiFi.RSSI(i), 
                      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "公开" : "加密");
        delay(10); // 稍微延时防止串口发送缓存溢出
    }
    Serial.println("----------------------------------------------");
    
    return n;
}

/**
 * @brief 上电或运行时降级拉起的串口交互扫描配网（输错完全重新扫描）
 */
void wifi_drv_interactive_config(String &out_ssid, String &out_pwd) {
    String selected_ssid = "";
    int network_count = 0;

    // 外层大循环：只要没有成功选对网络，就一直在这个闭环里重新扫描和输入
    while (true) {
        // 1. 调用独立的扫描打印功能
        network_count = wifi_drv_scan_and_print();
        
        if (network_count <= 0) {
            Serial.println(">> 3秒后尝试重新扫描...");
            delay(3000);
            continue; // 没扫到网络，直接触发下一次重新扫描
        }

        // 2. 引导用户选择编号
        Serial.println(">> 请输入您要连接的 WiFi 【编号】，然后按下回车：");
        
        while (Serial.available() > 0) Serial.read(); // 清空串口历史残留
        while (!Serial.available()) { delay(100); }   // 阻塞等待用户敲击
        
        String choice_str = Serial.readStringUntil('\n');
        choice_str.trim();
        int choice = choice_str.toInt() - 1;

        // 3. 校验编号合法性
        if (choice >= 0 && choice < network_count) {
            selected_ssid = WiFi.SSID(choice);
            Serial.printf(">> 已选择网络: %s\n", selected_ssid.c_str());
            break; // 编号完全合法，跳出重新扫描的大循环，进入下一步输入密码！
        }
        
        // 4. 输错了，触发重新扫描
        Serial.printf(">> [输入错误]: 编号 '%s' 超出范围！将重新扫描并刷新网络列表...\n", choice_str.c_str());
        WiFi.scanDelete(); // 释放上一次扫描占用的堆内存
        delay(1000);       // 留出 1 秒给用户看清错误提示
    }

    // 核心优化：走到这里，说明 SSID 已经安全拿到，接下来处理密码输入
    String selected_pwd = "";
    while (true) {
        Serial.println(">> 请输入该 WiFi 的【密码】，然后按下回车（若无密码直接回车）：");
        
        while (Serial.available() > 0) Serial.read(); 
        while (!Serial.available()) { delay(100); }   
        
        selected_pwd = Serial.readStringUntil('\n');
        selected_pwd.trim();
        
        if (selected_pwd.length() == 0) {
            Serial.println(">> [提示]: 您未输入密码，确认连公开网络请按回车，重新输入请输入 'R'：");
            while (Serial.available() > 0) Serial.read();
            while (!Serial.available()) { delay(100); }
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            confirm.toUpperCase();
            if (confirm == "R") continue; 
        }
        break; 
    }

    // 5. 将临时变量刷给传出引用
    out_ssid = selected_ssid;
    out_pwd = selected_pwd;
    
    // 彻底释放 WiFi 扫描占用的内存
    WiFi.scanDelete(); 

    // 调用初始化函数连接网络并自动固化到 NVS
    wifi_drv_init(out_ssid.c_str(), out_pwd.c_str());
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

        WiFi.disconnect(true);
        delay(100);
        WiFi.begin(out_ssid.c_str(), out_pwd.c_str());
        
        return true;
    }else {
        Serial.println("\n>> [WiFi NVS]: 历史网络连接超时（可能热点未开启或更换了环境）。");
        return false;
    }
    
    Serial.println("\n>> [WiFi NVS]: 未检测到历史网络记录，请通过串口配网。");
    return false;
}

bool wifi_drv_is_connected(void) {
    return (WiFi.status() == WL_CONNECTED);
}

void wifi_drv_loop(String &ssid, String &password, bool &configured) {
    static unsigned long last_wifi_check = 0;
    static uint8_t disconnect_counter = 0;
    if (millis() < 2000) {
        return; 
    }
    if (millis() - last_wifi_check >= 5000) {
        last_wifi_check = millis();
        
        if (WiFi.status() == WL_CONNECTED) {
            disconnect_counter = 0;
            return;
        }

        if (ssid.length() > 0) {
            disconnect_counter++;
            Serial.printf(">> [WiFi]: 断开！检测到未联网状态持续累计 %d 次...\n", disconnect_counter);
            
            if (disconnect_counter >= 3) {
                Serial.println("\n>> [WiFi]: 累计 15 秒未连通，强行拉起串口扫描配网...");
                disconnect_counter = 0; 
                
                WiFi.disconnect(false); // false 代表只断开当前连接，不销毁存储的 STA 配置
                delay(200); 
                
                // 直接利用传进来的引用进行修改
                wifi_drv_interactive_config(ssid, password); 
                
                if (WiFi.status() == WL_CONNECTED) {
                    configured = true;
                } else {
                    configured = false;
                }
            } 
            else {
                Serial.println(">> [WiFi]: 底层正在后台自动重连中，静默等待...");
            }
        }
    }
}