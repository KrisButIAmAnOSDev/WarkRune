#pragma once
#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4
#define PAGE_SIZE_4K 0x1000

void paging_init();
void paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
void paging_unmap(uint32_t virt);
uint32_t paging_get_physical(uint32_t virt);
