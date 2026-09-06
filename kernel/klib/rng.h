#pragma once
#include "types.h"

static inline u32 _lcg_state_a() { static u32 v = 1125 ^ 11; return v; }
static inline u32 _lcg_state_b() { static u32 v = 16672 ^ 11; return v; }
static inline u32 _lcg_state_c() { static u32 v = 69420 ^ 11; return v; }

static inline void lcg_init() {
    u32 lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    u32 seed = lo ^ hi ^ 11;
    u32 a = 1125 ^ seed;
    u32 b = 16672 ^ seed;
    u32 c = 69420 ^ seed;
    for (int i = 0; i < 8; i++) {
        a = a * 1664525   + 1013904223;
        b = b * 22695477  + 1;
        c = c * 134775813 + 1;
    }
    _lcg_state_a() = a;
    _lcg_state_b() = b;
    _lcg_state_c() = c;
}

static inline u32 lcg_u32() {
    u32 a = _lcg_state_a() * 1664525   + 1013904223;
    u32 b = _lcg_state_b() * 22695477  + 1;
    u32 c = _lcg_state_c() * 134775813 + 1;
    _lcg_state_a() = a;
    _lcg_state_b() = b;
    _lcg_state_c() = c;
    u32 x = a ^ b ^ c;
    x ^= (x >> 13);
    x ^= (x << 17);
    x ^= (x >> 5);
    return x;
}

static inline u32 lcg_range(u32 lo, u32 hi) {
    if (lo >= hi) return lo;
    return lo + lcg_u32() % (hi - lo + 1);
}

static inline i32 lcg_range_i(i32 lo, i32 hi) {
    if (lo >= hi) return lo;
    return lo + (i32)(lcg_u32() % (u32)(hi - lo + 1));
}

static inline f32 lcg_f32() {
    return (f32)lcg_u32() / (f32)0xFFFFFFFFU;
}

static inline bool lcg_bool() {
    return (lcg_u32() & 1) == 1;
}

static inline u32 lcg_bits(u32 n) {
    if (n >= 32) return lcg_u32();
    return lcg_u32() & ((1U << n) - 1);
}
