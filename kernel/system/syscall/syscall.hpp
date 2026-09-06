#pragma once
#include <stdint.h>

/* ── Core single-purpose syscalls (0-9) ─────────────────────────────── */
#define SYS_PRINT      0   /* print(str, color) */
#define SYS_EXIT       1   /* exit() */
#define SYS_READ       2   /* read(fd, buf, n) */
#define SYS_WRITE      3   /* write(fd, buf, n) */
#define SYS_OPEN       4   /* open(path) -> fd */
#define SYS_CLOSE      5   /* close(fd) */
#define SYS_READ_KEY   6   /* read_key() -> char */
#define SYS_PIPE       7   /* pipe(fds[2]) */
#define SYS_MOUSE      8   /* mouse(MouseState*) */
#define SYS_SHMEM      9   /* shmem(name, size, op) */

/* ── Consolidated multiplexed syscalls (10-16) ──────────────────────── */
#define SYS_MEM       10   /* mem(op, …) — malloc/free/kmem_info/pmm_info */
#define SYS_PROC      11   /* proc(op, …) — getpid/fork/wait/kill/yield */
#define SYS_POWER     12   /* power(op) — shutdown/reboot */
#define SYS_TIME      13   /* time(op, …) — uptime/rtc/sleep */
#define SYS_FS        14   /* fs(op, path, …) — ls/mkdir/rm/rmdir/create */
#define SYS_DRAW      15   /* draw(op, …) — clear/flip/rect/char/printxy/get_y/set_y */
#define SYS_EXEC      16   /* exec(op, path) — run ELF */

void     syscall_init();
uint32_t syscall(uint32_t num, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
