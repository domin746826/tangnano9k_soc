CROSS   ?= riscv64-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv32im -mabi=ilp32 -O3 -ffreestanding -nostdlib -Wall -Wextra
LDFLAGS := -T linker.ld -nostdlib -lgcc

all: fpv_game.bin

fpv_game.elf: main.c linker.ld
	$(CC) $(CFLAGS) main.c -o $@ $(LDFLAGS)

fpv_game.bin: fpv_game.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f fpv_game.elf fpv_game.bin

.PHONY: all clean