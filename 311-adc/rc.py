# Опыт с RC-цепью на приборе 311-adc. По номиналам деталей ведёт опыт, по 5τ на заряд
# и разряд, сохраняет записи в rc.csv, график в rc.png, номиналы и найденную τ в rc.log
# и сверяет τ с R·C. Приём потока устроен как в get_telemetry.py из п3.1.4.
#
#   python rc.py --r 10 --c 100   — опыт с RC-цепью, R в кОм, C в мкФ
#   python rc.py --plot rc.csv    — разбор готовой записи, номиналы берутся из rc.log

import argparse
import csv
import time

import matplotlib
import serial
from serial.tools import list_ports

# окно не нужно: график сразу сохраняется в файл
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# настоящая τ отличается от R·C из-за допуска деталей: C ±20 %, R ±5 %
TAU_LOW = 0.8 * 0.95
TAU_HIGH = 1.2 * 1.05

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


def collect(device, seconds, period_ms, actions=()):
    records = []
    pending = list(actions)
    with serial.Serial(device, timeout=0.1) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        port.write(("tm_start %d\r" % period_ms).encode("ascii"))
        started = time.monotonic()
        deadline = started + seconds
        received = ""
        try:
            while time.monotonic() < deadline:
                # команды опыта — в свои моменты от начала приёма
                while pending and time.monotonic() - started >= pending[0][0]:
                    port.write((pending.pop(0)[1] + "\r").encode("ascii"))
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
            port.write(b"charge_off\r")
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


def rc_plan(tau):
    """Период телеметрии, команды опыта и его длительность по ожидаемой τ."""
    # около ста отсчётов на τ, но не чаще, чем умеет прибор
    period_ms = max(10, round(tau * 10))
    # полсекунды покоя, 5τ заряда, 5τ разряда и полсекунды в запас
    actions = [(0.5, "charge_on"), (0.5 + 5 * tau, "charge_off")]
    return period_ms, actions, 1 + 10 * tau


def crossing(voltages, start, level, rising):
    """Номер первой записи от start, на которой напряжение перешло уровень."""
    for i in range(start, len(voltages)):
        if (voltages[i] >= level) if rising else (voltages[i] <= level):
            return i
    return None


def measure_tau(times, voltages):
    """τ заряда и разряда по уровням напряжения, как по триггеру осциллографа."""
    top = max(voltages)
    peak = voltages.index(top)
    result = {"top": top}
    # заряд: от начала подъёма (2 % вершины) до 63 % вершины
    start = crossing(voltages, 0, 0.02 * top, True)
    end = crossing(voltages, start, 0.632 * top, True) if start is not None else None
    if end is not None and end <= peak:
        result["charge"] = (times[start], times[end], voltages[end])
    # разряд: от начала спада (98 % вершины) до 37 % вершины
    start = crossing(voltages, peak, 0.98 * top, False)
    end = crossing(voltages, start, 0.368 * top, False) if start is not None else None
    if end is not None:
        result["discharge"] = (times[start], times[end], voltages[end])
    return result


