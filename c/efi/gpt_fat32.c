#include "gpt_fat32.h"
#include "soc.h"
#include "sd.h"

static const uint8_t GUID_UNUSED[16] = { 0 };

static const uint8_t GUID_EFI_SYSTEM[16] = { // esp
    0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
    0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B
};

static const uint8_t GUID_MS_BASIC_DATA[16] = { // fat16/32/ntfs
    0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
    0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7
};

static const uint8_t GUID_LINUX_FS[16] = { // ext2/ext4/root
    0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47,
    0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4
};

uint8_t sector_data[512] __attribute__((aligned(4)));

static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

part_type_t gpt_identify_guid(const uint8_t *guid) {
    if (memcmp(guid, GUID_UNUSED, 16) == 0) return PART_UNUSED;
    if (memcmp(guid, GUID_EFI_SYSTEM, 16) == 0) return PART_EFI_SYSTEM;
    if (memcmp(guid, GUID_MS_BASIC_DATA, 16) == 0) return PART_MS_BASIC_DATA;
    if (memcmp(guid, GUID_LINUX_FS, 16) == 0) return PART_LINUX_FS;
    return PART_UNKNOWN;
}

// 0 if gpt
// 1 if not gpt
// other if err
int read_gpt(parsed_gpt *gpt_data) {
    // protective mbr
    int res;
    res = sd_read_sector(0, sector_data);
    if(res != 0) {
        return res;
    }
    if(!(sector_data[450] == 0xEE && sector_data[510] == 0x55 && sector_data[511] == 0xAA)) {
        return 1;
    }

    // check for gpt header
    res = sd_read_sector(1, sector_data);
    if(res != 0) {
        return res;
    }

    if(memcmp(sector_data, "EFI PART", 8)) { // not gpt
        return 1;
    }

    gpt_data->FirstUsableLBA = rd32(&sector_data[40]);
    gpt_data->LastUsableLBA = rd32(&sector_data[48]);
    gpt_data->PartitionEntryLBA = rd32(&sector_data[72]);
    gpt_data->NumberOfEntries = rd32(&sector_data[80]);
    gpt_data->SizeOfEntry = rd32(&sector_data[84]);
    gpt_data->part_num = 0;
    // read parts
    res = sd_read_sector(gpt_data->PartitionEntryLBA, sector_data);
    if(res != 0) {
        return res;
    }

    uint32_t sectors_to_read = (MAX_GPT_PARTS + 3) / 4;

    for (uint32_t s = 0; s < sectors_to_read; s++) {
        res = sd_read_sector(gpt_data->PartitionEntryLBA + s, sector_data);
        if (res != 0) return res;

        for (int i = 0; i < 4; i++) {
            int slot = s * 4 + i;
            if (slot >= MAX_GPT_PARTS) break;

            uint8_t *entry = sector_data + (i * 128);
            part_entry *p  = &gpt_data->partitions[slot];

            p->partition_type = gpt_identify_guid(entry);
            if (p->partition_type == PART_UNUSED) {
                continue;
            }

            p->first_lba = rd32(&entry[32]);
            p->last_lba = rd32(&entry[40]);

            for (int c = 0; c < 36; c++) {
                uint8_t ch = entry[56 + c * 2];
                p->part_name[c] = ch;
                if (ch == 0) break;
            }
            p->part_name[36] = '\0';
            gpt_data->part_num++;
        }
    }
    return 0;


}