#pragma once
#include <stdint.h>

bool ata_init();
bool ata_read(uint32_t lba, uint8_t count, uint16_t* buf);
bool ata_write(uint32_t lba, uint8_t count, uint16_t* buf);
