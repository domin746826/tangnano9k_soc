CROSS   ?= riscv64-elf-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
SIZE     = $(CROSS)size

CFLAGS   = -march=rv32im_zifencei -mabi=ilp32 -mstrict-align -Os -Wall -Wextra \
           -ffreestanding -nostdlib -fno-builtin \
           -ffunction-sections -fdata-sections -fno-unwind-tables

LDFLAGS  = -T linker.ld -nostdlib -Wl,--gc-sections

SRCS     = start.S main.c soc.c
TARGET   = bios

all: $(TARGET).hex

$(TARGET).elf: $(SRCS) linker.ld soc.h
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).bin
	@dd if=/dev/zero bs=1 count=8192 2>/dev/null | dd if=$< conv=notrunc of=$(TARGET)_pad.bin 2>/dev/null
	@hexdump -v -e '1/4 "%08x\n"' $(TARGET)_pad.bin > $@
	@rm -f $(TARGET)_pad.bin
	@echo "Wygenerowano $@"

dump: $(TARGET).elf
	$(OBJDUMP) -d -M no-aliases $< > $(TARGET).lst
	@less $(TARGET).lst

clean:
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).lst

.PHONY: all dump clean