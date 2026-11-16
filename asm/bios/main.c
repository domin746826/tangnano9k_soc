#include "soc.h"

#define BOOT_MAGIC  0x394B4754UL // "TG9K"
#define CHUNK_WORDS 64
#define MAX_FLASH_B (76 * 1024 - 16)

static void __attribute__((noinline)) delay_us(uint32_t val) {
    uint32_t start = REG_TIMER_US;
    while ((uint32_t)(REG_TIMER_US - start) < val);
}

static void __attribute__((noinline)) flash_cmd(uint32_t ctrl, uint32_t us) {
    BIOSFLASH_CTRL = ctrl;
    delay_us(us);
}

static uint32_t uart_get_u32(void) {
    uint32_t v = 0;
    for (int i = 0; i < 32; i += 8) {
        int c;
        while ((c = uart_getc_nonblock()) == -1);
        v |= (uint32_t)c << i;
    }
    return v;
}

static void uart_put_hex32(uint32_t val) {
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t n = (val >> i) & 0xF;
        uart_putc(n < 10 ? ('0' + n) : ('A' + n - 10));
    }
}

static uint32_t crc32_update(uint32_t crc, uint32_t word) {
    for (int b = 0; b < 32; b++) {
        uint32_t mix = (crc ^ word) & 1;
        crc >>= 1;
        if (mix) crc ^= 0xEDB88320UL;
        word >>= 1;
    }
    return crc;
}

static uint32_t flash_read_word(uint32_t word_idx) {
    uint32_t base = (word_idx & 0x7FFF) | (1UL << BIOSFLASH_XE) | (1UL << BIOSFLASH_YE);
    flash_cmd(base, 1);
    flash_cmd(base | (1UL << BIOSFLASH_SE), 1);
    uint32_t data = BIOSFLASH_DOUT;
    flash_cmd(0, 1);
    return data;
}

static void flash_erase_page(uint32_t page_idx) {
    uint32_t xe = ((page_idx << 9) & 0x7FFF) | (1UL << BIOSFLASH_XE);
    flash_cmd(xe | (1UL << BIOSFLASH_ERASE), 6);
    flash_cmd(xe | (1UL << BIOSFLASH_ERASE) | (1UL << BIOSFLASH_NVSTR), 120000);
    flash_cmd(xe | (1UL << BIOSFLASH_NVSTR), 6);
    flash_cmd(0, 15);
}

static void flash_program_word(uint32_t word_idx, uint32_t data) {
    uint32_t xe   = (word_idx & 0x7FFF) | (1UL << BIOSFLASH_XE);
    uint32_t prog = xe | (1UL << BIOSFLASH_PROG);
    uint32_t nv   = prog | (1UL << BIOSFLASH_NVSTR);

    BIOSFLASH_DIN = data;
    flash_cmd(prog, 6);
    flash_cmd(nv, 12);
    flash_cmd(nv | (1UL << BIOSFLASH_YE), 12);
    flash_cmd(nv, 2);
    flash_cmd(xe | (1UL << BIOSFLASH_NVSTR), 6);
    flash_cmd(0, 10);
}

static int boot_from_flash(uint32_t *entry_out) {
    (void)flash_read_word(0);

    uint32_t hdr[4]; // [0]=magic, [1]=size, [2]=crc32, [3]=entry
    for (int i = 0; i < 4; i++) {
        hdr[i] = flash_read_word(i);
    }

    if (hdr[0] != BOOT_MAGIC || hdr[1] == 0 || hdr[1] > MAX_FLASH_B || (hdr[1] & 3)) {
        uart_puts("[BIOS] Bad hdr\n");
        return 0;
    }

    uint32_t *dst  = (uint32_t *)hdr[3];
    uint32_t words = hdr[1] >> 2;
    uint32_t crc   = 0xFFFFFFFFUL;

    for (uint32_t i = 0; i < words; i++) {
        uint32_t w = flash_read_word(4 + i);
        dst[i] = w;
        crc = crc32_update(crc, w);
    }

    if (~crc != hdr[2]) {
        uart_puts("[BIOS] CRC err\n");
        return 0;
    }

    *entry_out = hdr[3];
    return 1;
}

static void uart_update_flash(void) {
    uart_puts("[BIOS] Flash upd\n");

    while ((uint8_t)uart_getc_nonblock() != 'S');
    uart_putc('K');

    uint32_t hdr[4];
    uint32_t calc = 0xFFFFFFFFUL;
    for (int i = 0; i < 4; i++) {
        hdr[i] = uart_get_u32();
        calc = crc32_update(calc, hdr[i]);
    }
    uint32_t hdr_crc = uart_get_u32();

    if (hdr[0] != BOOT_MAGIC || ~calc != hdr_crc || hdr[1] == 0 || hdr[1] > MAX_FLASH_B) {
        uart_putc('E');
        return;
    }

    uint32_t pages = (16 + hdr[1] + 2047) >> 11;
    for (uint32_t p = 0; p < pages; p++) {
        flash_erase_page(p);
    }

    for (int i = 0; i < 4; i++) {
        flash_program_word(i, hdr[i]);
    }
    uart_putc('K');

    uint32_t total_words = hdr[1] >> 2;
    uint32_t words_written = 0;
    uint32_t buf[CHUNK_WORDS];

    while (words_written < total_words) {
        uint32_t chunk = total_words - words_written;
        if (chunk > CHUNK_WORDS) chunk = CHUNK_WORDS;

        uint32_t crc = 0xFFFFFFFFUL;
        for (uint32_t i = 0; i < chunk; i++) {
            buf[i] = uart_get_u32();
            crc = crc32_update(crc, buf[i]);
        }

        if (~crc != uart_get_u32()) {
            uart_putc('E');
            continue;
        }

        for (uint32_t i = 0; i < chunk; i++) {
            flash_program_word(4 + words_written + i, buf[i]);
        }
        words_written += chunk;
        uart_putc('K');
    }
    uart_puts("[BIOS] Done\n");
}

int main(void) {
    UART.baud = 521;
    UART.en   = 1;

    uart_puts("TG9K BIOS\n");

    if ((REG_BTN & 1) == 0) {
        uart_update_flash();
    }

    uint32_t entry_addr;
    while (1) {
        if (boot_from_flash(&entry_addr)) {
            uart_puts("[BIOS] Jump ");
            uart_put_hex32(entry_addr);
            uart_puts("\n");
            while (!UART.tx_rdy);

            dcache_flush();
            icache_flush();

            ((void (*)(void))entry_addr)();
        }
        uart_update_flash();
    }
}