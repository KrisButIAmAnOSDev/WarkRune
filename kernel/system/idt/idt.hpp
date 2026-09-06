#pragma once
#include <stdint.h>
#include "exceptions.hpp"

extern "C" {
    void idt_load();
    void idt_set_gate(int num, uint32_t handler, int selector, int flags);
    void timer_handler();
    void kbd_handler();
    void mouse_irq();
    uint8_t read_scancode();
}

inline void idt_init() {
    idt_set_gate(0,    (uint32_t)exception_divide_zero,  0x08, 0x8E);
    idt_set_gate(4,    (uint32_t)exception_overflow,     0x08, 0x8E);
    idt_set_gate(6,    (uint32_t)exception_invalid_op,   0x08, 0x8E);
    idt_set_gate(7,    (uint32_t)exception_no_fpu,       0x08, 0x8E);
    idt_set_gate(8,    (uint32_t)exception_double_fault, 0x08, 0x8E);
    idt_set_gate(12,   (uint32_t)exception_stack_fault,  0x08, 0x8E);
    idt_set_gate(13,   (uint32_t)exception_gpf,          0x08, 0x8E);
    idt_set_gate(14,   (uint32_t)exception_page_fault,   0x08, 0x8E);
    idt_set_gate(0x20, (uint32_t)timer_handler,          0x08, 0x8E);
    idt_set_gate(0x21, (uint32_t)kbd_handler,            0x08, 0x8E);
    idt_set_gate(0x2C, (uint32_t)mouse_irq, 0x08, 0x8E);
    idt_load();
}
