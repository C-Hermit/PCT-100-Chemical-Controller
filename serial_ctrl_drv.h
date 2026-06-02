#ifndef _SERIAL_CTRL_DRV_H
#define _SERIAL_CTRL_DRV_H

#include <Arduino.h>

/**
 * @brief 串口调参/配置核心轮询句柄
 * @note 放置于主 loop() 中，高频非阻塞执行
 */
void serial_ctrl_loop(void);

#endif