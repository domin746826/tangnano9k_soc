
#include "soc.h"


int main(void) {
    FB.addr = 0x007C0000;
    FB.en = 1; 

    uint32_t start_line = FB.line;
    fillScreen(COLOR_BLACK);
    fb_puts(10, 10, "Hello world! Tang Nano 9k SoC!", COLOR_RED, COLOR_BLACK);
    dcache_flush();
    uint32_t end_line = FB.line;

    UART.baud = 521;
    UART.en = 1;
    uart_puts("Hello there uart!\n");
    return 0;
}