.section .text
.globl _start

_start:
    lui  x1, 0xC0000
    addi x2, x0, 0x2A

loop:
    sw   x2, 0(x1)
    xori x2, x2, 0x3F 
    lui  x3, 0x00300 

delay:
    addi x3, x3, -1 # dekr
    bne  x3, x0, delay
    jal  x0, loop