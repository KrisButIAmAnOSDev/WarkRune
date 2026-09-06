#pragma once
#include <stdint.h>

#define SHMEM_MAX 16

void* shmem_create(const char* name, uint32_t size);
void* shmem_open(const char* name);
void  shmem_close(const char* name);
void  shmem_init();
