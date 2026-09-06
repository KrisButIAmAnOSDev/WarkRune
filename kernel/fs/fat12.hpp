#pragma once
#include <stdint.h>

#define FAT12_MAX_FDS 16

bool fat12_init(uint32_t lba_start);
int  fat12_open(const char* path);
int  fat12_create(const char* path);
int  fat12_read(int fd, void* buf, uint32_t size);
int  fat12_write(int fd, const void* buf, uint32_t size);
void fat12_flush(int fd);
void fat12_close(int fd);
int  fat12_size(int fd);
bool fat12_delete(const char* path);
bool fat12_rename(const char* old_path, const char* new_path);
bool fat12_mkdir(const char* path);
bool fat12_rmdir(const char* path);
bool fat12_ls(const char* path, void (*cb)(const char* name, uint32_t size, bool is_dir));
