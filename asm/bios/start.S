.section .text.init
.global _start

_start:
    lui sp, 0x00700
    la a0, _sidata
    la a1, _sidata
    la a2, _edata
    bgeu a1, a2, copy_data_done

copy_data_loop:
    lw t0, 0(a0)
    sw t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, copy_data_loop

copy_data_done:
    la a0, _sbss
    la a1, _ebss
    bgeu a0, a1, clear_bss_done

clear_bss_loop:
    sw x0, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, clear_bss_loop

clear_bss_done:

    jal ra, main

halt:
    j halt
