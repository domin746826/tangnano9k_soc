.section .text
.globl _start

_start:
    lui  x1, 0xC0000      # x1 = 0xC0000000 (adres MMIO LED)
    addi x2, x0, 0x2A     # x2 = 0b101010 (0x2A)
    sw   x2, 0(x1)        # zapisz x2 na ledy pod 0xC0000000

halt:
    jal  x0, halt         # nieskończona pętla w miejscu (offset 0)