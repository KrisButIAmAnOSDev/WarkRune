#pragma once
#include <stdint.h>

struct MouseState {
    int32_t x;
    int32_t y;
    bool left;
    bool right;
    bool middle;
    int32_t dx;
    int32_t dy;
};

void mouse_init();
MouseState mouse_get();
void mouse_set_bounds(int32_t w, int32_t h);
