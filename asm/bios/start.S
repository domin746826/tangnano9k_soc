.section .text.init
.global _start

_start:
    # 1. Ustaw wskaźnik stosu (sp / x2) w PSRAM (np. na końcu pierwszego 1 MB: 0x00100000)
    lui  sp, 0x00100

    # 2. Skopiuj sekcję .data (zmienne globalne z wartością początkową) z ROM do PSRAM
    la   a0, _sidata         # adres źródłowy w ROM
    la   a1, _sdata          # adres docelowy w PSRAM
    la   a2, _edata          # koniec sekcji .data w PSRAM
    bgeu a1, a2, copy_data_done

copy_data_loop:
    lw   t0, 0(a0)
    sw   t0, 0(a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, copy_data_loop

copy_data_done:
    # 3. Wyzeruj sekcję .bss (zmienne globalne niezainicjalizowane) w PSRAM
    la   a0, _sbss
    la   a1, _ebss
    bgeu a0, a1, clear_bss_done

clear_bss_loop:
    sw   x0, 0(a0)
    addi a0, a0, 4
    bltu a0, a1, clear_bss_loop

clear_bss_done:
    # 4. Skok do funkcji main() w C
    jal  ra, main

    # Jeśli main() kiedykolwiek zwróci wartość, zatrzymaj CPU w pętli
halt:
    j    halt

    .section .font, "a"
.global font_rom
font_rom:
    .incbin "font8x16.bin"