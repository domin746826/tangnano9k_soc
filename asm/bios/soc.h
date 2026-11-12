#ifndef SOC_H
#define SOC_H

#include <stdint.h>

#define MMIO_BASE       0xC0000000UL
#define REG_LED         (*(volatile uint32_t *)(MMIO_BASE + 0x00))

static inline void led_set(uint8_t mask) {
    REG_LED = (~mask) & 0x3F;
}


#define FB_BASE         0x007C0000UL
#define FB_WIDTH        480
#define FB_HEIGHT       272

#define FRAMEBUFFER     ((volatile uint16_t *)FB_BASE)
#define FRAMEBUFFER_32  ((volatile uint32_t *)FB_BASE)

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

static inline void fb_putpixel(int x, int y, uint16_t color) {
    FRAMEBUFFER[y * FB_WIDTH + x] = color;
}

#endif // SOC_H