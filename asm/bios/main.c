#include "soc.h"

// =============================================================================
// 1. Przepychanie D-Cache (Write-Back Eviction) -> naprawia ugryziony róg!
// =============================================================================
void dcache_flush(void) {
    volatile uint32_t *dummy = (volatile uint32_t *)0x00200000UL;
    for (int i = 0; i < 4096; i += 4) {
        (void)dummy[i];
    }
}

// =============================================================================
// 2. Wystylizowane logo "TG9K_SoC" (8 znaków 6x7 = 56 bajtów!)
// =============================================================================
static const uint8_t logo_glyphs[8][7] = {
    {0x3F, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C}, // T
    {0x1E, 0x30, 0x30, 0x37, 0x33, 0x33, 0x1E}, // G
    {0x1E, 0x33, 0x33, 0x1F, 0x03, 0x13, 0x1E}, // 9
    {0x33, 0x36, 0x3C, 0x38, 0x3C, 0x36, 0x33}, // K
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F}, // _
    {0x1E, 0x30, 0x1E, 0x03, 0x03, 0x33, 0x1E}, // S
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E}, // o
    {0x1E, 0x33, 0x30, 0x30, 0x30, 0x33, 0x1E}  // C
};

// Szybkie wypełnianie prostokąta
void fb_fill_rect(int x0, int y0, int w, int h, uint16_t col) {
    for (int y = y0; y < y0 + h; y++) {
        volatile uint16_t *row = &FRAMEBUFFER[y * FB_WIDTH + x0];
        for (int x = 0; x < w; x++) {
            row[x] = col;
        }
    }
}

// Podkręcony, mocno nasycony generator palety (Neon Magenta <-> Deep Violet <-> Electric Cyan)
uint16_t cyber_gradient(int pos) {
    int p = pos & 63;
    int v = (p < 32) ? (p * 8) : ((63 - p) * 8); // pełny zakres 0..248
    int r = 255 - v;                             // 255 -> 7
    int g = (v * 7) >> 3;                        // 0   -> 217 (mocny kontrast!)
    int b = 220 + (v >> 3);                      // 220 -> 251
    return RGB565(r, g, b);
}

// =============================================================================
// 3. Rysowanie znaku i tekstu z tablicy fontów 8x16 pod 0x80001000
// =============================================================================
#define FONT_ROM ((const uint8_t *)0x80001000UL)

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

// =============================================================================
// 4. Główna animacja rozruchowa TG9K_SoC
// =============================================================================
int main(void) {
    // 1. Ciemnogranatowe tło techniczne + górny/dolny pasek
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, RGB565(8, 12, 24));
    fb_fill_rect(0, 0, FB_WIDTH, 3, RGB565(0, 220, 255));
    fb_fill_rect(0, FB_HEIGHT - 3, FB_WIDTH, 3, RGB565(255, 0, 128));

    // 2. Ukośne paski techniczne (Hazard Stripes) nad i pod ramką logo
    for (int y = 52; y < 60; y++) {
        for (int x = 60; x < 420; x++) {
            if (((x + y) & 8) == 0) {
                fb_putpixel(x, y, RGB565(255, 180, 0));
                fb_putpixel(x, y + 108, RGB565(0, 200, 255));
            }
        }
    }

    // 3. Teksty diagnostyczne BIOS-u (x = 24, żeby 54-znakowy napis zmieścił się w 480px)
    fb_puts(24, 185, "ARCH : CUSTOM RV32IM SoftCore @ Tang Nano 9K",           RGB565(180, 220, 255), RGB565(8, 12, 24));
    fb_puts(24, 205, "MEM  : 8192 KB PSRAM + I-CACHE + D-CACHE + W-BACK",      RGB565(180, 220, 255), RGB565(8, 12, 24));
    fb_puts(24, 225, "BOOT : WAITING FOR SD/GPT STAGE2... (WORK IN PROGRESS)", RGB565(255, 210, 80),  RGB565(8, 12, 24));

    // Ramka paska postępu (Progress Bar)
    fb_fill_rect(58, 248, 364, 12, RGB565(30, 45, 70));

    // 4. Pętla animacji: płynący gradient przez całe "TG9K_SoC" + skaner paska i LED-ów
    int frame = 0;
    while (1) {
        int start_x = 72;
        int start_y = 78;

        for (int ch = 0; ch < 8; ch++) {
            for (int gy = 0; gy < 7; gy++) {
                uint8_t row_bits = logo_glyphs[ch][gy];
                for (int gx = 0; gx < 6; gx++) {
                    if (row_bits & (0x20 >> gx)) {
                        int px = start_x + ch * 42 + gx * 6;
                        int py = start_y + gy * 6;

                        // Gradient idzie teraz płynnie przez wszystkie 8 znaków (TG9K_SoC)!
                        uint16_t col = cyber_gradient(ch * 7 + gx + (gy * 2) - frame);

                        // Cień pod kafelkiem + właściwy kafelek 5x5
                        fb_fill_rect(px + 2, py + 2, 5, 5, RGB565(0, 20, 40));
                        fb_fill_rect(px, py, 5, 5, col);
                    }
                }
            }
        }

        // Animowany skaner w pasku postępu na dole (z tym samym gradientem!)
        int bar_pos = (frame * 6) % 340;
        fb_fill_rect(60, 250, 360, 8, RGB565(12, 20, 36));
        fb_fill_rect(60 + bar_pos, 250, 20, 8, cyber_gradient(frame * 2));

        // Wypchnij brudne linie z D-Cache do PSRAM po narysowaniu klatki
        dcache_flush();

        // Efekt fali na 6 diodach LED płytki Tang Nano 9K
        int led_idx = frame % 10;
        if (led_idx >= 6) led_idx = 10 - led_idx;
        led_set(1 << led_idx);

        frame++;
        for (volatile int d = 0; d < 40000; d++);
    }

    return 0;
}