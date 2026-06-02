#include "wifi_drv.h"
#include <WiFi.h>
#include <Preferences.h>

// ==================== 驱动内部私有静态状态 ====================
static Preferences prefs;
static String   _drv_ssid = "";
static String   _drv_pwd  = "";
static volatile bool _drv_status_changed = false;

// 非阻塞连接跟踪
static bool     _connecting = false;
static uint32_t _connect_start = 0;
// 连接成功时是否将 _tmp_ssid/_tmp_pwd 写入 NVS
static bool     _save_creds = false;
static String   _tmp_ssid = "";
static String   _tmp_pwd  = "";

// 状态变化检测（断线重连后通知外部）
static bool     _was_connected = false;

// ==================== 内部函数声明 ====================
static void wifi_drv_save_credentials(const char* ssid, const char* password);
static void wifi_drv_begin_connect(const char* ssid, const char* password);
static void wifi_drv_poll_connecting(void);

// ==================== 内部函数实现 ====================

static void wifi_drv_save_credentials(const char* ssid, const char* password) {
    prefs.begin("wifi_space", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pwd", password);
    prefs.end();
    _drv_ssid = ssid;
    _drv_pwd  = password;
    Serial.println(">> [WiFi NVS]: 凭据已保存到闪存。");
}

static void wifi_drv_begin_connect(const char* ssid, const char* password) {
    if (ssid == NULL || strlen(ssid) == 0) return;

    Serial.printf("\n>> [WiFi]: 连接 %s ...\n", ssid);
    WiFi.disconnect(true);
    delay(50);
    WiFi.begin(ssid, password);

    _connecting = true;
    _connect_start = millis();
}

static void wifi_drv_poll_connecting(void) {
    if (!_connecting) return;

    if (WiFi.status() == WL_CONNECTED) {
        _connecting = false;

        if (_save_creds) {
            wifi_drv_save_credentials(_tmp_ssid.c_str(), _tmp_pwd.c_str());
            _save_creds = false;
        }

        Serial.printf(">> [WiFi]: 连接成功！IP: %s\n", WiFi.localIP().toString().c_str());
        _drv_status_changed = true;
        return;
    }

    // 15 秒超时
    if (millis() - _connect_start >= 15000) {
        _connecting = false;
        Serial.println(">> [WiFi]: 连接超时(15s)。使用 'wifi scan' 重新扫描。");
    }
}

// ==================== 对外接口 ====================

void wifi_drv_init(void) {
    prefs.begin("wifi_space", true);
    _drv_ssid = prefs.getString("ssid", "");
    _drv_pwd  = prefs.getString("pwd", "");
    prefs.end();

    if (_drv_ssid.length() > 0) {
        Serial.println(">> [WiFi NVS]: 发现历史凭据，异步回连中...");
        wifi_drv_begin_connect(_drv_ssid.c_str(), _drv_pwd.c_str());
    } else {
        Serial.println(">> [WiFi]: 无凭据。输入 'wifi scan' 扫描网络。");
    }
}

void wifi_drv_loop(void) {
    // 非阻塞等待连接完成
    wifi_drv_poll_connecting();

    // 跳过启动初期
    if (millis() < 2000) return;

    static unsigned long last_check = 0;
    static uint8_t disconnect_count = 0;

    if (millis() - last_check < 5000) return;
    last_check = millis();

    bool now_connected = (WiFi.status() == WL_CONNECTED);

    // 状态从断连→连接：通知 OLED 等外部模块
    if (now_connected && !_was_connected) {
        _drv_status_changed = true;
        disconnect_count = 0;
    }
    _was_connected = now_connected;

    if (now_connected) return;

    // ---- 断线恢复 ----
    disconnect_count++;
    if (disconnect_count >= 3) {
        disconnect_count = 0;
        Serial.println(">> [WiFi]: 断线 15s，尝试恢复...");

        if (_drv_ssid.length() > 0 && !_connecting) {
            wifi_drv_begin_connect(_drv_ssid.c_str(), _drv_pwd.c_str());
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

// ==================== 串口命令调用接口 ====================

void wifi_drv_scan_start(void) {
    // 清除上次扫描结果
    int prev = WiFi.scanComplete();
    if (prev > 0) WiFi.scanDelete();

    _connecting = false;  // 取消待完成的连接
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true, false, false);
    Serial.println(">> [WiFi]: 扫描中...（使用 'wifi list' 查看结果）");
}

int wifi_drv_scan_fetch(wifi_scan_entry_t* buf, int max) {
    int n = WiFi.scanComplete();
    if (n <= 0) return 0;
    if (n > max) n = max;

    // 按 RSSI 降序排列的索引数组
    int idx[n];
    for (int i = 0; i < n; i++) idx[i] = i;

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (WiFi.RSSI(idx[j]) < WiFi.RSSI(idx[j + 1])) {
                int t = idx[j]; idx[j] = idx[j + 1]; idx[j + 1] = t;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        snprintf(buf[i].ssid, sizeof(buf[i].ssid), "%s", WiFi.SSID(idx[i]).c_str());
        buf[i].rssi = WiFi.RSSI(idx[i]);
        buf[i].encrypted = (WiFi.encryptionType(idx[i]) != WIFI_AUTH_OPEN);
    }

    return n;  // 不删结果，下次 scan_start 或 clear 时自动清理
}

void wifi_drv_connect(const char* ssid, const char* password) {
    if (ssid == NULL || strlen(ssid) == 0) return;

    _tmp_ssid = ssid;
    _tmp_pwd  = password ? password : "";
    _save_creds = true;

    wifi_drv_begin_connect(ssid, _tmp_pwd.c_str());
}

void wifi_drv_clear_credentials(void) {
    prefs.begin("wifi_space", false);
    prefs.remove("ssid");
    prefs.remove("pwd");
    prefs.end();
    _drv_ssid = "";
    _drv_pwd  = "";
    _connecting = false;
    WiFi.disconnect(true);
    WiFi.scanDelete();
    Serial.println(">> [WiFi NVS]: 凭据已清除。");
}
