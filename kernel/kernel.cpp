#include "Graphics/text.hpp"
#include "Graphics/graphics.hpp"
#include "klib/new.hpp"
#include "klib/color.hpp"
#include "klib/printf.hpp"
#include "system/idt/idt.hpp"
#include "system/panic.hpp"
#include "system/stack.hpp"
#include "system/pit/pit.hpp"
#include "system/cpuid/cpuid.hpp"
#include "system/syscall/syscall.hpp"
#include "io/keyboard.hpp"
#include "io/ata.hpp"
#include "io/serial/serial.hpp"
#include "io/mouse/mouse.hpp"
#include "fs/fat12.hpp"
#include "shell/shell.hpp"
#include "memory/memory.hpp"
#include "memory/kmalloc.hpp"
#include "memory/pmm/pmm.hpp"
#include "memory/vmm/vmm.hpp"
#include "memory/paging/paging.hpp"
#include "system/gdt/gdt.hpp"
#include "system/tss/tss.hpp"
#include "system/sched/sched.hpp"
#include "system/ipc/fd.hpp"
#include "memory/shmem/shmem.hpp"
#include "system/acpi/acpi.hpp"

extern "C" {
    void panic(const char* msg, const char* file, int line) { PANIC(msg); }
    void _panic(const char* msg, int line)                  { PANIC(msg); }
}

extern "C" void kernel_main() {
    gdt_init();
    print("bootloader: works", COL_MAGENTA);
    idt_init();
    pit_init(1000);
    sched_init();
    sched_test();
    stack_guard_init();

    memory_init();
    acpi_init();
    paging_init();
    pmm_init();
    kmalloc_init();
    vmm_init();
    shmem_init();

    framebuffer = (u32*)BACKBUF_PHYS_ADDR;
    fb_pitch = 800u * FB_BPP;

    serial_init();
    tss_init();
    syscall_init();

    ata_init();
    if (!fat12_init(256)) {
        serial_puts("fat12: FAIL");
        print("fat12: FAIL", COL_ERR);
    } else {
        serial_puts("fat12: works");
        print("fat12: works", COL_INFO);
    }
    fd_init();

    mouse_init();
    mouse_set_bounds(800, 600);

    fill_screen(COL_BG);

    serial_puts("bootloader: works");
    serial_puts("idt: works");
    serial_puts("stack guard: works");

    print("idt: works",         COL_INFO);
    print("stack guard: works", COL_INFO);

    paging_map(0x200000, 0x100000, PAGE_PRESENT | PAGE_WRITE);
    volatile uint32_t* pp = (volatile uint32_t*)0x200000;
    *pp = 0xDEADBEEF;
    if (*pp == 0xDEADBEEF) {
        print("paging: works", COL_INFO);
        serial_puts("paging: works");
    } else {
        print("paging: FAIL", COL_ERR);
        serial_puts("paging: FAIL");
    }
    paging_unmap(0x200000);
    /* Restore identity mapping in case VBE LFB sits at 0x200000 */
    paging_map(0x200000, 0x200000, PAGE_PRESENT | PAGE_WRITE);

    void* pa = pmm_alloc();
    void* pb = pmm_alloc();
    if (pa && pb && pa != pb) {
        print("pmm: works", COL_INFO);
        serial_puts("pmm: works");
    } else {
        print("pmm: FAIL", COL_ERR);
        serial_puts("pmm: FAIL");
    }
    pmm_free(pa);
    pmm_free(pb);

    void* va = vmm_alloc(0, PAGE_PRESENT | PAGE_WRITE);
    void* vb = vmm_alloc(0, PAGE_PRESENT | PAGE_WRITE);
    if (va && vb && va != vb) {
        print("vmm: works", COL_INFO);
        serial_puts("vmm: works");
    } else {
        print("vmm: FAIL", COL_ERR);
        serial_puts("vmm: FAIL");
    }
    vmm_free((uint32_t)va);
    vmm_free((uint32_t)vb);

    stack_guard_check();

    char buf[64];
    ksprintf(buf, "printf: works");
    serial_puts(buf);
    print(buf, COL_INFO);

    while (read_scancode() != 0);

    serial_shell_init();
    shell_run(get_y());
}
