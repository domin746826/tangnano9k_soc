CROSS   ?= riscv64-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv32im -mabi=ilp32 -O3 -ffreestanding -nostdlib -Wall -Wextra
LDFLAGS := -T linker.ld -nostdlib -lgcc

all: cube_demo.bin

cube_demo.elf: main.c linker.ld
	$(CC) $(CFLAGS) main.c -o $@ $(LDFLAGS)

cube_demo.bin: cube_demo.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f cube_demo.elf cube_demo.bin

.PHONY: all clean