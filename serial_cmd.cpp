#include "serial_cmd.h"
#include "wifi_drv.h"
#include "relay.h"
#include <WiFi.h>

// ==================== 外部状态引用 ====================
// 与 PCT_100.ino 共享业务状态
extern struct SystemState {
    bool power_on;
    bool is_auto;
    uint8_t manual_code;
    float current_temp;
    unsigned int current_lux;
    float temp_th;
    unsigned int light_th;
    bool wifi_connected;
} sys;

extern volatile bool oled_need_refresh;
extern void report_device_status(void);

// ==================== 内部函数声明 ====================
static void cmd_wifi_scan(void);
static void cmd_wifi_list(void);
static void cmd_wifi_conn(const String& args);
static void cmd_wifi_status(void);
static void cmd_wifi_forget(void);
static void cmd_set(const String& args);
static void cmd_get_status(void);
static void print_help(void);

// ==================== 入口：主 loop 轮询 ====================

void serial_cmd_loop(void) {
    if (Serial.available() <= 0) return;

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    // 按首单词派发
    if (input.startsWith("wifi ")) {
        String sub = input.substring(5);
        if (sub == "scan")            cmd_wifi_scan();
        else if (sub == "list")       cmd_wifi_list();
        else if (sub.startsWith("conn"))  cmd_wifi_conn(sub.substring(5));
        else if (sub == "status")     cmd_wifi_status();
        else if (sub == "forget")     cmd_wifi_forget();
        else Serial.println(">> wifi 命令: scan, list, conn <n> [pwd], status, forget");
    }
    else if (input.startsWith("set ")) {
        cmd_set(input.substring(4));
    }
    else if (input.startsWith("get ")) {
        if (input.substring(4) == "status") cmd_get_status();
        else Serial.println(">> get 命令: get status");
    }
    else if (input == "help" || input == "?") {
        print_help();
    }
    else {
        Serial.println(">> 未知命令，输入 help 查看可用命令。");
    }
}

// ==================== WiFi 命令实现 ====================

static void cmd_wifi_scan(void) {
    wifi_drv_scan_start();
}

static void cmd_wifi_list(void) {
    wifi_scan_entry_t results[WIFI_SCAN_MAX];
    int n = wifi_drv_scan_fetch(results, WIFI_SCAN_MAX);

    if (n <= 0) {
        Serial.println(">> 无扫描结果。请先执行 'wifi scan' (扫描中请稍候再试)。");
        return;
    }

    Serial.println("\n可用 WiFi 网络（按信号强度排序）：");
    Serial.println("----------------------------------------------");
    Serial.printf("%-5s | %-24s | %-4s | %s\n", "编号", "SSID", "信号", "加密");
    Serial.println("----------------------------------------------");
    for (int i = 0; i < n; i++) {
        Serial.printf("%-5d | %-24.24s | %-4d | %s\n",
                      i + 1,
                      results[i].ssid,
                      results[i].rssi,
                      results[i].encrypted ? "是" : "否");
    }
    Serial.println("----------------------------------------------");
    Serial.println("使用 'wifi conn <编号> <密码>' 连接。");
}

static void cmd_wifi_conn(const String& args) {
    args.trim();
    if (args.length() == 0) {
        Serial.println(">> 用法: wifi conn <编号> [密码]");
        return;
    }

    // 第一个空格前是编号，之后是密码（可选）
    int sp = args.indexOf(' ');
    String idx_str = (sp >= 0) ? args.substring(0, sp) : args;
    String pwd     = (sp >= 0) ? args.substring(sp + 1) : "";
    pwd.trim();

    int idx = idx_str.toInt() - 1;  // 1-based → 0-based
    if (idx < 0) {
        Serial.println(">> 编号无效。请先执行 'wifi list' 查看可用网络。");
        return;
    }

    wifi_scan_entry_t results[WIFI_SCAN_MAX];
    int n = wifi_drv_scan_fetch(results, WIFI_SCAN_MAX);
    if (idx >= n) {
        Serial.println(">> 编号超出范围。请先执行 'wifi scan' 刷新列表。");
        return;
    }

    Serial.printf(">> 正在连接 %s ...\n", results[idx].ssid);
    wifi_drv_connect(results[idx].ssid, pwd.c_str());
}

