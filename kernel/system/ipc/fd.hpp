#pragma once
#include <stdint.h>

#define FD_MAX   32
#define PIPE_BUF 512
#define PIPE_MAX 8

enum FDType {
    FD_NONE,
    FD_STDIN,
    FD_STDOUT,
    FD_STDERR,
    FD_FILE,
    FD_PIPE_READ,
    FD_PIPE_WRITE
};

struct Pipe {
    bool    used;
    uint8_t buf[PIPE_BUF];
    int     read_pos;
    int     write_pos;
    int     count;
    bool    write_closed;
};

struct FileDesc {
    bool   used;
    FDType type;
    int    fat_fd;
    int    pipe_id;
};

void fd_init();
int  fd_open(const char* path);
int  fd_create(const char* path);
int  fd_read(int fd, void* buf, uint32_t size);
int  fd_write(int fd, const void* buf, uint32_t size);
void fd_close(int fd);
void fd_flush(int fd);
int  fd_size(int fd);
bool fd_pipe(int* read_fd, int* write_fd);
