#include "system/cpuid/cpuid.hpp"

static void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    asm volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

CPUInfo cpuid_get() {
    CPUInfo info = {};
    uint32_t eax, ebx, ecx, edx;

    cpuid(0, &eax, &ebx, &ecx, &edx);
    *(uint32_t*)(info.vendor + 0) = ebx;
    *(uint32_t*)(info.vendor + 4) = edx;
    *(uint32_t*)(info.vendor + 8) = ecx;
    info.vendor[12] = 0;

    cpuid(1, &eax, &ebx, &ecx, &edx);
    info.stepping = eax & 0xF;
    info.model    = (eax >> 4) & 0xF;
    info.family   = (eax >> 8) & 0xF;
    info.has_fpu  = edx & (1 << 0);
    info.has_apic = edx & (1 << 9);
    info.has_msr  = edx & (1 << 5);
    info.has_pae  = edx & (1 << 6);
    info.has_sse  = edx & (1 << 25);
    info.has_sse2 = edx & (1 << 26);
    info.has_sse3 = ecx & (1 << 0);
    info.has_avx  = ecx & (1 << 28);

    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        uint32_t* brand = (uint32_t*)info.brand;
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        info.brand[48] = 0;
    } else {
        info.brand[0] = 0;
    }

    return info;
}