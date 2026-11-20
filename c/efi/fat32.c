#include "fat32.h"
#include "sd.h"

#define FAT32_EOF 0x0FFFFFF8UL

static uint8_t sector_data[512] __attribute__((aligned(4)));

static inline uint32_t cluster_to_lba(const fat32_info *fs, uint32_t clus) {
    return fs->data_start_lba + (clus - 2) * (uint32_t)fs->sec_per_clus;
}

static uint32_t fat32_next_cluster(const fat32_info *fs, uint32_t clus) {
    uint32_t fat_sec = fs->fat_start_lba + (clus / 128);
    uint32_t fat_off = (clus % 128) * 4;

    if (sd_read_sector(fat_sec, sector_data) != 0) {
        return 0x0FFFFFFFUL;
    }
    return (*(const uint32_t *)&sector_data[fat_off]) & 0x0FFFFFFFUL;
}

static char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - ('a' - 'A');
    return c;
}

static void name_to_fat83(const char *src, int len, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    if (len == 1 && src[0] == '.') {
        out[0] = '.';
        return;
    }
    if (len == 2 && src[0] == '.' && src[1] == '.') {
        out[0] = '.';
        out[1] = '.';
        return;
    }

    int i = 0, dst = 0;
    while (i < len && src[i] != '.') {
        if (dst < 8) out[dst++] = to_upper(src[i]);
        i++;
    }
    if (i < len && src[i] == '.') {
        i++;
        dst = 8;
        while (i < len && dst < 11) {
            out[dst++] = to_upper(src[i++]);
        }
    }
}

static void fat83_to_name(const char in[11], int is_dir, char out[FAT32_MAX_NAME]) {
    int p = 0;
    int name_end = 8;
    while (name_end > 0 && in[name_end - 1] == ' ') name_end--;

    for (int i = 0; i < name_end; i++) {
        out[p++] = in[i];
    }

    int ext_end = 11;
    while (ext_end > 8 && in[ext_end - 1] == ' ') ext_end--;

    if (ext_end > 8) {
        if (!is_dir) out[p++] = '.';
        for (int i = 8; i < ext_end; i++) {
            out[p++] = in[i];
        }
    }
    out[p] = '\0';
}

static int has_extension(const char *filename, const char *ext) {
    if (!ext || !*ext) return 1;
    if (ext[0] == '.') ext++;

    const char *dot = 0;
    for (const char *p = filename; *p; p++) {
        if (*p == '.') dot = p;
    }
    if (!dot) return 0;

    dot++;
    int i = 0;
    while (dot[i] && ext[i]) {
        if (to_upper(dot[i]) != to_upper(ext[i])) return 0;
        i++;
    }
    return (dot[i] == '\0' && ext[i] == '\0');
}

int read_f32_info(uint32_t first_lba, fat32_info *finfo) {
    int res = sd_read_sector(first_lba, sector_data);
    if (res != 0) return res;

    if (sector_data[510] != 0x55 || sector_data[511] != 0xAA) return 1;

    fat32_bpb_t *bpb = (fat32_bpb_t *)sector_data;
    if (bpb->bytes_per_sec != 512 || bpb->sec_per_clus == 0 || bpb->fat_sz_32 == 0) {
        return 2;
    }

    finfo->part_lba = first_lba;
    finfo->sec_per_clus = bpb->sec_per_clus;
    finfo->root_clus = bpb->root_clus;
    finfo->cwd_clus = bpb->root_clus;
    finfo->fat_start_lba = first_lba + bpb->rsvd_sec_cnt;
    finfo->data_start_lba = finfo->fat_start_lba + ((uint32_t)bpb->num_fats * bpb->fat_sz_32);

    return 0;
}

void fat32_opendir(fat32_info *finfo, uint32_t dir_clus, fat32_DIR *dir) {
    dir->fs = finfo;
    dir->curr_clus = (dir_clus == 0) ? finfo->root_clus : dir_clus;
    dir->sec_in_clus = 0;
    dir->entry_idx = 0;
}

