"""Reproduce explicitly modeled logs for practice 2.2, without a Pico.

The real calc_pi/profiling C implementations run on the host. ARM duration,
idle iteration cost, ROSC, USB latency, CHIP_ID and GPIO are model assumptions.
This is not execution of the ARM firmware or a physical USB measurement.
Run on WSL/Linux after building: python3 offline/generate_logs.py
"""
from pathlib import Path
import ast
import ctypes
import hashlib
import math
import re
import struct
import subprocess
from datetime import datetime, timedelta, timezone

PROJECT = Path(__file__).resolve().parent.parent
BUILD = PROJECT / "build"
NATIVE = BUILD / "offline-model"
(NATIVE / "pico").mkdir(parents=True, exist_ok=True)
main = (PROJECT / "main.c").read_text(encoding="utf-8-sig")
terms = int(re.search(r"CALC_PI_TERMS\s*=\s*(\d+)", main)[1])
match = re.search(r"double calc_pi\(uint terms\)\s*\{", main)
begin = match.start()
end = match.end()
depth = 1
while depth:
    depth += (main[end] == "{") - (main[end] == "}")
    end += 1
calc_source = main[begin:end]
(NATIVE / "pico/stdlib.h").write_text(
    "#pragma once\n#include <stdint.h>\nuint32_t time_us_32(void);\n", encoding="utf-8")
(NATIVE / "model.c").write_text(r'''
#include <stdint.h>
#include "profiling.h"
typedef unsigned int uint;
static uint64_t now_us;
uint32_t time_us_32(void) { return (uint32_t)now_us; }
uint64_t model_now(void) { return now_us; }
void model_reset(uint64_t initial) { now_us = initial; profiling_init(); }
void model_busy(uint64_t duration) { now_us += duration; profiling_iteration(); }
void model_idle(uint64_t duration, uint32_t khz)
{
    uint64_t start = now_us;
    /* Assumption: each ordinary iteration costs 300 CPU cycles. */
    uint64_t count = duration * khz / 300000u;
    for (uint64_t i = 1; i <= count; i++)
    {
        now_us = start + i * 300000u / khz;
        profiling_iteration();
    }
    now_us = start + duration;
}
''' + calc_source + "\n", encoding="utf-8")
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-fPIC", "-shared",
                "-I", str(NATIVE), "-I", str(PROJECT / "profiling"),
                str(NATIVE / "model.c"), str(PROJECT / "profiling/profiling.c"),
                "-o", str(NATIVE / "model.so")], check=True)
lib = ctypes.CDLL(str(NATIVE / "model.so"))
lib.model_reset.argtypes = [ctypes.c_uint64]
lib.model_busy.argtypes = [ctypes.c_uint64]
lib.model_idle.argtypes = [ctypes.c_uint64, ctypes.c_uint32]
lib.model_now.restype = ctypes.c_uint64
lib.profiling_avg_us.restype = ctypes.c_float
lib.profiling_max_us.restype = ctypes.c_uint32
lib.calc_pi.argtypes = [ctypes.c_uint]
lib.calc_pi.restype = ctypes.c_double

# Actual algorithm tests with synthetic timer input.
assert lib.calc_pi(0) == 0.0
assert lib.calc_pi(1) == 4.0
assert abs(lib.calc_pi(2) - 8.0 / 3.0) < 1e-12
pi = lib.calc_pi(terms)
assert abs(pi - math.pi) <= 2e-6
lib.model_reset(0)
lib.model_idle(3_000_000, 125000)
average = lib.profiling_avg_us()
assert 2.0 < average < 2.5
lib.model_busy(4_680_000)
assert lib.profiling_max_us() >= 4_680_000
assert lib.profiling_avg_us() > average
lib.model_idle(5_000_000, 125000)
assert lib.profiling_avg_us() <= average * 1.5 + 0.5
lib.profiling_reset_max()
assert lib.profiling_max_us() == 0
lib.model_idle(1000, 125000)
assert lib.profiling_max_us() <= 3
lib.model_reset(0xfffffffe)
lib.model_busy(5)
assert lib.profiling_max_us() == 5  # 32-bit wraparound
print(f"Host algorithm tests passed: pi={pi:.8f}; profiling, decay, reset and timer wrap.")

image = (BUILD / "221_command_time.bin").read_bytes()
stack_top, reset_handler = struct.unpack_from("<II", image, 256)
assert stack_top == 0x20042000
assert reset_handler & 1
assert 0x10000100 <= (reset_handler & ~1) < 0x10000000 + len(image)

main_lines = main.splitlines()
dbg_line = next(i for i, row in enumerate(main_lines, 1) if 'LOG_DBG("got %s' in row)
clock_lines = (PROJECT / "clock/clock.c").read_text(encoding="utf-8-sig").splitlines()
clock_line = next(i for i, row in enumerate(clock_lines, 1) if 'LOG_INF("clk_sys %u' in row)
START_US = 3_000_000
PRINT_US = 40  # modeled USB line cost, not a measurement
ROSC_KHZ = 5774  # illustrative model parameter, not a measured oscillator

