#pragma once
#include <stdint.h>

void tss_init();
void tss_set_esp0(uint32_t esp0);
