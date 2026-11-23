#!/usr/bin/env python3
"""
Клиентская программа для ПК (Лабораторная работа 3, Вариант 2 «Компас»).
Запуск:
    python3 compass_client.py /dev/ttyUSB1
    python3 compass_client.py /dev/ttyUSB1 --tui
"""

import curses
import datetime
import math
import os
import select
import sys
import termios
import threading
import time

BAUD_RATE = 38400
LOG_FILE_PATH = "uart_compass.log"


class SerialChannel:
    """Работа с последовательным портом через POSIX termios без внешних зависимостей."""

    def __init__(self, port_path: str, log_path: str = LOG_FILE_PATH):
        self.port_path = port_path
        self.log_path = log_path
        self.fd = -1
        self.rx_buf = bytearray()
        self.lock = threading.Lock()
        self._open_port()

    def _open_port(self) -> None:
        self.fd = os.open(self.port_path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = termios.B38400
        attrs[5] = termios.B38400
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)

    def log_message(self, direction: str, text: str) -> None:
        ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        with self.lock:
            with open(self.log_path, "a", encoding="utf-8") as f:
                f.write(f"[{ts}] {direction}: {text}\n")

    def send_command(self, cmd: str) -> None:
        line = cmd.strip()
        if not line:
            return
        payload = (line + "\r\n").encode("ascii", errors="ignore")
        os.write(self.fd, payload)
        self.log_message("TX", line)

    def poll_lines(self) -> list[str]:
        lines: list[str] = []
        rlist, _, _ = select.select([self.fd], [], [], 0.0)
        if not rlist:
            return lines
        try:
            chunk = os.read(self.fd, 512)
        except BlockingIOError:
            return lines
        if not chunk:
            return lines
        self.rx_buf.extend(chunk)
        while b"\n" in self.rx_buf:
            raw_line, _, rest = self.rx_buf.partition(b"\n")
            self.rx_buf = bytearray(rest)
            text = raw_line.decode("ascii", errors="ignore").strip()
            if text:
                self.log_message("RX", text)
                lines.append(text)
        return lines

    def close(self) -> None:
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


class CompassModel:
    def __init__(self):
        self.connected = False
        self.sensor_ok = False
        self.x = 0
        self.y = 0
        self.z = 0
        self.sx = 100
        self.sy = 100
        self.sz = 100
        self.deg = 0
        self.vx = 0
        self.vy = -18
        self.last_status = "Отправлен PING, ожидание ответа стенда..."

    def handle_line(self, line: str) -> None:
        if line == "PONG" or line.startswith("READY"):
            self.connected = True
            self.last_status = f"Стенд на связи ({line})"
            return
        if line == "OK":
            self.connected = True
            self.last_status = "Масштабные коэффициенты применены (OK)"
            return
        if line == "ERR":
            self.last_status = "Ошибка команды на стенде (ERR)"
            return
        if line.startswith("DATA "):
            self.connected = True
            parts = line.split()[1:]
            kv = {}
            for p in parts:
                if "=" in p:
                    k, v = p.split("=", 1)
                    try:
                        kv[k] = int(v)
                    except ValueError:
                        pass
            self.x = kv.get("X", self.x)
            self.y = kv.get("Y", self.y)
            self.z = kv.get("Z", self.z)
            self.sx = kv.get("SX", self.sx)
            self.sy = kv.get("SY", self.sy)
            self.sz = kv.get("SZ", self.sz)
            self.deg = kv.get("DEG", self.deg)
            self.vx = kv.get("VX", self.vx)
            self.vy = kv.get("VY", self.vy)
            self.sensor_ok = bool(kv.get("OK", 1))


