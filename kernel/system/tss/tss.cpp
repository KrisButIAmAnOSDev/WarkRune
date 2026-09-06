#include "system/tss/tss.hpp"

struct Tss {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

static Tss kernel_tss;

extern "C" void gdt_set_tss_base(uint32_t base);
extern "C" void tss_flush();

void tss_init() {
    uint8_t* p = (uint8_t*)&kernel_tss;
    for (uint32_t i = 0; i < sizeof(Tss); i++) p[i] = 0;
    kernel_tss.ss0 = 0x10;
    kernel_tss.esp0 = 0x9FFFC;
    kernel_tss.iomap_base = (uint16_t)sizeof(Tss);
    gdt_set_tss_base((uint32_t)&kernel_tss);
    tss_flush();
}

void tss_set_esp0(uint32_t esp0) {
    kernel_tss.esp0 = esp0;
}
