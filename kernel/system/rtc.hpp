#pragma once
#include <stdint.h>

static inline uint8_t rtc_read(uint8_t reg) {
    __asm__ volatile("outb %0, $0x70" :: "a"(reg));
    uint8_t val;
    __asm__ volatile("inb $0x71, %0" : "=a"(val));
    return val;
}

static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

struct RTCTime {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

static inline RTCTime rtc_get() {
    RTCTime t;
    t.sec   = bcd_to_bin(rtc_read(0x00));
    t.min   = bcd_to_bin(rtc_read(0x02));
    t.hour  = bcd_to_bin(rtc_read(0x04));
    t.day   = bcd_to_bin(rtc_read(0x07));
    t.month = bcd_to_bin(rtc_read(0x08));
    t.year  = bcd_to_bin(rtc_read(0x09));
    return t;
}
