#ifndef SOC_H
#define SOC_H

#include <stdint.h>

#define MMIO_BASE 0xC0000000UL

// UARTCTRL 0xC000_0008
typedef union {
    volatile uint32_t raw;
    struct {
        volatile uint8_t tx;
        volatile uint8_t rx;
        union {
            volatile uint16_t cfg_raw;
            struct {
                volatile uint16_t en : 1;
                volatile uint16_t rx_rdy : 1;
                volatile uint16_t tx_rdy : 1;
                volatile uint16_t baud : 13; // sys_clk / baudrate
            };
        };
    };
} uart_reg_t;

// FBCTRL (0xC000_000C) + FBSTAT (0xC000_0010)
typedef struct {
    union {
        volatile uint32_t ctrl;
        struct {
            volatile uint32_t addr : 23;
            volatile uint32_t _res : 8;
            volatile uint32_t en : 1;
        };
    };
    union {
        volatile uint32_t stat;
        struct {
            volatile uint32_t line : 9;
            volatile uint32_t _pad : 23;
        };
    };
} fb_regs_t;


#define REG_LED (*(volatile uint32_t *)(MMIO_BASE + 0x00))
#define REG_BTN (*(volatile uint32_t *)(MMIO_BASE + 0x04))
#define UART (*(uart_reg_t *)(MMIO_BASE + 0x08))
#define FB (*(fb_regs_t  *)(MMIO_BASE + 0x0C))

#define REG_TIMER_US (*(volatile uint32_t *)(MMIO_BASE + 0x20))
#define REG_MTIME_LO (*(volatile uint32_t *)(MMIO_BASE + 0x24))
#define REG_MTIME_HI (*(volatile uint32_t *)(MMIO_BASE + 0x28))

// avr style (1<<BIOSFLASH_NVSTR) for fast use
#define BIOSFLASH_NVSTR 20
#define BIOSFLASH_PROG 19
#define BIOSFLASH_ERASE 18
#define BIOSFLASH_SE 17
#define BIOSFLASH_YE 16
#define BIOSFLASH_XE 15
#define BIOSFLASH_CTRL (*(volatile uint32_t *)(MMIO_BASE + 0x14))  // [20]nvstr [19]prog [18]erase [17]se [16]ye [15]xe [14:6]xadr [5:0]yadr
#define BIOSFLASH_DOUT (*(volatile uint32_t *)(MMIO_BASE + 0x18))
#define BIOSFLASH_DIN (*(volatile uint32_t *)(MMIO_BASE + 0x1C))

#define SPI_EN   0
#define SPI_BUSY 1
#define SPI_CS   2
#define SPI_CTRL (*(volatile uint32_t *)(MMIO_BASE + 0x2C)) // [31:16] sck divider, [2] cs (1=active/low), [1] busy, [0] en
#define SPI_DATA (*(volatile uint32_t *)(MMIO_BASE + 0x30)) // [7:0] data in, [15:8] data out


#define FB_BASE 0x007C0000UL
#define FB_WIDTH 480
#define FB_HEIGHT 272

#define FRAMEBUFFER ((uint16_t *)FB_BASE)
#define FRAMEBUFFER_32 ((uint32_t *)FB_BASE)

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3)))

#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

// #define FONT_ROM ((const uint8_t *)0x80001000UL)

void fb_putc(int x, int y, char c, uint16_t fg, uint16_t bg);

void fb_puts(int x, int y, char *s, uint16_t fg, uint16_t bg);


static inline void led_set(uint8_t mask) {
    REG_LED = (~mask) & 0x3F;
}

static inline void fb_putpixel(int x, int y, uint16_t color) {
    FRAMEBUFFER[y * FB_WIDTH + x] = color;
}

static inline void fb_wait_vblank(void) {
    while (FB.line < FB_HEIGHT);
}

static inline void uart_init(uint16_t baud_div) {
    UART.cfg_raw = (baud_div << 3) | 1;
}

static inline void uart_putc(char c) {
    while (!UART.tx_rdy);
    UART.tx = (uint8_t)c;
}

void uart_puts(char *s);

static inline int uart_getc_nonblock(void) {
    if (!UART.rx_rdy) return -1;
    uint8_t c = UART.rx;
    UART.rx = 0;
    return c;
}

static inline void dcache_flush(void) {
    __asm__ volatile ("fence" ::: "memory");
}

static inline void icache_flush(void) {
    __asm__ volatile ("fence.i" ::: "memory");
}

void fb_hline(int x, int y, int amount, uint16_t color);
void fillScreen(uint16_t color);

#endif // SOC_H