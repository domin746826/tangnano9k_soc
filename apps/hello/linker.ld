OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* 8 MB RAM od 0x00000000 (architektura von Neumanna) */
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 8M
}

SECTIONS
{
    . = 0x00000000;

    .text : {
        *(.text.init)       /* _start musi wylądować dokładnie pod 0x00000000 */
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

    /*
     * Górne 512 KB zarezerwowane na dwa bufory obrazu (480x272x2 = 0x3FC00 każdy):
     *   0x00780000 - Backbuffer
     *   0x007C0000 - Frontbuffer (FB_BASE)
     * Stos rośnie w dół od 0x00780000.
     */
    _stack_top = 0x00780000;
}