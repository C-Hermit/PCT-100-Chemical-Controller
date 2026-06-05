#ifndef _SERIAL_CMD_H
#define _SERIAL_CMD_H

#include <Arduino.h>

// 串口命令轮询：挂在主 loop() 中，解析用户输入并执行
void serial_cmd_loop(void);

#endif
