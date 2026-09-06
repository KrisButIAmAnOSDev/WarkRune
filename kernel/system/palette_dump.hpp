#pragma once

#define VGA_MEM  ((volatile unsigned char*)0xA0000)
#define SCR_W    320

static inline void palette_dump() {
    for (int color = 0; color < 256; color++) {
        int cell_x = (color % 16) * 20;
        int cell_y = (color / 16) * 12;
        for (int y = 0; y < 11; y++)
            for (int x = 0; x < 19; x++)
                VGA_MEM[(cell_y + y) * SCR_W + (cell_x + x)] = color;
    }
}