int fat32_readdir(fat32_DIR *dir, fat32_entry_t *out) {
    fat32_info *fs = dir->fs;

    while (dir->curr_clus >= 2 && dir->curr_clus < FAT32_EOF) {
        uint32_t lba = cluster_to_lba(fs, dir->curr_clus) + dir->sec_in_clus;
        if (sd_read_sector(lba, sector_data) != 0) {
            return -1;
        }

        while (dir->entry_idx < 16) {
            fat32_dirent_t *e = (fat32_dirent_t *)(sector_data + dir->entry_idx * 32);
            dir->entry_idx++;

            if ((uint8_t)e->name[0] == 0x00) {
                dir->curr_clus = FAT32_EOF;
                return 0;
            }
            if ((uint8_t)e->name[0] == 0xE5) continue;
            if (e->attr == 0x0F) continue;
            if (e->attr & 0x08) continue;

            out->attr = e->attr;
            out->is_dir = (e->attr & 0x10) ? 1 : 0;
            out->first_clus = ((uint32_t)e->clus_hi << 16) | e->clus_lo;
            out->file_size = e->file_size;

            if (out->is_dir && out->first_clus == 0) {
                out->first_clus = fs->root_clus;
            }

            fat83_to_name(e->name, out->is_dir, out->name);
            return 1;
        }

        dir->entry_idx = 0;
        dir->sec_in_clus++;
        if (dir->sec_in_clus >= fs->sec_per_clus) {
            dir->sec_in_clus = 0;
            dir->curr_clus = fat32_next_cluster(fs, dir->curr_clus);
        }
    }

    return 0;
}

static int fat32_find_in_dir(fat32_info *fs, uint32_t dir_clus, const char *seg, int seg_len, fat32_entry_t *out) {
    char target83[11];
    name_to_fat83(seg, seg_len, target83);

    uint32_t clus = (dir_clus == 0) ? fs->root_clus : dir_clus;

    while (clus >= 2 && clus < FAT32_EOF) {
        uint32_t base_lba = cluster_to_lba(fs, clus);

        for (uint8_t s = 0; s < fs->sec_per_clus; s++) {
            if (sd_read_sector(base_lba + s, sector_data) != 0) return -1;

            for (int i = 0; i < 16; i++) {
                fat32_dirent_t *e = (fat32_dirent_t *)(sector_data + i * 32);

                if ((uint8_t)e->name[0] == 0x00) return 0;
                if ((uint8_t)e->name[0] == 0xE5) continue;
                if (e->attr == 0x0F || (e->attr & 0x08)) continue;

                if (memcmp(e->name, target83, 11) == 0) {
                    out->attr = e->attr;
                    out->is_dir = (e->attr & 0x10) ? 1 : 0;
                    out->first_clus = ((uint32_t)e->clus_hi << 16) | e->clus_lo;
                    out->file_size = e->file_size;
                    if (out->is_dir && out->first_clus == 0) {
                        out->first_clus = fs->root_clus;
                    }
                    fat83_to_name(e->name, out->is_dir, out->name);
                    return 1;
                }
            }
        }
        clus = fat32_next_cluster(fs, clus);
    }
    return 0;
}

static int fat32_resolve_path(fat32_info *fs, const char *path, fat32_entry_t *out) {
    if (!path || !*path) return -1;

    uint32_t curr_clus = fs->cwd_clus;
    if (path[0] == '/') {
        curr_clus = fs->root_clus;
        while (*path == '/') path++;
        if (*path == '\0') {
            out->name[0] = '/';
            out->name[1] = '\0';
            out->attr = 0x10;
            out->is_dir = 1;
            out->first_clus = fs->root_clus;
            out->file_size = 0;
            return 1;
        }
    }

    fat32_entry_t entry;
    while (*path) {
        const char *start = path;
        int len = 0;
        while (path[len] && path[len] != '/') len++;

        int found = fat32_find_in_dir(fs, curr_clus, start, len, &entry);
        if (found != 1) return 0;

        path += len;
        while (*path == '/') path++;

        if (*path != '\0') {
            if (!entry.is_dir) return 0;
            curr_clus = entry.first_clus;
        }
    }

    *out = entry;
    return 1;
}

