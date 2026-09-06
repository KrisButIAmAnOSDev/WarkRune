#include "fs/elf/elf.hpp"
#include "fs/fat12.hpp"
#include "memory/kmalloc.hpp"
#include "memory/vmm/vmm.hpp"
#include "memory/paging/paging.hpp"
#include "memory/pmm/pmm.hpp"
#include "system/tss/tss.hpp"
#include "system/kctx.hpp"
#include "io/serial/serial.hpp"
#include "klib/str.h"

extern "C" void jump_usermode(uint32_t entry, uint32_t esp);

static void copy_str(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void path_to_upper(char* dst, const char* src) {
    int i = 0;
    while (src[i] && i < 255) {
        char c = src[i];
        dst[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
        i++;
    }
    dst[i] = 0;
}

bool elf_valid(const uint8_t* data) {
    return data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';
}

ElfLoadResult elf_load(const char* path) {
    ElfLoadResult result = {};

    char upper[256];
    path_to_upper(upper, path);

    int fd = fat12_open(upper);
    if (fd < 0) fd = fat12_open(path);
    if (fd < 0) {
        copy_str(result.error, "file not found", 64);
        return result;
    }

    int size = fat12_size(fd);
    if (size <= 0) {
        fat12_close(fd);
        copy_str(result.error, "empty file", 64);
        return result;
    }

    uint8_t* buf = (uint8_t*)kmalloc(size);
    if (!buf) {
        fat12_close(fd);
        copy_str(result.error, "out of memory", 64);
        return result;
    }

    fat12_read(fd, buf, size);
    fat12_close(fd);

    if (!elf_valid(buf)) {
        kfree(buf);
        copy_str(result.error, "not an ELF file", 64);
        return result;
    }

    ElfHeader* hdr = (ElfHeader*)buf;
    if (hdr->bits != ELF_CLASS32) { kfree(buf); copy_str(result.error, "not 32-bit ELF", 64); return result; }
    if (hdr->machine != ELF_X86)  { kfree(buf); copy_str(result.error, "not x86 ELF", 64); return result; }
    if (hdr->type != ELF_EXEC)    { kfree(buf); copy_str(result.error, "not executable ELF", 64); return result; }

    uint32_t load_base = 0xFFFFFFFF;
    uint32_t load_end  = 0;
    ElfProgramHeader* phdrs = (ElfProgramHeader*)(buf + hdr->phoff);
    if (hdr->phoff + (uint32_t)hdr->phnum * sizeof(ElfProgramHeader) > (uint32_t)size) {
        kfree(buf);
        copy_str(result.error, "program headers out of bounds", 64);
        return result;
    }

    for (int i = 0; i < hdr->phnum; i++) {
        ElfProgramHeader* ph = &phdrs[i];
        if (ph->type != PT_LOAD) continue;
        if (ph->vaddr < load_base) load_base = ph->vaddr;
        if (ph->vaddr + ph->memsz > load_end) load_end = ph->vaddr + ph->memsz;
    }

    if (load_base == 0xFFFFFFFF) {
        kfree(buf);
        copy_str(result.error, "no loadable segments", 64);
        return result;
    }

    uint32_t total = load_end - load_base;
    uint32_t pages = (total + 0xFFF) / 0x1000;

    for (uint32_t p = 0; p < pages; p++) {
        void* frame = pmm_alloc();
        if (!frame) {
            for (uint32_t j = 0; j < p; j++)
                pmm_free((void*)(paging_get_physical(load_base + j * 0x1000) & 0xFFFFF000));
            kfree(buf);
            copy_str(result.error, "pmm out of frames", 64);
            return result;
        }
        uint32_t virt = load_base + p * 0x1000;
        paging_map(virt, (uint32_t)frame, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    for (int i = 0; i < hdr->phnum; i++) {
        ElfProgramHeader* ph = &phdrs[i];
        if (ph->type != PT_LOAD) continue;
        if (ph->offset + ph->filesz > (uint32_t)size) {
            for (uint32_t j = 0; j < pages; j++)
                pmm_free((void*)(paging_get_physical(load_base + j * 0x1000) & 0xFFFFF000));
            kfree(buf);
            copy_str(result.error, "segment exceeds file size", 64);
            return result;
        }
        if (ph->memsz > 0xFFFFFFFF - ph->vaddr) {
            for (uint32_t j = 0; j < pages; j++)
                pmm_free((void*)(paging_get_physical(load_base + j * 0x1000) & 0xFFFFF000));
            kfree(buf);
            copy_str(result.error, "segment vaddr overflow", 64);
            return result;
        }
        uint8_t* dst = (uint8_t*)ph->vaddr;
        uint8_t* src = buf + ph->offset;
        for (uint32_t b = 0; b < ph->filesz; b++) dst[b] = src[b];
        for (uint32_t b = ph->filesz; b < ph->memsz; b++) dst[b] = 0;
    }

    result.ok    = true;
    result.entry = hdr->entry;
    result.base  = load_base;
    result.size  = total;

    kfree(buf);
    return result;
}

void elf_unload(uint32_t base, uint32_t size) {
    uint32_t pages = (size + 0xFFF) / 0x1000;
    for (uint32_t p = 0; p < pages; p++) {
        uint32_t virt = base + p * 0x1000;
        uint32_t phys = paging_get_physical(virt);
        if (phys) { paging_unmap(virt); pmm_free((void*)(phys & 0xFFFFF000)); }
    }
}

static void copy_err(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

#define USER_STACK_BASE  0x800000u
#define USER_STACK_PAGES 4u
#define USER_STACK_STEP  0x10000u
#define MAX_EXEC_DEPTH   4
#define EXEC_KSTACK_SIZE 0x4000u

static uint8_t exec_kstack[MAX_EXEC_DEPTH][EXEC_KSTACK_SIZE] __attribute__((aligned(16)));
static KCtxBuf exec_ctx_stack[MAX_EXEC_DEPTH];
static int     exec_ctx_depth = 0;

static uint32_t kstack_top(int depth) {
    if (depth < 0 || depth >= MAX_EXEC_DEPTH) return 0x9FFFC;
    return (uint32_t)exec_kstack[depth] + EXEC_KSTACK_SIZE;
}

void elf_exit() {
    if (exec_ctx_depth > 0)
        klongjmp(exec_ctx_stack[exec_ctx_depth - 1]);
    asm volatile("cli; hlt");
}

void elf_exec(const char* path, char* err_out, int err_sz) {
    serial_puts("[elf_exec] loading: ");
    serial_puts(path);
    serial_putc('\n');

    ElfLoadResult r = elf_load(path);
    if (!r.ok) {
        serial_puts("[elf_exec] load failed: ");
        serial_puts(r.error);
        serial_putc('\n');
        if (err_out) copy_err(err_out, r.error, err_sz);
        return;
    }

    serial_puts("[elf_exec] loaded ok\n");

    int depth = exec_ctx_depth;
    if (depth >= MAX_EXEC_DEPTH) {
        if (err_out) copy_err(err_out, "exec: too deep", err_sz);
        return;
    }

    uint32_t stack_base = USER_STACK_BASE + (uint32_t)depth * USER_STACK_STEP;
    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        void* frame = pmm_alloc();
        if (!frame) {
            for (uint32_t j = 0; j < i; j++) {
                uint32_t phys = paging_get_physical(stack_base + j * 0x1000);
                if (phys) { paging_unmap(stack_base + j * 0x1000); pmm_free((void*)(phys & 0xFFFFF000)); }
            }
            if (err_out) copy_err(err_out, "pmm: no stack frame", err_sz);
            elf_unload(r.base, r.size);
            return;
        }
        paging_map(stack_base + i * 0x1000, (uint32_t)frame, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    uint32_t user_esp = stack_base + USER_STACK_PAGES * 0x1000;
    tss_set_esp0(kstack_top(depth));

    serial_puts("[elf_exec] jumping to ring3\n");

    exec_ctx_depth++;
    if (ksetjmp(exec_ctx_stack[depth]) == 0) {
        jump_usermode(r.entry, user_esp);
    }
    asm volatile("sti");
    exec_ctx_depth = depth;

    serial_puts("[elf_exec] returned from ring3\n");

    tss_set_esp0(depth > 0 ? kstack_top(depth - 1) : 0x9FFFC);

    for (uint32_t i = 0; i < USER_STACK_PAGES; i++) {
        uint32_t virt = stack_base + i * 0x1000;
        uint32_t phys = paging_get_physical(virt);
        if (phys) { paging_unmap(virt); pmm_free((void*)(phys & 0xFFFFF000)); }
    }
    elf_unload(r.base, r.size);
}
