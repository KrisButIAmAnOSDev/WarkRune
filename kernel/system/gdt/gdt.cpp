#include "system/gdt/gdt.hpp"

extern "C" void gdt_load();

void gdt_init() {
    gdt_load();
}
