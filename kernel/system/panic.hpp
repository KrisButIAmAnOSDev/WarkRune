#pragma once
#include "../Graphics/text.hpp"

inline void _panic_print_hex(uint32_t val, char* buf) {
    buf[0]='0'; buf[1]='x';
    for (int i = 7; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        buf[9 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[10] = 0;
}

inline void _panic_print_int(int val, char* buf) {
    if (val == 0) { buf[0]='0'; buf[1]=0; return; }
    char tmp[16]; int i = 0;
    if (val < 0) { buf[0]='-'; val = (val == (-2147483647 - 1)) ? 2147483647 : -val; i=1; }
    int start = i;
    while (val > 0) { tmp[i - start] = '0' + (val % 10); val /= 10; i++; }
    for (int j = start; j < i; j++) buf[j] = tmp[j - start];
    buf[i] = 0;
}

extern uint64_t get_heap_base();
extern uint64_t get_heap_size();
extern uint16_t* e820_count;

#define PANIC(msg) do { \
    asm volatile("cli"); \
    fill_screen(0x1A0000); \
    set_y(0); \
    print("kernel panic",         0xFF5555); \
    print("--------------------", 0x333350); \
    print("reason:",              0xFFCC00); \
    print(msg,                    0xFFFFFF); \
    print("--------------------", 0x333350); \
    print("file:",                0xFFCC00); \
    print(__FILE__,               0xFFFFFF); \
    print("function:",            0xFFCC00); \
    print(__func__,               0xFFFFFF); \
    print("line:",                0xFFCC00); \
    char _lb[16]; _panic_print_int(__LINE__, _lb); \
    print(_lb,                    0xFFFFFF); \
    print("--------------------", 0x333350); \
    print("-- memory --",         0xFFCC00); \
    char _hb[12]; _panic_print_hex((uint32_t)get_heap_base(), _hb); \
    char _hbuf[32]; \
    _hbuf[0]='h';_hbuf[1]='e';_hbuf[2]='a';_hbuf[3]='p';_hbuf[4]=' '; \
    _hbuf[5]='b';_hbuf[6]='a';_hbuf[7]='s';_hbuf[8]='e';_hbuf[9]=':';_hbuf[10]=' ';_hbuf[11]=0; \
    char _hbuf2[32]; \
    for(int _ci=0;_hbuf[_ci];_ci++) _hbuf2[_ci]=_hbuf[_ci]; \
    int _hbl=11; for(int _ci=0;_hb[_ci];_ci++) _hbuf2[_hbl++]=_hb[_ci]; _hbuf2[_hbl]=0; \
    print(_hbuf2,                 0x888899); \
    char _hs[12]; _panic_print_hex((uint32_t)get_heap_size(), _hs); \
    char _hsbuf[32]; \
    _hsbuf[0]='h';_hsbuf[1]='e';_hsbuf[2]='a';_hsbuf[3]='p';_hsbuf[4]=' '; \
    _hsbuf[5]='s';_hsbuf[6]='i';_hsbuf[7]='z';_hsbuf[8]='e';_hsbuf[9]=':';_hsbuf[10]=' ';_hsbuf[11]=0; \
    char _hsbuf2[32]; \
    for(int _ci=0;_hsbuf[_ci];_ci++) _hsbuf2[_ci]=_hsbuf[_ci]; \
    int _hsl=11; for(int _ci=0;_hs[_ci];_ci++) _hsbuf2[_hsl++]=_hs[_ci]; _hsbuf2[_hsl]=0; \
    print(_hsbuf2,                0x888899); \
    print("--------------------", 0x333350); \
    print("-- registers --",      0xFFCC00); \
    uint32_t _eax,_ebx,_ecx,_edx,_esp,_ebp,_esi,_edi,_efl; \
    asm volatile( \
        "mov %%eax,%0\n" "mov %%ebx,%1\n" \
        "mov %%ecx,%2\n" "mov %%edx,%3\n" \
        "mov %%esp,%4\n" "mov %%ebp,%5\n" \
        "mov %%esi,%6\n" "mov %%edi,%7\n" \
        "pushfl\n"       "popl %8\n" \
        :"=m"(_eax),"=m"(_ebx),"=m"(_ecx),"=m"(_edx), \
         "=m"(_esp),"=m"(_ebp),"=m"(_esi),"=m"(_edi),"=m"(_efl) \
    ); \
    auto _preg = [](const char* name, uint32_t val) { \
        char _rv[12]; _panic_print_hex(val, _rv); \
        char _rb[32]; int _ri=0; \
        for(int _ci=0;name[_ci];_ci++) _rb[_ri++]=name[_ci]; \
        _rb[_ri++]='='; \
        for(int _ci=0;_rv[_ci];_ci++) _rb[_ri++]=_rv[_ci]; \
        _rb[_ri]=0; \
        print(_rb, 0x888899); \
    }; \
    _preg("eax", _eax); _preg("ebx", _ebx); \
    _preg("ecx", _ecx); _preg("edx", _edx); \
    _preg("esp", _esp); _preg("ebp", _ebp); \
    _preg("esi", _esi); _preg("edi", _edi); \
    _preg("eflags", _efl); \
    print("--------------------", 0x333350); \
    print("-- system --",         0xFFCC00); \
    print("os:   warkrune",       0x666680); \
    print("arch: x86 32bit",      0x666680); \
    print("system halted",        0xFF5555); \
    while(1){ asm volatile("hlt"); } \
} while(0)