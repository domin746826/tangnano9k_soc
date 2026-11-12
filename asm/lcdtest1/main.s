# cuby.S - statyczny wzor kolorowych kostek (kafelki 32x32) na LCD 480x272 RGB565
#
# Framebuffer w PSRAM: 0x007C0000 (to samo co FB_BASE=21'h1F0000 w lcd_driver).
# Pisze po 2 piksele naraz (sw), potem "flushuje" D-cache, bo LCD czyta PSRAM
# bezposrednio, a Twoj dcache jest write-back i nie ma flusha:
# odczyt 4 KB z innego tagu wypycha wszystkie 64 linie.
#
# Uzywa tylko: lui addi slli srli andi xor or sub bne sw lw jal jalr
#
# Build (jak memtest):
#   riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
#       -Wl,-Ttext=0x80000000 -o cuby.elf cuby.S
#   riscv64-unknown-elf-objcopy -O binary cuby.elf cuby.bin

    .section .text
    .globl _start

_start:
    lui  s0, 0x7C0               # s0 = FB = 0x007C0000
    lui  s1, 0xC0000             # s1 = MMIO (LED)
    li   s2, 240                 # slow w wierszu (480 px / 2)
    li   s3, 272                 # wiersze
    mv   t0, s0                  # wskaznik zapisu
    li   t1, 0                   # y

row:
    srli t2, t1, 5               # ty = y / 32
    slli t3, t2, 3
    sub  t3, t3, t2              # 7*ty  (0..56)  -> zielony
    slli t3, t3, 5               # G << 5
    li   t4, 0                   # xw = indeks slowa (2 piksele) w wierszu

col:
    srli t5, t4, 4               # tx = xw / 16  (32 px)
    xor  t6, t5, t2
    andi t6, t6, 1               # szachownica (tx ^ ty) & 1
    sub  t6, zero, t6            # 0 albo 0xFFFFFFFF
    andi t6, t6, 27
    addi t6, t6, 4               # B = 4 albo 31
    or   t6, t6, t3              # | G
    slli a0, t5, 12              # R = 2*tx  (bity 15..11)
    or   t6, t6, a0              # kolor RGB565
    slli a0, t6, 16
    or   a0, a0, t6              # dwa piksele w slowie
    sw   a0, 0(t0)
    addi t0, t0, 4
    addi t4, t4, 1
    bne  t4, s2, col
    addi t1, t1, 1
    bne  t1, s3, row

    jal  ra, flush               # dopchnij reszte z cache'a do PSRAM

    li   t0, 0x3E                # LED0 swieci (aktywne niskie)
    sw   t0, 0(s1)
done:
    j    done

# wypycha wszystkie linie D-cache (64 x 64 B), czytajac 4 KB spod adresu 0
flush:
    li   t0, 0
    li   t1, 4096
1:  lw   t2, 0(t0)
    addi t0, t0, 64
    bne  t0, t1, 1b
    ret