static void cmd_wifi_status(void) {
    bool connected = wifi_drv_is_connected();
    Serial.println("\n============ [WiFi 状态] ============");
    Serial.printf("   状态: %s\n", connected ? "已连接" : "未连接");
    if (connected) {
        Serial.printf("   SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("   IP:   %s\n", wifi_drv_get_local_ip().c_str());
        Serial.printf("   信号: %d dBm\n", WiFi.RSSI());
    }
    Serial.println("======================================");
}

static void cmd_wifi_forget(void) {
    wifi_drv_clear_credentials();
}

// ==================== 参数设置命令 ====================

static void cmd_set(const String& args) {
    args.trim();
    int eq = args.indexOf('=');
    if (eq < 0) {
        Serial.println(">> 格式: set <key>=<value> (如 set temp=30.5)");
        return;
    }

    String key = args.substring(0, eq);
    String val = args.substring(eq + 1);
    key.trim();
    val.trim();
    if (key.length() == 0 || val.length() == 0) {
        Serial.println(">> 键或值为空。");
        return;
    }

    bool changed = false;

    if (key == "temp") {
        sys.temp_th = val.toFloat();
        Serial.printf(">> 温度阈值 -> %.1f °C\n", sys.temp_th);
        changed = true;
    }
    else if (key == "light") {
        sys.light_th = val.toInt();
        Serial.printf(">> 光照阈值 -> %u Lux\n", sys.light_th);
        changed = true;
    }
    else if (key == "mode") {
        if (val == "auto") {
            sys.is_auto = true;
            Serial.println(">> 模式 -> 自动");
            changed = true;
        } else if (val == "manual") {
            sys.is_auto = false;
            Serial.println(">> 模式 -> 手动");
            changed = true;
        } else {
            Serial.println(">> 模式值无效 (auto/manual)");
        }
    }
    else {
        Serial.printf(">> 未知参数: %s (可用: temp, light, mode)\n", key.c_str());
    }

    if (changed) {
        oled_need_refresh = true;
        report_device_status();
    }
}

// ==================== 状态查询 ====================

static void cmd_get_status(void) {
    Serial.println("\n============ [系统参数] ============");
    Serial.printf("  总闸:      %s\n", sys.power_on ? "ON" : "OFF");
    Serial.printf("  模式:      %s\n", sys.is_auto ? "自动" : "手动");
    Serial.printf("  手动编码:  %d%d\n",
                  (sys.manual_code & 0x02) ? 1 : 0,
                  (sys.manual_code & 0x01) ? 1 : 0);
    Serial.printf("  温度:      %.1f / %.1f °C\n", sys.current_temp, sys.temp_th);
    Serial.printf("  光照:      %u / %u Lux\n", sys.current_lux, sys.light_th);
    Serial.printf("  灯光:      %s\n", (get_led_status() == RELAY_ON) ? "ON" : "OFF");
    Serial.printf("  风扇:      %s\n", (get_fun_status() == RELAY_ON) ? "ON" : "OFF");
    Serial.println("====================================");
}

static void print_help(void) {
    Serial.println("\n可用命令：");
    Serial.println("  wifi scan             扫描 WiFi 网络");
    Serial.println("  wifi list             显示扫描结果（按信号排序）");
    Serial.println("  wifi conn <n> [pwd]   连接第 n 个网络");
    Serial.println("  wifi status           显示 WiFi 连接状态");
    Serial.println("  wifi forget           清除保存的 WiFi 凭据");
    Serial.println("  set temp=<value>      设置温度阈值");
    Serial.println("  set light=<value>     设置光照阈值");
    Serial.println("  set mode=auto|manual  切换自动/手动模式");
    Serial.println("  get status            显示设备参数");
    Serial.println("  help                  显示本帮助");
}