int fat32_cd(fat32_info *finfo, const char *path) {
    fat32_entry_t entry;
    if (fat32_resolve_path(finfo, path, &entry) != 1) return -1;
    if (!entry.is_dir) return -2;

    finfo->cwd_clus = entry.first_clus;
    return 0;
}

int fat32_ls(fat32_info *finfo, char out_names[][FAT32_MAX_NAME], int max_entries) {
    fat32_DIR dir;
    fat32_entry_t ent;
    int count = 0;
    int err;

    fat32_opendir(finfo, finfo->cwd_clus, &dir);
    while ((err = fat32_readdir(&dir, &ent)) == 1) {
        if (ent.name[0] == '.' && (ent.name[1] == '\0' || (ent.name[1] == '.' && ent.name[2] == '\0'))) {
            continue;
        }

        if (count < max_entries) {
            memcpy(out_names[count], ent.name, FAT32_MAX_NAME);
            count++;
        } else {
            break;
        }
    }

    return (err < 0) ? err : count;
}

int fat32_open(fat32_info *finfo, const char *path, fat32_FILE *file) {
    fat32_entry_t entry;
    if (fat32_resolve_path(finfo, path, &entry) != 1) return -1;
    if (entry.is_dir) return -2;

    file->fs = finfo;
    file->start_clus = entry.first_clus;
    file->curr_clus = entry.first_clus;
    file->file_size = entry.file_size;
    file->pos = 0;
    return 0;
}

int fat32_seek(fat32_FILE *file, uint32_t offset) {
    if (offset > file->file_size) {
        offset = file->file_size;
    }

    uint32_t clus_bytes = (uint32_t)file->fs->sec_per_clus * 512;
    uint32_t curr_idx = file->pos / clus_bytes;
    uint32_t target_idx = offset / clus_bytes;

    if (target_idx < curr_idx) {
        file->curr_clus = file->start_clus;
        curr_idx = 0;
    }

    while (curr_idx < target_idx && file->curr_clus >= 2 && file->curr_clus < FAT32_EOF) {
        file->curr_clus = fat32_next_cluster(file->fs, file->curr_clus);
        curr_idx++;
    }

    file->pos = offset;
    return 0;
}

uint32_t fat32_read(fat32_FILE *file, void *buf, uint32_t len) {
    if (file->pos >= file->file_size || len == 0) return 0;
    if (file->pos + len > file->file_size) {
        len = file->file_size - file->pos;
    }

    fat32_info *fs = file->fs;
    uint8_t *dst   = (uint8_t *)buf;
    uint32_t total = 0;
    uint32_t clus_bytes = (uint32_t)fs->sec_per_clus * 512;

    while (total < len && file->curr_clus >= 2 && file->curr_clus < FAT32_EOF) {
        uint32_t off_in_clus = file->pos % clus_bytes;
        uint32_t sec_idx     = off_in_clus / 512;
        uint32_t off_in_sec  = off_in_clus % 512;

        uint32_t lba = cluster_to_lba(fs, file->curr_clus) + sec_idx;
        if (sd_read_sector(lba, sector_data) != 0) break;

        uint32_t chunk = 512 - off_in_sec;
        if (chunk > (len - total)) chunk = len - total;

        memcpy(dst + total, sector_data + off_in_sec, chunk);
        total     += chunk;
        file->pos += chunk;

        if ((file->pos % clus_bytes) == 0 && file->pos < file->file_size) {
            file->curr_clus = fat32_next_cluster(fs, file->curr_clus);
        }
    }

    return total;
}

int fat32_read_all(fat32_info *finfo, const char *path, void *dst, uint32_t max_len, uint32_t *bytes_read) {
    fat32_FILE f;
    int res = fat32_open(finfo, path, &f);
    if (res != 0) return res;

    if (f.file_size > max_len) return -3;

    uint32_t rd = fat32_read(&f, dst, f.file_size);
    if (bytes_read) *bytes_read = rd;
    return (rd == f.file_size) ? 0 : -4;
}