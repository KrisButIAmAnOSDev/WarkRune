#include "io/serial/serial.hpp"
#include "io/io.hpp"

#define COM1 0x3F8

void serial_init() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c) {
    if (c == '\n') {
        while (!(inb(COM1 + 5) & 0x20));
        outb(COM1, '\r');
    }
    while (!(inb(COM1 + 5) & 0x20));
    outb(COM1, c);
}

void serial_puts(const char* s) {
    while (*s) serial_putc(*s++);
    serial_putc('\n');
}

char serial_getc() {
    while (!(inb(COM1 + 5) & 0x01));
    return inb(COM1);
}

int serial_ready() {
    return inb(COM1 + 5) & 0x01;
}

void serial_puthex(uint32_t n) {
    serial_puts("0x");
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
        serial_putc(hex[(n >> i) & 0xF]);
}