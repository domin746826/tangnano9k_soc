all: bios.hex

bios.hex: main.s
	riscv-none-elf-gcc -march=rv32im -mabi=ilp32 -nostdlib -Wl,-Ttext=0x80000000 -o bios.elf main.s
	riscv-none-elf-objcopy -O binary bios.elf bios.bin
	truncate -s 2048 bios.bin && hexdump -v -e '1/4 "%08x\n"' bios.bin > bios.hex

clean:
	rm -f bios.elf bios.bin bios.hex
