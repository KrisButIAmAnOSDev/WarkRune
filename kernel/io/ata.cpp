// io/ata.cpp
#include "io/ata.hpp"
#include "io/io.hpp"

#define ATA_DATA       0x1F0
#define ATA_SECTOR_CNT 0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_CMD        0x1F7
#define ATA_STATUS     0x1F7

static bool ata_wait() {
    for (int i = 0; i < 100000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & 0x01) return false;
        if (!(s & 0x80) && (s & 0x08)) return true;
    }
    return false;
}

bool ata_init() {
    outb(ATA_DRIVE, 0xA0);
    for (int i = 0; i < 4; i++) inb(ATA_STATUS);
    return inb(ATA_STATUS) != 0xFF;
}

bool ata_read(uint32_t lba, uint8_t count, uint16_t* buf) {
    outb(ATA_DRIVE,      0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)lba);
    outb(ATA_LBA_MID,    (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
    outb(ATA_CMD,        0x20);
    for (int s = 0; s < count; s++) {
        if (!ata_wait()) return false;
        for (int i = 0; i < 256; i++)
            buf[s * 256 + i] = inw(ATA_DATA);
    }
    return true;
}

bool ata_write(uint32_t lba, uint8_t count, uint16_t* buf) {
    outb(ATA_DRIVE,      0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, count);
    outb(ATA_LBA_LO,     (uint8_t)lba);
    outb(ATA_LBA_MID,    (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI,     (uint8_t)(lba >> 16));
    outb(ATA_CMD,        0x30);
    for (int s = 0; s < count; s++) {
        if (!ata_wait()) return false;
        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, buf[s * 256 + i]);
    }
    if (!ata_wait()) return false;
    outb(ATA_CMD, 0xE7);
    if (!ata_wait()) return false;
    return true;
}