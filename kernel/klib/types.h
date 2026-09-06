#pragma once

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;
typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;
typedef float               f32;
typedef double              f64;
typedef u8                  byte;
typedef u32                 usize;
typedef i32                 isize;
typedef u32                 uptr;
typedef i32                 iptr;

#define U8_MAX   ((u8)0xFF)
#define U16_MAX  ((u16)0xFFFF)
#define U32_MAX  ((u32)0xFFFFFFFFU)
#define U64_MAX  ((u64)0xFFFFFFFFFFFFFFFFULL)
#define I8_MIN   ((i8)-128)
#define I8_MAX   ((i8)127)
#define I16_MIN  ((i16)-32768)
#define I16_MAX  ((i16)32767)
#define I32_MIN  ((i32)-2147483648)
#define I32_MAX  ((i32)2147483647)

#ifndef NULL
    #ifdef __cplusplus
        #define NULL nullptr
    #else
        #define NULL ((void*)0)
    #endif
#endif

#ifndef __cplusplus
    typedef u8 bool;
    #define true  1
    #define false 0
#endif