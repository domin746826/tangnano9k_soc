#include "../../c/efi/soc.h"

/* Fallback dzielnika UART (sys_clk / baudrate), używany tylko gdy UART.en == 0 */
#ifndef BAUD_DIV
#define BAUD_DIV (50000000UL / 115200UL)
#endif

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

void delay_us(uint32_t del) {
    uint32_t start = REG_TIMER_US;
    while ((uint32_t)(REG_TIMER_US - start) < del) {
        __asm__ volatile ("nop");
    }
}

void uart_puts(char *s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s++);
    }
}

int main(void) {
    if (!UART.en) {
        uart_init(BAUD_DIV);
    }

    uart_puts("Hello, World! (RV32IM @ 0x00000000)\n");

    uint8_t led_state = 0x01;
    uint32_t tick = 0;

    while (1) {
        led_set(led_state);
        led_state = (led_state & 0x20) ? 0x01 : (uint8_t)(led_state << 1);

        if ((tick & 1U) == 0) {
            uart_puts("blink\n");
        }

        tick++;
        delay_us(250000); /* 250 ms */
    }

    return 0;
}