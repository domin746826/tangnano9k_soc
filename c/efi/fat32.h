#ifndef FAT32_H_
#define FAT32_H_

#include "soc.h"

#define FAT32_MAX_NAME 13 // 8 + '.' + 3 + '\0'

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

typedef struct __attribute__((packed)) {
    char name[11]; // 0x00 name 8.3
    uint8_t attr; // 0x0B: 0x10 = dir, 0x0F = LFN, 0x08 = Volume ID
    uint8_t nt_res; // 0x0C
    uint8_t crt_time_tenth; // 0x0D
    uint16_t crt_time; // 0x0E
    uint16_t crt_date; // 0x10
    uint16_t lst_acc_date; // 0x12
    uint16_t clus_hi; // 0x14: upper 16 bits of cluster
    uint16_t wrt_time; // 0x16
    uint16_t wrt_date; // 0x18
    uint16_t clus_lo; // 0x1A: lower 16 bits of cluster
    uint32_t file_size; // 0x1C: file size in bytes
} fat32_dirent_t;

typedef struct {
    uint32_t part_lba;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_clus;
    uint32_t cwd_clus;
    uint8_t sec_per_clus;
} fat32_info;

typedef struct {
    char name[FAT32_MAX_NAME];
    uint8_t attr;
    uint8_t is_dir;
    uint32_t first_clus;
    uint32_t file_size;
} fat32_entry_t;

typedef struct {
    fat32_info *fs;
    uint32_t curr_clus;
    uint8_t sec_in_clus;
    uint8_t entry_idx;
} fat32_DIR;

typedef struct {
    fat32_info *fs;
    uint32_t start_clus;
    uint32_t curr_clus;
    uint32_t file_size;
    uint32_t pos;
} fat32_FILE;

int read_f32_info(uint32_t first_lba, fat32_info *finfo);

void fat32_opendir(fat32_info *finfo, uint32_t dir_clus, fat32_DIR *dir);
int fat32_readdir(fat32_DIR *dir, fat32_entry_t *out); // 1 = ok, 0 = end, <0 = err

int fat32_cd(fat32_info *finfo, const char *path);
int fat32_ls(fat32_info *finfo, char out_names[][FAT32_MAX_NAME], int max_entries);

int fat32_open(fat32_info *finfo, const char *path, fat32_FILE *file);
int fat32_seek(fat32_FILE *file, uint32_t offset);
uint32_t fat32_read(fat32_FILE *file, void *buf, uint32_t len);
int fat32_read_all(fat32_info *finfo, const char *path, void *dst, uint32_t max_len, uint32_t *bytes_read);

#endif