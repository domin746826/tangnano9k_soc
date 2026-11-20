CROSS   ?= riscv64-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv32im -mabi=ilp32 -Os -ffreestanding -nostdlib -Wall -Wextra
LDFLAGS := -T linker.ld -nostdlib -lgcc

all: hello_blink.bin

hello_blink.elf: main.c linker.ld
	$(CC) $(CFLAGS) main.c -o $@ $(LDFLAGS)

hello_blink.bin: hello_blink.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f hello_blink.elf hello_blink.bin

.PHONY: all clean