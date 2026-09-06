#include "memory/pmm/pmm.hpp"
#include "memory/memory.hpp"

/* Number of pages reserved for the kmalloc heap right after the bitmap.
   Must match KMALLOC_PAGES in kmalloc.cpp. */
#define KMALLOC_RESERVED_PAGES 128u

static uint32_t* bitmap    = nullptr;
static uint32_t  total     = 0;
static uint32_t  used      = 0;
static uint32_t  bmap_skip = 0;

static void set(uint32_t f) { bitmap[f/32] |=  (1u << (f%32)); }
static void clr(uint32_t f) { bitmap[f/32] &= ~(1u << (f%32)); }
static bool tst(uint32_t f) { return (bitmap[f/32] >> (f%32)) & 1u; }

void pmm_init() {
    uint16_t count = *(uint16_t*)0x5F00;

    uint64_t best_base = 0, best_len = 0;
    for (uint16_t i = 0; i < count; i++) {
        E820Entry* e = (E820Entry*)(0x6000 + i * sizeof(E820Entry));
        if (e->type == 1 && e->base >= 0x100000 && e->length > best_len) {
            best_base = e->base;
            best_len  = e->length;
        }
    }

    if (!best_base || !best_len) return;

    total      = (uint32_t)(best_len / PAGE_SIZE);
    bitmap     = (uint32_t*)(uintptr_t)best_base;
    uint32_t bmap_bytes = ((total + 31) / 32) * 4;
    bmap_skip  = (bmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Reserve the kmalloc heap pages that sit right after the bitmap */
    bmap_skip += KMALLOC_RESERVED_PAGES;

    /* Mark all frames as used, then free the allocatable range */
    for (uint32_t i = 0; i < (total + 31) / 32; i++)
        bitmap[i] = 0xFFFFFFFF;

    for (uint32_t i = bmap_skip; i < total; i++)
        clr(i);

    used = bmap_skip;
}

void* pmm_alloc() {
    for (uint32_t i = bmap_skip; i < total; i++) {
        if (!tst(i)) {
            set(i);
            used++;
            return (void*)(uintptr_t)((uintptr_t)bitmap + i * PAGE_SIZE);
        }
    }
    return nullptr;
}

void pmm_free(void* addr) {
    if (!addr) return;
    uint32_t frame = ((uintptr_t)addr - (uintptr_t)bitmap) / PAGE_SIZE;
    if (frame < bmap_skip || frame >= total) return;
    if (!tst(frame)) return;
    clr(frame);
    used--;
}

uint32_t pmm_used()       { return used; }
uint32_t pmm_free_count() { return total - used; }
uint32_t pmm_total()      { return total; }