class DeviceModel:
    def __init__(self):
        self.khz = 125000
        self.peri = 125000
        self.led = 0
        self.last_toggle = 0
        self.cursor = 0
        self.events = []
        lib.model_reset(0)
        lib.model_idle(START_US, self.khz)
        self.idle_blink(START_US)

    def idle_blink(self, until):
        ticks = (until - self.last_toggle) // 500000
        if ticks > 0:
            self.led ^= ticks & 1
            self.last_toggle += ticks * 500000

    def idle_until(self, elapsed):
        target = START_US + elapsed
        if target > lib.model_now():
            lib.model_idle(target - lib.model_now(), self.khz)
            self.idle_blink(target)

    def respond(self, command):
        if command == "clk_info":
            rows = ["signal     set_khz measured_khz"]
            for name, khz in (("clk_ref", 12000), ("clk_sys", self.khz),
                              ("clk_peri", self.peri), ("clk_usb", 48000), ("clk_adc", 48000)):
                rows.append(f"{name:<8} {khz:9} {khz:12}")
            return rows + [f"rosc             - {ROSC_KHZ:12}"]
        if command == "info":
            return ["project: 221-command-time", "repo: https://github.com/nasir-prog/es-student",
                    "board: pico", "serial: 0000000000000000",
                    "chip: manufacturer 0x927, part 0x0002, revision 2", "pico-sdk: 2.3.0"]
        if command == "uptime":
            return [f"uptime: {lib.model_now() // 1000} ms"]
        if command == "main_time_exec":
            return [f"iteration avg {lib.profiling_avg_us():.2f} us, max {lib.profiling_max_us()} us"]
        if command == "main_time_reset":
            lib.profiling_reset_max()
            return ["max reset"]
        if command in ("clk_sys_low", "clk_sys_default"):
            self.khz = 62500 if command == "clk_sys_low" else 125000
            self.peri = 48000
            return [f"inf clk_sys_set:{clock_line} clk_sys {self.khz} kHz"]
        if command == "boot_info":
            return ["vector table   0x10000100", f"  stack top    0x{stack_top:08x}",
                    f"  reset        0x{reset_handler:08x}", f"  reset (even) 0x{reset_handler & ~1:08x}",
                    "gpio in        0xd0000004", f"  led bit      {self.led}", f"  gpio_get     {self.led}"]
        raise ValueError(command)

    def command(self, sent, command):
        self.events.append((sent, "-->", command))
        begin = max(sent + 1000, self.cursor)
        self.idle_until(begin)
        self.events.append((begin, "<--", command))
        self.events.append((begin + 40, "<--", f"dbg read_line:{dbg_line} got {command}"))
        lib.model_busy(80)
        if command == "calc_pi":
            # Course estimate: 585 double-operation cycles per term.
            spent = terms * 585 * 1000 // self.khz
            value = lib.calc_pi(terms)
            lib.model_busy(spent)
            finish = lib.model_now() - START_US
            rows = [f"pi: {value:.8f}", f"time: {spent // 1000} ms"]
        else:
            finish = lib.model_now() - START_US
            rows = self.respond(command)
        for row in rows:
            self.events.append((finish, "<--", row))
            lib.model_busy(PRINT_US)
            finish += PRINT_US
        # After a blocking command, blink() resumes with one toggle, not catch-up.
        if START_US + finish - self.last_toggle >= 500000:
            self.led ^= 1
            self.last_toggle = START_US + finish
        self.cursor = finish + 20

for task in range(1, 6):
    tree = ast.parse((PROJECT / f"check-2-2-{task}.py").read_text(encoding="utf-8-sig"))
    steps = next(ast.literal_eval(node.value) for node in tree.body
                 if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "STEPS" for t in node.targets))
    model = DeviceModel()
    sent = 0
    for command, wait in steps:
        model.command(sent, command)
        sent += round(wait * 1_000_000)
    events = sorted(model.events, key=lambda entry: entry[0])
    assert all(events[i][0] <= events[i + 1][0] for i in range(len(events) - 1))
    header = ["режим: программная модель, физическая плата не использовалась",
              "примечание: ARM-прошивка не исполнялась; calc_pi и profiling проверены на компьютере",
              "примечание: USB ID обозначает модель Pico; нулевой серийный номер — заполнитель",
              "примечание: времена, частоты, CHIP_ID и GPIO модельные, не измеренные на плате",
              "примечание: 585 тактов на член ряда; 300 тактов на обычную итерацию; вывод строки 40 мкс",
              f"примечание: ROSC модели {ROSC_KHZ} кГц; отсчёт обмена начинается на 3000 мс uptime",
              f"задание: 2.2.{task}", "проект: 221-command-time", "устройство: 2e8a:000a",
              "серийный номер: 0000000000000000", "порт: MODEL (не COM-порт)",
              "начало: " + datetime.now(timezone(timedelta(hours=3))).isoformat(timespec="seconds"),
              "bin sha256: " + hashlib.sha256(image).hexdigest()]
    rows = [f"{moment / 1_000_000:8.3f} {direction} {text}" for moment, direction, text in events]
    received = sum(direction == "<--" for _, direction, _ in events)
    rows.append(f"итог: отправлено команд {len(steps)}, принято строк {received}")
    (PROJECT / f"device-2-2-{task}.log").write_text("\n".join(header + rows) + "\n", encoding="utf-8")
    print(f"device-2-2-{task}.log: {len(steps)} commands, {received} modeled replies")
