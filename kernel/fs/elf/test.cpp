typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef unsigned int       uintptr_t;
typedef unsigned int       size_t;

#define WIDTH  800
#define HEIGHT 600
#define VRAM 0x5000
#define COL_MAGENTA   0xFF00FF

void _start() {
    uint32_t* screen = (uint32_t*)VRAM;
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            screen[y * WIDTH + x] = COL_MAGENTA;
        }
    }

    while (1) {}
}