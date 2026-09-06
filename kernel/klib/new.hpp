#pragma once
#include "klib/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void  kmalloc_init();
void* kmalloc(usize size);
void  kfree(void* ptr);
void* krealloc(void* ptr, usize new_size);
usize ksize(void* ptr);

#ifdef __cplusplus
}
#endif
