CROSS   ?= riscv64-elf-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
SIZE     = $(CROSS)size

CFLAGS   = -march=rv32im -mabi=ilp32 -mstrict-align -Os -Wall -Wextra \
           -ffreestanding -nostdlib -fno-builtin

LDFLAGS  = -T linker.ld -nostdlib -Wl,--gc-sections

SRCS     = start.S main.c soc.c sd.c gpt_fat32.c fb_printf.c
TARGET   = app

all: $(TARGET).bin

$(TARGET).elf: $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@ 
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(TARGET).elf $(TARGET).bin