#include "scheduler.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define SCHED_PRIO_MAX  SCHED_PRIO_BACKGROUND

typedef struct {
    sched_task_fn_t fn;
    uint32_t interval_ms;
    TaskHandle_t handle;        // FreeRTOS 任务句柄
    uint32_t stack_size;
    uint8_t priority;
    bool    active;
} sched_task_t;

static sched_task_t _tasks[SCHEDULER_MAX_TASKS];
static int _task_count = 0;
static int _name_counter = 0;

// 调度器优先级 → FreeRTOS 任务优先级（数值越大优先级越高）
static UBaseType_t _map_priority(uint8_t prio) {
    (void)prio;
    return 1;   // 统一使用默认优先级
}

// ─── FreeRTOS 任务入口 ──────────────────────────────────────
//
// 使用 ulTaskNotifyTake() 实现周期 + 事件驱动双模调度：
//   - 超时到达 → 函数按周期执行
//   - sched_wake() 通知到达 → 函数立即执行（忽略剩余间隔）
//
static void _task_entry(void *pv) {
    int id = (int)(intptr_t)pv;
    if (id < 0 || id >= SCHEDULER_MAX_TASKS || !_tasks[id].active) {
        vTaskDelete(NULL);
    }

    const TickType_t ticks = pdMS_TO_TICKS(_tasks[id].interval_ms);
    while (1) {
        ulTaskNotifyTake(pdTRUE, ticks);
        if (_tasks[id].active && _tasks[id].fn) {
            _tasks[id].fn();
        }
    }
}

// ─── 公共 API ──────────────────────────────────────────────

void sched_init(void) {
    for (int i = 0; i < SCHEDULER_MAX_TASKS; i++) {
        _tasks[i].fn      = NULL;
        _tasks[i].handle  = NULL;
        _tasks[i].active  = false;
    }
    _task_count = 0;
    _name_counter = 0;
}

int sched_add(sched_task_fn_t fn, uint32_t interval_ms, uint32_t stack_size) {
    if (!fn)                          return -1;
    if (_task_count >= SCHEDULER_MAX_TASKS) return -1;

    int id = _task_count;
    _tasks[id].fn          = fn;
    _tasks[id].interval_ms = interval_ms;
    _tasks[id].stack_size  = stack_size;
    _tasks[id].priority    = 1;
    _tasks[id].active      = true;

    // 生成唯一任务名（FreeRTOS 内部会拷贝，局部 buffer 安全）
    char name[configMAX_TASK_NAME_LEN];
    snprintf(name, sizeof(name), "s%d", _name_counter++);

    BaseType_t ret = xTaskCreate(
        _task_entry,                       // 任务函数
        name,                              // 任务名（调试用）
        stack_size,                        // 栈大小（字节）
        (void*)(intptr_t)id,               // 参数：任务 ID
        _map_priority(_tasks[id].priority),// FreeRTOS 优先级
        &_tasks[id].handle                 // 输出：任务句柄
    );

    if (ret != pdPASS) {
        _tasks[id].active = false;
        _tasks[id].handle = NULL;
        return -1;
    }

    _task_count++;
    return id;
}

void sched_wake(int task_id) {
    if (task_id < 0 || task_id >= _task_count)          return;
    if (!_tasks[task_id].handle)                         return;

    xTaskNotifyGive(_tasks[task_id].handle);
}

void sched_run(void) {
    // FreeRTOS 独立调度各任务，无需主动分发
    // 保留空函数保持 API 兼容（loop() 中仍可调用）
}
