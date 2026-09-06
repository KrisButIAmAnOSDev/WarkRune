#pragma once
#include <stdint.h>

void pcspk_beep(uint32_t freq, uint32_t duration_ms);
void pcspk_stop();
void play_note(uint32_t freq, uint32_t duration);