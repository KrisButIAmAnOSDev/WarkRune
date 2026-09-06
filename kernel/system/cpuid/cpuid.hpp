#pragma once
#include <stdint.h>

struct CPUInfo {
    char vendor[13];
    char brand[49];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    bool has_fpu;
    bool has_sse;
    bool has_sse2;
    bool has_sse3;
    bool has_avx;
    bool has_apic;
    bool has_msr;
    bool has_pae;
};

CPUInfo cpuid_get();