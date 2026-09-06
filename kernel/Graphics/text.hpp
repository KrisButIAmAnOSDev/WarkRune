#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t vbe_framebuffer;
extern uint32_t vbe_pitch;
extern uint32_t vbe_width;
extern uint32_t vbe_height;
extern uint32_t vbe_bpp;

extern const uint8_t font_data[];
#define BACKBUF_PHYS_ADDR 0xA00000u
extern uint32_t* g_backbuf;

void fb_present();
void fill_screen(uint32_t color);
void draw_rect(int x, int y, int w, int h, uint32_t color);
void printxy(const char* str, int x, int y, uint32_t color);
void printx(const char* str, int x, uint32_t color);
void print(const char* str, uint32_t color);
extern "C" void set_y(int y);
extern "C" int  get_y();

static inline void put_pixel(int x, int y, uint32_t color) {
    if ((unsigned)x < 800u && (unsigned)y < 600u)
        g_backbuf[(unsigned)y * 800u + (unsigned)x] = color;
}

#ifdef __cplusplus
}
#endif
