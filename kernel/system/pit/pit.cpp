#include "system/pit/pit.hpp"
#include "io/io.hpp"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ  1193182

static volatile uint64_t ticks = 0;
static uint32_t pit_hz = 1000;

extern "C" void pit_tick() {
    ticks++;
}

void pit_init(uint32_t freq) {
    pit_hz = freq;
    uint32_t divisor = PIT_BASE_HZ / freq;
    outb(PIT_COMMAND,  0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));
}

void pit_sleep(uint32_t ms) {
    uint64_t target = ticks + (uint64_t)ms * pit_hz / 1000;
    while (ticks < target) asm volatile("hlt");
}

uint64_t pit_ticks() {
    asm volatile("cli");
    uint64_t t = ticks;
    asm volatile("sti");
    return t;
}

uint64_t pit_uptime_ms() {
    asm volatile("cli");
    uint64_t t = ticks;
    asm volatile("sti");
    return t * 1000 / pit_hz;
}