def analyze_rc(csv_name, r_kohm, c_uf):
    tau = r_kohm * c_uf / 1000
    low, high = TAU_LOW * tau, TAU_HIGH * tau
    times, voltages, _ = load_csv(csv_name)
    found = measure_tau(times, voltages)
    top = found["top"]

    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.plot(times, voltages, label="напряжение на конденсаторе")
    ax.axhline(0.632 * top, color="gray", linestyle="--", linewidth=1)
    ax.axhline(0.368 * top, color="gray", linestyle="--", linewidth=1)
    ax.text(times[-1], 0.632 * top, "63 %", va="bottom", ha="right", color="gray")
    ax.text(times[-1], 0.368 * top, "37 %", va="bottom", ha="right", color="gray")

    lines = ["R_kohm = %g" % r_kohm, "C_uf = %g" % c_uf, "tau_expected_s = %.3f" % tau,
             "tau_min_s = %.3f" % low, "tau_max_s = %.3f" % high]
    passed = True
    for key, name in (("charge", "заряда"), ("discharge", "разряда")):
        if key not in found:
            print("τ %s: уровень не пройден, проверьте цепь и номиналы" % name)
            lines.append("tau_%s_s = none" % key)
            passed = False
            continue
        start, end, level = found[key]
        measured = end - start
        inside = low <= measured <= high
        passed = passed and inside
        # от начала и конца заряда отсчитывается ожидаемый момент перехода уровня
        ax.axvline(start, color="tab:purple", linestyle="--", linewidth=1,
                   label="начало и конец заряда" if key == "charge" else None)
        ax.axvspan(start + low, start + high, color="tab:green", alpha=0.15,
                   label="ожидаемый переход уровня" if key == "charge" else None)
        ax.plot([end], [level], "o", color="tab:red",
                label="уровень пройден" if key == "charge" else None)
        print("τ %s: %.3f с, ожидается %.2f–%.2f с, %s" %
              (name, measured, low, high, "в диапазоне" if inside else "вне диапазона"))
        lines.append("tau_%s_s = %.3f" % (key, measured))
    lines.append("result = %s" % ("ok" if passed else "fail"))

    ax.set_title("R = %g кОм, C = %g мкФ, ожидаемая τ = %.2f с" % (r_kohm, c_uf, tau))
    ax.set_xlabel("Время, с")
    ax.set_ylabel("Напряжение на GP26, В")
    ax.grid(True)
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig("rc.png")
    print("График сохранён в rc.png")

    with open("rc.log", "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("Номиналы и τ записаны в rc.log")
    print("Опыт сошёлся с расчётом" if passed else "Опыт не сошёлся с расчётом")


def read_log(name):
    """Номиналы R и C из rc.log прошлого опыта или None, если файла нет."""
    values = {}
    try:
        with open(name, encoding="utf-8") as f:
            for line in f:
                key, _, value = line.partition("=")
                values[key.strip()] = value.strip()
        return float(values["R_kohm"]), float(values["C_uf"])
    except (OSError, KeyError, ValueError):
        return None


parser = argparse.ArgumentParser(description="Опыт с RC-цепью на приборе 311-adc")
parser.add_argument("--r", type=float, help="сопротивление резистора, кОм")
parser.add_argument("--c", type=float, help="ёмкость конденсатора, мкФ")
parser.add_argument("--plot", metavar="CSV", help="разобрать готовую запись опыта, без платы")
args = parser.parse_args()

if (args.r is None) != (args.c is None):
    parser.error("нужны оба номинала, --r и --c")
if args.r is None:
    # без номиналов можно только разобрать запись, номиналы — из прошлого опыта
    found = read_log("rc.log") if args.plot else None
    if found is None:
        parser.error("укажите номиналы деталей: --r <кОм> --c <мкФ>")
    args.r, args.c = found

tau = args.r * args.c / 1000
print("Ожидаемая τ = R·C = %.3f с" % tau)
if not 1 <= args.r <= 100:
    print("Резистор вне пределов от 1 до 100 кОм, проверка курса такой не примет")
if tau < 0.2:
    print("τ меньше 0,2 с: отсчётов на τ будет мало, возьмите детали из таблицы")

if args.plot:
    analyze_rc(args.plot, args.r, args.c)
else:
    board = find_board()
    if board is None:
        print("Плата не найдена. Проверьте кабель и запишите на плату прошивку прибора.")
    else:
        period_ms, actions, seconds = rc_plan(tau)
        print("Плата на порту %s, опыт %.1f с, период %d мс" % (board.device, seconds, period_ms))
        records = collect(board.device, seconds, period_ms, actions)
        if not records:
            print("Ни одной строки телеметрии не принято.")
        else:
            save_csv(records, "rc.csv")
            print("Записей %d, сохранены в rc.csv" % len(records))
            analyze_rc("rc.csv", args.r, args.c)
