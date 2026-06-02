#ifndef _WIFI_DRV_H
#define _WIFI_DRV_H

#include <Arduino.h>

/**
 * @brief 异步/同步启动 WiFi 连接
 * @param ssid WiFi名称
 * @param password WiFi密码
 */
void wifi_drv_init(const char* ssid, const char* password);

/**
 * @brief 获取当前 WiFi 连接状态
 * @return true: 已连接, false: 断开
 */
bool wifi_drv_is_connected(void);

/**
 * @brief WiFi 状态维护句柄（可放置于主 loop，用于断线自动重连）
 */
void wifi_drv_loop(const char* ssid, const char* password);
//开机自动回连接口，使用 String 引用向外同步历史数据
bool wifi_drv_auto_connect(String &out_ssid, String &out_pwd);

#endif