#pragma once
#include "types.h"
#include "compiler.h"

static INLINE usize str_len(const char* s) {
    usize n = 0;
    while (*s++) n++;
    return n;
}
static INLINE bool str_eq(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}
static INLINE i32 str_cmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}
static INLINE i32 str_ncmp(const char* a, const char* b, usize n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (u8)*a - (u8)*b;
}
static INLINE usize str_copy(char* dst, const char* src, usize cap) {
    usize i = 0;
    if (!cap) return 0;
    while (i < cap - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return i;
}
static INLINE usize str_cat(char* dst, const char* src, usize cap) {
    usize i = str_len(dst);
    while (i < cap - 1 && *src) dst[i++] = *src++;
    dst[i] = 0;
    return i;
}
static INLINE bool str_starts(const char* s, const char* prefix) {
    while (*prefix) if (*s++ != *prefix++) return false;
    return true;
}
static INLINE bool str_ends(const char* s, const char* suffix) {
    usize sl = str_len(s), xl = str_len(suffix);
    if (xl > sl) return false;
    return str_eq(s + sl - xl, suffix);
}
static INLINE const char* str_find(const char* s, char c) {
    while (*s) { if (*s == c) return s; s++; }
    return 0;
}
static INLINE const char* str_rfind(const char* s, char c) {
    const char* last = 0;
    while (*s) { if (*s == c) last = s; s++; }
    return last;
}
static INLINE bool str_empty(const char* s) {
    return !s || !*s;
}
static INLINE void str_upper(char* s) {
    while (*s) { if (*s>='a'&&*s<='z') *s-=32; s++; }
}
static INLINE void str_lower(char* s) {
    while (*s) { if (*s>='A'&&*s<='Z') *s+=32; s++; }
}
static INLINE usize str_count(const char* s, char c) {
    usize n = 0;
    while (*s) if (*s++ == c) n++;
    return n;
}
static INLINE i32 str_to_int(const char* s) {
    i32 n = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        i32 d = *s++ - '0';
        if (n > (I32_MAX - d) / 10) { n = sign == 1 ? I32_MAX : I32_MIN; break; }
        n = n * 10 + d;
    }
    return n * sign;
}
static INLINE u32 str_to_hex(const char* s) {
    if (s[0]=='0' && (s[1]=='x'||s[1]=='X')) s += 2;
    u32 n = 0;
    while (1) {
        char c = *s++;
        if      (c >= '0' && c <= '9') { if (n > (U32_MAX - (u32)(c-'0')) / 16) { n = U32_MAX; break; } n = n*16 + (c-'0'); }
        else if (c >= 'a' && c <= 'f') { if (n > (U32_MAX - (u32)(c-'a'+10)) / 16) { n = U32_MAX; break; } n = n*16 + (c-'a'+10); }
        else if (c >= 'A' && c <= 'F') { if (n > (U32_MAX - (u32)(c-'A'+10)) / 16) { n = U32_MAX; break; } n = n*16 + (c-'A'+10); }
        else break;
    }
    return n;
}