OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* 8 KB BSRAM ROM pod 0x80000000 */
    ROM   (rx)  : ORIGIN = 0x80000000, LENGTH = 8K

    /* PSRAM na zmienne i stos (pierwszy 1 MB, a wyżej np. od 0x00100000 framebuffer) */
    PSRAM (rwx) : ORIGIN = 0x00000000, LENGTH = 1M
}

SECTIONS
{
    /* Kod programu i stałe lecą do 8 KB ROM */
    .text : ALIGN(4)
    {
        KEEP(*(.text.init))   /* _start musi być dokładnie pod 0x80000000 */
        *(.text*)
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        _etext = .;
    } > ROM

    /* Wartości początkowe zmiennych globalnych leżą w ROM (LMA), ale adresowane są w PSRAM (VMA) */
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

    /* Zmienne zerowane przy starcie (tylko w PSRAM, nie zajmują miejsca w ROM!) */
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