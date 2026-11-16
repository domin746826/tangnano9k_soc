OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY {
    /* 1 MB przestrzeni od 6 MB (0x00600000) do 7 MB (0x00700000) */
    PSRAM_APP (rwx) : ORIGIN = 0x00600000, LENGTH = 1024K
}

SECTIONS {
    . = 0x00600000;

    .text : ALIGN(4) {
        KEEP(*(.text.start))
        *(.text*)
    } > PSRAM_APP

    .rodata : ALIGN(4) {
        *(.rodata*)
        *(.srodata*)
    } > PSRAM_APP

    .data : ALIGN(4) {
        *(.data*)
        *(.sdata*)
    } > PSRAM_APP

    .bss : ALIGN(4) {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > PSRAM_APP

    /* Szczyt stosu na 7 MB (0x00700000) - w RISC-V stos rosnie w dol! */
    _stack_top = 0x00700000;
}