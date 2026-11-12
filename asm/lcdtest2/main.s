# cube_m.S (RV32IM, bez div) - obracajacy sie szescian 3D (wireframe, perspektywa) na LCD 480x272 RGB565
#
# * mnozenie sprzetowe (instrukcja mul), zero tablic w ROM (tylko rejestry + 128 B scratch)
# * katy: dwa oscylatory 2D "magic circle" (c,s) w Q14 - stabilne, bez dryfu;
#   macierz obrotu liczona od nowa co klatke z (ca,sa,cb,sb)
# * perspektywa: x' = x*(256 - z) >> 8  (instrukcja mul)
# * 12 krawedzi: Bresenham, kasowanie poprzedniej klatki = przerysowanie
#   starych krawedzi na czarno (bez czyszczenia calego ekranu)
# * po kazdej klatce flush D-cache (LCD czyta PSRAM prosto, dcache jest write-back)
#
# Framebuffer: 0x007C0000 (jak FB_BASE w lcd_driver). Scratch: DATA = 0x00300000.
# Uzywa tylko: lui addi slli srli srai andi xori xor or and add sub mul
#              beq bne blt bge bgeu lw sw sh jal jalr
#
# Build:
#   riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -static \
#       -Wl,-Ttext=0x80000000 -o cube_m.elf cube_m.S
#   riscv64-unknown-elf-objcopy -O binary cube_m.elf cube_m.bin

    .equ KA, 6                   # predkosc obrotu wokol Y: ~1/2^KA rad na klatke
    .equ KB, 7                   # predkosc obrotu wokol X (inna => tumbling)
    .equ SIZE, 64                # polowa krawedzi w pikselach (z M mozna wiekszy)
    .equ DELAY, 30000            # petla opoznienia na klatke (~60 fps przy ~8 MIPS); 0 = bez limitu

    # rejestry trwale:
    #   s0 = framebuffer      s1 = bufor wierzcholkow NEW (OLD = s1 ^ 64)
    #   s2 = ca  s3 = sa  s4 = cb  s5 = sb   (Q14)
    # baza obrotu (tylko w obrebie klatki):
    #   s6=ex.x s7=ex.y s8=ex.z   s9=ey.y s10=ey.z   s11=ez.x gp=ez.y tp=ez.z   (ey.x = 0)

    .section .text
    .globl _start

_start:
    lui  s0, 0x7C0               # FB   = 0x007C0000
    lui  s1, 0x300               # DATA = 0x00300000
    li   s2, 15137               # kat poczatkowy ~22.5 deg (cos, sin w Q14)
    li   s3, 6270
    li   s4, 15137
    li   s5, 6270

    # czysty ekran
    mv   t0, s0
    li   t1, 0x3FC00             # 480*272*2 bajtow
    add  t1, t1, s0
1:  sw   zero, 0(t0)
    addi t0, t0, 4
    bne  t0, t1, 1b

    # wyzeruj bufory wierzcholkow NEW i OLD (2 x 64 B)
    mv   t0, s1
    addi t1, s1, 128
1:  sw   zero, 0(t0)
    addi t0, t0, 4
    bne  t0, t1, 1b

    jal  ra, flush

# ---------------------------------------------------------------- klatka
frame:
    # --- oscylatory katow:  s += c>>k ; c -= s>>k
    srai t0, s2, KA
    add  s3, s3, t0
    srai t0, s3, KA
    sub  s2, s2, t0
    srai t0, s4, KB
    add  s5, s5, t0
    srai t0, s5, KB
    sub  s4, s4, t0

    # --- macierz obrotu R = Rx(b)*Ry(a); kolumny:
    #     ex = ( ca, sb*sa, -cb*sa )   ey = ( 0, cb, sb )   ez = ( sa, -sb*ca, cb*ca )
    mul  s7, s5, s3
    srai s7, s7, 14              # ex.y =  sb*sa
    mul  s8, s4, s3
    srai s8, s8, 14
    sub  s8, zero, s8            # ex.z = -cb*sa
    mul  gp, s5, s2
    srai gp, gp, 14
    sub  gp, zero, gp            # ez.y = -sb*ca
    mul  tp, s4, s2
    srai tp, tp, 14              # ez.z =  cb*ca
    mv   s6, s2                  # ex.x = ca
    mv   s9, s4                  # ey.y = cb
    mv   s10, s5                 # ey.z = sb
    mv   s11, s3                 # ez.x = sa

    # --- 8 wierzcholkow (+-1,+-1,+-1) -> ekran, zapis do [s1 + i*8]
    li   t6, 0                   # i
