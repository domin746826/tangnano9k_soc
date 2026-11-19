#include "soc.h"
#include "sd.h"
#include "gpt_fat32.h"
#include "fb_printf.h"


static uint8_t sector_buffer[512];

int main(void) {
    uart_puts("TG9K_SoC uEFI v1.0\n");

    FB.addr = 0x007C0000;
    FB.en = 1;
    fillScreen(COLOR_BLACK);
    
    fb_print("TG9K_SoC uEFI v1.0\n");
    SPI_CTRL = ((uint32_t)250 << 16) | (1UL << SPI_EN);
    dcache_flush();
    delay_us(500000);

    uint8_t init_res = sd_init();
    if (init_res != 0x00) {
        fb_print("SD Init Fail: 0x");
        print_hex_byte(init_res);
        fb_print("\n");
        while(1) {}
    }

    parsed_gpt gpt_data;

    int res = read_gpt(&gpt_data);
    if(res == 0) {
    } else {
        fb_print("Something went wrong, err code ");
        print_hex_byte(res);
        while(1);
    }

    dcache_flush();

    fb_printf("Found GPT partition scheme, %d partitions\n", gpt_data.part_num);
    for(int i = 0; i < gpt_data.part_num; i++) {
        fb_printf(" %d. Partition %s, type ", i+1, gpt_data.partitions[i].part_name);
        switch(gpt_data.partitions[i].partition_type) {
            case PART_UNUSED:
                fb_print("unused\n");
                break;
            case PART_EFI_SYSTEM:
                fb_print("EFI System (ESP)\n");
                break;
            case PART_MS_BASIC_DATA:
                fb_print("MS Basic Data\n");
                break;
            case PART_LINUX_FS:
                fb_print("Linux Filesystem\n");
                break;
            default: 
                fb_print("unknown\n");
                break;
        }
        fb_printf("    - First LBA: %d\n", gpt_data.partitions[i].first_lba);
        fb_printf("    - Last LBA: %d\n", gpt_data.partitions[i].last_lba);
    }

    dcache_flush();

    char sranie[2] = {0, 0};
    while (1) {
        int c = uart_getc_nonblock();
        if(c != -1) {
            sranie[0] = c;
            fb_print(sranie);
            dcache_flush();
        }
    }
    return 0;
}