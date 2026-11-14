

#include "soc.h"
static inline void fb_putpixel(int x, int y, uint16_t color) {
    FRAMEBUFFER[y * FB_WIDTH + x] = color;
}

static inline void dcache_flush(void) {
    __asm__ volatile ("fence" ::: "memory");
}

static inline void icache_flush(void) {
    __asm__ volatile ("fence.i" ::: "memory");
}

void fb_hline(int x, int y, int amount, uint16_t color) {

}
