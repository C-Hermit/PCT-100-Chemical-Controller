#include "wifi_drv.h"
#include <WiFi.h>
#include <Preferences.h>

// ==================== 🛠️ 异步配网状态枚举 ====================
typedef enum {
    WIFI_STATE_IDLE = 0,
    WIFI_STATE_SCANNING,
    WIFI_STATE_WAIT_CHOICE,
    WIFI_STATE_WAIT_PWD
} wifi_interact_state_t;

// ==================== 驱动内部私有静态状态变量 ====================
static Preferences prefs;
static String   _drv_ssid = "";
static String   _drv_pwd  = "";
static bool     _drv_configured = false;
static volatile bool _drv_status_changed = false;

// 异步状态机核心变量
static wifi_interact_state_t _interact_state = WIFI_STATE_IDLE;
static uint32_t _state_timeout_ms = 5000; // 默认阶段超时
static uint32_t _state_start_time = 0;
static int      _scanned_count = 0;

static String   _tmp_ssid = "";
static String   _tmp_pwd  = "";

// ==================== 内部私有功能函数声明 ====================
static void wifi_drv_save_credentials(const char* ssid, const char* password);
static void wifi_drv_start_async_scan(void);
static void wifi_drv_init_connect(const char* ssid, const char* password);
static void wifi_drv_handle_interact_fsm(void);

static void wifi_drv_save_credentials(const char* ssid, const char* password) {
    prefs.begin("wifi_space", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pwd", password);
    prefs.end();
    Serial.println(">> [WiFi NVS]: 新的 WiFi 凭据已固化到闪存中。");
}

/**
 * @brief 开启异步 WiFi 扫描（完全不阻塞）
 */
static void wifi_drv_start_async_scan(void) {
    Serial.println("\n>> [WiFi]: 正在后台异步扫描附近无线网络...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    // 第一个参数位 true 表示【异步扫描】，调用后立刻返回，不阻塞
    WiFi.scanNetworks(true, false, false); 
    _interact_state = WIFI_STATE_SCANNING;
    _state_start_time = millis();
}

static void wifi_drv_init_connect(const char* ssid, const char* password) {
    if (ssid == NULL || strlen(ssid) == 0) return;
    
    Serial.printf("\n>> [WiFi]: 正在连接网络 -> %s\n", ssid);
    WiFi.disconnect(true);
    delay(50);
    WiFi.begin(ssid, password);
    
    // 此处由于在 init 或特定的断线状态触发，允许有限阻塞 3 秒快速尝试，或直接交由后台
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 6) { 
        delay(500);
        Serial.print(".");
        timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n>> [WiFi]: 连接成功！IP: %s\n", WiFi.localIP().toString().c_str());
        wifi_drv_save_credentials(ssid, password);
        _drv_configured = true;
        _drv_status_changed = true; 
    } else {
        Serial.println("\n>> [WiFi]: 首次连接未果，交由底层后台持续自动重连...");
        _drv_configured = false;
    }
}

/**
 * @brief 🌟 核心：串口交互异步状态机（每次执行耗时接近 0 ms）
 */
static void wifi_drv_handle_interact_fsm(void) {
    switch (_interact_state) {
        
        case WIFI_STATE_IDLE:
            break;

        case WIFI_STATE_SCANNING: {
            // 检查异步扫描是否完成
            int16_t scan_result = WiFi.scanComplete();
            if (scan_result == WIFI_SCAN_RUNNING) {
                // 还在扫，直接退出，等下一轮 loop 检查
                return; 
            } else if (scan_result == WIFI_SCAN_FAILED || scan_result <= 0) {
                Serial.println(">> [WiFi]: 异步扫描未找到网络，退出。");
                WiFi.scanDelete();
                _interact_state = WIFI_STATE_IDLE;
            } else {
                // 扫描成功，打印列表
                _scanned_count = scan_result;
                Serial.println(">> [WiFi]: 扫描完成！");
                Serial.println("----------------------------------------------");
                Serial.printf("%-5s | %-20s | %-4s | %s\n", "编号", "WiFi 名称 (SSID)", "信号", "加密");
                Serial.println("----------------------------------------------");
                for (int i = 0; i < _scanned_count; ++i) {
                    Serial.printf("%-5d | %-20.20s | %-4d | %s\n", 
                                  i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), 
                                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "公开" : "加密");
                }
                Serial.println("----------------------------------------------");
                Serial.printf(">> 请输入连接的编号（限时 %d 秒，无人输入自动跳过）：\n", _state_timeout_ms / 1000);
                
                while (Serial.available() > 0) Serial.read(); // 读清缓存
                _interact_state = WIFI_STATE_WAIT_CHOICE;
                _state_start_time = millis(); // 刷新阶段计时器
            }
            break;
        }

        case WIFI_STATE_WAIT_CHOICE: {
            // 检查是否超时
            if (millis() - _state_start_time >= _state_timeout_ms) {
                Serial.println("\n>> [WiFi]: 输入编号超时，已自动跳过配网。");
                WiFi.scanDelete();
                _interact_state = WIFI_STATE_IDLE;
                return;
            }

            // 非阻塞检查是否有输入
            if (Serial.available() > 0) {
                String choice_str = Serial.readStringUntil('\n');
                choice_str.trim();
                int choice = choice_str.toInt() - 1;

                if (choice >= 0 && choice < _scanned_count) {
                    _tmp_ssid = WiFi.SSID(choice);
                    Serial.printf(">> 已选择网络: %s\n", _tmp_ssid.c_str());
                    Serial.printf(">> 请输入该网络的密码（限时 %d 秒）：\n", _state_timeout_ms / 1000);
                    
                    _interact_state = WIFI_STATE_WAIT_PWD;
                    _state_start_time = millis(); // 重新倒计时密码输入
                } else {
                    Serial.printf(">> [输入错误]: 编号不合法。重新拉起扫描...\n");
                    WiFi.scanDelete();
                    wifi_drv_start_async_scan();
                }
            }
            break;
        }

        case WIFI_STATE_WAIT_PWD: {
            if (millis() - _state_start_time >= _state_timeout_ms) {
                Serial.println("\n>> [WiFi]: 输入密码超时，已自动跳过配网。");
                WiFi.scanDelete();
                _interact_state = WIFI_STATE_IDLE;
                return;
            }

            if (Serial.available() > 0) {
                _tmp_pwd = Serial.readStringUntil('\n');
                _tmp_pwd.trim();
                
                WiFi.scanDelete(); // 释放内存
                
                _drv_ssid = _tmp_ssid;
                _drv_pwd  = _tmp_pwd;
                
                _interact_state = WIFI_STATE_IDLE;
                
                // 触发连接
                wifi_drv_init_connect(_drv_ssid.c_str(), _drv_pwd.c_str());
            }
            break;
        }
    }
}

