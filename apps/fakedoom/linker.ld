OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 8M
}

SECTIONS
{
    . = 0x00000000;

    .text : {
        *(.text.init)
        *(.text .text.*)
    } > RAM

    .rodata : {
        . = ALIGN(4);
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > RAM

    .data : {
        . = ALIGN(4);
        *(.data .data.*)
        . = ALIGN(4);
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > RAM

    /* 0x00780000: bufor 1 (255 KB), 0x007C0000: bufor 0 / FB_BASE (255 KB) */
    _stack_top = 0x00780000;
}