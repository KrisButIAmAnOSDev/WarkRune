#include "kernel/klib/printf.hpp"
#include "shell/shell.hpp"
#include "Graphics/text.hpp"
#include "io/keyboard.hpp"
#include "io/ata.hpp"
#include "io/pcspk/pcspk.hpp"
#include "io/serial/serial.hpp"
#include "io/mouse/mouse.hpp"
#include "fs/fat12.hpp"
#include "fs/elf/elf.hpp"
#include "system/panic.hpp"
#include "system/rtc.hpp"
#include "system/pit/pit.hpp"
#include "system/cpuid/cpuid.hpp"
#include "system/syscall/syscall.hpp"
#include "klib/str.h"
#include "klib/color.hpp"
#include "memory/memory.hpp"
#include "memory/kmalloc.hpp"
#include "memory/pmm/pmm.hpp"
#include "memory/vmm/vmm.hpp"
#include "memory/paging/paging.hpp"

#define CHAR_W 8
#define CHAR_H 10
#define SCREEN_W 800
#define SCREEN_H 600

#undef CMD_BUF
#define CMD_BUF 128

#define PROMPT "WarkRune> "
#define PROMPT_PX (10 * CHAR_W)

static char cwd[256] = "/";
static bool g_serial_mode = false;
static int32_t last_cx = -1, last_cy = -1;

// Cursor sprite footprint: 8 wide x 12 tall
#define CURS_W 8
#define CURS_H 12
static uint32_t cursor_save[CURS_W * CURS_H];
static bool cursor_saved = false;

// Save back-buffer pixels under the cursor before drawing it
static void cursor_save_bg(int32_t x, int32_t y) {
    for (int py = 0; py < CURS_H; py++)
        for (int px = 0; px < CURS_W; px++) {
            int sx = x + px, sy = y + py;
            if ((unsigned)sx < 800u && (unsigned)sy < 600u)
                cursor_save[py * CURS_W + px] = g_backbuf[(unsigned)sy * 800u + (unsigned)sx];
            else
                cursor_save[py * CURS_W + px] = COL_BG;
        }
    cursor_saved = true;
}

// Restore those back-buffer pixels (erase cursor without destroying content)
static void cursor_restore_bg(int32_t x, int32_t y) {
    if (!cursor_saved) return;
    for (int py = 0; py < CURS_H; py++)
        for (int px = 0; px < CURS_W; px++) {
            int sx = x + px, sy = y + py;
            if ((unsigned)sx < 800u && (unsigned)sy < 600u)
                g_backbuf[(unsigned)sy * 800u + (unsigned)sx] = cursor_save[py * CURS_W + px];
        }
}

static void draw_cursor_sprite(int32_t x, int32_t y) {
    // Vertical stem: 2 wide, 12 tall
    draw_rect(x, y, 2, CURS_H, 0xFFFFFF);
    // Horizontal cap: 8 wide, 2 tall
    draw_rect(x, y, CURS_W, 2, 0xFFFFFF);
}

static void update_cursor() {
    MouseState m = mouse_get();

    // Restore pixels at old position before moving
    if (last_cx >= 0 && last_cy >= 0)
        cursor_restore_bg(last_cx, last_cy);

    // Save pixels at new position, then draw cursor over them
    cursor_save_bg(m.x, m.y);
    draw_cursor_sprite(m.x, m.y);

    last_cx = m.x;
    last_cy = m.y;
    fb_present();
}

static void shell_puts(const char* s, uint32_t color) {
    if (!g_serial_mode) {
        if (get_y() + CHAR_H > SCREEN_H) {
            fill_screen(COL_BG);
            set_y(0);
        }
        print(s, color);
    }
    serial_puts(s);
}

