#ifndef _WIFI_DRV_H
#define _WIFI_DRV_H

#include <Arduino.h>

// ==================== 扫描结果结构体 ====================
#define WIFI_SCAN_MAX 20

typedef struct {
    char ssid[33];      // WiFi 名称 (最大 32 字符 + null)
    int8_t rssi;        // 信号强度 (dBm)
    bool encrypted;     // 是否加密
} wifi_scan_entry_t;

// ==================== 基础接口 ====================

// 初始化：从 NVS 读取凭据并异步回连（不阻塞）
void wifi_drv_init(void);

// 核心非阻塞轮询：连接跟踪、断线检测、自动重连
void wifi_drv_loop(void);

// 查询当前 WiFi 是否已连通
bool wifi_drv_is_connected(void);

// 获取当前本地 IP 地址（"0.0.0.0" 表示未连接）
String wifi_drv_get_local_ip(void);

// 检查 WiFi 状态是否发生过改变，用于通知 OLED 刷新
bool wifi_drv_check_renotify(void);

// ==================== 串口命令调用接口 ====================

// 异步扫描 WiFi（非阻塞）
void wifi_drv_scan_start(void);

// 获取扫描结果（按信号强度降序排列）。
// 返回网络数量，0 = 未就绪/失败。
// 结果在内存中保持有效，直到下次调用 scan_start() 或 clear_credentials()。
int wifi_drv_scan_fetch(wifi_scan_entry_t* buf, int max);

// 连接指定网络并保存凭据到 NVS（非阻塞）
void wifi_drv_connect(const char* ssid, const char* password);

// 清除 NVS 中保存的 WiFi 凭据
void wifi_drv_clear_credentials(void);

#endif