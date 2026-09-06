#pragma once
#include "types.h"
#include "compiler.h"

static INLINE void* mem_set(void* dst, u8 val, usize n) {
    u8* d = (u8*)dst;
    while (n--) *d++ = val;
    return dst;
}
static INLINE void* mem_copy(void* dst, const void* src, usize n) {
    u8* d       = (u8*)dst;
    const u8* s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dst;
}
static INLINE void* mem_move(void* dst, const void* src, usize n) {
    u8* d       = (u8*)dst;
    const u8* s = (const u8*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}
static INLINE i32 mem_cmp(const void* a, const void* b, usize n) {
    const u8* pa = (const u8*)a;
    const u8* pb = (const u8*)b;
    while (n--) {
        if (*pa != *pb) return (i32)*pa - (i32)*pb;
        pa++; pb++;
    }
    return 0;
}
static INLINE void mem_zero(void* dst, usize n) {
    u8* d = (u8*)dst;
    while (n--) *d++ = 0;
}
static INLINE bool mem_eq(const void* a, const void* b, usize n) {
    return mem_cmp(a, b, n) == 0;
}
static INLINE bool mem_is_zero(const void* s, usize n) {
    const u8* p = (const u8*)s;
    while (n--) if (*p++) return false;
    return true;
}

static INLINE void mem_set32(void* dst, u32 val, usize count) {
    u32* d = (u32*)dst;
    while (count--) *d++ = val;
}

#ifdef __cplusplus
template<typename T>
static INLINE void zero_obj(T& obj) {
    mem_zero(&obj, sizeof(T));
}
template<typename T>
static INLINE bool obj_eq(const T& a, const T& b) {
    return mem_eq(&a, &b, sizeof(T));
}
#endif
