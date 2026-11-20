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
 * 2. Podwójne buforowanie (480x272 RGB565):
 *    cała klatka -> dcache_flush() -> fb_wait_vblank() -> nowy adres w FB.ctrl
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
    FB.ctrl = (1U << 31) | (front_addr & 0x7FFFFFU);
}

static void fb_swap_buffers(void) {
    dcache_flush();
    fb_wait_vblank();

    uint32_t tmp = front_addr;
    front_addr = back_addr;
    back_addr  = tmp;
    backbuffer = (uint16_t *)back_addr;

    FB.ctrl = (1U << 31) | (front_addr & 0x7FFFFFU);
}

/* ============================================================================
 * 3. Tekst HUD (fb_set_cursor + fb_print)
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
    if (c >= '0' && c <= '9') return digits[c - '0'][row];
    switch (c) {
        case 'F': { static const uint8_t g[8]={0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0}; return g[row]; }
        case 'P': { static const uint8_t g[8]={0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0}; return g[row]; }
        case 'S': { static const uint8_t g[8]={0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0}; return g[row]; }
        case 'T': { static const uint8_t g[8]={0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0}; return g[row]; }
        case ':': { static const uint8_t g[8]={0x00,0x18,0x18,0x00,0x00,0x18,0x18,0}; return g[row]; }
        default:  return 0x00;
    }
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

/* ============================================================================
 * 4. Arytmetyka stałoprzecinkowa 16.16 (sprzętowe mul + mulh)
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

static inline uint16_t shade_rgb565(uint16_t c, uint32_t light) {
    if (light >= 252) return c;
    uint32_t r = ((c >> 11) & 0x1F) * light >> 8;
    uint32_t g = ((c >> 5)  & 0x3F) * light >> 8;
    uint32_t b = (c         & 0x1F) * light >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* ============================================================================
 * 5. Pre-cieniowane tekstury ścian [tex][light][x][y], tło oraz duszki
 * ============================================================================ */

#define WALL_TEX_COUNT 4
#define LIGHT_LEVELS   8
#define TEX_SHIFT      6
#define TEX_SIZE       64
#define TEX_MASK       63

#define SPR_SIZE       32
#define SPR_MASK       31

/* Układ kolumnowy [x][y] + 8 poziomów jasności: zero przeliczania RGB w pętli ściany */
static uint16_t wall_tex_lit[WALL_TEX_COUNT][LIGHT_LEVELS][TEX_SIZE][TEX_SIZE];
static uint16_t sprite_tex[2][SPR_SIZE * SPR_SIZE];
static uint32_t sky_row32[FB_HEIGHT / 2];
static uint32_t floor_row32[FB_HEIGHT / 2];

