#include "memory/paging/paging.hpp"
#include "Graphics/text.hpp"

static uint32_t page_tables[1024][1024]
    __attribute__((aligned(4096)))
    __attribute__((section(".pagetab")));

static uint32_t page_directory[1024]
    __attribute__((aligned(4096)))
    __attribute__((section(".pagetab")));

static void flush_tlb(uint32_t virt) {
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_idx] & PAGE_PRESENT))
        page_directory[dir_idx] = (uint32_t)page_tables[dir_idx] | PAGE_PRESENT | PAGE_WRITE;
    else if ((flags & PAGE_USER) && !(page_directory[dir_idx] & PAGE_USER))
        page_directory[dir_idx] |= PAGE_USER;

    page_tables[dir_idx][table_idx] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    flush_tlb(virt);
}

void paging_unmap(uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_idx] & PAGE_PRESENT)) return;

    page_tables[dir_idx][table_idx] = 0;
    flush_tlb(virt);
}

uint32_t paging_get_physical(uint32_t virt) {
    uint32_t dir_idx   = virt >> 22;
    uint32_t table_idx = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_idx] & PAGE_PRESENT)) return 0;

    uint32_t entry = page_tables[dir_idx][table_idx];
    if (!(entry & PAGE_PRESENT)) return 0;

    return (entry & 0xFFFFF000) | (virt & 0xFFF);
}

void paging_init() {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = PAGE_WRITE;
        for (int j = 0; j < 1024; j++)
            page_tables[i][j] = 0;
    }

    for (uint32_t addr = 0; addr < 0x4000000; addr += PAGE_SIZE_4K)
        paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);

    for (uint32_t addr = 0xA0000; addr < 0xC0000; addr += PAGE_SIZE_4K)
        paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);

    for (uint32_t addr = 0xC0000; addr < 0x100000; addr += PAGE_SIZE_4K)
        paging_map(addr, addr, PAGE_PRESENT);

    uint32_t fb = *(uint32_t*)0x5000;
    if (fb > 0)
        for (uint32_t addr = fb; addr < fb + 0x400000; addr += PAGE_SIZE_4K)
            paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);

    for (uint32_t addr = BACKBUF_PHYS_ADDR;
         addr < BACKBUF_PHYS_ADDR + 0x400000u;
         addr += PAGE_SIZE_4K)
        paging_map(addr, addr, PAGE_PRESENT | PAGE_WRITE);

    asm volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}
