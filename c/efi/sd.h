#ifndef SD_H
#define SD_H

#include <stdint.h>

uint8_t sd_init(void);

int sd_read_sector(uint32_t lba, uint8_t *buffer);

int sd_write_sector(uint32_t lba, const uint8_t *buffer);

extern uint8_t sd_is_sdhc;

#endif // SD_H