#pragma once
#include "system/panic.hpp"

extern "C" void _panic(const char* msg, int line);

#define STACK_BOTTOM    0x80000
#define STACK_MAGIC_1   0x44656C74
#define STACK_MAGIC_2   0x6172756E
#define STACK_MAGIC_3   0x5448524E

static inline void stack_guard_init() {
    *((volatile unsigned int*)(STACK_BOTTOM))     = STACK_MAGIC_1;
    *((volatile unsigned int*)(STACK_BOTTOM + 4)) = STACK_MAGIC_2;
    *((volatile unsigned int*)(STACK_BOTTOM + 8)) = STACK_MAGIC_3;
}

static inline void stack_guard_check() {
    if (*((volatile unsigned int*)(STACK_BOTTOM))     != STACK_MAGIC_1)
        _panic("Stack overflow", 0);
    if (*((volatile unsigned int*)(STACK_BOTTOM + 4)) != STACK_MAGIC_2)
        _panic("Stack overflow", 0);
    if (*((volatile unsigned int*)(STACK_BOTTOM + 8)) != STACK_MAGIC_3)
        _panic("Stack overflow", 0);
}