static void shell_readline(char* buf, int cap) {
    if (get_y() + CHAR_H > SCREEN_H) {
        fill_screen(COL_BG);
        set_y(0);
    }
    printxy(PROMPT, 0, get_y(), COL_PROMPT);
    serial_puts(PROMPT);

    int len = 0;
    int x = PROMPT_PX;
    int y = get_y();
    set_y(get_y() + CHAR_H);

    while (serial_ready()) serial_getc();

    while (1) {
        char c = 0;
        while ((c = read_key()) == 0) {
            update_cursor();
        }

        if (c == 0) continue;

        serial_putc(c);

        if (c == '\r' || c == '\n') {
            buf[len] = '\0';
            break;
        }

        if (c == 0x08 || c == 0x7F) {
            if (len > 0) {
                len--;
                buf[len] = '\0';
                x -= CHAR_W;
                draw_rect(x, y, CHAR_W, 8, COL_BG);
                printxy(buf, PROMPT_PX, y, COL_FG);
            }
            continue;
        }

        if (len < cap - 1) {
            buf[len++] = c;
            char tmp[2] = { c, '\0' };
            printxy(tmp, x, y, COL_FG);
            x += CHAR_W;
        }
    }
}

static void build_path(char* out, const char* rel) {
    if (!rel || rel[0] == 0) {
        int i = 0;
        while (cwd[i] && i < 254) { out[i] = cwd[i]; i++; }
        out[i] = 0;
        return;
    }
    if (rel[0] == '/') {
        int i = 0;
        while (rel[i] && i < 254) { out[i] = rel[i]; i++; }
        out[i] = 0;
    } else {
        int i = 0;
        while (cwd[i] && i < 254) { out[i] = cwd[i]; i++; }
        if (i > 0 && out[i - 1] != '/') out[i++] = '/';
        int j = 0;
        while (rel[j] && i < 254) { out[i] = rel[j]; i++; j++; }
        out[i] = 0;
    }
}

static void cmd_help();
static void cmd_clear();
static void cmd_panic();
static void cmd_time();
static void cmd_uptime();
static void cmd_paging();
static void cmd_free();
static void cmd_fat12_test();
static void cmd_freedom();
static void cmd_cpuid();
static void cmd_syscall();
static void cmd_exec(const char* args);
static void cmd_memtesting();

struct Cmd {
    const char* name;
    const char* desc;
    void (*fn)();
};

static const Cmd cmds[] = {
    { "help",       "show this list",        cmd_help       },
    { "clear",      "clear screen",          cmd_clear      },
    { "panic",      "trigger kernel panic",  cmd_panic      },
    { "time",       "show time",             cmd_time       },
    { "uptime",     "show uptime",           cmd_uptime     },
    { "paging",     "test paging",           cmd_paging     },
    { "free",       "show memory usage",     cmd_free       },
    { "fat12",      "test FAT12 filesystem", cmd_fat12_test },
    { "freedom",    "play a tune",           cmd_freedom    },
    { "cpuid",      "print cpuid info",      cmd_cpuid      },
    { "syscall",    "test syscall",          cmd_syscall    },
    { "memtesting", "test all memory",       cmd_memtesting },
};

static const int N_CMDS = (int)(sizeof(cmds) / sizeof(cmds[0]));

static void to_upper(char* dst, const char* src) {
    int i = 0;
    while (src[i]) {
        char c = src[i];
        dst[i] = (c >= 'a' && c <= 'z') ? c - 32 : c;
        i++;
    }
    dst[i] = 0;
}

static int fat12_open_case(const char* path) {
    int fd = fat12_open(path);
    if (fd >= 0) return fd;
    char up[256];
    to_upper(up, path);
    return fat12_open(up);
}

static bool fat12_delete_case(const char* path) {
    if (fat12_delete(path)) return true;
    char up[256];
    to_upper(up, path);
    return fat12_delete(up);
}

static bool fat12_rmdir_case(const char* path) {
    if (fat12_rmdir(path)) return true;
    char up[256];
    to_upper(up, path);
    return fat12_rmdir(up);
}

static bool fat12_mkdir_case(const char* path) {
    if (fat12_mkdir(path)) return true;
    char up[256];
    to_upper(up, path);
    return fat12_mkdir(up);
}

