#ifndef GPT_FAT32_H_
#define GPT_FAT32_H_
#include <stdint.h>

#define MAX_GPT_PARTS 8

typedef enum {
    PART_UNUSED = 0,
    PART_EFI_SYSTEM,
    PART_MS_BASIC_DATA,
    PART_LINUX_FS,
    PART_UNKNOWN
} part_type_t;


part_type_t gpt_identify_guid(const uint8_t *guid);
typedef struct {
    part_type_t partition_type;
    // uint8_t uuid[16];
    uint32_t first_lba;
    uint32_t last_lba;
    // uint8_t attributes[8];
    char part_name[37];
} part_entry;

typedef struct {
    uint32_t FirstUsableLBA;
    uint32_t LastUsableLBA;
    uint32_t PartitionEntryLBA;
    uint32_t NumberOfEntries;
    uint32_t SizeOfEntry;
    uint8_t part_num;
    part_entry partitions[MAX_GPT_PARTS];
} parsed_gpt;

int read_gpt(parsed_gpt *gpt_data);


#endif