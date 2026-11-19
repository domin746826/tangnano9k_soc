#ifndef FAT32_H_
#define FAT32_H_
#include "soc.h"

typedef struct __attribute__((packed)) {
    uint8_t jmp[3]; // 0x00
    uint8_t oem[8]; // 0x03
    uint16_t bytes_per_sec; // 0x0B
    uint8_t sec_per_clus; // 0x0D
    uint16_t rsvd_sec_cnt; // 0x0E
    uint8_t num_fats; // 0x10
    uint16_t root_ent_cnt; // 0x11 always 0 in fat32
    uint16_t tot_sec_16; // 0x13 always 0 in fat32
    uint8_t media; // 0x15
    uint16_t fat_sz_16; // 0x16 always 0 in fat32
    uint16_t sec_per_trk; // 0x18
    uint16_t num_heads; // 0x1A
    uint32_t hidd_sec; // 0x1C
    uint32_t tot_sec_32; // 0x20
    uint32_t fat_sz_32; // 0x24
    uint16_t ext_flags; // 0x28
    uint16_t fs_ver; // 0x2A
    uint32_t root_clus; // 0x2C
} fat32_bpb_t;

#endif