def run_tui(channel: SerialChannel, model: CompassModel) -> None:
    def _curses_main(stdscr):
        curses.curs_set(0)
        stdscr.nodelay(True)
        stdscr.timeout(50)

        edit_vals = [model.sx, model.sy, model.sz]
        selected_axis = 0

        channel.send_command("PING")
        last_ping = time.monotonic()

        while True:
            for line in channel.poll_lines():
                model.handle_line(line)

            now = time.monotonic()
            if not model.connected and (now - last_ping) > 1.0:
                channel.send_command("PING")
                last_ping = now

            ch = stdscr.getch()
            if ch in (ord("q"), ord("Q")):
                break
            elif ch == curses.KEY_UP:
                selected_axis = (selected_axis - 1) % 3
            elif ch == curses.KEY_DOWN:
                selected_axis = (selected_axis + 1) % 3
            elif ch == curses.KEY_LEFT:
                edit_vals[selected_axis] = max(0, edit_vals[selected_axis] - 5)
            elif ch == curses.KEY_RIGHT:
                edit_vals[selected_axis] = min(200, edit_vals[selected_axis] + 5)
            elif ch in (ord("r"), ord("R")):
                edit_vals = [100, 100, 100]
                channel.send_command("SCALE 100 100 100")
            elif ch in (ord("p"), ord("P")):
                channel.send_command("PING")
            elif ch in (10, 13, curses.KEY_ENTER):
                channel.send_command(
                    f"SCALE {edit_vals[0]} {edit_vals[1]} {edit_vals[2]}"
                )

            stdscr.erase()
            stdscr.addstr(0, 2, "=== SDK-1.1M ЛР3 Вариант 2: КОМПАС (LSM9DS) ===")
            conn_str = "ПОДКЛЮЧЕН" if model.connected else "ОЖИДАНИЕ ОТВЕТА..."
            stdscr.addstr(1, 2, f"Порт: {channel.port_path} | Статус: {conn_str} | Лог: {channel.log_path}")
            stdscr.addstr(2, 2, f"Датчик: {'OK' if model.sensor_ok else 'НЕТ ОТВЕТА'}")

            stdscr.addstr(4, 2, "Показания магнитного поля (с учётом масштаба):")
            stdscr.addstr(5, 4, f"Ось X : {model.x:6d} мГс   (применён масштаб: {model.sx:3d} %)")
            stdscr.addstr(6, 4, f"Ось Y : {model.y:6d} мГс   (применён масштаб: {model.sy:3d} %)")
            stdscr.addstr(7, 4, f"Ось Z : {model.z:6d} мГс   (применён масштаб: {model.sz:3d} %)")
            stdscr.addstr(8, 4, f"Азимут: {model.deg:3d} град")

            stdscr.addstr(10, 2, "Настройка масштабных коэффициентов (0..200 %):")
            names = ["Масштаб X", "Масштаб Y", "Масштаб Z"]
            for i in range(3):
                marker = "->" if i == selected_axis else "  "
                stdscr.addstr(11 + i, 4, f"{marker} {names[i]}: [ {edit_vals[i]:3d} % ]")

            stdscr.addstr(15, 2, "Управление: Стрелки Вверх/Вниз — выбор оси, Влево/Вправо — шаг 5 %")
            stdscr.addstr(16, 2, "            Enter — отправить на стенд, R — сброс 100%, P — PING, Q — выход")
            stdscr.addstr(18, 2, f"Сообщение: {model.last_status[:70]}")

            # Псевдографический круговой компас справа
            cx, cy, r = 64, 8, 5
            stdscr.addstr(cy - r - 1, cx, "N")
            stdscr.addstr(cy + r + 1, cx, "S")
            stdscr.addstr(cy, cx - r * 2 - 2, "W")
            stdscr.addstr(cy, cx + r * 2 + 2, "E")
            for a_deg in range(0, 360, 15):
                rad = math.radians(a_deg)
                px = int(round(cx + math.cos(rad) * r * 2))
                py = int(round(cy + math.sin(rad) * r))
                if 0 <= py < 22 and 0 <= px < 78:
                    stdscr.addch(py, px, ".")

            rad = math.radians(model.deg)
            for step in range(1, r + 1):
                px = int(round(cx + math.cos(rad) * step * 2))
                py = int(round(cy - math.sin(rad) * step))
                if 0 <= py < 22 and 0 <= px < 78:
                    stdscr.addch(py, px, "*" if step == r else "+")
            stdscr.addch(cy, cx, "O")

            stdscr.refresh()

    curses.wrapper(_curses_main)


