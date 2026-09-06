#pragma once
#include <stdint.h>

#define PAGE_SIZE 4096

void     pmm_init();
void*    pmm_alloc();
void     pmm_free(void* addr);
uint32_t pmm_used();
uint32_t pmm_free_count();
uint32_t pmm_total();
