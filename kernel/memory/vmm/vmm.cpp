#include "memory/vmm/vmm.hpp"
#include "memory/pmm/pmm.hpp"
#include "memory/paging/paging.hpp"

static uint32_t next_virt = 0xD0000000;

void vmm_init() {
    next_virt = 0xD0000000;
}

void* vmm_alloc(uint32_t virt, uint32_t flags) {
    void* frame = pmm_alloc();
    if (!frame) return nullptr;

    if (virt == 0) {
        if (next_virt + PAGE_SIZE < next_virt) {
            pmm_free(frame);
            return nullptr;
        }
        virt = next_virt;
        next_virt += PAGE_SIZE;
    }

    paging_map(virt, (uint32_t)frame, flags);
    return (void*)(uintptr_t)virt;
}

void vmm_free(uint32_t virt) {
    uint32_t phys = paging_get_physical(virt);
    if (!phys) return;
    paging_unmap(virt);
    pmm_free((void*)(uintptr_t)phys);
}
