#include "scheduler.h"

typedef struct {
    sched_task_fn_t fn;
    uint32_t interval_ms;
    uint32_t last_run;
    bool    wake_flag;
    bool    active;
} sched_task_t;

static sched_task_t _tasks[SCHEDULER_MAX_TASKS];
static int _task_count = 0;

void sched_init(void) {
    _task_count = 0;
}

int sched_add(sched_task_fn_t fn, uint32_t interval_ms) {
    if (_task_count >= SCHEDULER_MAX_TASKS || fn == NULL) return -1;
    _tasks[_task_count].fn          = fn;
    _tasks[_task_count].interval_ms = interval_ms;
    _tasks[_task_count].last_run    = 0;
    _tasks[_task_count].wake_flag   = false;
    _tasks[_task_count].active      = true;
    return _task_count++;
}

void sched_wake(int task_id) {
    if (task_id >= 0 && task_id < _task_count) {
        _tasks[task_id].wake_flag = true;
    }
}

void sched_run(void) {
    unsigned long now = millis();
    for (int i = 0; i < _task_count; i++) {
        if (!_tasks[i].active) continue;

        bool due = _tasks[i].wake_flag;
        due = due || (now - _tasks[i].last_run >= _tasks[i].interval_ms);

        if (due) {
            _tasks[i].last_run  = now;
            _tasks[i].wake_flag = false;
            _tasks[i].fn();
        }
    }
}
