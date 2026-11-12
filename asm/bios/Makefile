# Narzędzia toolchaina (zmień prefiks na riscv64-unknown-elf- jeśli takiego używasz w CachyOS/Archu)
CROSS   ?= riscv-none-elf-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
SIZE     = $(CROSS)size

# Flagi kompilacji:
# -march=rv32im     -> generuj sprzętowe MUL/DIV/REM!
# -mabi=ilp32       -> 32-bitowe int/wskaźniki
# -mstrict-align    -> zakaz niewyrównanych dostępów LW/SW/LH/SH!
# -Os               -> optymalizacja pod kątem rozmiaru kodu w 8 KB ROM
CFLAGS   = -march=rv32im -mabi=ilp32 -mstrict-align -Os -Wall -Wextra \
           -ffreestanding -nostdlib -fno-builtin

LDFLAGS  = -T linker.ld -nostdlib -Wl,--gc-sections

SRCS     = start.S main.c
TARGET   = bios

all: $(TARGET).hex

$(TARGET).elf: $(SRCS) linker.ld soc.h font8x16.bin
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@
	$(SIZE) $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Konwersja pliku binarnego na 2048 linii po 32-bit hex (8 KB) dla $readmemh
$(TARGET).hex: $(TARGET).bin
	@dd if=/dev/zero bs=1 count=8192 2>/dev/null | dd if=$< conv=notrunc of=$(TARGET)_pad.bin 2>/dev/null
	@hexdump -v -e '1/4 "%08x\n"' $(TARGET)_pad.bin > $@
	@rm -f $(TARGET)_pad.bin
	@echo "Wygenerowano $@ (2048 slow 32-bit = 8 KB)"

# Podgląd wygenerowanego asemblera (przydatne do sprawdzania, jak GCC używa mul/div/sh/sb)
dump: $(TARGET).elf
	$(OBJDUMP) -d -M no-aliases $< > $(TARGET).lst
	@less $(TARGET).lst

clean:
	rm -f $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).lst

.PHONY: all dump clean