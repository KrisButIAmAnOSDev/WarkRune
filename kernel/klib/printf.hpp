#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Minimal kernel printf — supports %d %u %x %X %s %c %llu %llx %%, width, pad, left-align

static inline void _kputc(char* buf, int* pos, int cap, char c) {
    if (*pos < cap) buf[*pos] = c;
    (*pos)++;
}

static inline void _kputs(char* buf, int* pos, int cap, const char* s, int len, int width, int left_align, char pad) {
    int printed = 0;
    if (!left_align) {
        while (printed < width - len) { _kputc(buf, pos, cap, pad); printed++; }
    }
    for (int i = 0; i < len; i++) _kputc(buf, pos, cap, s[i]);
    printed += len;
    if (left_align) {
        while (printed < width) { _kputc(buf, pos, cap, ' '); printed++; }
    }
}

static inline int _kfmt_ull(char* out, unsigned long long val, int base, int upper) {
    char tmp[21];
    int len = 0;
    if (val == 0) { out[0] = '0'; return 1; }
    while (val) {
        unsigned d = val % base;
        tmp[len++] = d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10;
        val /= base;
    }
    for (int i = 0; i < len; i++) out[i] = tmp[len - 1 - i];
    return len;
}

static inline int _kfmt_ll(char* out, long long val) {
    if (val < 0) { out[0] = '-'; return 1 + _kfmt_ull(out + 1, (unsigned long long)(-(val + 1)) + 1, 10, 0); }
    return _kfmt_ull(out, (unsigned long long)val, 10, 0);
}

static inline int kvsprintf(char* buf, int cap, const char* fmt, va_list ap) {
    int pos = 0;
    if (cap <= 0) return 0;

    while (*fmt && pos < cap - 1) {
        if (*fmt != '%') { _kputc(buf, &pos, cap, *fmt); fmt++; continue; }
        fmt++; // skip '%'

        // parse flags
        int left_align = 0;
        char pad = ' ';
        if (*fmt == '-') { left_align = 1; fmt++; }
        if (*fmt == '0') { pad = '0'; fmt++; }

        // parse width
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }

        // parse length modifier
        int ll = 0;
        if (*fmt == 'l') { fmt++; if (*fmt == 'l') { ll = 1; fmt++; } }

        char tmp[21];
        int len = 0;

        switch (*fmt) {
        case 'd': case 'i':
            if (ll) len = _kfmt_ll(tmp, va_arg(ap, long long));
            else    len = _kfmt_ll(tmp, va_arg(ap, int));
            _kputs(buf, &pos, cap, tmp, len, width, left_align, pad);
            break;
        case 'u':
            if (ll) len = _kfmt_ull(tmp, va_arg(ap, unsigned long long), 10, 0);
            else    len = _kfmt_ull(tmp, va_arg(ap, unsigned int), 10, 0);
            _kputs(buf, &pos, cap, tmp, len, width, left_align, pad);
            break;
        case 'x':
            if (ll) len = _kfmt_ull(tmp, va_arg(ap, unsigned long long), 16, 0);
            else    len = _kfmt_ull(tmp, va_arg(ap, unsigned int), 16, 0);
            _kputs(buf, &pos, cap, tmp, len, width, left_align, pad);
            break;
        case 'X':
            if (ll) len = _kfmt_ull(tmp, va_arg(ap, unsigned long long), 16, 1);
            else    len = _kfmt_ull(tmp, va_arg(ap, unsigned int), 16, 1);
            _kputs(buf, &pos, cap, tmp, len, width, left_align, pad);
            break;
        case 's': {
            const char* s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            len = 0; while (s[len]) len++;
            _kputs(buf, &pos, cap, s, len, width, left_align, ' ');
            break;
        }
        case 'c':
            _kputc(buf, &pos, cap, (char)va_arg(ap, int));
            break;
        case '%':
            _kputc(buf, &pos, cap, '%');
            break;
        default:
            _kputc(buf, &pos, cap, '%');
            _kputc(buf, &pos, cap, *fmt);
            break;
        }
        fmt++;
    }
    if (pos < cap) buf[pos] = '\0';
    else buf[cap - 1] = '\0';
    return pos;
}

static inline int ksprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsprintf(buf, 0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return r;
}

static inline int ksnprintf(char* buf, int cap, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsprintf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}
