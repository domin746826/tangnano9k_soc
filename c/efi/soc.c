#include "soc.h"

#include <stdint.h>

const uint8_t font8x16_data[] = {
    #embed "font8x16.bin"
};

void fb_hline(int x, int y, int amount, uint16_t color) {
    if (amount <= 0) return;

    uint16_t *ptr = (uint16_t *)&FRAMEBUFFER[y * FB_WIDTH + x];

    if ((uintptr_t)ptr & 2) { // align to dword on start
        *ptr++ = color;
        amount--;
    }

    // fast 32bit fill
    uint32_t color32 = ((uint32_t)color << 16) | color;
    uint32_t *ptr32 = (uint32_t *)ptr;
    uint32_t *end32 = ptr32 + (amount >> 1);

    while (ptr32 < end32) {
        *ptr32++ = color32;
    }

    if (amount & 1) {
        *((uint16_t *)ptr32) = color; // align to dword on end
    }
}

void fillScreen(uint16_t color) {
    const uint32_t totalWords = (FB_WIDTH * FB_HEIGHT) >> 1;
    uint32_t fill_color = ((uint32_t)color << 16) | color;

    for (uint32_t i = 0; i < totalWords; i += 8) {
        FRAMEBUFFER_32[i + 0] = fill_color;
        FRAMEBUFFER_32[i + 1] = fill_color;
        FRAMEBUFFER_32[i + 2] = fill_color;
        FRAMEBUFFER_32[i + 3] = fill_color;
        FRAMEBUFFER_32[i + 4] = fill_color;
        FRAMEBUFFER_32[i + 5] = fill_color;
        FRAMEBUFFER_32[i + 6] = fill_color;
        FRAMEBUFFER_32[i + 7] = fill_color;
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

void uart_puts(char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void fb_putc(int x, int y, char c, uint16_t fg, uint16_t bg) {
    const uint8_t *glyph = &font8x16_data[(uint8_t)c * 16];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        volatile uint16_t *dst = &FRAMEBUFFER[(y + row) * FB_WIDTH + x];
        for (int col = 0; col < 8; col++) {
            dst[col] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

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

        // Sprawdzamy przewijanie niezależnie od tego, czy to był znak, czy '\n'
        if (print_y >= FB_HEIGHT) {
            fb_scroll(16, COLOR_BLACK); // Przesuwamy cały ekran o 1 linię (16 px) w górę
            print_y -= 16;              // Cofamy kursor na ostatnią widoczną linię
        }
        
        str++;
    }
}

void fb_scroll(int num, uint16_t color) {
    if (num <= 0) return;
    if (num >= FB_HEIGHT) {
        fillScreen(color);
        return;
    }

    uint32_t *dst = FRAMEBUFFER_32;
    uint32_t *src = FRAMEBUFFER_32 + (num * FB_WIDTH / 2);
    
    uint32_t words_to_copy = ((FB_HEIGHT - num) * FB_WIDTH) / 2;
    for (uint32_t i = 0; i < words_to_copy; i++) {
        dst[i] = src[i];
    }

    uint32_t *clear_dst = FRAMEBUFFER_32 + words_to_copy;
    uint32_t clear_words = (num * FB_WIDTH) / 2;
    uint32_t color32 = ((uint32_t)color << 16) | color;
    
    for (uint32_t i = 0; i < clear_words; i++) {
        clear_dst[i] = color32;
    }
}


void delay_us(uint32_t del) {
    uint32_t t0 = REG_TIMER_US;
    while ((uint32_t)(REG_TIMER_US - t0) < del);
}

void fb_puts(int x, int y, char *s, uint16_t fg, uint16_t bg) {
    while (*s) {
        fb_putc(x, y, *s++, fg, bg);
        x += 8;
    }
}

void *memcpy(void *dst, const void *src, unsigned int n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memset(void *dst, int c, unsigned int n) {
    uint8_t *d = (uint8_t *)dst;
    while (n--) *d++ = (uint8_t)c;
    return dst;
}

int memcmp(const void *s1, const void *s2, unsigned int n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}