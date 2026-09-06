#include "memory/shmem/shmem.hpp"
#include "memory/vmm/vmm.hpp"
#include "memory/paging/paging.hpp"

struct ShmemRegion {
    char     name[32];
    bool     used;
    uint32_t virt_addr;
    uint32_t size;
};

static ShmemRegion regions[SHMEM_MAX];

void shmem_init() {
    for (int i = 0; i < SHMEM_MAX; i++)
        regions[i].used = false;
}

static bool name_eq(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void name_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void* shmem_create(const char* name, uint32_t size) {
    for (int i = 0; i < SHMEM_MAX; i++)
        if (regions[i].used && name_eq(regions[i].name, name))
            return (void*)regions[i].virt_addr;

    int free_slot = -1;
    for (int i = 0; i < SHMEM_MAX; i++)
        if (!regions[i].used) { free_slot = i; break; }
    if (free_slot < 0) return nullptr;

    uint32_t pages = (size + 0xFFF) / 0x1000;
    if (pages == 0) pages = 1;

    uint32_t virt = 0;
    for (uint32_t p = 0; p < pages; p++) {
        void* page = vmm_alloc(0, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        if (!page) {
            for (uint32_t cleanup = 0; cleanup < p; cleanup++)
                vmm_free(virt + cleanup * 0x1000);
            return nullptr;
        }
        if (p == 0) virt = (uint32_t)page;
    }

    regions[free_slot].used      = true;
    regions[free_slot].virt_addr = virt;
    regions[free_slot].size      = pages * 0x1000;
    name_copy(regions[free_slot].name, name, 32);

    for (uint32_t b = 0; b < pages * 0x1000; b++)
        ((uint8_t*)virt)[b] = 0;

    return (void*)virt;
}

void* shmem_open(const char* name) {
    for (int i = 0; i < SHMEM_MAX; i++)
        if (regions[i].used && name_eq(regions[i].name, name))
            return (void*)regions[i].virt_addr;
    return nullptr;
}

void shmem_close(const char* name) {
    for (int i = 0; i < SHMEM_MAX; i++) {
        if (regions[i].used && name_eq(regions[i].name, name)) {
            uint32_t pages = regions[i].size / 0x1000;
            for (uint32_t p = 0; p < pages; p++)
                vmm_free(regions[i].virt_addr + p * 0x1000);
            regions[i].used = false;
            return;
        }
    }
}
