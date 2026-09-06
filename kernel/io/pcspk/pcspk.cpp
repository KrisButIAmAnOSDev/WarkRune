#include "pcspk.hpp"
#include "io/io.hpp"
#include "system/pit/pit.hpp"

void pcspk_beep(uint32_t freq, uint32_t duration_ms) {
    uint32_t divisor = 1193182 / freq;
    outb(0x43, 0xB6);
    outb(0x42, divisor & 0xFF);
    outb(0x42, (divisor >> 8) & 0xFF);
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 0x03);
    pit_sleep(duration_ms);
    outb(0x61, tmp);
}

void pcspk_stop() {
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & 0xFC);
}

void play_note(uint32_t freq, uint32_t duration) {
    if (freq > 0) pcspk_beep(freq, duration);
    else pit_sleep(duration);
}