static int fat12_create_case(const char* path) {
    int fd = fat12_create(path);
    if (fd >= 0) return fd;
    char up[256];
    to_upper(up, path);
    return fat12_create(up);
}

static bool fat12_ls_case(const char* path, void (*cb)(const char*, uint32_t, bool)) {
    if (fat12_ls(path, cb)) return true;
    char up[256];
    to_upper(up, path);
    return fat12_ls(up, cb);
}

static bool has_lower_alpha(const char* s) {
    for (int i = 0; s && s[i]; i++) {
        if (s[i] >= 'a' && s[i] <= 'z') return true;
    }
    return false;
}

static void upper_path(char* dst, const char* src) {
    to_upper(dst, src);
}

static void dispatch(const char* input) {
    if (input[0] == 0) return;

    char cmd[32] = {};
    const char* args = nullptr;
    int i = 0;
    while (input[i] && input[i] != ' ' && i < 31) { cmd[i] = input[i]; i++; }
    cmd[i] = 0;
    if (input[i] == ' ') args = input + i + 1;

    if (g_serial_mode) {
        if (str_eq(cmd, "fire") || str_eq(cmd, "freedom") ||
            str_eq(cmd, "exec") || str_eq(cmd, "clear")) {
            serial_puts("(screen command not available on serial)\n");
            return;
        }
    }

    if (str_eq(cmd, "ls")) {
        char path[256];
        build_path(path, args);
        int len = 0;
        while (path[len]) len++;
        if (len > 1 && path[len - 1] == '/') path[len - 1] = 0;
        fat12_ls_case(path, [](const char* name, uint32_t size, bool is_dir) {
            char buf[64];
            if (is_dir)
                ksnprintf(buf, sizeof(buf), "[DIR] %s", name);
            else
                ksnprintf(buf, sizeof(buf), "      %s  (%u bytes)", name, size);
            shell_puts(buf, is_dir ? COL_INFO : COL_FG);
        });
        fb_present();
        return;
    }

    if (str_eq(cmd, "cd")) {
        if (!args || args[0] == 0) { cwd[0] = '/'; cwd[1] = 0; return; }
        if (args[0] == '.' && args[1] == '.' && args[2] == 0) {
            int j = 0;
            while (cwd[j]) j++;
            j--;
            if (j <= 0) return;
            if (cwd[j] == '/') j--;
            while (j > 0 && cwd[j] != '/') j--;
            if (j == 0) { cwd[0] = '/'; cwd[1] = 0; }
            else { cwd[j] = 0; }
            shell_puts(cwd, COL_INFO);
            fb_present();
            return;
        }
        char path[256];
        build_path(path, args);
        int j = 0;
        while (path[j]) { cwd[j] = path[j]; j++; }
        cwd[j] = 0;
        shell_puts(cwd, COL_INFO);
        fb_present();
        return;
    }

    if (str_eq(cmd, "pwd")) { shell_puts(cwd, COL_INFO); fb_present(); return; }

    if (str_eq(cmd, "mkdir")) {
        if (!args || args[0] == 0) { shell_puts("usage: mkdir <name>", COL_ERR); fb_present(); return; }
        char path[256];
        build_path(path, args);
        bool ok = fat12_mkdir(path);
        if (!ok && has_lower_alpha(path)) {
            char up[256];
            upper_path(up, path);
            ok = fat12_mkdir(up);
        }
        shell_puts(ok ? "ok" : "mkdir: failed", ok ? COL_OK : COL_ERR);
        fb_present();
        return;
    }

    if (str_eq(cmd, "touch")) {
        if (!args || args[0] == 0) { shell_puts("usage: touch <name>", COL_ERR); fb_present(); return; }
        char path[256];
        build_path(path, args);
        int fd = fat12_create(path);
        if (fd < 0 && has_lower_alpha(path)) {
            char up[256];
            upper_path(up, path);
            fd = fat12_create(up);
        }
        if (fd >= 0) { fat12_close(fd); shell_puts("ok", COL_OK); }
        else shell_puts("touch: failed", COL_ERR);
        fb_present();
        return;
    }

    if (str_eq(cmd, "rm")) {
        if (!args || args[0] == 0) { shell_puts("usage: rm <file>", COL_ERR); fb_present(); return; }
        char path[256];
        build_path(path, args);
        bool ok = fat12_delete(path);
        if (!ok && has_lower_alpha(path)) {
            char up[256];
            upper_path(up, path);
            ok = fat12_delete(up);
        }
        shell_puts(ok ? "ok" : "rm: failed", ok ? COL_OK : COL_ERR);
        fb_present();
        return;
    }

    if (str_eq(cmd, "rmdir")) {
        if (!args || args[0] == 0) { shell_puts("usage: rmdir <dir>", COL_ERR); fb_present(); return; }
        char path[256];
        build_path(path, args);
        bool ok = fat12_rmdir(path);
        if (!ok && has_lower_alpha(path)) {
            char up[256];
            upper_path(up, path);
            ok = fat12_rmdir(up);
        }
        shell_puts(ok ? "ok" : "rmdir: not empty", ok ? COL_OK : COL_ERR);
        fb_present();
        return;
    }

    if (str_eq(cmd, "cat")) {
        if (!args || args[0] == 0) { shell_puts("usage: cat <file>", COL_ERR); fb_present(); return; }
        char path[256];
        build_path(path, args);
        int fd = fat12_open(path);
        if (fd < 0 && has_lower_alpha(path)) {
            char up[256];
            upper_path(up, path);
            fd = fat12_open(up);
        }
        if (fd < 0) { shell_puts("cat: not found", COL_ERR); fb_present(); return; }
        char buf[512];
        int n;
        while ((n = fat12_read(fd, buf, sizeof(buf) - 1)) > 0) {
            buf[n] = 0;
            shell_puts(buf, COL_FG);
        }
        fat12_close(fd);
        fb_present();
        return;
    }

    if (str_eq(cmd, "exec")) { cmd_exec(args); fb_present(); return; }

    for (int j = 0; j < N_CMDS; j++) {
        if (str_eq(cmds[j].name, cmd)) {
            cmds[j].fn();
            fb_present();
            return;
        }
    }

    char msg[80];
    ksnprintf(msg, sizeof(msg), "unknown: %s (type help)", cmd);
    shell_puts(msg, COL_ERR);
    fb_present();
}

