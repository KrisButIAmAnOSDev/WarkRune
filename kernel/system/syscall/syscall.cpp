#include "system/syscall/syscall.hpp"
#include "Graphics/text.hpp"
#include "Graphics/graphics.hpp"
#include "memory/kmalloc.hpp"
#include "memory/pmm/pmm.hpp"
#include "memory/paging/paging.hpp"
#include "system/idt/idt.hpp"
#include "system/rtc.hpp"
#include "system/pit/pit.hpp"
#include "system/ipc/fd.hpp"
#include "fs/fat12.hpp"
#include "fs/elf/elf.hpp"
#include "io/keyboard.hpp"
#include "io/mouse/mouse.hpp"
#include "io/serial/serial.hpp"
#include "shell/shell.hpp"
#include "klib/color.hpp"
#include "system/sched/sched.hpp"
#include "memory/shmem/shmem.hpp"
#include "system/acpi/acpi.hpp"

#define SCREEN_H 600
#define USER_SPACE_END 0x80000000

static inline bool valid_user_ptr(uint32_t p, uint32_t len = 0) {
    if (p >= USER_SPACE_END || p == 0) return false;
    if (len > 0 && len > USER_SPACE_END - p) return false;
    return true;
}

/* ── Path helper ──────────────────────────────────────────────────────── */
static char s_kpath[256];
static void copy_kpath(const char* upath) {
    int i = 0;
    while (i < 255 && upath[i]) { s_kpath[i] = upath[i]; i++; }
    s_kpath[i] = 0;
}

/* ── ls callback state ────────────────────────────────────────────────── */
static char* s_ls_buf;
static int   s_ls_cap;
static int   s_ls_pos;
static void ls_cb(const char* name, uint32_t /*size*/, bool is_dir) {
    if (s_ls_pos >= s_ls_cap - 2) return;
    if (is_dir) s_ls_buf[s_ls_pos++] = '/';
    for (int i = 0; name[i] && s_ls_pos < s_ls_cap - 2; i++)
        s_ls_buf[s_ls_pos++] = name[i];
    if (s_ls_pos < s_ls_cap - 2) s_ls_buf[s_ls_pos++] = '\n';
}

/* ── Serial helpers ───────────────────────────────────────────────────── */
static void serial_int(int n) {
    if (n == 0) { serial_putc('0'); return; }
    if (n < 0) { serial_putc('-'); if (n == (-2147483647 - 1)) { serial_puts("2147483648"); return; } n = -n; }
    char tmp[12]; int i = 0;
    while (n) { tmp[i++] = '0' + n % 10; n /= 10; }
    while (i-- > 0) serial_putc(tmp[i]);
}

/* ── RTC formatter ────────────────────────────────────────────────────── */
static void rtc_format(char* buf, int cap) {
    if (!buf || cap < 20) return;
    RTCTime t = rtc_get();
    buf[0]  = '0' + t.day / 10;    buf[1]  = '0' + t.day % 10;
    buf[2]  = '/';
    buf[3]  = '0' + t.month / 10;  buf[4]  = '0' + t.month % 10;
    buf[5]  = '/';
    buf[6]  = '2'; buf[7] = '0';
    buf[8]  = '0' + t.year / 10;   buf[9]  = '0' + t.year % 10;
    buf[10] = ' ';
    buf[11] = '0' + t.hour / 10;   buf[12] = '0' + t.hour % 10;
    buf[13] = ':';
    buf[14] = '0' + t.min / 10;    buf[15] = '0' + t.min % 10;
    buf[16] = ':';
    buf[17] = '0' + t.sec / 10;    buf[18] = '0' + t.sec % 10;
    buf[19] = 0;
    (void)cap;
}

/* ══════════════════════════════════════════════════════════════════════
   Main syscall dispatcher  –  signature extended to 5 args (num,a,b,c,d)
   ══════════════════════════════════════════════════════════════════════ */
