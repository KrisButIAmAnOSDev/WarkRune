#pragma once
#include <stdint.h>

#define MAX_TASKS     4
#define QUANTUM_TICKS 20

enum TaskState : uint8_t {
    TASK_DEAD    = 0,
    TASK_READY   = 1,
    TASK_RUNNING = 2,
};

struct Task {
    uint32_t  esp;
    uint32_t  esp0;
    TaskState state;
    uint32_t  id;
    uint32_t  entry;
    uint32_t  user_esp;
};

void     sched_init();
int      sched_add_task(uint32_t entry, uint32_t user_esp, uint32_t* esp0_out);
void     sched_kill_current();
void     sched_kill_task(uint32_t id);
void     sched_yield();
Task*    sched_current_task();
uint32_t sched_task_count();
void     sched_test();
extern "C" uint32_t sched_tick(uint32_t cur_esp);