static void cmd_help() {
    shell_puts("commands:", COL_INFO);
    for (int i = 0; i < N_CMDS; i++) {
        char line[80];
        ksnprintf(line, sizeof(line), "  %-12s %s", cmds[i].name, cmds[i].desc);
        shell_puts(line, COL_FG);
    }
    shell_puts("  ls [path]    cat <file>   cd <path>", COL_FG);
    shell_puts("  pwd          mkdir        touch   rm   rmdir", COL_FG);
}

static void cmd_clear() {
    if (!g_serial_mode) {
        fill_screen(COL_BG);
        set_y(0);
    }
}

static void cmd_panic()  { PANIC("idk ask shell.cpp"); }

static void cmd_time() {
    RTCTime t = rtc_get();
    char buf[64];
    ksnprintf(buf, sizeof(buf), "%02d/%02d/20%02d  %02d:%02d:%02d",
        t.day, t.month, t.year, t.hour, t.min, t.sec);
    shell_puts(buf, COL_INFO);
}

static void cmd_uptime() {
    char buf[64];
    uint64_t ms = pit_uptime_ms();
    uint64_t s  = ms / 1000;
    uint64_t m  = s  / 60;
    ksnprintf(buf, sizeof(buf), "uptime: %llum %llus %llums", m, s % 60, ms % 1000);
    shell_puts(buf, COL_INFO);
}