extern "C" uint32_t syscall_handler(uint32_t num,
                                     uint32_t a, uint32_t b,
                                     uint32_t c, uint32_t d) {
    switch (num) {

    /* ── 0: SYS_PRINT ──────────────────────────────────────────────── */
    case SYS_PRINT:
        if (!valid_user_ptr(a)) return (uint32_t)-1;
        if (get_y() + 10 > SCREEN_H) { fill_screen(COL_BG); set_y(0); }
        print((const char*)a, b);
        return 0;

    /* ── 1: SYS_EXIT ───────────────────────────────────────────────── */
    case SYS_EXIT:
        elf_exit();
        return 0;

    /* ── 2: SYS_READ ───────────────────────────────────────────────── */
    case SYS_READ:
        if (!valid_user_ptr(b, c)) return (uint32_t)-1;
        return (uint32_t)fd_read((int)a, (void*)b, c);

    /* ── 3: SYS_WRITE ──────────────────────────────────────────────── */
    case SYS_WRITE:
        if (!valid_user_ptr(b, c)) return (uint32_t)-1;
        return (uint32_t)fd_write((int)a, (const void*)b, c);

    /* ── 4: SYS_OPEN ───────────────────────────────────────────────── */
    case SYS_OPEN: {
        if (!valid_user_ptr(a)) return (uint32_t)-1;
        copy_kpath((const char*)a);
        serial_puts("[open] '"); serial_puts(s_kpath); serial_puts("' -> ");
        int r = fd_open(s_kpath);
        serial_int(r); serial_putc('\n');
        return (uint32_t)r;
    }

    /* ── 5: SYS_CLOSE ──────────────────────────────────────────────── */
    case SYS_CLOSE:
        fd_close((int)a);
        return 0;

    /* ── 6: SYS_READ_KEY ───────────────────────────────────────────── */
    case SYS_READ_KEY: {
        char k = 0;
        while ((k = read_key()) == 0) {
            if (serial_ready()) serial_shell_poll(serial_getc());
            shell_update_cursor();
        }
        return (uint32_t)(uint8_t)k;
    }

    /* ── 7: SYS_PIPE ───────────────────────────────────────────────── */
    case SYS_PIPE: {
        int* fds_out = (int*)a;
        if (!fds_out || !valid_user_ptr(a, 8)) return (uint32_t)-1;
        int rfd, wfd;
        if (!fd_pipe(&rfd, &wfd)) return (uint32_t)-1;
        fds_out[0] = rfd; fds_out[1] = wfd;
        return 0;
    }

    /* ── 8: SYS_MOUSE ──────────────────────────────────────────────── */
    case SYS_MOUSE: {
        MouseState* out = (MouseState*)a;
        if (!out || !valid_user_ptr(a, sizeof(MouseState))) return (uint32_t)-1;
        *out = mouse_get();
        return 0;
    }

    /* ── 9: SYS_SHMEM ──────────────────────────────────────────────── */
    case SYS_SHMEM: {
        const char* name = (const char*)a;
        uint32_t size = b, op = c;
        if (!name || !valid_user_ptr(a)) return 0;
        if (op == 0) return (uint32_t)shmem_create(name, size);
        if (op == 1) return (uint32_t)shmem_open(name);
        if (op == 2) { shmem_close(name); return 0; }
        return 0;
    }

    /* ── 10: SYS_MEM ───────────────────────────────────────────────── */
    case SYS_MEM:
        if (a == 0) return (uint32_t)kmalloc(b);
        if (a == 1) { kfree((void*)b); return 0; }
        if (a == 2) {
            if (!valid_user_ptr(b, 8)) return (uint32_t)-1;
            kmem_info((uint32_t*)b, (uint32_t*)c);
            return 0;
        }
        if (a == 3) {
            if (b && !valid_user_ptr(b, 4)) return (uint32_t)-1;
            if (c && !valid_user_ptr(c, 4)) return (uint32_t)-1;
            if (b) *(uint32_t*)b = pmm_used() * 4;
            if (c) *(uint32_t*)c = pmm_free_count() * 4;
            return 0;
        }
        return (uint32_t)-1;

    /* ── 11: SYS_PROC ──────────────────────────────────────────────── */
    case SYS_PROC:
        if (a == 0) return sched_current_task() ? sched_current_task()->id : 1;
        if (a == 1) return sched_current_task() ? sched_current_task()->id : 1; /* fork stub */
        if (a == 2) return 0;                       /* wait stub */
        if (a == 3) { sched_kill_task(b); return 0; }
        if (a == 4) { sched_yield(); return 0; }
        return (uint32_t)-1;

    /* ── 12: SYS_POWER ─────────────────────────────────────────────── */
    case SYS_POWER:
        if (a == 0) { acpi_shutdown(); return 0; }
        if (a == 1) { acpi_reboot();   return 0; }
        return 0;

    /* ── 13: SYS_TIME ──────────────────────────────────────────────── */
    case SYS_TIME:
        if (a == 0) return (uint32_t)(pit_uptime_ms() / 1000);
        if (a == 1) {
            if (!valid_user_ptr(b, (uint32_t)c)) return (uint32_t)-1;
            rtc_format((char*)b, (int)c); return 0;
        }
        if (a == 2) { pit_sleep(b); return 0; }
        return (uint32_t)-1;

    /* ── 14: SYS_FS ────────────────────────────────────────────────── */
    case SYS_FS:
        if (a == 0) {
            if (!valid_user_ptr(b) || !valid_user_ptr(c, (uint32_t)d)) return (uint32_t)-1;
            copy_kpath((const char*)b);
            s_ls_buf = (char*)c;
            s_ls_cap = (int)d;
            s_ls_pos = 0;
            fat12_ls(s_kpath, ls_cb);
            if (s_ls_pos < s_ls_cap) s_ls_buf[s_ls_pos] = 0;
            return (uint32_t)s_ls_pos;
        }
        if (a == 1) {                               /* mkdir(path) */
            if (!valid_user_ptr(b)) return (uint32_t)-1;
            copy_kpath((const char*)b);
            serial_puts("[mkdir] '"); serial_puts(s_kpath); serial_puts("' -> ");
            bool ok = fat12_mkdir(s_kpath);
            serial_puts(ok ? "ok\n" : "fail\n");
            return ok ? 0 : (uint32_t)-1;
        }
        if (a == 2) {                               /* rm(path) */
            if (!valid_user_ptr(b)) return (uint32_t)-1;
            copy_kpath((const char*)b);
            serial_puts("[rm] '"); serial_puts(s_kpath); serial_puts("' -> ");
            bool ok = fat12_delete(s_kpath);
            serial_puts(ok ? "ok\n" : "fail\n");
            return ok ? 0 : (uint32_t)-1;
        }
        if (a == 3) {                               /* rmdir(path) */
            if (!valid_user_ptr(b)) return (uint32_t)-1;
            copy_kpath((const char*)b);
            serial_puts("[rmdir] '"); serial_puts(s_kpath); serial_puts("' -> ");
            bool ok = fat12_rmdir(s_kpath);
            serial_puts(ok ? "ok\n" : "fail\n");
            return ok ? 0 : (uint32_t)-1;
        }
        if (a == 4) {                               /* create(path) */
            if (!valid_user_ptr(b)) return (uint32_t)-1;
            copy_kpath((const char*)b);
            serial_puts("[create] '"); serial_puts(s_kpath); serial_puts("' -> ");
            int fd = fd_create(s_kpath);
            serial_int(fd); serial_putc('\n');
            if (fd >= 0) fd_close(fd);
            return (fd >= 0) ? 0 : (uint32_t)-1;
        }
        return (uint32_t)-1;

    /* ── 15: SYS_DRAW ──────────────────────────────────────────────── */
    case SYS_DRAW:
        if (a == 0) { fill_screen(COL_BG); set_y(0); return 0; }
        if (a == 1) { if (vbe_bpp) fb_present(); return 0; }
        if (a == 2) {                               /* rect: b=x|(y<<16), c=w|(h<<16), d=col */
            draw_rect((int)(b & 0xFFFF), (int)(b >> 16),
                      (int)(c & 0xFFFF), (int)(c >> 16), d);
            return 0;
        }
        if (a == 3) {                               /* char: b=ch, c=x|(y<<16), d=col */
            char tmp[2] = { (char)b, 0 };
            printxy(tmp, (int)(c & 0xFFFF), (int)(c >> 16), d);
            return 0;
        }
        if (a == 4) {                               /* printxy: b=str, c=x|(y<<16), d=col */
            printxy((const char*)b, (int)(c & 0xFFFF), (int)(c >> 16), d);
            return 0;
        }
        if (a == 5) return (uint32_t)get_y();
        if (a == 6) { set_y((int)b); return 0; }
        return (uint32_t)-1;

    /* ── 16: SYS_EXEC ──────────────────────────────────────────────── */
    case SYS_EXEC: {
        if (!valid_user_ptr(b)) return (uint32_t)-1;
        copy_kpath((const char*)b);
        char err[64] = {};
        elf_exec(s_kpath, err, sizeof(err));
        if (err[0]) serial_puts(err);
        return 0;
    }

    default:
        return (uint32_t)-1;
    }
}

extern "C" void idt_set_gate(int, uint32_t, int, int);
extern "C" void syscall_stub();

void syscall_init() {
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEE);
}

uint32_t syscall(uint32_t num, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return syscall_handler(num, a, b, c, d);
}
