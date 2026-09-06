#pragma once
#include <stdint.h>

void serial_init();
void serial_putc(char c);
void serial_puts(const char* s);
char serial_getc();
int serial_ready();
void serial_puthex(uint32_t n);