static void cmd_memtesting() {
    void* a = kmalloc(64);
    void* b = kmalloc(128);
    if (!a || !b) {
        shell_puts("kmalloc: FAIL", COL_ERR);
        if (a) kfree(a);
        if (b) kfree(b);
        return;
    }
    kfree(a);
    void* cc = kmalloc(64);
    shell_puts(cc ? "kmalloc: ok" : "kmalloc: FAIL", cc ? COL_OK : COL_ERR);
    kfree(b); kfree(cc);

    void* delta = pmm_alloc();
    void* rune  = pmm_alloc();
    pmm_free(delta);
    void* undertale = pmm_alloc();
    shell_puts(undertale == delta ? "pmm: ok" : "pmm: reuse FAIL",
               undertale == delta ? COL_OK : COL_ERR);
    pmm_free(rune); pmm_free(undertale);

    void* kris   = vmm_alloc(0, PAGE_PRESENT | PAGE_WRITE);
    void* ralsei = vmm_alloc(0, PAGE_PRESENT | PAGE_WRITE);
    if (!kris || !ralsei || kris == ralsei) {
        shell_puts("vmm: FAIL", COL_ERR);
        if (kris)   vmm_free((uint32_t)kris);
        if (ralsei) vmm_free((uint32_t)ralsei);
        return;
    }
    volatile uint32_t* pa = (volatile uint32_t*)kris;
    volatile uint32_t* pb = (volatile uint32_t*)ralsei;
    *pa = 0xAAAAAAAA; *pb = 0xBBBBBBBB;
    shell_puts(*pa == 0xAAAAAAAA && *pb == 0xBBBBBBBB ? "vmm: ok" : "vmm: FAIL",
               *pa == 0xAAAAAAAA && *pb == 0xBBBBBBBB ? COL_OK : COL_ERR);
    vmm_free((uint32_t)kris); vmm_free((uint32_t)ralsei);
}

static void cmd_paging() {
    paging_map(0x600000, 0x400000, PAGE_PRESENT | PAGE_WRITE);
    volatile uint32_t* p = (volatile uint32_t*)0x600000;
    *p = 0xDEADBEEF;
    shell_puts(*p == 0xDEADBEEF ? "paging: ok" : "paging: FAIL",
               *p == 0xDEADBEEF ? COL_OK : COL_ERR);
    paging_unmap(0x600000);
    shell_puts(paging_get_physical(0x600000) == 0 ? "paging unmap: ok" : "paging unmap: FAIL",
               paging_get_physical(0x600000) == 0 ? COL_OK : COL_ERR);
}

static void cmd_free() {
    char buf[128];
    uint32_t used  = pmm_used();
    uint32_t freec = pmm_free_count();
    uint32_t total = pmm_total();
    ksnprintf(buf, sizeof(buf), "total: %u pages (%u MB)", total, (total * PAGE_SIZE) / (1024*1024));
    shell_puts(buf, COL_INFO);
    ksnprintf(buf, sizeof(buf), "used:  %u pages (%u KB)", used, (used * PAGE_SIZE) / 1024);
    shell_puts(buf, COL_ERR);
    ksnprintf(buf, sizeof(buf), "free:  %u pages (%u MB)", freec, (freec * PAGE_SIZE) / (1024*1024));
    shell_puts(buf, COL_OK);
}

