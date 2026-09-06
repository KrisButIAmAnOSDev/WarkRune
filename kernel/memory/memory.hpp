#pragma once
#include <stdint.h>

struct E820Entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed));

inline E820Entry* e820_map   = (E820Entry*)0x6000;
inline uint16_t*  e820_count = (uint16_t*)0x5F00;

void memory_init();
uint64_t get_heap_base();
uint64_t get_heap_size();