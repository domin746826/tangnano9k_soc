#include "soc.h"
#include "sd.h"
#include "gpt_fat32.h"
#include "fb_printf.h"
#include "fat32.h"
#include <stddef.h>

#define LOAD_ADDR 0x00000000UL
#define MAX_LOAD_SIZE 0x00600000UL

#define MENU_START_Y 48
#define MAX_VISIBLE 12 
#define STATUS_Y 256

#define DEBOUNCE_US 30000UL
#define LONG_PRESS_US 700000UL

#define BTN_PRESSED()   (!(REG_BTN & 1U))

static int is_bin_file(const char *name) {
    int len = 0;
    while (name[len]) len++;
    if (len < 5) return 0;

    return (name[len - 4] == '.' &&
            name[len - 3] == 'B' &&
            name[len - 2] == 'I' &&
            name[len - 1] == 'N');
}

static void draw_menu_header(void) {
    fillScreen(COLOR_BLACK);
    fb_set_cursor(0, 0);
    fb_print("TG9K_SoC uEFI v1.0 - Boot Menu");
    fb_set_cursor(0, 16);
    fb_print("Short press: next | Long press: boot");
    fb_hline(0, 38, FB_WIDTH, COLOR_CYAN);
    dcache_flush();
}

static void draw_menu_item(int slot, int file_idx, const char *name, int selected) {
    char line[40];
    memset(line, ' ', sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    line[2] = selected ? '>' : ' ';
    line[4] = '[';

    int num = file_idx + 1;
    int p = 5;
    if (num >= 10) line[p++] = (char)('0' + (num / 10));
    line[p++] = (char)('0' + (num % 10));
    line[p++] = ']';
    line[p++] = ' ';

    for (int i = 0; name[i] && p < 37; i++) {
        line[p++] = name[i];
    }

    uint16_t fg = selected ? COLOR_BLACK : COLOR_WHITE;
    uint16_t bg = selected ? COLOR_CYAN  : COLOR_BLACK;
    fb_puts(0, MENU_START_Y + slot * 16, line, fg, bg);
}

static void draw_menu_list(char files[][FAT32_MAX_NAME], int count, int scroll, int selected) {
    int visible = count - scroll;
    if (visible > MAX_VISIBLE) visible = MAX_VISIBLE;

    for (int slot = 0; slot < visible; slot++) {
        int idx = scroll + slot;
        draw_menu_item(slot, idx, files[idx], idx == selected);
    }
    dcache_flush();
}

static void boot_selected_file(fat32_info *esp, const char *filename) {
    uint32_t loaded_size = 0;

    fb_set_cursor(0, STATUS_Y);
    fb_printf("Loading %s...                    ", filename);
    dcache_flush();

    int res = fat32_read_all(esp, filename, (void *)LOAD_ADDR, MAX_LOAD_SIZE, &loaded_size);
    if (res != 0) {
        fb_set_cursor(0, STATUS_Y);
        fb_printf("Load error: %d                     ", res);
        dcache_flush();
        return;
    }

    fb_set_cursor(0, STATUS_Y);
    fb_printf("Loaded %u B. Jumping...            ", loaded_size);
    dcache_flush();
    icache_flush();

    void (*entry)(uint32_t, uint32_t) = (void (*)(uint32_t, uint32_t))LOAD_ADDR;
    entry(0, 0);
}

int main(void) {
    uart_puts("TG9K_SoC uEFI v1.0\n");

    FB.addr = FB_BASE;
    FB.en = 1;
    fillScreen(COLOR_BLACK);

    fb_set_cursor(0, 0);
    fb_print("TG9K_SoC uEFI v1.0\n\n");
    fb_print("Initializing SD card... ");
    dcache_flush();

    SPI_CTRL = ((uint32_t)250 << 16) | (1UL << SPI_EN);
    delay_us(500000);

    uint8_t init_res = sd_init();
    if (init_res != 0x00) {
        fb_print("FAIL (0x");
        print_hex_byte(init_res);
        fb_print(")\n");
        dcache_flush();
        while (1) {}
    }
    fb_print("OK\n");
    dcache_flush();

    fb_print("Reading GPT partition table... ");
    dcache_flush();

    parsed_gpt gpt_data;
    int res = read_gpt(&gpt_data);
    if (res != 0) {
        fb_print("FAIL (0x");
        print_hex_byte((uint8_t)res);
        fb_print(")\n");
        dcache_flush();
        while (1) {}
    }
    fb_printf("OK (%d partitions)\n", gpt_data.part_num);
    dcache_flush();

    fb_print("Locating EFI System Partition... ");
    dcache_flush();

    part_entry *first_efi_part = NULL;
    for (int i = 0; i < gpt_data.part_num; i++) {
        if (gpt_data.partitions[i].partition_type == PART_EFI_SYSTEM && first_efi_part == NULL) {
            first_efi_part = &gpt_data.partitions[i];
        }
    }

    if (first_efi_part == NULL) {
        fb_print("NOT FOUND\n");
        dcache_flush();
        while (1) {}
    }
    fb_printf("OK (LBA %u)\n", first_efi_part->first_lba);
    dcache_flush();

    fb_print("Mounting FAT32 filesystem... ");
    dcache_flush();

    fat32_info esp;
    if (read_f32_info(first_efi_part->first_lba, &esp) != 0) {
        fb_print("FAIL\n");
        dcache_flush();
        while (1) {}
    }
    fb_print("OK\n");
    dcache_flush();

    fb_print("Opening /EFI/BOOT... ");
    dcache_flush();

    if (fat32_cd(&esp, "/EFI/BOOT") != 0) {
        fb_print("NOT FOUND\n");
        dcache_flush();
        while (1) {}
    }
    fb_print("OK\n");
    dcache_flush();

    fb_print("Scanning for .BIN images... ");
    dcache_flush();

    char raw_files[64][FAT32_MAX_NAME];
    char bin_files[64][FAT32_MAX_NAME];
    int total = fat32_ls(&esp, raw_files, 64);
    int bin_count = 0;

    for (int i = 0; i < total; i++) {
        if (is_bin_file(raw_files[i])) {
            memcpy(bin_files[bin_count++], raw_files[i], FAT32_MAX_NAME);
        }
    }

    if (bin_count == 0) {
        fb_print("NONE FOUND\n");
        dcache_flush();
        while (1) {}
    }
    fb_printf("OK (%d found)\n", bin_count);
    dcache_flush();

    delay_us(400000);

    int selected = 0;
    int scroll = 0;
    int was_pressed = 0;
    int long_triggered = 0;
    uint32_t press_start_us = 0;

    draw_menu_header();
    draw_menu_list(bin_files, bin_count, scroll, selected);

    while (1) {
        int move_next = 0;
        int do_boot = 0;

        if (BTN_PRESSED()) {
            uint32_t now = REG_TIMER_US;
            if (!was_pressed) {
                was_pressed = 1;
                long_triggered = 0;
                press_start_us = now;
            } else if (!long_triggered && (now - press_start_us) >= LONG_PRESS_US) {
                long_triggered = 1;
                do_boot = 1;
            }
        } else {
            if (was_pressed) {
                uint32_t held_us = REG_TIMER_US - press_start_us;
                if (!long_triggered && held_us >= DEBOUNCE_US) {
                    move_next = 1;
                }
                was_pressed = 0;
                long_triggered = 0;
            }
        }

        int c = uart_getc_nonblock();
        if (c == ' ') move_next = 1;
        else if (c == '\r' || c == '\n') do_boot = 1;

        if (move_next) {
            int prev = selected;
            selected = (selected + 1) % bin_count;

            int new_scroll = scroll;
            if (selected < new_scroll) {
                new_scroll = selected;
            } else if (selected >= new_scroll + MAX_VISIBLE) {
                new_scroll = selected - MAX_VISIBLE + 1;
            }

            if (new_scroll != scroll) {
                scroll = new_scroll;
                draw_menu_list(bin_files, bin_count, scroll, selected);
            } else {
                draw_menu_item(prev - scroll, prev, bin_files[prev], 0);
                draw_menu_item(selected - scroll, selected, bin_files[selected], 1);
                dcache_flush();
            }
        }

        if (do_boot) {
            boot_selected_file(&esp, bin_files[selected]);
        }
    }

    return 0;
}