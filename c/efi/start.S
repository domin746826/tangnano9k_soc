.section .text.start
.global _start

_start:
    lui t0, 0xC0000
    sw zero, 0(t0)
    li t1, '!' // send ! if everything works from psram
    sb t1, 8(t0)

    la sp, _stack_top

    la t0, __bss_start
    la t1, __bss_end
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:
    call main

halt:
    j halt