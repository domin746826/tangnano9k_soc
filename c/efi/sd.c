#include "soc.h"
#include "sd.h"


#define SPI_SET_DIV(div) do { \
    SPI_CTRL = (SPI_CTRL & 0x0000FFFFUL) | ((uint32_t)(div) << 16); \
} while(0)

static inline uint8_t spi_transfer(uint8_t out_byte) {
    while (SPI_CTRL & (1UL << SPI_BUSY));
    SPI_DATA = ((uint32_t)out_byte << 8);
    while (SPI_CTRL & (1UL << SPI_BUSY));
    return (uint8_t)(SPI_DATA & 0xFF);
}

static inline void sd_cs_select(void) {
    SPI_CTRL |= (1UL << SPI_CS);
    spi_transfer(0xFF);
}

static inline void sd_cs_deselect(void) {
    SPI_CTRL &= ~(1UL << SPI_CS);
    spi_transfer(0xFF);
}

uint8_t sd_is_sdhc = 0;

static uint8_t sd_send_cmd_keep_cs(uint8_t cmd, uint32_t arg, uint8_t crc) {
    sd_cs_select();

    spi_transfer(cmd);
    spi_transfer((arg >> 24) & 0xFF);
    spi_transfer((arg >> 16) & 0xFF);
    spi_transfer((arg >> 8) & 0xFF);
    spi_transfer(arg & 0xFF);
    spi_transfer(crc | 0x01);

    uint8_t resp = 0xFF;
    for (int i = 0; i < 16; i++) {
        resp = spi_transfer(0xFF);
        if ((resp & 0x80) == 0) break;
    }
    return resp;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t resp = sd_send_cmd_keep_cs(cmd, arg, crc);
    sd_cs_deselect();
    return resp;
}

uint8_t sd_init(void) {
    sd_is_sdhc = 0;
    SPI_CTRL &= ~(1UL << SPI_CS);
    {
        uint32_t t0 = REG_TIMER_US;
        while ((uint32_t)(REG_TIMER_US - t0) < 2000);
    }

    for (int i = 0; i < 12; i++) {
        spi_transfer(0xFF);
    }

    int retry = 100;
    uint8_t resp = 0xFF;
    while (retry--) {
        resp = sd_send_cmd(0x40, 0x00000000, 0x95);
        if (resp == 0x01) break;
        {
            uint32_t t0 = REG_TIMER_US;
            while ((uint32_t)(REG_TIMER_US - t0) < 5000);
        }
    }

    if (resp != 0x01) {
        return resp;
    }

    uint8_t is_sdv2 = 0;
    uint8_t r7[4] = {0};
    resp = sd_send_cmd_keep_cs(0x48, 0x000001AA, 0x87);
    if (resp == 0x01) {
        for (int i = 0; i < 4; i++) r7[i] = spi_transfer(0xFF);
    }
    sd_cs_deselect();

    if (resp == 0x01 && r7[2] == 0x01 && r7[3] == 0xAA) {
        is_sdv2 = 1;
    }

    uint32_t acmd41_arg = is_sdv2 ? 0x40000000UL : 0x00000000UL;
    uint8_t acmd41_crc = is_sdv2 ? 0x77 : 0xE5;
    retry = 1000;
    while (retry--) {
        sd_send_cmd(0x77, 0x00000000, 0x65);
        resp = sd_send_cmd(0x69, acmd41_arg, acmd41_crc);
        if (resp == 0x00) break;
        {
            uint32_t t0 = REG_TIMER_US;
            while ((uint32_t)(REG_TIMER_US - t0) < 2000);
        }
    }

    if (resp != 0x00) {
        return resp;
    }

    if (is_sdv2) {
        uint8_t ocr[4];
        resp = sd_send_cmd_keep_cs(0x7A, 0x00000000, 0xFD);
        for (int i = 0; i < 4; i++) ocr[i] = spi_transfer(0xFF);
        sd_cs_deselect();
        if (resp == 0x00 && (ocr[0] & 0x40)) {
            sd_is_sdhc = 1;
        }
    }
    SPI_SET_DIV(5);
    return 0x00;
}

int sd_read_sector(uint32_t lba, uint8_t *buffer) {
    uint32_t addr = sd_is_sdhc ? lba : (lba << 9);

    uint8_t resp = sd_send_cmd_keep_cs(0x51, addr, 0xFF);
    if (resp != 0x00) {
        sd_cs_deselect();
        return -1;
    }

    int timeout = 150000;
    while (timeout--) {
        resp = spi_transfer(0xFF);
        if (resp != 0xFF) break;
    }
    if (resp != 0xFE) {
        sd_cs_deselect();
        return -2;
    }

    for (int i = 0; i < 512; i++) {
        buffer[i] = spi_transfer(0xFF);
    }
    spi_transfer(0xFF);
    spi_transfer(0xFF);
    sd_cs_deselect();
    return 0;
}

int sd_write_sector(uint32_t lba, const uint8_t *buffer) {
    uint32_t addr = sd_is_sdhc ? lba : (lba << 9);
    uint8_t resp = sd_send_cmd_keep_cs(0x58, addr, 0xFF);
    if (resp != 0x00) {
        sd_cs_deselect();
        return -1;
    }
    spi_transfer(0xFF);
    spi_transfer(0xFE);
    for (int i = 0; i < 512; i++) {
        spi_transfer(buffer[i]);
    }

    spi_transfer(0xFF);
    spi_transfer(0xFF);
    uint8_t data_resp = spi_transfer(0xFF);
    if ((data_resp & 0x1F) != 0x05) {
        sd_cs_deselect();
        return -2;
    }
    int timeout = 300000;
    while (timeout--) {
        if (spi_transfer(0xFF) == 0xFF) break;
    }
    sd_cs_deselect();
    return (timeout > 0) ? 0 : -3;
}