#include "system/ipc/fd.hpp"
#include "fs/fat12.hpp"
#include "io/keyboard.hpp"
#include "io/serial/serial.hpp"
#include "Graphics/text.hpp"
#include "Graphics/graphics.hpp"
#include "klib/color.hpp"

extern "C" void* memcpy(void* dst, const void* src, unsigned int n);

static FileDesc fds[FD_MAX];
static Pipe     pipes[PIPE_MAX];

void fd_init() {
    for (int i = 0; i < FD_MAX; i++)
        fds[i] = { false, FD_NONE, -1, -1 };
    for (int i = 0; i < PIPE_MAX; i++) {
        pipes[i].used         = false;
        pipes[i].read_pos     = 0;
        pipes[i].write_pos    = 0;
        pipes[i].count        = 0;
        pipes[i].write_closed = false;
    }
    fds[0] = { true, FD_STDIN,  -1, -1 };
    fds[1] = { true, FD_STDOUT, -1, -1 };
    fds[2] = { true, FD_STDERR, -1, -1 };
}

static int alloc_fd() {
    for (int i = 3; i < FD_MAX; i++)
        if (!fds[i].used) return i;
    return -1;
}

static int alloc_pipe() {
    for (int i = 0; i < PIPE_MAX; i++)
        if (!pipes[i].used) return i;
    return -1;
}

bool fd_pipe(int* read_fd, int* write_fd) {
    int pid = alloc_pipe();
    if (pid < 0) return false;

    int rfd = alloc_fd();
    if (rfd < 0) return false;
    fds[rfd].used = true;

    int wfd = alloc_fd();
    if (wfd < 0) { fds[rfd].used = false; return false; }

    pipes[pid].used         = true;
    pipes[pid].read_pos     = 0;
    pipes[pid].write_pos    = 0;
    pipes[pid].count        = 0;
    pipes[pid].write_closed = false;

    fds[rfd] = { true, FD_PIPE_READ,  -1, pid };
    fds[wfd] = { true, FD_PIPE_WRITE, -1, pid };

    *read_fd  = rfd;
    *write_fd = wfd;
    return true;
}

int fd_open(const char* path) {
    int fat = fat12_open(path);
    if (fat < 0) return -1;
    int fd = alloc_fd();
    if (fd < 0) { fat12_close(fat); return -1; }
    fds[fd] = { true, FD_FILE, fat, -1 };
    return fd;
}

int fd_create(const char* path) {
    int fat = fat12_create(path);
    if (fat < 0) return -1;
    int fd = alloc_fd();
    if (fd < 0) { fat12_close(fat); return -1; }
    fds[fd] = { true, FD_FILE, fat, -1 };
    return fd;
}

int fd_create_case(const char* path) {
    int fd = fd_create(path);
    if (fd >= 0) return fd;
    bool has_lower = false;
    for (int i = 0; path[i]; i++) {
        if (path[i] >= 'a' && path[i] <= 'z') { has_lower = true; break; }
    }
    if (!has_lower) return -1;
    char up[256];
    int n = 0;
    while (path[n] && n < 255) {
        char c = path[n];
        up[n] = (c >= 'a' && c <= 'z') ? c - 32 : c;
        n++;
    }
    up[n] = 0;
    return fd_create(up);
}

int fd_read(int fd, void* buf, uint32_t size) {
    if (fd < 0 || fd >= FD_MAX || !fds[fd].used) return -1;
    uint8_t* dst = (uint8_t*)buf;

    switch (fds[fd].type) {
        case FD_STDIN: {
            uint32_t n = 0;
            while (n < size) {
                char k = 0;
                if (serial_ready()) k = serial_getc();
                else k = read_key();
                if (k == 0) continue;
                dst[n++] = (uint8_t)k;
                if (k == '\r' || k == '\n') break;
            }
            return (int)n;
        }
        case FD_PIPE_READ: {
            Pipe& p = pipes[fds[fd].pipe_id];
            if (p.count == 0 && p.write_closed) return 0;
            asm volatile("cli");
            uint32_t n = 0;
            while (n < size && p.count > 0) {
                dst[n++]   = p.buf[p.read_pos];
                p.read_pos = (p.read_pos + 1) % PIPE_BUF;
                p.count--;
            }
            asm volatile("sti");
            return (int)n;
        }
        case FD_FILE:
            return fat12_read(fds[fd].fat_fd, buf, size);
        default:
            return -1;
    }
}

int fd_write(int fd, const void* buf, uint32_t size) {
    if (fd < 0 || fd >= FD_MAX || !fds[fd].used) return -1;
    const uint8_t* src = (const uint8_t*)buf;

    switch (fds[fd].type) {
        case FD_STDOUT:
        case FD_STDERR: {
            char kbuf[257];
            uint32_t n = size < 256 ? size : 256;
            memcpy(kbuf, buf, n);
            kbuf[n] = 0;
            print(kbuf, fds[fd].type == FD_STDERR ? COL_ERR : COL_FG);
            serial_puts(kbuf);
            return (int)size;
        }
        case FD_PIPE_WRITE: {
            Pipe& p = pipes[fds[fd].pipe_id];
            if (p.write_closed) return -1;
            asm volatile("cli");
            uint32_t n = 0;
            while (n < size && p.count < PIPE_BUF) {
                p.buf[p.write_pos] = src[n++];
                p.write_pos = (p.write_pos + 1) % PIPE_BUF;
                p.count++;
            }
            asm volatile("sti");
            return (int)n;
        }
        case FD_FILE:
            return fat12_write(fds[fd].fat_fd, buf, size);
        default:
            return -1;
    }
}

void fd_flush(int fd) {
    if (fd < 0 || fd >= FD_MAX || !fds[fd].used) return;
    if (fds[fd].type == FD_FILE)
        fat12_flush(fds[fd].fat_fd);
}

void fd_close(int fd) {
    if (fd < 0 || fd >= FD_MAX || !fds[fd].used) return;
    if (fds[fd].type == FD_FILE)
        fat12_close(fds[fd].fat_fd);
    if (fds[fd].type == FD_PIPE_WRITE && fds[fd].pipe_id >= 0) {
        pipes[fds[fd].pipe_id].write_closed = true;
        bool any_reader = false;
        for (int i = 0; i < FD_MAX; i++)
            if (fds[i].used && fds[i].type == FD_PIPE_READ
                && fds[i].pipe_id == fds[fd].pipe_id)
                any_reader = true;
        if (!any_reader)
            pipes[fds[fd].pipe_id].used = false;
    }
    if (fds[fd].type == FD_PIPE_READ && fds[fd].pipe_id >= 0) {
        bool any_writer = false;
        for (int i = 0; i < FD_MAX; i++)
            if (fds[i].used && fds[i].type == FD_PIPE_WRITE
                && fds[i].pipe_id == fds[fd].pipe_id)
                any_writer = true;
        if (!any_writer)
            pipes[fds[fd].pipe_id].used = false;
    }
    fds[fd] = { false, FD_NONE, -1, -1 };
}

int fd_size(int fd) {
    if (fd < 0 || fd >= FD_MAX || !fds[fd].used) return -1;
    if (fds[fd].type == FD_FILE)
        return fat12_size(fds[fd].fat_fd);
    return -1;
}
