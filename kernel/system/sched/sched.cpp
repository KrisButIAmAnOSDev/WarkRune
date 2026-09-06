#include "system/sched/sched.hpp"
#include "system/tss/tss.hpp"
#include "io/serial/serial.hpp"

#define KSTACK_SIZE  0x4000u

static uint8_t kstacks[MAX_TASKS][KSTACK_SIZE] __attribute__((aligned(16)));

static Task   tasks[MAX_TASKS];
static int    task_count   = 0;
static int    cur_task     = 0;
static int    ticks_left   = QUANTUM_TICKS;

void sched_init() {
    tasks[0].esp   = 0;
    tasks[0].esp0  = (uint32_t)kstacks[0] + KSTACK_SIZE;
    tasks[0].state = TASK_RUNNING;
    tasks[0].id    = 0;
    tasks[0].entry  = 0;
    tasks[0].user_esp = 0;
    task_count     = 1;
    cur_task       = 0;
    ticks_left     = QUANTUM_TICKS;
}

int sched_add_task(uint32_t entry, uint32_t user_esp, uint32_t* esp0_out) {
    if (task_count >= MAX_TASKS) return -1;

    int idx = task_count;
    uint32_t* sp = (uint32_t*)((uint32_t)kstacks[idx] + KSTACK_SIZE);

    *--sp = 0x23;
    *--sp = user_esp;
    *--sp = 0x200;
    *--sp = 0x1B;
    *--sp = entry;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    tasks[idx].esp   = (uint32_t)sp;
    tasks[idx].esp0  = (uint32_t)kstacks[idx] + KSTACK_SIZE;
    tasks[idx].state = TASK_READY;
    tasks[idx].id    = (uint32_t)idx;
    tasks[idx].entry = entry;
    tasks[idx].user_esp = user_esp;

    if (esp0_out) *esp0_out = tasks[idx].esp0;
    task_count++;
    return idx;
}

void sched_kill_current() {
    tasks[cur_task].state = TASK_DEAD;
}

void sched_kill_task(uint32_t id) {
    if (id < (uint32_t)task_count)
        tasks[id].state = TASK_DEAD;
}

void sched_yield() {
    /* Nothing explicit needed — the preemptive timer will switch tasks.
       In a cooperative context this is simply a no-op. */
}

Task* sched_current_task() {
    return &tasks[cur_task];
}

uint32_t sched_task_count() {
    return (uint32_t)task_count;
}

void sched_test() {
    asm volatile("cli");
    int pass = 0;
    int fail = 0;

    Task saved[MAX_TASKS];
    int saved_count = task_count;
    int saved_cur = cur_task;
    int saved_ticks = ticks_left;
    for (int i = 0; i < MAX_TASKS; i++) saved[i] = tasks[i];

    task_count = 1;
    cur_task = 0;
    ticks_left = QUANTUM_TICKS;
    tasks[0] = {0xA000u, 0xA100u, TASK_RUNNING, 0, 0, 0};
    for (int i = 0; i < QUANTUM_TICKS - 1; i++) sched_tick(0xA000u);
    if (sched_tick(0xA000u) == 0xA000u) pass++; else fail++;

    task_count = 2;
    cur_task = 0;
    ticks_left = QUANTUM_TICKS;
    tasks[0] = {0xA000u, 0xA100u, TASK_RUNNING, 0, 0, 0};
    tasks[1] = {0xB000u, 0xB100u, TASK_READY, 1, 0, 0};
    for (int i = 0; i < QUANTUM_TICKS - 1; i++) sched_tick(0xA000u);
    if (sched_tick(0xA000u) == 0xB000u && cur_task == 1) pass++; else fail++;

    task_count = 3;
    cur_task = 0;
    ticks_left = QUANTUM_TICKS;
    tasks[0] = {0xA000u, 0xA100u, TASK_RUNNING, 0, 0, 0};
    tasks[1] = {0xB000u, 0xB100u, TASK_DEAD, 1, 0, 0};
    tasks[2] = {0xC000u, 0xC100u, TASK_READY, 2, 0, 0};
    for (int i = 0; i < QUANTUM_TICKS - 1; i++) sched_tick(0xA000u);
    if (sched_tick(0xA000u) == 0xC000u && cur_task == 2) pass++; else fail++;

    task_count = 1;
    cur_task = 0;
    ticks_left = QUANTUM_TICKS;
    tasks[0] = {0xA000u, 0xA100u, TASK_RUNNING, 0, 0, 0};
    int idx = sched_add_task(0xDEAD0000u, 0xBEEF0000u, nullptr);
    if (idx == 1 && tasks[1].state == TASK_READY) pass++; else fail++;

    task_count = 2;
    cur_task = 0;
    ticks_left = QUANTUM_TICKS;
    tasks[0] = {0xA000u, 0xA100u, TASK_RUNNING, 0, 0, 0};
    tasks[1] = {0xB000u, 0xB100u, TASK_READY, 1, 0, 0};
    sched_kill_current();
    if (tasks[0].state == TASK_DEAD) pass++; else fail++;

    task_count = saved_count;
    cur_task = saved_cur;
    ticks_left = saved_ticks;
    for (int i = 0; i < MAX_TASKS; i++) tasks[i] = saved[i];

    serial_puts("[sched_test] ");
    serial_puts(pass == 5 && fail == 0 ? "PASS\n" : "FAIL\n");
    asm volatile("sti");
}

extern "C" uint32_t sched_tick(uint32_t cur_esp) {
    tasks[cur_task].esp = cur_esp;

    if (--ticks_left > 0)
        return cur_esp;

    ticks_left = QUANTUM_TICKS;

    int next = cur_task;
    for (int i = 1; i <= task_count; i++) {
        int candidate = (cur_task + i) % task_count;
        TaskState s = tasks[candidate].state;
        if (s == TASK_READY || s == TASK_RUNNING) {
            next = candidate;
            break;
        }
    }

    if (next == cur_task)
        return cur_esp;

    tasks[cur_task].state = TASK_READY;
    tasks[next].state = TASK_RUNNING;
    cur_task = next;
    tss_set_esp0(tasks[next].esp0);

    return tasks[next].esp;
}