static void generate_assets(void) {
    const int half_h = FB_HEIGHT / 2;
    for (int y = 0; y < half_h; y++) {
        uint8_t c = (uint8_t)(10 + ((half_h - y) >> 3));
        uint16_t sc16 = RGB565(c >> 1, c, (uint8_t)(c + 18));
        sky_row32[y] = ((uint32_t)sc16 << 16) | sc16;

        uint8_t g = (uint8_t)(16 + (y >> 2));
        uint16_t fc16 = RGB565(g, g, (uint8_t)(g + 6));
        floor_row32[y] = ((uint32_t)fc16 << 16) | fc16;
    }

    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            uint16_t base[WALL_TEX_COUNT];

            /* 0: Czerwona cegła */
            {
                int row = y >> 3;
                int bx = (x + ((row & 1) ? 8 : 0)) & 15;
                int by = y & 7;
                if (bx == 0 || by == 0) {
                    base[0] = RGB565(165, 160, 155);
                } else {
                    uint8_t n = (uint8_t)(((x * 11 + y * 17) & 31));
                    base[0] = RGB565(145 + n, 48 + (n >> 1), 38);
                }
            }

            /* 1: Cyber / Labirynt techniczny */
            {
                int border = (x < 2 || x >= 62 || y < 2 || y >= 62);
                int grid   = ((x & 15) == 0 || (y & 15) == 0);
                int core   = (x >= 24 && x < 40 && y >= 24 && y < 40);
                if (border) base[1] = RGB565(40, 190, 230);
                else if (core) base[1] = RGB565(0, 230, 180);
                else if (grid) base[1] = RGB565(20, 110, 150);
                else base[1] = RGB565(18, 34, 56);
            }

            /* 2: Drewniane panele */
            {
                int frame = (x < 5 || x >= 59 || y < 5 || y >= 59);
                int diag  = ((x - y) >= -3 && (x - y) <= 3);
                int grain = ((y * 7 + (x >> 2)) & 15);
                if (frame || diag) base[2] = RGB565(155, 108, 52);
                else if ((x & 15) == 0) base[2] = RGB565(65, 42, 18);
                else base[2] = RGB565(118 + grain, 78 + (grain >> 1), 34);
            }

            /* 3: Pasy ostrzegawcze */
            {
                int stripe = ((x + y) >> 3) & 1;
                if (y < 6 || y >= 58) base[3] = RGB565(80, 85, 95);
                else if (stripe) base[3] = RGB565(235, 185, 15);
                else base[3] = RGB565(30, 30, 35);
            }

            for (int t = 0; t < WALL_TEX_COUNT; t++) {
                for (int l = 0; l < LIGHT_LEVELS; l++) {
                    uint32_t light = (uint32_t)(36 + l * 31);
                    wall_tex_lit[t][l][x][y] = shade_rgb565(base[t], light);
                }
            }
        }
    }

    for (int y = 0; y < SPR_SIZE; y++) {
        for (int x = 0; x < SPR_SIZE; x++) {
            int dx = x - 16;
            int dy = y - 16;
            int d2 = dx * dx + dy * dy;
            int idx = y * SPR_SIZE + x;

            /* 0: Kryształ energii */
            int manhattan = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (manhattan < 5) {
                sprite_tex[0][idx] = RGB565(240, 255, 255);
            } else if (manhattan < 11) {
                sprite_tex[0][idx] = RGB565(20, 225, 255);
            } else if (manhattan == 11) {
                sprite_tex[0][idx] = RGB565(10, 110, 180);
            } else {
                sprite_tex[0][idx] = 0x0000;
            }

            /* 1: Dron strażniczy */
            if (d2 <= 144) {
                if (dy >= -3 && dy <= 1 && dx >= -7 && dx <= 7) {
                    sprite_tex[1][idx] = RGB565(255, 235, 40);
                } else if (d2 <= 64) {
                    sprite_tex[1][idx] = RGB565(240, 50, 35);
                } else {
                    sprite_tex[1][idx] = RGB565(145, 25, 25);
                }
            } else {
                sprite_tex[1][idx] = 0x0000;
            }
        }
    }
}

/* ============================================================================
 * 6. Mapa świata (16x16) i obiekty 3D (duszki)
 * ============================================================================ */

#define MAP_W 16
#define MAP_H 16

static const uint8_t world_map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,2,0,0,0,0,0,0,0,0,1},
    {1,0,3,3,0,0,2,0,3,0,3,0,0,4,0,1},
    {1,0,3,0,0,0,0,0,0,0,0,0,0,4,0,1},
    {1,0,0,0,0,2,2,0,0,0,3,0,0,0,0,1},
    {1,0,0,0,0,2,0,0,0,0,0,0,2,2,0,1},
    {1,2,0,2,0,0,0,0,3,3,0,0,0,2,0,1},
    {1,0,0,0,0,0,0,0,3,0,0,0,0,0,0,1},
    {1,0,3,0,0,0,0,0,0,0,0,2,0,3,0,1},
    {1,0,3,0,2,2,0,0,0,0,0,2,0,3,0,1},
    {1,0,0,0,2,0,0,0,3,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,3,0,2,2,0,4,0,1},
    {1,0,2,2,0,0,3,0,0,0,0,2,0,4,0,1},
    {1,0,0,2,0,0,3,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,2,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

typedef struct {
    int32_t x, y;
    int32_t vx, vy;
    uint8_t type;
    uint8_t active;
} sprite_t;

#define NUM_SPRITES 8
static sprite_t sprites[NUM_SPRITES];

static void reset_entities(void) {
    static const int32_t init_data[NUM_SPRITES][5] = {
        { ( 3 << 16) + 0x8000, ( 3 << 16) + 0x8000,      0,      0, 0 },
        { (14 << 16) + 0x8000, ( 2 << 16) + 0x8000,      0,      0, 0 },
        { ( 5 << 16) + 0x8000, (10 << 16) + 0x8000,      0,      0, 0 },
        { (12 << 16) + 0x8000, (11 << 16) + 0x8000,      0,      0, 0 },
        { ( 7 << 16) + 0x8000, (14 << 16) + 0x8000,      0,      0, 0 },
        { ( 7 << 16) + 0x8000, ( 4 << 16) + 0x8000,  0x300,      0, 1 },
        { (11 << 16) + 0x8000, ( 7 << 16) + 0x8000,      0,  0x300, 1 },
        { ( 3 << 16) + 0x8000, (13 << 16) + 0x8000,  0x300,      0, 1 }
    };
    for (int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].x      = init_data[i][0];
        sprites[i].y      = init_data[i][1];
        sprites[i].vx     = init_data[i][2];
        sprites[i].vy     = init_data[i][3];
        sprites[i].type   = (uint8_t)init_data[i][4];
        sprites[i].active = 1;
    }
}

