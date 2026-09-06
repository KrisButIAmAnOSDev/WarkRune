#include "memory.hpp"

static uint64_t heap_base = 0;
static uint64_t heap_size = 0;

void memory_init() {
    uint16_t count = *e820_count;

    for (uint16_t i = 0; i < count; i++) {
        E820Entry& e = e820_map[i];
        if (e.type == 1 && e.base >= 0x100000 && e.length > heap_size) {
            heap_base = e.base;
            heap_size = e.length;
        }
    }
}

uint64_t get_heap_base() { return heap_base; }
uint64_t get_heap_size() { return heap_size; }
