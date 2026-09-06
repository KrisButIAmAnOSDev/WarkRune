#pragma once
#include "../klib/types.h"
#include "Graphics/text.hpp"


#define FB_WIDTH  800
#define FB_HEIGHT 600
#define FB_BPP    4

extern u32 fb_pitch;
extern u32* framebuffer;

inline void clear_screen() {
    fill_screen(0x00000000);
}

inline uint32_t* get_framebuffer() {
    return (uint32_t*)(*(uint32_t*)0x5000);
}

inline uint32_t get_screen_width() {
    return *(uint32_t*)0x5004;
}
