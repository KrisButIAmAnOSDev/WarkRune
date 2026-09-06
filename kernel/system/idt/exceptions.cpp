#include "exceptions.hpp"
#include "Graphics/graphics.hpp"
#include "io/serial/serial.hpp"

static void serial_hex32(uint32_t v) {
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        int n = (v >> (i * 4)) & 0xF;
        serial_putc(n < 10 ? '0' + n : 'a' + n - 10);
    }
}

extern "C" void _page_fault_handler(uint32_t cr2) {
    serial_puts("\n[KERNEL PANIC] page fault CR2=");
    serial_hex32(cr2);
    serial_puts(" FB=");
    serial_hex32((uint32_t)framebuffer);
    serial_puts("\n[kernel halted]\n");
    asm volatile("cli");
    while (1) { asm volatile("hlt"); }
}