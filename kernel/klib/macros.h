#pragma once
#include "types.h"

#define ARRAY_SIZE(x)           (sizeof(x) / sizeof((x)[0]))
#define OFFSET_OF(type, member) ((usize)&((type*)0)->member)
#define CONTAINER_OF(ptr, type, member) \
    ((type*)((u8*)(ptr) - OFFSET_OF(type, member)))

#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)        ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define ABS(x)                  ((x) < 0 ? -(x) : (x))

#define ALIGN_UP(x, a)          (((x) + (a) - 1) & ~((a) - 1))
#define ALIGN_DOWN(x, a)        ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a)        (((x) & ((a) - 1)) == 0)
#define PAGE_ALIGN(x)           ALIGN_UP(x, 4096)
#define IS_PAGE_ALIGNED(x)      IS_ALIGNED(x, 4096)

#define KB(x)                   ((usize)(x) * 1024)
#define MB(x)                   ((usize)(x) * 1024 * 1024)
#define GB(x)                   ((usize)(x) * 1024 * 1024 * 1024)

#define BIT(n)                  (1U << (n))
#define BIT64(n)                (1ULL << (n))
#define BIT_SET(x, n)           ((x) |=  BIT(n))
#define BIT_CLR(x, n)           ((x) &= ~BIT(n))
#define BIT_FLIP(x, n)          ((x) ^=  BIT(n))
#define BIT_CHECK(x, n)         (((x) >> (n)) & 1)

#define UNUSED(x)               ((void)(x))
#define STRINGIFY(x)            #x
#define CONCAT(a, b)            a##b

#define IN_RANGE(x, lo, hi)     ((x) >= (lo) && (x) <= (hi))
#define IS_POW2(x)              ((x) && !((x) & ((x) - 1)))
#define ROUND_UP(x, n)          (((x) + (n) - 1) / (n) * (n))
#define ROUND_DOWN(x, n)        ((x) / (n) * (n))

#define SWAP(a, b)              do { \
    __typeof__(a) _t = (a);          \
    (a) = (b); (b) = _t;             \
} while(0)

//kernel specific
#define PHYS_TO_VIRT(x)         ((x) + 0xC0000000)
#define VIRT_TO_PHYS(x)         ((x) - 0xC0000000)
#define IS_KERNEL_ADDR(x)       ((uptr)(x) >= 0xC0000000)
#define IS_USER_ADDR(x)         ((uptr)(x) <  0xC0000000)
