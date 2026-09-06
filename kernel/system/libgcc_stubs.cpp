typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int size_t;

extern "C" void* memcpy(void* dst, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

extern "C" void* memset(void* dst, int val, size_t n) {
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)val;
    return dst;
}

extern "C" uint64_t __udivmoddi4(uint64_t num, uint64_t den, uint64_t *rem_p) {
    uint64_t quot = 0, qbit = 1;

    if (den == 0) {
        if (rem_p) *rem_p = 0;
        return 0;
    }

    while ((int64_t)den >= 0) {
        den <<= 1;
        qbit <<= 1;
    }

    while (qbit > 0) {
        if (den <= num) {
            num -= den;
            quot += qbit;
        }
        den >>= 1;
        qbit >>= 1;
    }

    if (rem_p)
        *rem_p = num;

    return quot;
}

extern "C" uint64_t __udivdi3(uint64_t num, uint64_t den) {
    return __udivmoddi4(num, den, (uint64_t *)0);
}

extern "C" uint64_t __umoddi3(uint64_t num, uint64_t den) {
    uint64_t v;
    (void)__udivmoddi4(num, den, &v);
    return v;
}
