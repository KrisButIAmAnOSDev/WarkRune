#include "memory/kmalloc.hpp"
#include "memory/memory.hpp"

/* ── Heap placement ───────────────────────────────────────────────────
   We place the kmalloc heap at E820-free RAM, starting right after the
   PMM bitmap area (pmm_init reserves the first 8 pages = 32KB for its
   bitmap, then an additional 128 pages = 512KB for our heap).
   Base = get_heap_base() + 8 * PAGE_SIZE  (just past the PMM bitmap)
   Size = 512 KB
   pmm.cpp extends bmap_skip by KMALLOC_PAGES so PMM never re-allocates
   these frames.
   ─────────────────────────────────────────────────────────────────── */
#define KMALLOC_PAGE_SIZE  4096u
#define KMALLOC_PMM_SKIP   8u          /* PMM bitmap pages */
#define KMALLOC_PAGES      128u        /* 512 KB / 4 KB */
#define KMALLOC_HEAP_SIZE  (KMALLOC_PAGES * KMALLOC_PAGE_SIZE)

#define BLOCK_MAGIC 0xDEADBEEF

struct BlockHeader {
    u32          magic;
    usize        size;
    u8           free;
    BlockHeader* next;
};

static u8*          heap_pool  = nullptr;
static BlockHeader* heap_head  = nullptr;
static u8           heap_ready = 0;

static void merge_free_blocks() {
    BlockHeader* cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(BlockHeader) + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void kmalloc_init() {
    /* Derive heap base from E820 best free region (same region PMM uses).
       We start KMALLOC_PMM_SKIP pages in (past the PMM bitmap) and use
       KMALLOC_PAGES consecutive pages.  pmm.cpp bumps its bmap_skip to
       skip these pages so there is no double-use. */
    uint64_t base = get_heap_base();
    if (base < 0x100000u) base = 0x100000u;          /* must be above 1 MB */
    heap_pool = (u8*)(uintptr_t)(base +
                (uint64_t)KMALLOC_PMM_SKIP * KMALLOC_PAGE_SIZE);

    heap_head        = (BlockHeader*)heap_pool;
    heap_head->magic = BLOCK_MAGIC;
    heap_head->size  = KMALLOC_HEAP_SIZE - sizeof(BlockHeader);
    heap_head->free  = 1;
    heap_head->next  = nullptr;
    heap_ready       = 1;
}

void* kmalloc(usize size) {
    if (!heap_ready) kmalloc_init();
    if (size == 0) return nullptr;

    usize aligned = (size + 7) & ~(usize)7;

    asm volatile("cli");
    BlockHeader* cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= aligned) {
            if (cur->size >= aligned + sizeof(BlockHeader) + 8) {
                BlockHeader* split = (BlockHeader*)((u8*)(cur + 1) + aligned);
                split->magic = BLOCK_MAGIC;
                split->size  = cur->size - aligned - sizeof(BlockHeader);
                split->free  = 1;
                split->next  = cur->next;
                cur->size    = aligned;
                cur->next    = split;
            }
            cur->free = 0;
            asm volatile("sti");
            return (void*)(cur + 1);
        }
        cur = cur->next;
    }
    asm volatile("sti");
    return nullptr;
}

void kfree(void* ptr) {
    if (!ptr) return;
    asm volatile("cli");
    BlockHeader* hdr = ((BlockHeader*)ptr) - 1;
    if (hdr->magic != BLOCK_MAGIC) { asm volatile("sti"); return; }
    hdr->free = 1;
    merge_free_blocks();
    asm volatile("sti");
}

void* krealloc(void* ptr, usize new_size) {
    if (!ptr)      return kmalloc(new_size);
    if (!new_size) { kfree(ptr); return nullptr; }

    BlockHeader* hdr = ((BlockHeader*)ptr) - 1;
    if (hdr->magic != BLOCK_MAGIC) return nullptr;
    if (hdr->size >= new_size) return ptr;

    void* fresh = kmalloc(new_size);
    if (!fresh) return nullptr;

    u8*   src = (u8*)ptr;
    u8*   dst = (u8*)fresh;
    usize n   = hdr->size;
    for (usize i = 0; i < n; i++) dst[i] = src[i];

    kfree(ptr);
    return fresh;
}

usize ksize(void* ptr) {
    if (!ptr) return 0;
    BlockHeader* hdr = ((BlockHeader*)ptr) - 1;
    if (hdr->magic != BLOCK_MAGIC) return 0;
    return hdr->size;
}

void kmem_info(uint32_t* used_out, uint32_t* free_out) {
    uint32_t used = 0, free_bytes = 0;
    BlockHeader* cur = heap_head;
    while (cur) {
        if (cur->free) free_bytes += (uint32_t)cur->size;
        else           used       += (uint32_t)cur->size;
        cur = cur->next;
    }
    if (used_out) *used_out = used;
    if (free_out) *free_out = free_bytes;
}
