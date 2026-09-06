#pragma once
#include <stdint.h>

extern "C" uint8_t read_scancode();

static inline const char sc_map[58] = {
    0,    0,   '1','2','3','4','5','6','7','8','9','0','-','=', 0x08,
    0,   'q', 'w','e','r','t','y','u','i','o','p','[',']', '\r',
    0,   'a', 's','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/', 0,
    0,    0,  ' '
};

static inline const char sc_map_shift[58] = {
    0,    0,   '!','@','#','$','%','^','&','*','(',')','_','+', 0x08,
    0,   'Q', 'W','E','R','T','Y','U','I','O','P','{','}', '\r',
    0,   'A', 'S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?', 0,
    0,    0,  ' '
};

extern "C" uint8_t kbd_shift_down;

inline char read_key() {
    uint8_t sc = read_scancode();
    if (sc == 0)   return 0;
    if (sc == 0x2A || sc == 0x36) { kbd_shift_down = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { kbd_shift_down = 0; return 0; }
    if (sc & 0x80) return 0;
    if (sc >= 58)  return 0;
    return kbd_shift_down ? sc_map_shift[sc] : sc_map[sc];
}

