// #include "soc.h"

// uint16_t print_y = 0;
// uint16_t print_x = 0;

// void fb_print(char *str) {
//     while(*str) {
//         if (*str == '\n') {
//             print_y += 16;
//             print_x = 0;
//         } else {
//             fb_putc(print_x, print_y, *str, COLOR_WHITE, COLOR_BLACK);
//             print_x += 8;
            
//             if (print_x >= FB_WIDTH) {
//                 print_x = 0;
//                 print_y += 16;
//             }
//         }
//         str++;
//     }
// }

// void delay_us(uint32_t del) {
//     uint32_t t0 = REG_TIMER_US;
//     while ((uint32_t)(REG_TIMER_US - t0) < del);
// }

// int main(void) {
//     uart_puts("TG9K_SoC uEFI v1.0\n");

//     FB.addr = 0x007C0000;
//     FB.en = 1;
//     fillScreen(COLOR_BLACK);
//     fb_print("TG9K_SoC uEFI v1.0\n");
//     fb_print("Loading available devices... ");
//     dcache_flush();
//     delay_us(1500000);
//     fb_print("No bootable devices found\n");
//     fb_print("Uart data: \n> ");
//     dcache_flush();

//     char sranie[2] = {0, 0};


//     while (1) {
//         // REG_LED++;
//         // uint32_t t0 = REG_TIMER_US;
//         // while ((uint32_t)(REG_TIMER_US - t0) < 200000); // 200 ms
//         int c = uart_getc_nonblock();
//         if(c != -1) {
//             sranie[0] = c;
//             fb_print(sranie);
//             dcache_flush();
//         }
//     }
//     return 0;
// }
#include "soc.h"
#include "sd.h"

uint16_t print_y = 0;
uint16_t print_x = 0;

void fb_print(char *str) {
    while(*str) {
        if (*str == '\n') {
            print_y += 16;
            print_x = 0;
        } else {
            fb_putc(print_x, print_y, *str, COLOR_WHITE, COLOR_BLACK);
            print_x += 8;
            
            if (print_x >= FB_WIDTH) {
                print_x = 0;
                print_y += 16;
            }
        }
        str++;
    }
}

void print_hex_byte(uint8_t b) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex[(b >> 4) & 0xF];
    buf[1] = hex[b & 0xF];
    buf[2] = '\0';
    fb_print(buf);
    uart_puts(buf);
}

void delay_us(uint32_t del) {
    uint32_t t0 = REG_TIMER_US;
    while ((uint32_t)(REG_TIMER_US - t0) < del);
}

static uint8_t sector_buffer[512];

int main(void) {
    uart_puts("TG9K_SoC uEFI v1.0\n");

    FB.addr = 0x007C0000;
    FB.en = 1;
    fillScreen(COLOR_BLACK);
    
    fb_print("TG9K_SoC uEFI v1.0\n");
    fb_print("Loading SPI controller... ");
    
    *(volatile uint32_t *)(MMIO_BASE + 0x2C) = ((uint32_t)250 << 16) | (1UL << 0);
    fb_print("OK\n");
    
    dcache_flush();
    delay_us(500000);

    uint8_t init_res = sd_init();
    if (init_res != 0x00) {
        fb_print("SD Init Fail: 0x");
        print_hex_byte(init_res);
        fb_print("\n");
    } else {
        fb_print("SD Init OK (0x00)\n");
            
        if (sd_read_sector(0, sector_buffer) == 0) {
            fb_print("[UEFI] LBA 0 read:\n");
            uart_puts("[UEFI] LBA 0 read:\n");

            for (int i = 0; i < 512; i++) {
                print_hex_byte(sector_buffer[i]);
                fb_print(" ");
                uart_putc(' ');

                if ((i & 15) == 15) {
                    fb_print("\n");
                    uart_puts("\n");
                }
            }
        } else {
            fb_print("[UEFI] Can't read sector\n");
        }
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