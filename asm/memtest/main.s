# LED0 - CPU alive
# LED1 - 4 bytes OK
# LED2 - 1 KB OK
# LED3 - 64 KB OK (> 4 KB cache, so it forces writeback on cache)
# LED4 - 1 MB OK
# LED5 - 8 MB OK
#
# err : LED 0..N-1 on, LED N blinks

.equ RAM_BASE, 0x00000000
.equ SEED, 0xA5A5A5A5
.equ LED_ACTIVE_LOW, 1

.section .text
.globl _start

.macro STAGE size, leds
    li   a1, \size
    jal  ra, memtest
    bnez a0, fail
    li   s0, \leds
    mv   t0, s0
    jal  ra, leds_write
.endm

_start:
    lui  s1, 0xC0000 # led)
    li   s0, 0x01
    mv   t0, s0
    jal  ra, leds_write # cpu alive

    STAGE 4,         0x03 
    STAGE 1024,      0x07
    STAGE 0x10000,   0x0F
    STAGE 0x100000,  0x1F
    STAGE 0x800000,  0x3F

done:
    j    done


fail:
    addi s2, s0, 1
blink:
    mv   t0, s0
    jal  ra, leds_write
    jal  ra, delay
    or   t0, s0, s2
    jal  ra, leds_write
    jal  ra, delay
    j    blink


leds_write:
.if LED_ACTIVE_LOW
    xori t0, t0, -1
.endif
    sw   t0, 0(s1)
    ret

delay:
    lui  t1, 0x100
1:  addi t1, t1, -1
    bnez t1, 1b
    ret


memtest:
    li   t3, SEED
    li   t5, RAM_BASE
    add  t4, t5, a1

    mv   t0, t5
1:  xor  t1, t0, t3
    sw   t1, 0(t0)
    addi t0, t0, 4
    bne  t0, t4, 1b

    mv   t0, t5
2:  xor  t1, t0, t3
    lw   t2, 0(t0)
    bne  t2, t1, 9f
    xori t1, t1, -1
    sw   t1, 0(t0)
    addi t0, t0, 4
    bne  t0, t4, 2b

    mv   t0, t5
3:  xor  t1, t0, t3
    xori t1, t1, -1
    lw   t2, 0(t0)
    bne  t2, t1, 9f
    addi t0, t0, 4
    bne  t0, t4, 3b

    li   a0, 0
    ret
9:  li   a0, 1
    ret