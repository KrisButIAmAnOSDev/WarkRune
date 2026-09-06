#include "io/mouse/mouse.hpp"
#include "io/io.hpp"

static volatile int32_t mouse_x = 400;
static volatile int32_t mouse_y = 300;
static volatile bool mouse_left = false;
static volatile bool mouse_right = false;
static volatile bool mouse_middle = false;
static volatile int32_t mouse_dx = 0;
static volatile int32_t mouse_dy = 0;
static int32_t bound_w = 800;
static int32_t bound_h = 600;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static void mouse_wait_write() {
    int timeout = 100000;
    while (timeout-- && (inb(0x64) & 0x02));
}

static void mouse_wait_read() {
    int timeout = 100000;
    while (timeout-- && !(inb(0x64) & 0x01));
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, data);
}

static uint8_t mouse_read() {
    mouse_wait_read();
    return inb(0x60);
}

extern "C" void mouse_irq_handler() {
    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            mouse_packet[0] = data;
            if (data & 0x08) mouse_cycle++;
            break;
        case 1:
            mouse_packet[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_packet[2] = data;
            mouse_cycle = 0;

            mouse_left   = mouse_packet[0] & 0x01;
            mouse_right  = mouse_packet[0] & 0x02;
            mouse_middle = mouse_packet[0] & 0x04;

            int32_t dx = (int32_t)(int8_t)mouse_packet[1];
            int32_t dy = (int32_t)(int8_t)mouse_packet[2];

            mouse_dx = dx;
            mouse_dy = -dy;

            mouse_x += dx;
            mouse_y -= dy;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= bound_w) mouse_x = bound_w - 1;
            if (mouse_y >= bound_h) mouse_y = bound_h - 1;
            break;
    }

    outb(0x20, 0x20);
    outb(0xA0, 0x20);
}

void mouse_init() {
    outb(0x64, 0xAD);
    outb(0x64, 0xA7);

    while (inb(0x64) & 1) inb(0x60);

    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t status = inb(0x60);

    status |= 0x03;
    status |= 0x20;

    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    outb(0x64, 0xAE);
    outb(0x64, 0xA8);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();
}

void mouse_set_bounds(int32_t w, int32_t h) {
    bound_w = w;
    bound_h = h;
}

MouseState mouse_get() {
    return { mouse_x, mouse_y, mouse_left, mouse_right, mouse_middle, mouse_dx, mouse_dy };
}
