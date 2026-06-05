#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <Arduino.h>

#define SCHEDULER_MAX_TASKS 8

typedef void (*sched_task_fn_t)(void);

// 初始化调度器
void sched_init(void);

// 注册定时任务：fn = 函数指针, interval_ms = 周期(ms), stack_size = FreeRTOS 任务栈大小(字节)。
// 返回任务 ID（用于 sched_wake），-1 表示注册失败（超过最大数量）。
int sched_add(sched_task_fn_t fn, uint32_t interval_ms, uint32_t stack_size);

// 唤醒指定任务，使其在下一个 sched_run() 周期立即执行（忽略间隔时间）
void sched_wake(int task_id);

// 在主 loop() 中调用，检查所有任务是否到期并执行
void sched_run(void);

#endif
