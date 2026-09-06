#pragma once

#include "klib/types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void  kmalloc_init();
void* kmalloc(usize size);
void  kfree(void* ptr);
void* krealloc(void* ptr, usize new_size);
usize ksize(void* ptr);
void  kmem_info(uint32_t* used_out, uint32_t* free_out);

#ifdef __cplusplus
}
#endif