static void cmd_fat12_test() {
    shell_puts("--- FAT12 test ---", COL_INFO);

    // 1. List root directory
    shell_puts("[1] ls /", COL_PROMPT);
    int count = 0;
    fat12_ls("/", [](const char* name, uint32_t size, bool is_dir) {
        char buf[64];
        if (is_dir)
            ksnprintf(buf, sizeof(buf), "  [DIR]  %s", name);
        else
            ksnprintf(buf, sizeof(buf), "  [FILE] %s (%u bytes)", name, size);
        shell_puts(buf, COL_FG);
        (void)buf;
    });
    fb_present();

    // 2. Open and read HELLO.TXT
    shell_puts("[2] cat HELLO.TXT", COL_PROMPT);
    int fd = fat12_open("HELLO.TXT");
    if (fd < 0) {
        shell_puts("  open: FAIL", COL_ERR);
    } else {
        shell_puts("  open: ok", COL_OK);
        char buf[128];
        int n = fat12_read(fd, buf, 127);
        if (n > 0) { buf[n] = 0; shell_puts(buf, COL_FG); }
        else shell_puts("  read: empty or FAIL", COL_ERR);
        fat12_close(fd);
    }
    fb_present();

    // 3. Create and write a file
    shell_puts("[3] touch + write TEST.TXT", COL_PROMPT);
    int wfd = fat12_create("TEST.TXT");
    if (wfd < 0) {
        shell_puts("  create: FAIL", COL_ERR);
    } else {
        shell_puts("  create: ok", COL_OK);
        const char* msg = "FAT12 write works!\n";
        int written = fat12_write(wfd, msg, 18);
        fat12_close(wfd);
        if (written == 18) shell_puts("  write: ok", COL_OK);
        else shell_puts("  write: FAIL", COL_ERR);
    }
    fb_present();

    // 4. Read it back
    shell_puts("[4] cat TEST.TXT", COL_PROMPT);
    fd = fat12_open("TEST.TXT");
    if (fd >= 0) {
        char buf[128];
        int n = fat12_read(fd, buf, 127);
        if (n > 0) { buf[n] = 0; shell_puts(buf, COL_FG); }
        else shell_puts("  read: empty", COL_ERR);
        fat12_close(fd);
    } else {
        shell_puts("  open: FAIL", COL_ERR);
    }
    fb_present();

    // 5. mkdir + rmdir
    shell_puts("[5] mkdir + rmdir TESTDIR", COL_PROMPT);
    bool ok = fat12_mkdir("TESTDIR");
    shell_puts(ok ? "  mkdir: ok" : "  mkdir: FAIL", ok ? COL_OK : COL_ERR);
    if (ok) {
        bool ok2 = fat12_rmdir("TESTDIR");
        shell_puts(ok2 ? "  rmdir: ok" : "  rmdir: FAIL", ok2 ? COL_OK : COL_ERR);
    }
    fb_present();

    // 6. List root again
    shell_puts("[6] ls / (final)", COL_PROMPT);
    fat12_ls("/", [](const char* name, uint32_t size, bool is_dir) {
        char buf[64];
        if (is_dir)
            ksnprintf(buf, sizeof(buf), "  [DIR]  %s", name);
        else
            ksnprintf(buf, sizeof(buf), "  [FILE] %s (%u bytes)", name, size);
        shell_puts(buf, COL_FG);
        (void)buf;
    });
    fb_present();

    shell_puts("--- FAT12 test done ---", COL_INFO);
}

static void cmd_freedom() {
    uint32_t nl = 200, r = 80;
    play_note(262,nl); pit_sleep(r); play_note(294,nl); pit_sleep(r);
    play_note(330,nl); pit_sleep(r); play_note(262,nl); pit_sleep(r);
    play_note(392,nl); pit_sleep(r); play_note(440,nl); pit_sleep(r);
    play_note(392,nl); pit_sleep(r); play_note(330,nl); pit_sleep(400);
    play_note(262,nl); pit_sleep(r); play_note(294,nl); pit_sleep(r);
    play_note(330,nl); pit_sleep(r); play_note(262,nl); pit_sleep(r);
    play_note(349,nl); pit_sleep(r); play_note(392,nl); pit_sleep(r);
    play_note(349,nl); pit_sleep(r); play_note(294,nl); pit_sleep(400);
    play_note(392,nl); pit_sleep(r); play_note(440,nl); pit_sleep(r);
    play_note(392,nl); pit_sleep(r); play_note(330,nl); pit_sleep(r);
    play_note(262,nl); pit_sleep(r); play_note(294,nl); pit_sleep(r);
    play_note(262,nl); pit_sleep(600);
}

