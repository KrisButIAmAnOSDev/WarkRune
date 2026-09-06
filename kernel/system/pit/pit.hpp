#pragma once
#include <stdint.h>

void pit_init(uint32_t hz);
void pit_sleep(uint32_t ms);
uint64_t pit_ticks();
uint64_t pit_uptime_ms();