static bool wifi_drv_auto_connect(void) {
    prefs.begin("wifi_space", true);
    _drv_ssid = prefs.getString("ssid", "");
    _drv_pwd  = prefs.getString("pwd", "");
    prefs.end();

    if (_drv_ssid.length() > 0) {
        Serial.println("\n>> [WiFi NVS]: 发现历史网络记录，正在尝试自动回连...");
        wifi_drv_init_connect(_drv_ssid.c_str(), _drv_pwd.c_str());
        return _drv_configured;
    }
    return false;
}

// ==================== 🔓 对外公开接口实现 ====================

void wifi_drv_init(void) {
    if (wifi_drv_auto_connect()) {
        return;
    }
    Serial.println(">> [WiFi]: 自动联网失败，拉起异步串口配网，不阻塞主时序。");
    _state_timeout_ms = 5000; // 设定交互配网中各阶段超时为 5 秒
    wifi_drv_start_async_scan(); 
}

void wifi_drv_loop(void) {
    // 🌟 核心：不论何时，高频刷新串口配网状态机（无输入时耗时为 0 毫秒）
    wifi_drv_handle_interact_fsm();

    static unsigned long last_wifi_check = 0;
    static uint8_t disconnect_counter = 0;

    if (millis() < 2000) return;

    if (millis() - last_wifi_check >= 5000) {
        last_wifi_check = millis();
        
        if (WiFi.status() == WL_CONNECTED) {
            disconnect_counter = 0;
            return;
        }

        // 只有当前没有在进行串口交互配网时，才进行断线累计判定
        if (_drv_ssid.length() > 0 && _interact_state == WIFI_STATE_IDLE) {
            disconnect_counter++;
            Serial.printf(">> [WiFi]: 断开！检测到未联网状态持续累计 %d 次...\n", disconnect_counter);
            
            if (disconnect_counter >= 3) {
                Serial.println("\n>> [WiFi]: 累计 15 秒未连通，触发异步换网流...");
                disconnect_counter = 0; 
                WiFi.disconnect(false);
                
                _state_timeout_ms = 5000; // 运行时断开只给 5 秒看控制台的机会
                wifi_drv_start_async_scan(); // 优雅拉起异步扫描，loop 不受任何阻碍
            } else {
                Serial.println(">> [WiFi]: 底层正在后台自动重连中，静默等待...");
            }
        }
    }
}

bool wifi_drv_is_connected(void) {
    return (WiFi.status() == WL_CONNECTED);
}

String wifi_drv_get_local_ip(void) {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

bool wifi_drv_check_renotify(void) {
    if (_drv_status_changed) {
        _drv_status_changed = false; 
        return true;
    }
    return false;
}