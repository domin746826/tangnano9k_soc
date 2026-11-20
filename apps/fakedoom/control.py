import sys
import threading
import time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
TICK_SEC = 0.020  # 50 Hz przy trzymaniu klawisza

KEY_MAP = {
    "w": b"w", "up": b"w",
    "s": b"s", "down": b"s",
    "a": b"a", "left": b"a",
    "d": b"d", "right": b"d",
    "q": b"q",
    "e": b"e",
    "space": b" ", "f": b" ",
}


def uart_reader(ser: serial.Serial, stop_event: threading.Event):
    while not stop_event.is_set():
        try:
            data = ser.read(ser.in_waiting or 1)
            if data:
                sys.stdout.write(data.decode("utf-8", errors="replace"))
                sys.stdout.flush()
        except serial.SerialException:
            break


def run_pynput(ser: serial.Serial, stop_event: threading.Event):
    from pynput import keyboard

    pressed = set()
    lock = threading.Lock()

    def normalize(key):
        if hasattr(key, "char") and key.char:
            return key.char.lower()
        name = getattr(key, "name", "")
        return name.lower() if name else ""

    def on_press(key):
        if key == keyboard.Key.esc:
            stop_event.set()
            return False
        k = normalize(key)
        if k in KEY_MAP:
            with lock:
                pressed.add(k)

    def on_release(key):
        k = normalize(key)
        with lock:
            pressed.discard(k)

    print(f"[pynput] Połączono z {PORT} @ {BAUD}")
    print("W/S = przód/tył, A/D = obrót, Q/E = krok w bok, SPACJA = strzał, ESC = wyjście")

    listener = keyboard.Listener(on_press=on_press, on_release=on_release)
    listener.start()

    try:
        while not stop_event.is_set() and listener.is_alive():
            with lock:
                payload = b"".join(KEY_MAP[k] for k in pressed if k in KEY_MAP)
            if payload:
                ser.write(payload)
            time.sleep(TICK_SEC)
    finally:
        listener.stop()


def run_tkinter(ser: serial.Serial, stop_event: threading.Event):
    import tkinter as tk

    root = tk.Tk()
    root.title(f"RV32IM FPV ({PORT})")
    root.geometry("360x140")
    root.configure(bg="#141824")
    root.resizable(False, False)

    pressed = set()
    tk.Label(
        root,
        text="Kliknij to okno i steruj:\n\nW/S - przód/tył | A/D - obrót\nQ/E - strafe | SPACJA - strzał | ESC - wyjście",
        fg="#e6edf3", bg="#141824", font=("Consolas", 9), justify="left"
    ).pack(pady=14, padx=14)

    status = tk.StringVar(value="Wciśnięte: -")
    tk.Label(root, textvariable=status, fg="#3fb950", bg="#141824", font=("Consolas", 10, "bold")).pack()

    def on_press(e):
        k = e.keysym.lower()
        if k == "escape":
            stop_event.set()
            root.destroy()
            return
        if k in KEY_MAP:
            pressed.add(k)

    def on_release(e):
        pressed.discard(e.keysym.lower())

    def tick():
        if stop_event.is_set():
            return
        if pressed:
            payload = b"".join(KEY_MAP[k] for k in list(pressed) if k in KEY_MAP)
            ser.write(payload)
            status.set("Wciśnięte: " + " + ".join(sorted(k.upper() for k in pressed)))
        else:
            status.set("Wciśnięte: -")
        root.after(int(TICK_SEC * 1000), tick)

    root.bind("<KeyPress>", on_press)
    root.bind("<KeyRelease>", on_release)
    root.bind("<FocusOut>", lambda _e: pressed.clear())
    root.after(int(TICK_SEC * 1000), tick)
    root.mainloop()


def run_terminal_raw(ser: serial.Serial, stop_event: threading.Event):
    print(f"[Terminal] Połączono z {PORT} @ {BAUD} (Ctrl+C = wyjście)")
    if sys.platform == "win32":
        import msvcrt
        while not stop_event.is_set():
            if msvcrt.kbhit():
                ch = msvcrt.getch()
                if ch == b"\x03":
                    break
                ser.write(ch)
            time.sleep(0.01)
    else:
        import select
        import termios
        import tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            while not stop_event.is_set():
                r, _, _ = select.select([sys.stdin], [], [], 0.02)
                if r:
                    ch = sys.stdin.buffer.read(1)
                    if ch == b"\x03":
                        break
                    ser.write(ch)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)


def main():
    ser = serial.Serial(PORT, BAUD, timeout=0.05)
    stop_event = threading.Event()
    threading.Thread(target=uart_reader, args=(ser, stop_event), daemon=True).start()

    try:
        try:
            run_pynput(ser, stop_event)
        except ImportError:
            try:
                run_tkinter(ser, stop_event)
            except Exception:
                run_terminal_raw(ser, stop_event)
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        ser.close()


if __name__ == "__main__":
    main()