static void cmd_cpuid() {
    CPUInfo cpu = cpuid_get();
    char buf[128];
    ksnprintf(buf, sizeof(buf), "vendor:  %s", cpu.vendor);
    shell_puts(buf, COL_INFO);
    if (cpu.brand[0]) {
        ksnprintf(buf, sizeof(buf), "brand:   %s", cpu.brand);
        shell_puts(buf, COL_INFO);
    }
    ksnprintf(buf, sizeof(buf), "family:%u  model:%u  stepping:%u",
                   cpu.family, cpu.model, cpu.stepping);
    shell_puts(buf, COL_INFO);
    ksnprintf(buf, sizeof(buf), "fpu:%d apic:%d msr:%d pae:%d",
                   cpu.has_fpu, cpu.has_apic, cpu.has_msr, cpu.has_pae);
    shell_puts(buf, COL_INFO);
}

static void cmd_syscall() {
    syscall(SYS_PRINT, (uint32_t)"SYS_PRINT: ok", 0x00FF00, 0, 0);
    fb_present();
    shell_puts("", COL_FG);
    void* ptr = (void*)syscall(SYS_MEM, 0, 64, 0, 0);   /* SYS_MEM op=0: malloc */
    if (ptr) {
        shell_puts("SYS_MEM malloc: ok", COL_OK);
        syscall(SYS_MEM, 1, (uint32_t)ptr, 0, 0);        /* SYS_MEM op=1: free */
        shell_puts("SYS_MEM free: ok", COL_OK);
    } else {
        shell_puts("SYS_MEM malloc: FAIL", COL_ERR);
    }
    int fd = (int)syscall(SYS_OPEN, (uint32_t)"TEST.TXT", 0, 0, 0);
    if (fd >= 0) {
        shell_puts("SYS_OPEN: ok", COL_OK);
        char buf[64] = {};
        int n = (int)syscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, 63, 0);
        if (n > 0) { buf[n] = 0; shell_puts("SYS_READ: ok", COL_OK); shell_puts(buf, COL_FG); }
        else shell_puts("SYS_READ: FAIL", COL_ERR);
        syscall(SYS_CLOSE, (uint32_t)fd, 0, 0, 0);
        shell_puts("SYS_CLOSE: ok", COL_OK);
    } else {
        shell_puts("SYS_OPEN: FAIL", COL_ERR);
    }
    uint32_t pid = syscall(SYS_PROC, 0, 0, 0, 0);   /* SYS_PROC op=0: getpid */
    char buf[32];
    ksnprintf(buf, sizeof(buf), "SYS_PROC getpid: %u", pid);
    shell_puts(buf, COL_OK);
}

static void cmd_exec(const char* args) {
    const char* path = (args && args[0] != 0) ? args : "test.elf";
    char full[256];
    build_path(full, path);
    shell_puts("loading...", COL_INFO);
    char err[64] = {};
    elf_exec(full, err, sizeof(err));
    if (err[0]) shell_puts(err, COL_ERR);
}

extern "C" void shell_update_cursor() {
    update_cursor();
}

extern "C" void shell_run(int start_y) {
    set_y(start_y);
    fat12_init(256);
    while (1) {
        char buf[CMD_BUF];
        shell_readline(buf, CMD_BUF);
        dispatch(buf);
        fb_present();
    }
}

extern "C" void serial_shell_init() {
    serial_puts("\r\n[WarkRune ring0 serial shell]\r\n[ring0]> ");
}

extern "C" void serial_shell_poll(char c) {
    static char buf[128];
    static int  len = 0;

    serial_putc(c);

    if (c == '\r' || c == '\n') {
        buf[len] = 0;
        serial_putc('\n');
        if (len > 0) {
            g_serial_mode = true;
            dispatch(buf);
            g_serial_mode = false;
        }
        len = 0;
        serial_puts("[ring0]> ");
    } else if ((c == 0x08 || c == 0x7F) && len > 0) {
        len--;
        serial_puts("\b \b");
    } else if (len < 127 && c >= ' ') {
        buf[len++] = c;
    }
}