/* ============================================================================
 * 7. Renderowanie 3D paskami 16 px (1 linia cache 32 B, zero overdraw)
 * ============================================================================ */

#define RAY_COLS    (FB_WIDTH / 2)   /* 240 promieni, każdy rysuje parę pikseli (32-bit) */
#define STRIP_COLS  8                /* 8 par pikseli = 16 px = 32 B (pełna linia D-cache) */

static int32_t zbuffer[RAY_COLS];

#define FOV_SCALE  43254
#define INV_DET    99296

static void render_scene(int32_t posX, int32_t posY, int32_t dirX, int32_t dirY,
                         uint32_t frame, int muzzle_flash) {
    int32_t planeX = fp_mul(dirY, FOV_SCALE);
    int32_t planeY = -fp_mul(dirX, FOV_SCALE);

    int32_t cameraX = -FP_ONE;
    const int32_t cameraX_step = (2 * FP_ONE) / RAY_COLS;
    const int half_h = FB_HEIGHT / 2;

    uint32_t *fb32 = (uint32_t *)backbuffer;

    /* 30 pionowych pasów po 16 pikseli szerokości (32 bajty na wiersz) */
    for (int base_col = 0; base_col < RAY_COLS; base_col += STRIP_COLS) {
        int drawStart[STRIP_COLS];
        int drawEnd[STRIP_COLS];
        const uint16_t *tex_col[STRIP_COLS];
        int32_t texStep[STRIP_COLS];
        int32_t texPos[STRIP_COLS];

        int min_start = half_h;
        int max_end   = half_h;

        /* 1. Rzutowanie 8 promieni w obrębie pasa */
        for (int c = 0; c < STRIP_COLS; c++, cameraX += cameraX_step) {
            int32_t rayDirX = dirX + fp_mul(planeX, cameraX);
            int32_t rayDirY = dirY + fp_mul(planeY, cameraX);

            int mapX = posX >> FP_SHIFT;
            int mapY = posY >> FP_SHIFT;

            uint32_t abs_rdx = (uint32_t)((rayDirX < 0) ? -rayDirX : rayDirX);
            uint32_t abs_rdy = (uint32_t)((rayDirY < 0) ? -rayDirY : rayDirY);

            int32_t deltaDistX = (abs_rdx <= 32U) ? 0x08000000 : (int32_t)((0x80000000U / abs_rdx) << 1);
            int32_t deltaDistY = (abs_rdy <= 32U) ? 0x08000000 : (int32_t)((0x80000000U / abs_rdy) << 1);

            int stepX, stepY;
            int32_t sideDistX, sideDistY;

            if (rayDirX < 0) {
                stepX = -1;
                sideDistX = fp_mul(posX - (mapX << FP_SHIFT), deltaDistX);
            } else {
                stepX = 1;
                sideDistX = fp_mul(((mapX + 1) << FP_SHIFT) - posX, deltaDistX);
            }

            if (rayDirY < 0) {
                stepY = -1;
                sideDistY = fp_mul(posY - (mapY << FP_SHIFT), deltaDistY);
            } else {
                stepY = 1;
                sideDistY = fp_mul(((mapY + 1) << FP_SHIFT) - posY, deltaDistY);
            }

            int hit = 0;
            int side = 0;
            int steps = 0;

            while (!hit && steps < 48) {
                if (sideDistX < sideDistY) {
                    sideDistX += deltaDistX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sideDistY += deltaDistY;
                    mapY += stepY;
                    side = 1;
                }
                if ((unsigned)mapX >= MAP_W || (unsigned)mapY >= MAP_H) {
                    hit = 1;
                } else if (world_map[mapY][mapX] > 0) {
                    hit = world_map[mapY][mapX];
                }
                steps++;
            }

            int32_t perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
            if (perpWallDist < 0x0800) perpWallDist = 0x0800;
            zbuffer[base_col + c] = perpWallDist;

            int32_t lineHeight = (FB_HEIGHT << FP_SHIFT) / perpWallDist;
            int ds = (FB_HEIGHT - lineHeight) >> 1;
            int de = (FB_HEIGHT + lineHeight) >> 1;
            if (ds < 0) ds = 0;
            if (de >= FB_HEIGHT) de = FB_HEIGHT - 1;
            drawStart[c] = ds;
            drawEnd[c]   = de;

            if (ds < min_start) min_start = ds;
            if (de > max_end)   max_end   = de;

            int texNum = (hit > 0 && hit <= WALL_TEX_COUNT) ? (hit - 1) : 0;
            int32_t wallX = (side == 0)
                ? (posY + fp_mul(perpWallDist, rayDirY))
                : (posX + fp_mul(perpWallDist, rayDirX));
            wallX &= (FP_ONE - 1);

            int texX = (wallX >> (FP_SHIFT - TEX_SHIFT)) & TEX_MASK;
            if ((side == 0 && rayDirX < 0) || (side == 1 && rayDirY > 0)) {
                texX = TEX_MASK - texX;
            }

            int32_t dist_atten = perpWallDist >> (FP_SHIFT - 4);
            int32_t light = 255 - dist_atten * 7 + (muzzle_flash ? 45 : 0);
            if (side == 1) light = (light * 195) >> 8;
            int light_idx = (light - 36) >> 5;
            if (light_idx < 0) light_idx = 0;
            if (light_idx >= LIGHT_LEVELS) light_idx = LIGHT_LEVELS - 1;

            tex_col[c] = wall_tex_lit[texNum][light_idx][texX];
            int32_t tstep = (lineHeight > 0) ? ((TEX_SIZE << FP_SHIFT) / lineHeight) : 0;
            texStep[c] = tstep;
            texPos[c]  = (ds - ((FB_HEIGHT - lineHeight) >> 1)) * tstep;
        }

        /* 2. Rasteryzacja pasa od góry do dołu (pełna linia cache 32 B na wiersz, zero overdraw) */
        uint32_t *row32 = fb32 + base_col;
        const int stride32 = FB_WIDTH / 2;

        /* Pas czystego nieba */
        for (int y = 0; y < min_start; y++) {
            uint32_t sc = sky_row32[y];
            row32[0] = sc; row32[1] = sc; row32[2] = sc; row32[3] = sc;
            row32[4] = sc; row32[5] = sc; row32[6] = sc; row32[7] = sc;
            row32 += stride32;
        }

        /* Pas mieszany (ściany + niebo/podłoga) */
        for (int y = min_start; y <= max_end; y++) {
            uint32_t bg = (y < half_h) ? sky_row32[y] : floor_row32[y - half_h];
            for (int c = 0; c < STRIP_COLS; c++) {
                if (y < drawStart[c] || y > drawEnd[c]) {
                    row32[c] = bg;
                } else {
                    uint32_t c16 = tex_col[c][((uint32_t)texPos[c] >> FP_SHIFT) & TEX_MASK];
                    texPos[c] += texStep[c];
                    row32[c] = c16 | (c16 << 16);
                }
            }
            row32 += stride32;
        }

        /* Pas czystej podłogi */
        for (int y = max_end + 1; y < FB_HEIGHT; y++) {
            uint32_t fc = floor_row32[y - half_h];
            row32[0] = fc; row32[1] = fc; row32[2] = fc; row32[3] = fc;
            row32[4] = fc; row32[5] = fc; row32[6] = fc; row32[7] = fc;
            row32 += stride32;
        }
    }

    /* 3. Sortowanie i rysowanie duszków */
    int order[NUM_SPRITES];
    int32_t dist2[NUM_SPRITES];
    int count = 0;

    for (int i = 0; i < NUM_SPRITES; i++) {
        if (!sprites[i].active) continue;
        int32_t dx = (sprites[i].x - posX) >> 8;
        int32_t dy = (sprites[i].y - posY) >> 8;
        order[count] = i;
        dist2[count] = dx * dx + dy * dy;
        count++;
    }

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (dist2[i] < dist2[j]) {
                int ti = order[i]; order[i] = order[j]; order[j] = ti;
                int32_t td = dist2[i]; dist2[i] = dist2[j]; dist2[j] = td;
            }
        }
    }

    int32_t bob = fp_sin16((uint16_t)(frame << 10)) >> 4;

    for (int i = 0; i < count; i++) {
        sprite_t *sp = &sprites[order[i]];
        int32_t spriteX = sp->x - posX;
        int32_t spriteY = sp->y - posY;

        int32_t transformX = fp_mul(INV_DET, fp_mul(dirY, spriteX) - fp_mul(dirX, spriteY));
        int32_t transformY = fp_mul(INV_DET, fp_mul(-planeY, spriteX) + fp_mul(planeX, spriteY));

        if (transformY <= 0x1800) continue;

        int spriteScreenX = half_h * 0 + (FB_WIDTH / 2) + (((transformX >> 4) * (FB_WIDTH / 2)) / (transformY >> 4));
        int spriteSize = (FB_HEIGHT << FP_SHIFT) / transformY;
        if (spriteSize <= 1 || spriteSize > 512) continue;

        int vMove = (int)(bob / (transformY >> 12));
        int drawStartY = ((FB_HEIGHT - spriteSize) >> 1) + vMove;
        int drawEndY   = drawStartY + spriteSize;
        int y0 = (drawStartY < 0) ? 0 : drawStartY;
        int y1 = (drawEndY >= FB_HEIGHT) ? (FB_HEIGHT - 1) : drawEndY;

        int drawStartX = spriteScreenX - (spriteSize >> 1);
        int drawEndX   = drawStartX + spriteSize;
        int x0 = (drawStartX < 0) ? 0 : drawStartX;
        int x1 = (drawEndX >= FB_WIDTH) ? (FB_WIDTH - 1) : drawEndX;

        const uint16_t *stex = sprite_tex[sp->type & 1];

        for (int stripe = x0; stripe < x1; stripe++) {
            if (transformY >= zbuffer[stripe >> 1]) continue;

            int texX = (((stripe - drawStartX) * SPR_SIZE) / spriteSize) & SPR_MASK;

            for (int y = y0; y <= y1; y++) {
                int texY = (((y - drawStartY) * SPR_SIZE) / spriteSize) & SPR_MASK;
                uint16_t color = stex[(texY << 5) | texX];
                if (color != 0x0000) {
                    backbuffer[y * FB_WIDTH + stripe] = color;
                }
            }
        }
    }

    /* 4. Celownik */
    int cx = FB_WIDTH / 2;
    int cy = FB_HEIGHT / 2;
    uint16_t cross_col = muzzle_flash ? COLOR_RED : COLOR_GREEN;
    for (int d = -5; d <= 5; d++) {
        if (d >= -1 && d <= 1) continue;
        backbuffer[cy * FB_WIDTH + (cx + d)] = cross_col;
        backbuffer[(cy + d) * FB_WIDTH + cx] = cross_col;
    }

    /* 5. Minimapa (zapis 32-bitowy) */
    const int mm_x0 = FB_WIDTH - (MAP_W * 4) - 4;
    const int mm_y0 = 4;

    for (int my = 0; my < MAP_H; my++) {
        for (int mx = 0; mx < MAP_W; mx++) {
            uint8_t tile = world_map[my][mx];
            uint16_t c16 = RGB565(12, 16, 24);
            if (tile == 1) c16 = RGB565(190, 60, 50);
            else if (tile == 2) c16 = RGB565(30, 150, 200);
            else if (tile == 3) c16 = RGB565(150, 105, 50);
            else if (tile == 4) c16 = RGB565(220, 180, 20);
            uint32_t c32 = ((uint32_t)c16 << 16) | c16;

            int px = mm_x0 + mx * 4;
            int py = mm_y0 + my * 4;
            for (int dy = 0; dy < 4; dy++) {
                uint32_t *r32 = (uint32_t *)&backbuffer[(py + dy) * FB_WIDTH + px];
                r32[0] = c32;
                r32[1] = c32;
            }
        }
    }

    for (int i = 0; i < NUM_SPRITES; i++) {
        if (!sprites[i].active) continue;
        int sx = mm_x0 + (sprites[i].x >> (FP_SHIFT - 2));
        int sy = mm_y0 + (sprites[i].y >> (FP_SHIFT - 2));
        uint16_t sc = (sprites[i].type == 0) ? COLOR_CYAN : COLOR_RED;
        backbuffer[sy * FB_WIDTH + sx] = sc;
        backbuffer[sy * FB_WIDTH + sx + 1] = sc;
    }

    int ppx = mm_x0 + (posX >> (FP_SHIFT - 2));
    int ppy = mm_y0 + (posY >> (FP_SHIFT - 2));
    for (int step = 0; step < 6; step++) {
        int lx = ppx + ((dirX * step) >> FP_SHIFT);
        int ly = ppy + ((dirY * step) >> FP_SHIFT);
        if ((unsigned)lx < FB_WIDTH && (unsigned)ly < FB_HEIGHT) {
            backbuffer[ly * FB_WIDTH + lx] = COLOR_WHITE;
        }
    }
    backbuffer[ppy * FB_WIDTH + ppx] = COLOR_YELLOW;
}

