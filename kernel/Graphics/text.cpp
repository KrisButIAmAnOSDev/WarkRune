#include "Graphics/text.hpp"
#include <stdint.h>

static int _y = 0;
static bool g_fb_ready = false;

extern "C" void set_y(int y) { _y = y; }
extern "C" int  get_y() { return _y; }

uint32_t* g_backbuf = (uint32_t*)BACKBUF_PHYS_ADDR;

static inline bool fb_ready() {
    return vbe_framebuffer != 0 && vbe_pitch != 0 && vbe_width != 0 && vbe_height != 0 && vbe_bpp != 0;
}

void fb_present() {
    if (!fb_ready()) return;
    uint8_t* dst = (uint8_t*)(uintptr_t)vbe_framebuffer;
    uint32_t bpp = vbe_bpp;
    uint32_t bps = bpp >> 3;
    uint32_t w = vbe_width < 800u ? vbe_width : 800u;
    uint32_t h = vbe_height < 600u ? vbe_height : 600u;

    if (bpp == 32) {
        for (uint32_t y = 0; y < h; y++) {
            uint32_t* src_row = g_backbuf + y * 800u;
            uint32_t* dst_row = (uint32_t*)(dst + y * vbe_pitch);
            for (uint32_t x = 0; x < w; x++) dst_row[x] = src_row[x];
        }
    } else {
        for (uint32_t y = 0; y < h; y++) {
            uint32_t* src_row = g_backbuf + y * 800u;
            uint8_t* dst_row = dst + y * vbe_pitch;
            for (uint32_t x = 0; x < w; x++) {
                uint32_t color = src_row[x];
                uint8_t* pixel = dst_row + x * bps;
                if (bpp == 24) {
                    pixel[0] = (uint8_t)color;
                    pixel[1] = (uint8_t)(color >> 8);
                    pixel[2] = (uint8_t)(color >> 16);
                } else if (bpp == 16) {
                    uint16_t rgb565 = (uint16_t)(((color >> 19) & 0x1F) << 11) | (uint16_t)(((color >> 10) & 0x3F) << 5) | (uint16_t)((color >> 3) & 0x1F);
                    *(uint16_t*)pixel = rgb565;
                } else if (bpp == 8) {
                    uint8_t r = (uint8_t)(color >> 16);
                    uint8_t g = (uint8_t)(color >> 8);
                    uint8_t b = (uint8_t)color;
                    *pixel = (uint8_t)(((uint32_t)r + g + b) / 48);
                }
            }
        }
    }
}

extern "C" void fill_screen(uint32_t color) {
    if (!fb_ready()) return;
    uint32_t total = 800u * 600u;
    for (uint32_t i = 0; i < total; i++) g_backbuf[i] = color;
}

extern "C" void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            put_pixel(px, py, color);
}

extern "C" void printxy(const char* str, int x, int y, uint32_t color) {
    int cx = x;
    while (*str) {
        unsigned char c = (unsigned char)*str++;
        if (c < 0x20 || c > 0x7E) { cx += 8; continue; }
        const uint8_t* glyph = font_data + (uint32_t)(c - 0x20) * 8;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++)
                if (bits & (0x80u >> col))
                    put_pixel(cx + col, y + row, color);
        }
        cx += 8;
    }
}

extern "C" void printx(const char* str, int x, uint32_t color) {
    printxy(str, x, _y, color);
    _y += 10;
}

extern "C" void print(const char* str, uint32_t color) {
    printxy(str, 0, _y, color);
    _y += 10;
}
