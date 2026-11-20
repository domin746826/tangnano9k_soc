#include "../../c/efi/soc.h"

/* ============================================================================
 * 1. Start (_start @ 0x0000_0000) oraz podstawowe funkcje pomocnicze
 * ============================================================================ */

extern uint32_t _bss_start;
extern uint32_t _bss_end;
extern uint32_t _stack_top;

int main(void);

__attribute__((naked, section(".text.init")))
void _start(void) {
    __asm__ volatile (
        ".option push\n"
        ".option norelax\n"
        "la gp, __global_pointer$\n"
        ".option pop\n"
        "la sp, _stack_top\n"

        "la t0, _bss_start\n"
        "la t1, _bss_end\n"
        "1:\n"
        "bgeu t0, t1, 2f\n"
        "sw zero, 0(t0)\n"
        "addi t0, t0, 4\n"
        "j 1b\n"
        "2:\n"

        "call main\n"
        "3:\n"
        "j 3b\n"
    );
}

void *memset(void *dst, int c, unsigned int n) {
    uint8_t *p = (uint8_t *)dst;
    while (n--) *p++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned int n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void uart_puts(char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

/* ============================================================================
 * 2. Podwójne buforowanie (480x272 RGB565) z pełną synchronizacją vblank
 * ============================================================================ */

#define FB_BUF0_ADDR  (FB_BASE)              /* 0x007C0000 */
#define FB_BUF1_ADDR  (FB_BASE - 0x40000UL)  /* 0x00780000 */

static uint32_t front_addr = FB_BUF0_ADDR;
static uint32_t back_addr  = FB_BUF1_ADDR;
static uint16_t *backbuffer = (uint16_t *)FB_BUF1_ADDR;

static void fb_init_double_buffer(void) {
    front_addr = FB_BUF0_ADDR;
    back_addr  = FB_BUF1_ADDR;
    backbuffer = (uint16_t *)back_addr;
    dcache_flush();
    FB.ctrl = (1U << 31) | (front_addr & 0x7FFFFFU);
    dcache_flush();
}

static void fb_swap_buffers(void) {
    /* 1. Wypchnij wszystkie piksele backbuffera z D-cache/bufora zapisu do RAM */
    dcache_flush();

    /* 2. Jeśli poprzednia klatka skończyła się jeszcze w trakcie vblank,
     * poczekaj aż ruszy aktywne skanowanie (line < 272), żeby nie podmienić 2x w jednej ramce */
    while (FB.line >= FB_HEIGHT);

    /* 3. Poczekaj na koniec aktywnego obrazu (wejście w vblank: line >= 272) */
    while (FB.line < FB_HEIGHT);

    /* 4. Atomowy zapis nowego adresu i en=1 do FB.ctrl */
    uint32_t tmp = front_addr;
    front_addr = back_addr;
    back_addr  = tmp;
    backbuffer = (uint16_t *)back_addr;

    FB.ctrl = (1U << 31) | (front_addr & 0x7FFFFFU);

    /* 5. Bariera po zapisie MMIO, żeby podmiana FB.ctrl dotarła do kontrolera wideo
     * zanim CPU zacznie czyścić nowy backbuffer (czyli poprzedni frontbuffer) */
    dcache_flush();

    /* 6. Poczekaj aż vblank się skończy (kontroler zatrzasnął nowy adres i zaczął linię 0) */
    while (FB.line >= FB_HEIGHT);
}

/* Szybkie czyszczenie tła z gradientem pionowym (zapis 32 B = 1 linia cache na iterację) */
static void clear_background(void) {
    uint32_t *dst = (uint32_t *)backbuffer;
    for (int y = 0; y < FB_HEIGHT; y++) {
        uint8_t r = (uint8_t)(8 + (y >> 4));
        uint8_t g = (uint8_t)(12 + (y >> 3));
        uint8_t b = (uint8_t)(28 + (y >> 2));
        uint16_t c16 = RGB565(r, g, b);
        uint32_t c32 = ((uint32_t)c16 << 16) | c16;

        for (int x = 0; x < (FB_WIDTH / 2); x += 8) {
            dst[0] = c32; dst[1] = c32; dst[2] = c32; dst[3] = c32;
            dst[4] = c32; dst[5] = c32; dst[6] = c32; dst[7] = c32;
            dst += 8;
        }
    }
}

/* ============================================================================
 * 3. Licznik FPS (korzysta z fb_set_cursor i fb_print)
 * ============================================================================ */

static int weak_cur_x = 0;
static int weak_cur_y = 0;

static uint8_t get_glyph_row(char c, int row) {
    static const uint8_t digits[10][8] = {
        {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, /* 0 */
        {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, /* 1 */
        {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, /* 2 */
        {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, /* 3 */
        {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x0F,0x00}, /* 4 */
        {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, /* 5 */
        {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, /* 6 */
        {0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00}, /* 7 */
        {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, /* 8 */
        {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}  /* 9 */
    };
    static const uint8_t glyph_F[8] = {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00};
    static const uint8_t glyph_P[8] = {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00};
    static const uint8_t glyph_S[8] = {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00};
    static const uint8_t glyph_C[8] = {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00};

    if (c >= '0' && c <= '9') return digits[c - '0'][row];
    if (c == 'F') return glyph_F[row];
    if (c == 'P') return glyph_P[row];
    if (c == 'S') return glyph_S[row];
    if (c == ':') return glyph_C[row];
    return 0x00;
}

__attribute__((weak))
void fb_set_cursor(int x, int y) {
    weak_cur_x = x;
    weak_cur_y = y;
}

__attribute__((weak))
void fb_print(char *str) {
    while (*str) {
        char c = *str++;
        if (c == '\n') {
            weak_cur_x = 8;
            weak_cur_y += 10;
            continue;
        }
        for (int row = 0; row < 8; row++) {
            int py = weak_cur_y + row;
            if ((unsigned)py >= FB_HEIGHT) continue;
            uint8_t bits = get_glyph_row(c, row);
            for (int col = 0; col < 8; col++) {
                int px = weak_cur_x + col;
                if ((unsigned)px >= FB_WIDTH) continue;
                if (bits & (0x80 >> col)) {
                    backbuffer[py * FB_WIDTH + px] = COLOR_YELLOW;
                }
            }
        }
        weak_cur_x += 8;
    }
}

static void format_fps(char *buf, uint32_t fps) {
    buf[0] = 'F';
    buf[1] = 'P';
    buf[2] = 'S';
    buf[3] = ':';
    buf[4] = ' ';

    char tmp[10];
    int n = 0;
    if (fps == 0) {
        tmp[n++] = '0';
    } else {
        while (fps > 0 && n < 10) {
            tmp[n++] = (char)('0' + (fps % 10));
            fps /= 10;
        }
    }
    int pos = 5;
    while (n > 0) {
        buf[pos++] = tmp[--n];
    }
    buf[pos++] = ' ';
    buf[pos] = '\0';
}

static void draw_fps_hud(char *fps_str) {
    fb_set_cursor(8, 8);
    fb_print(fps_str);
}

/* ============================================================================
 * 4. Pre-cieniowane tekstury 64x64 w mikro-kafelkach 8x8 (6 ścian x 16 poziomów)
 * ============================================================================ */

#define TEX_SHIFT    6
#define TEX_SIZE     (1 << TEX_SHIFT)   /* 64 */
#define TEX_MASK     (TEX_SIZE - 1)     /* 63 */
#define LIGHT_LEVELS 16

static uint16_t textures_lit[6][LIGHT_LEVELS][TEX_SIZE * TEX_SIZE];

static inline uint16_t shade_rgb565(uint16_t c, uint32_t light) {
    if (light >= 252) return c;
    uint32_t r = ((c >> 11) & 0x1F) * light >> 8;
    uint32_t g = ((c >> 5)  & 0x3F) * light >> 8;
    uint32_t b = (c         & 0x1F) * light >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void generate_textures(void) {
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            /* Układ kafelków 8x8: sąsiednie piksele w U i V trafiają w tę samą linię D-cache */
            int idx = ((y & 0x38) << 6) | ((x & 0x38) << 3) | ((y & 7) << 3) | (x & 7);
            int border = (x < 2 || x >= 62 || y < 2 || y >= 62);
            uint16_t base[6];

            /* 0: Szachownica czerwono-biała */
            {
                int checker = ((x >> 3) ^ (y >> 3)) & 1;
                if (border) base[0] = RGB565(255, 220, 60);
                else if (checker) base[0] = RGB565(225, 35, 35);
                else base[0] = RGB565(240, 240, 240);
            }

            /* 1: Siatka PCB / cyber */
            {
                int grid = ((x & 7) == 0 || (y & 7) == 0);
                int node = ((x & 15) < 2 && (y & 15) < 2);
                if (border || node) base[1] = RGB565(120, 255, 255);
                else if (grid) base[1] = RGB565(0, 170, 210);
                else base[1] = RGB565(10, 28, 48);
            }

            /* 2: Ceglany mur */
            {
                int row = y >> 3;
                int bx = (x + ((row & 1) ? 8 : 0)) & 15;
                int by = y & 7;
                if (bx == 0 || by == 0) {
                    base[2] = RGB565(185, 180, 170);
                } else {
                    uint8_t shade = (uint8_t)(((x * 7 + y * 13) & 31));
                    base[2] = RGB565(150 + shade, 55 + (shade >> 1), 40);
                }
            }

            /* 3: Drewniana skrzynia */
            {
                int frame = (x < 6 || x >= 58 || y < 6 || y >= 58);
                int diag  = ((x - y) >= -4 && (x - y) <= 4);
                int grain = ((y * 5 + (x >> 2)) & 15);
                if (frame || diag) base[3] = RGB565(165, 115, 55);
                else if ((y & 7) == 0) base[3] = RGB565(75, 48, 20);
                else base[3] = RGB565(125 + grain, 82 + (grain >> 1), 36);
            }

            /* 4: Interferencja XOR */
            {
                uint8_t v = (uint8_t)((x ^ y) << 2);
                base[4] = border ? COLOR_WHITE : RGB565(v, (uint8_t)(255 - v), (uint8_t)(((x + y) << 1) & 0xFF));
            }

            /* 5: Pasy ostrzegawcze */
            {
                int stripe = ((x + y) >> 3) & 1;
                if (border) base[5] = RGB565(90, 90, 95);
                else if (stripe) base[5] = RGB565(245, 195, 15);
                else base[5] = RGB565(28, 28, 32);
            }

            for (int f = 0; f < 6; f++) {
                for (int l = 0; l < LIGHT_LEVELS; l++) {
                    uint32_t light = (uint32_t)(96 + ((l * 159) / (LIGHT_LEVELS - 1)));
                    textures_lit[f][l][idx] = shade_rgb565(base[f], light);
                }
            }
        }
    }
}

/* ============================================================================
 * 5. Arytmetyka stałoprzecinkowa (16.16) i interpolowany sinus 16-bitowy
 * ============================================================================ */

#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)

static const int16_t sin_lut[256] = {
         0,    402,    804,   1205,   1606,   2006,   2404,   2801,
      3196,   3590,   3981,   4370,   4756,   5139,   5520,   5897,
      6270,   6639,   7005,   7366,   7723,   8076,   8423,   8765,
      9102,   9434,   9760,  10080,  10394,  10702,  11003,  11297,
     11585,  11866,  12140,  12406,  12665,  12916,  13160,  13395,
     13623,  13842,  14053,  14256,  14449,  14635,  14811,  14978,
     15137,  15286,  15426,  15557,  15679,  15791,  15893,  15986,
     16069,  16143,  16207,  16261,  16305,  16340,  16364,  16379,
     16384,  16379,  16364,  16340,  16305,  16261,  16207,  16143,
     16069,  15986,  15893,  15791,  15679,  15557,  15426,  15286,
     15137,  14978,  14811,  14635,  14449,  14256,  14053,  13842,
     13623,  13395,  13160,  12916,  12665,  12406,  12140,  11866,
     11585,  11297,  11003,  10702,  10394,  10080,   9760,   9434,
      9102,   8765,   8423,   8076,   7723,   7366,   7005,   6639,
      6270,   5897,   5520,   5139,   4756,   4370,   3981,   3590,
      3196,   2801,   2404,   2006,   1606,  1205,    804,    402,
         0,   -402,   -804,  -1205,  -1606,  -2006,  -2404,  -2801,
     -3196,  -3590,  -3981,  -4370,  -4756,  -5139,  -5520,  -5897,
     -6270,  -6639,  -7005,  -7366,  -7723,  -8076,  -8423,  -8765,
     -9102,  -9434,  -9760, -10080, -10394, -10702, -11003, -11297,
    -11585, -11866, -12140, -12406, -12665, -12916, -13160, -13395,
    -13623, -13842, -14053, -14256, -14449, -14635, -14811, -14978,
    -15137, -15286, -15426, -15557, -15679, -15791, -15893, -15986,
    -16069, -16143, -16207, -16261, -16305, -16340, -16364, -16379,
    -16384, -16379, -16364, -16340, -16305, -16261, -16207, -16143,
    -16069, -15986, -15893, -15791, -15679, -15557, -15426, -15286,
    -15137, -14978, -14811, -14635, -14449, -14256, -14053, -13842,
    -13623, -13395, -13160, -12916, -12665, -12406, -12140, -11866,
    -11585, -11297, -11003, -10702, -10394, -10080,  -9760,  -9434,
     -9102,  -8765,  -8423,  -8076,  -7723,  -7366,  -7005,  -6639,
     -6270,  -5897,  -5520,  -5139,  -4756,  -4370,  -3981,  -3590,
     -3196,  -2801,  -2404,  -2006,  -1606,  -1205,   -804,   -402
};

static inline int32_t fp_sin16(uint16_t angle) {
    uint8_t idx = (uint8_t)(angle >> 8);
    int32_t frac = (int32_t)(angle & 0xFF);
    int32_t a = sin_lut[idx];
    int32_t b = sin_lut[(uint8_t)(idx + 1)];
    return ((a << 8) + (b - a) * frac) >> 6;
}

static inline int32_t fp_cos16(uint16_t angle) {
    return fp_sin16((uint16_t)(angle + 16384U));
}

static inline int32_t fp_mul(int32_t a, int32_t b) {
    uint32_t lo, hi;
    __asm__ (
        "mul  %0, %2, %3\n\t"
        "mulh %1, %2, %3\n\t"
        : "=&r"(lo), "=r"(hi)
        : "r"(a), "r"(b)
    );
    return (int32_t)((lo >> FP_SHIFT) | (hi << (32 - FP_SHIFT)));
}

/* ============================================================================
 * 6. Szybki rasterizer teksturowanych trójkątów (adresowanie kafelkowe 8x8)
 * ============================================================================ */

typedef struct {
    int32_t x, y;
    int32_t u, v;
} vertex2d_t;

#define TILE_UV_IDX(u, v) ( \
    (((uint32_t)(v) >> 10) & 0xE00U) | \
    (((uint32_t)(u) >> 13) & 0x1C0U) | \
    (((uint32_t)(v) >> 13) & 0x038U) | \
    (((uint32_t)(u) >> 16) & 0x007U) )

static void draw_textured_triangle(vertex2d_t v0, vertex2d_t v1, vertex2d_t v2,
                                   const uint16_t *tex, int32_t dudx, int32_t dvdx) {
    if (v0.y > v1.y) { vertex2d_t t = v0; v0 = v1; v1 = t; }
    if (v1.y > v2.y) { vertex2d_t t = v1; v1 = v2; v2 = t; }
    if (v0.y > v1.y) { vertex2d_t t = v0; v0 = v1; v1 = t; }

    int32_t total_h = v2.y - v0.y;
    if (total_h <= 0) return;

    int32_t dx_long = ((v2.x - v0.x) << FP_SHIFT) / total_h;
    int32_t du_long = (v2.u - v0.u) / total_h;
    int32_t dv_long = (v2.v - v0.v) / total_h;

    for (int part = 0; part < 2; part++) {
        int32_t y_start = (part == 0) ? v0.y : v1.y;
        int32_t y_end   = (part == 0) ? v1.y : v2.y;
        int32_t seg_h   = y_end - y_start;
        if (seg_h <= 0) continue;

        int32_t x_a = (v0.x << FP_SHIFT) + (y_start - v0.y) * dx_long;
        int32_t u_a = v0.u + (y_start - v0.y) * du_long;
        int32_t v_a = v0.v + (y_start - v0.y) * dv_long;

        int32_t x_b, u_b, v_b, dx_short, du_short, dv_short;
        if (part == 0) {
            x_b = v0.x << FP_SHIFT;
            u_b = v0.u;
            v_b = v0.v;
            dx_short = ((v1.x - v0.x) << FP_SHIFT) / seg_h;
            du_short = (v1.u - v0.u) / seg_h;
            dv_short = (v1.v - v0.v) / seg_h;
        } else {
            x_b = v1.x << FP_SHIFT;
            u_b = v1.u;
            v_b = v1.v;
            dx_short = ((v2.x - v1.x) << FP_SHIFT) / seg_h;
            du_short = (v2.u - v1.u) / seg_h;
            dv_short = (v2.v - v1.v) / seg_h;
        }

        for (int32_t y = y_start; y < y_end; y++) {
            if ((uint32_t)y < FB_HEIGHT) {
                int32_t xl = x_a >> FP_SHIFT;
                int32_t xr = x_b >> FP_SHIFT;
                int32_t ul = u_a;
                int32_t vl = v_a;

                if (xl > xr) {
                    int32_t tx = xl; xl = xr; xr = tx;
                    ul = u_b;
                    vl = v_b;
                }

                if (xl < 0) {
                    int32_t skip = -xl;
                    ul += skip * dudx;
                    vl += skip * dvdx;
                    xl = 0;
                }
                if (xr >= FB_WIDTH) {
                    xr = FB_WIDTH - 1;
                }

                uint16_t *row = &backbuffer[y * FB_WIDTH + xl];
                int32_t count = xr - xl + 1;

                while (count >= 4) {
                    row[0] = tex[TILE_UV_IDX(ul, vl)];
                    ul += dudx; vl += dvdx;
                    row[1] = tex[TILE_UV_IDX(ul, vl)];
                    ul += dudx; vl += dvdx;
                    row[2] = tex[TILE_UV_IDX(ul, vl)];
                    ul += dudx; vl += dvdx;
                    row[3] = tex[TILE_UV_IDX(ul, vl)];
                    ul += dudx; vl += dvdx;
                    row += 4;
                    count -= 4;
                }
                while (count-- > 0) {
                    *row++ = tex[TILE_UV_IDX(ul, vl)];
                    ul += dudx; vl += dvdx;
                }
            }

            x_a += dx_long;  u_a += du_long;  v_a += dv_long;
            x_b += dx_short; u_b += du_short; v_b += dv_short;
        }
    }
}

static void draw_textured_quad(vertex2d_t p0, vertex2d_t p1, vertex2d_t p2, vertex2d_t p3,
                               const uint16_t *tex) {
    int32_t denom = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    if (denom == 0) return;

    int32_t dudx = ((p1.u - p0.u) * (p2.y - p0.y) - (p2.u - p0.u) * (p1.y - p0.y)) / denom;
    int32_t dvdx = ((p1.v - p0.v) * (p2.y - p0.y) - (p2.v - p0.v) * (p1.y - p0.y)) / denom;

    draw_textured_triangle(p0, p1, p2, tex, dudx, dvdx);
    draw_textured_triangle(p0, p2, p3, tex, dudx, dvdx);
}

/* ============================================================================
 * 7. Geometria kostki 3D i pętla główna dema
 * ============================================================================ */

typedef struct {
    int32_t x, y, z;
} vec3_t;

static const vec3_t cube_verts[8] = {
    { -FP_ONE, -FP_ONE, -FP_ONE },
    {  FP_ONE, -FP_ONE, -FP_ONE },
    {  FP_ONE,  FP_ONE, -FP_ONE },
    { -FP_ONE,  FP_ONE, -FP_ONE },
    { -FP_ONE, -FP_ONE,  FP_ONE },
    {  FP_ONE, -FP_ONE,  FP_ONE },
    {  FP_ONE,  FP_ONE,  FP_ONE },
    { -FP_ONE,  FP_ONE,  FP_ONE }
};

/* Kolejność wierzchołków CCW patrząc z zewnątrz (prawoskrętny układ, +Z do widza) */
static const uint8_t cube_faces[6][4] = {
    { 4, 5, 6, 7 }, /* Przód  (+Z) */
    { 1, 0, 3, 2 }, /* Tył    (-Z) */
    { 0, 4, 7, 3 }, /* Lewa   (-X) */
    { 5, 1, 2, 6 }, /* Prawa  (+X) */
    { 3, 7, 6, 2 }, /* Góra   (+Y) */
    { 0, 1, 5, 4 }  /* Dół    (-Y) */
};

int main(void) {
    fb_init_double_buffer();
    generate_textures();

    if (UART.en) {
        uart_puts("RV32IM 3D Textured Cube Demo\n");
    }

    uint16_t ax = 0, ay = 0, az = 0;
    uint32_t frame = 0;
    int32_t cam_dist = 4 * FP_ONE;

    uint32_t prev_us     = REG_TIMER_US;
    uint32_t fps_last_us = prev_us;
    uint32_t fps_frames  = 0;
    char fps_str[16];
    format_fps(fps_str, 0);

    while (1) {
        uint32_t now_us = REG_TIMER_US;
        uint32_t dt_us  = now_us - prev_us;
        prev_us = now_us;
        if (dt_us > 100000U) dt_us = 100000U;

        uint32_t btn = REG_BTN;

        /* Płynny obrót z krokiem zależnym od czasu */
        if (!(btn & 0x01)) {
            ax = (uint16_t)(ax + ((dt_us * 14U) >> 10));
            ay = (uint16_t)(ay + ((dt_us * 21U) >> 10));
            az = (uint16_t)(az + ((dt_us *  7U) >> 10));
        }
        if ((btn & 0x02) && cam_dist > (2 * FP_ONE + (FP_ONE >> 1))) {
            cam_dist -= (FP_ONE >> 5);
        }
        if ((btn & 0x04) && cam_dist < (7 * FP_ONE)) {
            cam_dist += (FP_ONE >> 5);
        }

        led_set((uint8_t)(frame >> 2));
        clear_background();

        int32_t sx = fp_sin16(ax), cx = fp_cos16(ax);
        int32_t sy = fp_sin16(ay), cy = fp_cos16(ay);
        int32_t sz = fp_sin16(az), cz = fp_cos16(az);

        vertex2d_t proj[8];
        vec3_t rotated[8];

        for (int i = 0; i < 8; i++) {
            int32_t x = cube_verts[i].x;
            int32_t y = cube_verts[i].y;
            int32_t z = cube_verts[i].z;

            int32_t y1 = fp_mul(y, cx) - fp_mul(z, sx);
            int32_t z1 = fp_mul(y, sx) + fp_mul(z, cx);

            int32_t x2 = fp_mul(x, cy) + fp_mul(z1, sy);
            int32_t z2 = -fp_mul(x, sy) + fp_mul(z1, cy);

            int32_t x3 = fp_mul(x2, cz) - fp_mul(y1, sz);
            int32_t y3 = fp_mul(x2, sz) + fp_mul(y1, cz);

            rotated[i].x = x3;
            rotated[i].y = y3;
            rotated[i].z = z2;

            int32_t z_persp = cam_dist - z2;
            if (z_persp < (FP_ONE >> 2)) z_persp = (FP_ONE >> 2);

            proj[i].x = (FB_WIDTH / 2)  + (x3 * 240) / z_persp;
            proj[i].y = (FB_HEIGHT / 2) - (y3 * 240) / z_persp;
        }

        const int32_t uv_max = (TEX_SIZE << FP_SHIFT) - 1;

        for (int f = 0; f < 6; f++) {
            uint8_t i0 = cube_faces[f][0];
            uint8_t i1 = cube_faces[f][1];
            uint8_t i2 = cube_faces[f][2];
            uint8_t i3 = cube_faces[f][3];

            vertex2d_t p0 = proj[i0];
            vertex2d_t p1 = proj[i1];
            vertex2d_t p2 = proj[i2];
            vertex2d_t p3 = proj[i3];

            /* Odrzucanie tylnych ścian (cross >= 0 oznacza ścianę odwróconą od widza) */
            int32_t cross = (p1.x - p0.x) * (p2.y - p0.y) -
                            (p1.y - p0.y) * (p2.x - p0.x);
            if (cross >= 0) continue;

            vec3_t e1 = {
                rotated[i1].x - rotated[i0].x,
                rotated[i1].y - rotated[i0].y,
                rotated[i1].z - rotated[i0].z
            };
            vec3_t e2 = {
                rotated[i2].x - rotated[i0].x,
                rotated[i2].y - rotated[i0].y,
                rotated[i2].z - rotated[i0].z
            };
            int32_t nz = fp_mul(e1.x, e2.y) - fp_mul(e1.y, e2.x);
            int light_idx = 0;
            if (nz > 0) {
                light_idx = (int)(nz >> (FP_SHIFT - 2));
                if (light_idx >= LIGHT_LEVELS) light_idx = LIGHT_LEVELS - 1;
            }

            p0.u = 0;      p0.v = uv_max;
            p1.u = uv_max; p1.v = uv_max;
            p2.u = uv_max; p2.v = 0;
            p3.u = 0;      p3.v = 0;

            draw_textured_quad(p0, p1, p2, p3, textures_lit[f][light_idx]);
        }

        fps_frames++;
        uint32_t elapsed = now_us - fps_last_us;
        if (elapsed >= 500000U) {
            uint32_t fps = (fps_frames * 1000000U + (elapsed >> 1)) / elapsed;
            format_fps(fps_str, fps);
            fps_frames = 0;
            fps_last_us = now_us;
        }

        draw_fps_hud(fps_str);
        fb_swap_buffers();
        frame++;
    }

    return 0;
}