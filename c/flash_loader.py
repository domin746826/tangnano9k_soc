#!/usr/bin/env python3
import sys, struct, zlib, serial, time

if len(sys.argv) < 3:
    print(f"Uzycie: {sys.argv[0]} <port_tty> <plik.bin> [entry_hex]")
    sys.exit(1)

port       = sys.argv[1]
bin_path   = sys.argv[2]
entry_addr = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x00600000

with open(bin_path, "rb") as f:
    data = f.read()

while len(data) % 4 != 0:
    data += b'\x00'

if len(data) > (76 * 1024 - 16):
    print("BLAD: Plik za duzy dla User Flash (max ~76 KB)!")
    sys.exit(1)

magic    = 0x394B4754 # "TG9K"
data_crc = zlib.crc32(data) & 0xFFFFFFFF
hdr_raw  = struct.pack("<IIII", magic, len(data), data_crc, entry_addr)
hdr_crc  = zlib.crc32(hdr_raw) & 0xFFFFFFFF

print(f"[*] Obraz: {len(data)} B, Entry: 0x{entry_addr:08X}, CRC32: 0x{data_crc:08X}")

with serial.Serial(port, 115200, timeout=5) as ser:
    ser.reset_input_buffer()
    print("[*] Synchronizacja z ROM bootloaderem...")
    ser.write(b'S')
    ack = ser.read(1)
    if ack != b'K':
        print(f"[-] Brak odpowiedzi 'K' po synchronizacji (odebrano: {ack})")
        sys.exit(1)

    print("[*] Wysylanie naglowka i kasowanie stron Flasha (ok. 1-3 sekundy)...")
    ser.write(hdr_raw + struct.pack("<I", hdr_crc))
    ack = ser.read(1)
    if ack != b'K':
        print(f"[-] Blad naglowka / kasowania (odebrano: {ack})")
        sys.exit(1)

    chunk_size = 256
    offset = 0
    while offset < len(data):
        chunk = data[offset : offset + chunk_size]
        c_crc = zlib.crc32(chunk) & 0xFFFFFFFF
        ser.write(chunk + struct.pack("<I", c_crc))

        ack = ser.read(1)
        if ack == b'K':
            offset += len(chunk)
            print(f"\r[+] Zapisano: {offset}/{len(data)} B ({offset*100//len(data)}%)", end="", flush=True)
        elif ack == b'E':
            print(f"\n[!] Blad CRC paczki na offsecie {offset}, ponawiam...")
        else:
            print(f"\n[-] Timeout lub nieznana odpowiedz: {ack}")
            sys.exit(1)

    print("\n[+] Wgrano pomyslnie! Logi z plytki (Ctrl+C aby wyjsc):")
    ser.timeout = 0.1
    try:
        while True:
            chunk = ser.read(256)
            if chunk:
                print(chunk.decode(errors="replace"), end="", flush=True)
    except KeyboardInterrupt:
        print("\nKoniec.")