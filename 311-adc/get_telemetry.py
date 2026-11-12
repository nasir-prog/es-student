# Сбор телеметрии прибора 311-adc: запускает поток, принимает строки tm:,
# сохраняет записи в telemetry.csv и строит график в telemetry.png.
#
#   python get_telemetry.py                      — 10 секунд с периодом 100 мс
#   python get_telemetry.py --seconds 30 --period 50
#   python get_telemetry.py --plot telemetry.csv — только график из готового файла

import argparse
import csv
import time

import matplotlib
import serial
from serial.tools import list_ports

# окно не нужно: график сразу сохраняется в файл
matplotlib.use("Agg")
import matplotlib.pyplot as plt

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def parse_record(line):
    """Запись из строки tm: <мкс> <В> <°C> или None, если строка не телеметрии."""
    parts = line.split()
    if len(parts) != 4 or parts[0] != "tm:":
        return None
    try:
        return int(parts[1]), float(parts[2]), float(parts[3])
    except ValueError:
        return None


def collect(device, seconds, period_ms):
    records = []
    with serial.Serial(device, timeout=0.1) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        port.write(("tm_start %d\r" % period_ms).encode("ascii"))
        deadline = time.monotonic() + seconds
        received = ""
        try:
            while time.monotonic() < deadline:
                received += port.read(256).decode("ascii", "replace")
                while "\n" in received:
                    line, received = received.split("\n", 1)
                    record = parse_record(line.strip())
                    if record is not None:
                        records.append(record)
                        print("%.3f с  %.3f В  %.1f °C" % (record[0] / 1e6, record[1], record[2]))
        except KeyboardInterrupt:
            print("Остановлено по Ctrl+C")
        finally:
            port.write(b"tm_stop\r")
    return records


def save_csv(records, name):
    # время от первой записи в секундах: так график начинается с нуля
    start_us = records[0][0]
    with open(name, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["time_s", "voltage_v", "temp_c"])
        for time_us, voltage, temp in records:
            writer.writerow(["%.6f" % ((time_us - start_us) / 1e6), "%.3f" % voltage, "%.1f" % temp])


def load_csv(name):
    times, voltages, temps = [], [], []
    with open(name, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            times.append(float(row["time_s"]))
            voltages.append(float(row["voltage_v"]))
            temps.append(float(row["temp_c"]))
    return times, voltages, temps


def plot(csv_name, png_name):
    times, voltages, temps = load_csv(csv_name)
    fig, (top, bottom) = plt.subplots(2, 1, sharex=True, figsize=(8, 6))
    top.plot(times, voltages)
    top.set_ylabel("Напряжение на GP26, В")
    top.grid(True)
    bottom.plot(times, temps, color="tab:red")
    bottom.set_ylabel("Температура кристалла, °C")
    bottom.set_xlabel("Время, с")
    bottom.grid(True)
    fig.tight_layout()
    fig.savefig(png_name)
    print("График сохранён в " + png_name)


parser = argparse.ArgumentParser(description="Сбор телеметрии прибора 311-adc")
parser.add_argument("--seconds", type=float, default=10, help="сколько секунд принимать поток")
parser.add_argument("--period", type=int, default=100, help="период телеметрии, мс")
parser.add_argument("--plot", metavar="CSV", help="только построить график из готового файла")
args = parser.parse_args()

if args.plot:
    plot(args.plot, "telemetry.png")
else:
    # найти плату, принять поток, сохранить CSV и построить график
    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку прибора.")
    else:
        print("Плата на порту " + board.device + ", принимаю телеметрию")
        records = collect(board.device, args.seconds, args.period)
        if not records:
            print("Ни одной строки телеметрии не принято.")
        else:
            save_csv(records, "telemetry.csv")
            print("Записей %d, сохранены в telemetry.csv" % len(records))
            plot("telemetry.csv", "telemetry.png")
