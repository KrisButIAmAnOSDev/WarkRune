#include "graphics.hpp"

u32* framebuffer = (u32*)BACKBUF_PHYS_ADDR;
u32 fb_pitch = FB_WIDTH * FB_BPP;