/* ============================================================================
 * 8. Kolizje, fizyka ruchu i pętla główna gry
 * ============================================================================ */

static int is_walkable(int32_t x, int32_t y) {
    const int32_t r = 0x3800;
    int x0 = (x - r) >> FP_SHIFT;
    int x1 = (x + r) >> FP_SHIFT;
    int y0 = (y - r) >> FP_SHIFT;
    int y1 = (y + r) >> FP_SHIFT;
    if ((unsigned)x0 >= MAP_W || (unsigned)x1 >= MAP_W ||
        (unsigned)y0 >= MAP_H || (unsigned)y1 >= MAP_H) {
        return 0;
    }
    return (world_map[y0][x0] == 0 && world_map[y0][x1] == 0 &&
            world_map[y1][x0] == 0 && world_map[y1][x1] == 0);
}

static void format_hud(char *buf, uint32_t fps, uint32_t score) {
    const char *p1 = "FPS: ";
    int idx = 0;
    while (*p1) buf[idx++] = *p1++;

    buf[idx++] = (char)('0' + ((fps / 10) % 10));
    buf[idx++] = (char)('0' + (fps % 10));
    buf[idx++] = ' ';
    buf[idx++] = ' ';

    const char *p2 = "PTS: ";
    while (*p2) buf[idx++] = *p2++;

    buf[idx++] = (char)('0' + ((score / 1000) % 10));
    buf[idx++] = (char)('0' + ((score / 100) % 10));
    buf[idx++] = (char)('0' + ((score / 10) % 10));
    buf[idx++] = (char)('0' + (score % 10));
    buf[idx] = '\0';
}

