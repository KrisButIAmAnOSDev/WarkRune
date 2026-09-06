#pragma once

#define INLINE          __attribute__((always_inline)) inline
#define NOINLINE        __attribute__((noinline))
#define NORETURN        __attribute__((noreturn))
#define PACKED          __attribute__((packed))
#define ALIGNED(n)      __attribute__((aligned(n)))
#define UNUSED_ATTR     __attribute__((unused))
#define USED            __attribute__((used))
#define WEAK            __attribute__((weak))
#define NAKED           __attribute__((naked))
#define SECTION(s)      __attribute__((section(s)))
#define PURE            __attribute__((pure))
#define CONST_FUNC      __attribute__((const))
#define HOT             __attribute__((hot))
#define COLD            __attribute__((cold))
#define NODISCARD       __attribute__((warn_unused_result))

//branch prediction hints
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

//memory barriers
#define barrier()       asm volatile("" ::: "memory")
#define mb()            asm volatile("mfence" ::: "memory")
#define rmb()           asm volatile("lfence" ::: "memory")
#define wmb()           asm volatile("sfence" ::: "memory")

//volatile access
#define READ_ONCE(x)    (*(volatile __typeof__(x)*)&(x))
#define WRITE_ONCE(x,v) (*(volatile __typeof__(x)*)&(x) = (v))

//mmio
#define MMIO_READ8(addr)      (*((volatile u8*)(addr)))
#define MMIO_READ16(addr)     (*((volatile u16*)(addr)))
#define MMIO_READ32(addr)     (*((volatile u32*)(addr)))
#define MMIO_WRITE8(addr,v)   (*((volatile u8*)(addr))  = (v))
#define MMIO_WRITE16(addr,v)  (*((volatile u16*)(addr)) = (v))
#define MMIO_WRITE32(addr,v)  (*((volatile u32*)(addr)) = (v))

//compile time
#define STATIC_ASSERT(c, msg) typedef char __sa_##__LINE__[(c) ? 1 : -1]
#define ASSERT_SIZE(t, s)     STATIC_ASSERT(sizeof(t) == s, "wrong size")
#define ASSERT_ALIGN(t, a)    STATIC_ASSERT(__alignof__(t) == a, "wrong align")

#ifdef __cplusplus
    #define EXTERN_C       extern "C"
    #define EXTERN_C_BEGIN extern "C" {
    #define EXTERN_C_END   }
#else
    #define EXTERN_C
    #define EXTERN_C_BEGIN
    #define EXTERN_C_END
#endif