def run_gui(channel: SerialChannel, model: CompassModel) -> None:
    import tkinter as tk
    from tkinter import ttk

    root = tk.Tk()
    root.title(f"SDK-1.1M Компас (LSM9DS) — {channel.port_path}")
    root.geometry("860x520")
    root.minsize(720, 440)
    root.resizable(True, True)

    root.columnconfigure(0, weight=1)
    root.rowconfigure(0, weight=1)

    main_frame = ttk.Frame(root, padding=14)
    main_frame.grid(row=0, column=0, sticky="nsew")
    main_frame.columnconfigure(0, weight=1, minsize=340)
    main_frame.columnconfigure(1, weight=1, minsize=320)
    main_frame.rowconfigure(0, weight=1)

    left = ttk.Frame(main_frame)
    left.grid(row=0, column=0, sticky="nsew", padx=(0, 12))

    right = ttk.Frame(main_frame)
    right.grid(row=0, column=1, sticky="nsew")
    right.columnconfigure(0, weight=1)
    right.rowconfigure(0, weight=1)

    lbl_conn = ttk.Label(left, text="Подключение...", font=("Sans", 11, "bold"))
    lbl_conn.pack(anchor=tk.W, pady=(0, 8))

    lbl_x = ttk.Label(left, text="X: 0 мГс (100%)", font=("Monospace", 11))
    lbl_x.pack(anchor=tk.W, pady=2)
    lbl_y = ttk.Label(left, text="Y: 0 мГс (100%)", font=("Monospace", 11))
    lbl_y.pack(anchor=tk.W, pady=2)
    lbl_z = ttk.Label(left, text="Z: 0 мГс (100%)", font=("Monospace", 11))
    lbl_z.pack(anchor=tk.W, pady=2)
    lbl_deg = ttk.Label(left, text="Азимут: 0°", font=("Monospace", 11, "bold"))
    lbl_deg.pack(anchor=tk.W, pady=(6, 12))

    scales_box = ttk.LabelFrame(left, text="Масштабные коэффициенты (0..200 %)", padding=10)
    scales_box.pack(fill=tk.X, pady=4)

    var_sx = tk.IntVar(value=100)
    var_sy = tk.IntVar(value=100)
    var_sz = tk.IntVar(value=100)

    for row_idx, (title, var) in enumerate([("Ось X (%):", var_sx), ("Ось Y (%):", var_sy), ("Ось Z (%):", var_sz)]):
        ttk.Label(scales_box, text=title).grid(row=row_idx, column=0, sticky=tk.W, pady=4)
        spin = ttk.Spinbox(scales_box, from_=0, to=200, increment=5, textvariable=var, width=10)
        spin.grid(row=row_idx, column=1, padx=8, pady=4, sticky=tk.W)

    def send_scales():
        try:
            sx = max(0, min(200, int(var_sx.get())))
            sy = max(0, min(200, int(var_sy.get())))
            sz = max(0, min(200, int(var_sz.get())))
        except (ValueError, tk.TclError):
            sx, sy, sz = 100, 100, 100
        var_sx.set(sx)
        var_sy.set(sy)
        var_sz.set(sz)
        channel.send_command(f"SCALE {sx} {sy} {sz}")

    def reset_scales():
        var_sx.set(100)
        var_sy.set(100)
        var_sz.set(100)
        send_scales()

    btn_frame = ttk.Frame(left)
    btn_frame.pack(fill=tk.X, pady=10)
    ttk.Button(btn_frame, text="Применить масштаб", command=send_scales).pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(btn_frame, text="Сброс 100%", command=reset_scales).pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(btn_frame, text="Калибровка", command=lambda: channel.send_command("CAL")).pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(btn_frame, text="PING", command=lambda: channel.send_command("PING")).pack(side=tk.LEFT)

    lbl_status = ttk.Label(left, text="", foreground="#444444", wraplength=360)
    lbl_status.pack(anchor=tk.W, pady=(6, 0))

    canvas = tk.Canvas(right, width=320, height=320, bg="#101418", highlightthickness=1)
    canvas.grid(row=0, column=0, sticky="nsew")

    channel.send_command("PING")

    def tick():
        for line in channel.poll_lines():
            model.handle_line(line)

        sensor_tag = "ОК" if model.sensor_ok else "НЕТ ДАТЧИКА"
        lbl_conn.config(
            text=f"Стенд на связи | Магнитометр: {sensor_tag}"
            if model.connected
            else "Ожидание ответа от стенда..."
        )
        lbl_x.config(text=f"X: {model.x:6d} мГс  (масштаб {model.sx:3d}%)")
        lbl_y.config(text=f"Y: {model.y:6d} мГс  (масштаб {model.sy:3d}%)")
        lbl_z.config(text=f"Z: {model.z:6d} мГс  (масштаб {model.sz:3d}%)")
        lbl_deg.config(text=f"Азимут на север: {model.deg:3d}°")
        lbl_status.config(text=model.last_status)

        canvas.delete("all")
        cw = max(200, canvas.winfo_width())
        ch = max(200, canvas.winfo_height())
        cx, cy = cw // 2, ch // 2
        r = max(50, min(cw, ch) // 2 - 36)

        canvas.create_oval(cx - r, cy - r, cx + r, cy + r, outline="#4fc3f7", width=2)
        canvas.create_text(cx, cy - r - 14, text="N", fill="#ff5252", font=("Sans", 13, "bold"))
        canvas.create_text(cx, cy + r + 14, text="S", fill="#cfd8dc", font=("Sans", 11))
        canvas.create_text(cx - r - 14, cy, text="W", fill="#cfd8dc", font=("Sans", 11))
        canvas.create_text(cx + r + 14, cy, text="E", fill="#cfd8dc", font=("Sans", 11))

        v_norm = math.hypot(model.vx, model.vy)
        if v_norm > 0.1:
            tip_x = cx + (model.vx / v_norm) * (r - 10)
            tip_y = cy + (model.vy / v_norm) * (r - 10)
        else:
            rad = math.radians(model.deg)
            tip_x = cx + math.sin(rad) * (r - 10)
            tip_y = cy - math.cos(rad) * (r - 10)
        canvas.create_line(cx, cy, tip_x, tip_y, fill="#ffeb3b", width=3, arrow=tk.LAST, arrowshape=(14, 18, 7))
        canvas.create_oval(cx - 5, cy - 5, cx + 5, cy + 5, fill="#ffffff")

        root.after(50, tick)

    root.after(50, tick)
    root.mainloop()


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Использование: {sys.argv[0]} <COM-порт> [--tui]", file=sys.stderr)
        print(f"Пример:        {sys.argv[0]} /dev/ttyUSB1", file=sys.stderr)
        return 1

    port = sys.argv[1]
    force_tui = "--tui" in sys.argv[2:]

    channel = SerialChannel(port)
    model = CompassModel()

    try:
        if force_tui or not os.environ.get("DISPLAY"):
            run_tui(channel, model)
        else:
            try:
                import tkinter  # noqa: F401
                run_gui(channel, model)
            except Exception:
                run_tui(channel, model)
    finally:
        channel.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
