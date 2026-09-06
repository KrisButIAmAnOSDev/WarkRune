#pragma once
#include <stdint.h>

void  vmm_init();
void* vmm_alloc(uint32_t virt, uint32_t flags);
void  vmm_free(uint32_t virt);
