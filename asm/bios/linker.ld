OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ROM   (rx)  : ORIGIN = 0x80000000, LENGTH = 2K
    PSRAM (rwx) : ORIGIN = 0x00000000, LENGTH = 1M
}

SECTIONS
{
    .text : ALIGN(4)
    {
        KEEP(*(.text.init))
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        _etext = .;
    } > ROM

    _sidata = LOADADDR(.data);

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data*)
        *(.sdata*)
        . = ALIGN(4);
        _edata = .;
    } > PSRAM AT > ROM

    .font 0x80001000 :
    {
        KEEP(*(.font))
    } > ROM

    .bss : ALIGN(4)
    {
        _sbss = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > PSRAM
}