int main(void) {
    fb_init_double_buffer();
    generate_assets();
    reset_entities();

    if (UART.en) {
        uart_puts("RV32IM FPV Ready! WASD = move, Q/E = strafe, SPACE = fire\n");
    }

    int32_t posX = (1 << FP_SHIFT) + 0x8000;
    int32_t posY = (1 << FP_SHIFT) + 0x8000;
    uint16_t angle = 4096;

    int muzzle_flash = 0;
    uint32_t score = 0;
    uint32_t frame = 0;

    uint32_t fps_last_us = REG_TIMER_US;
    uint32_t fps_frames  = 0;
    uint32_t current_fps = 60;
    char hud_str[24];
    format_hud(hud_str, current_fps, score);

    while (1) {
        int32_t move_fwd    = 0;
        int32_t move_strafe = 0;
        int32_t rot_delta   = 0;
        int fire = 0;

        /* Odczyt komend z UART */
        int c;
        while ((c = uart_getc_nonblock()) >= 0) {
            if (c == 'w' || c == 'W') move_fwd = 0x3800;
            else if (c == 's' || c == 'S') move_fwd = -0x3800;
            else if (c == 'a' || c == 'A') rot_delta = 550;
            else if (c == 'd' || c == 'D') rot_delta = -550;
            else if (c == 'q' || c == 'Q') move_strafe = -0x3000;
            else if (c == 'e' || c == 'E') move_strafe = 0x3000;
            else if (c == ' ' || c == 'f' || c == 'F') fire = 1;
        }

        /* Aktualizacja obrotu i pozycji (ze ślizganiem po ścianach) */
        angle = (uint16_t)(angle + rot_delta);
        int32_t dirX = fp_cos16(angle);
        int32_t dirY = fp_sin16(angle);

        int32_t dx = fp_mul(dirX, move_fwd) + fp_mul(dirY, move_strafe);
        int32_t dy = fp_mul(dirY, move_fwd) - fp_mul(dirX, move_strafe);

        if (is_walkable(posX + dx, posY)) posX += dx;
        if (is_walkable(posX, posY + dy)) posY += dy;

        /* Strzał */
        if (fire) {
            muzzle_flash = 3;
            for (int i = 0; i < NUM_SPRITES; i++) {
                if (!sprites[i].active || sprites[i].type != 1) continue;
                int32_t sx = sprites[i].x - posX;
                int32_t sy = sprites[i].y - posY;
                int32_t forward = fp_mul(sx, dirX) + fp_mul(sy, dirY);
                int32_t lateral = fp_mul(sx, -dirY) + fp_mul(sy, dirX);
                if (lateral < 0) lateral = -lateral;
                if (forward > 0 && forward < (7 << FP_SHIFT) && lateral < 0x5800) {
                    sprites[i].active = 0;
                    score += 250;
                    break;
                }
            }
        } else if (muzzle_flash > 0) {
            muzzle_flash--;
        }

        /* Ruch dronów i zbieranie kryształów */
        int remaining = 0;
        for (int i = 0; i < NUM_SPRITES; i++) {
            if (!sprites[i].active) continue;
            remaining++;

            if (sprites[i].type == 1) {
                int32_t nx = sprites[i].x + sprites[i].vx;
                int32_t ny = sprites[i].y + sprites[i].vy;
                if (is_walkable(nx, ny)) {
                    sprites[i].x = nx;
                    sprites[i].y = ny;
                } else {
                    sprites[i].vx = -sprites[i].vx;
                    sprites[i].vy = -sprites[i].vy;
                }
            }

            int32_t dist_x = sprites[i].x - posX;
            int32_t dist_y = sprites[i].y - posY;
            if (dist_x < 0) dist_x = -dist_x;
            if (dist_y < 0) dist_y = -dist_y;
            if (sprites[i].type == 0 && dist_x < 0x7000 && dist_y < 0x7000) {
                sprites[i].active = 0;
                score += 100;
                remaining--;
            }
        }

        if (remaining == 0) {
            reset_entities();
        }

        led_set((uint8_t)((score / 100) | (muzzle_flash ? 0x3F : 0x00)));

        /* 1. Narysuj całą klatkę (scena 3D + HUD) */
        render_scene(posX, posY, dirX, dirY, frame, muzzle_flash);

        fps_frames++;
        uint32_t now_us = REG_TIMER_US;
        uint32_t elapsed = now_us - fps_last_us;
        if (elapsed >= 500000U) {
            current_fps = (fps_frames * 1000000U + (elapsed >> 1)) / elapsed;
            fps_frames = 0;
            fps_last_us = now_us;
        }

        format_hud(hud_str, current_fps, score);
        fb_set_cursor(8, 8);
        fb_print(hud_str);

        /* 2. Po narysowaniu całej klatki: dcache_flush -> vsync -> nowy addr w FB.ctrl */
        fb_swap_buffers();
        frame++;
    }

    return 0;
}