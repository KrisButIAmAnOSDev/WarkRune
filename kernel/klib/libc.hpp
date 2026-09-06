#pragma once
#include "fs/fat12.hpp"
#include "memory/kmalloc.hpp"

struct FILE { int fd; };
static inline FILE _fpool[16];

inline FILE* fopen(const char* path, const char*) {
    if (path[0] == '/') path++;
    int fd = fat12_open(path);
    if (fd < 0) return nullptr;
    _fpool[fd].fd = fd;
    return &_fpool[fd];
}

inline int fread(void* buf, int size, int count, FILE* f) {
    if (size <= 0 || count <= 0) return 0;
    uint32_t bytes = (uint32_t)size * (uint32_t)count;
    if (bytes / (uint32_t)size != (uint32_t)count) return 0;
    return fat12_read(f->fd, buf, bytes) / size;
}

inline void fclose(FILE* f) { fat12_close(f->fd); }
inline int fseek(FILE*, long, int) { return 0; }
inline long ftell(FILE*) { return 0; }
inline int feof(FILE*) { return 0; }
inline int fprintf(FILE*, const char*, ...) { return 0; }
inline void exit(int) { asm volatile("cli; hlt"); }
