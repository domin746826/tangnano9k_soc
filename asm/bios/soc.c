#include "soc.h"

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

void uart_puts(char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void fb_putc(int x, int y, char c, uint16_t fg, uint16_t bg) {
    const uint8_t *glyph = &FONT_ROM[(uint8_t)c * 16];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        volatile uint16_t *dst = &FRAMEBUFFER[(y + row) * FB_WIDTH + x];
        for (int col = 0; col < 8; col++) {
            dst[col] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void fb_puts(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    while (*s) {
        fb_putc(x, y, *s++, fg, bg);
        x += 8;
    }
}

