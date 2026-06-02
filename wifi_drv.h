#ifndef _WIFI_DRV_H
#define _WIFI_DRV_H

#include <Arduino.h>

// 初始化网络状态：尝试自动回连，失败则直接拉起串口配网
void wifi_drv_init(void);

// 核心非阻塞轮询状态机：挂载在主循环中，负责断线计数、重连、断网换网
void wifi_drv_loop(void);

// 查询当前 WiFi 硬件是否真正处于连通状态
bool wifi_drv_is_connected(void);

// 获取当前连接成功的本地 IP 地址（方便 OLED 等外设获取显示）
String wifi_drv_get_local_ip(void);

// 检查 WiFi 状态是否发生过改变（例如刚才断网换网成功了），用于通知外层刷新 OLEDUI
bool wifi_drv_check_renotify(void);

#endif