vloop:
    andi t2, t6, 1
    addi t2, t2, -1              # maska znaku x: 0 (=+) albo -1 (=-)
    srli t3, t6, 1
    andi t3, t3, 1
    addi t3, t3, -1              # y
    srli t4, t6, 2
    andi t4, t4, 1
    addi t4, t4, -1              # z

    xor  a0, s6, t2              # X = ex.x*sx + ez.x*sz
    sub  a0, a0, t2
    xor  t0, s11, t4
    sub  t0, t0, t4
    add  a0, a0, t0

    xor  a1, s7, t2              # Y = ex.y*sx + ey.y*sy + ez.y*sz
    sub  a1, a1, t2
    xor  t0, s9, t3
    sub  t0, t0, t3
    add  a1, a1, t0
    xor  t0, gp, t4
    sub  t0, t0, t4
    add  a1, a1, t0

    xor  a2, s8, t2              # Z = ex.z*sx + ey.z*sy + ez.z*sz
    sub  a2, a2, t2
    xor  t0, s10, t3
    sub  t0, t0, t3
    add  a2, a2, t0
    xor  t0, tp, t4
    sub  t0, t0, t4
    add  a2, a2, t0

    li   t0, SIZE                # Q14 -> piksele: *SIZE >> 14
    mul  a0, a0, t0
    srai a0, a0, 14
    mul  a1, a1, t0
    srai a1, a1, 14
    mul  a2, a2, t0
    srai a2, a2, 14

    li   a3, 256                 # perspektywa: s = 256 - z
    sub  a3, a3, a2
    mul  a0, a0, a3
    srai a0, a0, 8
    mul  a1, a1, a3
    srai a1, a1, 8
    addi a0, a0, 240             # srodek ekranu
    addi a1, a1, 136

    slli t0, t6, 3
    add  t0, t0, s1
    sw   a0, 0(t0)
    sw   a1, 4(t0)
    addi t6, t6, 1
    li   t0, 8
    bne  t6, t0, vloop

    # --- dwa przebiegi: (0) skasuj stare krawedzie na czarno, (1) narysuj nowe
    xori a5, s1, 64              # baza wierzcholkow OLD
    li   a6, 0                   # kolor czarny
pass:
    li   a7, 0                   # i
e_i:
    li   gp, 1                   # b = 1, 2, 4
e_b:
    and  t0, a7, gp
    bnez t0, e_next              # krawedz (i, i|b) tylko gdy bit b w i == 0
    slli t1, a7, 3
    add  t1, t1, a5
    lw   a0, 0(t1)
    lw   a1, 4(t1)
    or   t0, a7, gp
    slli t1, t0, 3
    add  t1, t1, a5
    lw   a2, 0(t1)
    lw   a3, 4(t1)
    mv   a4, a6
    jal  ra, line
e_next:
    slli gp, gp, 1
    li   t0, 8
    bne  gp, t0, e_b
    addi a7, a7, 1
    bne  a7, t0, e_i
    bnez a6, pass_done
    mv   a5, s1                  # drugi przebieg: nowe wierzcholki, kolor bialy
    li   a6, -1
    j    pass
pass_done:

    jal  ra, flush
    xori s1, s1, 64              # NEW <-> OLD

    li   t0, DELAY               # ogranicznik FPS
    beqz t0, frame
1:  addi t0, t0, -1
    bnez t0, 1b
    j    frame

# ---------------------------------------------------------------- procedury

# linia Bresenhama (a0,a1)-(a2,a3), kolor RGB565 w a4; niszczy a0-a3, t0-t6
# piksele poza ekranem sa pomijane (unsigned x<480, y<272)
line:
    sub  t0, a2, a0              # dx = |x1-x0|, sx
    li   t2, 1
    bge  t0, zero, 1f
    sub  t0, zero, t0
    li   t2, -1
1:  sub  t1, a3, a1              # dy = -|y1-y0|, sy
    li   t3, 1
    bge  t1, zero, 2f
    sub  t1, zero, t1
    li   t3, -1
2:  sub  t1, zero, t1
    add  t4, t0, t1              # err = dx + dy
l_loop:
    li   t5, 480
    bgeu a0, t5, l_skip
    li   t5, 272
    bgeu a1, t5, l_skip
    li   t6, 960                 # adres = FB + y*960 + x*2
    mul  t5, a1, t6
    slli t6, a0, 1
    add  t5, t5, t6
    add  t5, t5, s0
    sh   a4, 0(t5)
l_skip:
    bne  a0, a2, l_cont
    beq  a1, a3, l_done
l_cont:
    slli t5, t4, 1               # e2 = 2*err
    blt  t5, t1, l_nox           # e2 < dy  -> bez kroku w x
    add  t4, t4, t1
    add  a0, a0, t2
l_nox:
    blt  t0, t5, l_noy           # dx < e2  -> bez kroku w y
    add  t4, t4, t0
    add  a1, a1, t3
l_noy:
    j    l_loop
l_done:
    ret

# wypycha wszystkie linie D-cache (64 x 64 B), czytajac 4 KB spod adresu 0
flush:
    li   t0, 0
    li   t1, 4096
1:  lw   t2, 0(t0)
    addi t0, t0, 64
    bne  t0, t1, 1b
    ret