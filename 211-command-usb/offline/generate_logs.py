"""Expected exchanges derived from the ARM build, without a connected Pico.

This is a response model, not an emulator or a hardware test. It does not
supply a board serial number or invent stack/allocator addresses.
Run: python3 offline/generate_logs.py
"""
from pathlib import Path
import hashlib
import struct
import subprocess

PROJECT = Path(__file__).resolve().parent.parent
OUT = PROJECT / "offline"
ELF = PROJECT / "build/211_command_usb.elf"
BIN = PROJECT / "build/211_command_usb.bin"
image = BIN.read_bytes()
symbols = {}
for row in subprocess.check_output(
    ["arm-none-eabi-nm", "-n", str(ELF)], text=True
).splitlines():
    columns = row.split()
    if len(columns) == 3:
        symbols[columns[2]] = int(columns[0], 16)

BASE = symbols["__flash_binary_start"]

def word(address):
    return struct.unpack_from("<I", image, address - BASE)[0]

def halfword(address):
    return struct.unpack_from("<H", image, address - BASE)[0]

def string_at(address):
    offset = address - BASE
    end = image.index(0, offset)
    return image[offset:end].decode("ascii")

def region(name, start, end):
    return f"{name:<10} 0x{start:08x} 0x{end:08x} {end-start:8}"

flash_end = 0x10200000
image_end = symbols["__flash_binary_end"]
text_end = symbols["__etext"]
data_size = symbols["__data_end__"] - symbols["__data_start__"]
bss_size = symbols["__bss_end__"] - symbols["__bss_start__"]
boot_size = symbols["__boot2_end__"] - symbols["__boot2_start__"]
text_size = text_end - symbols["__boot2_end__"]
heap_size = symbols["__HeapLimit"] - symbols["__bss_end__"]
stack_size = symbols["__StackTop"] - symbols["__StackBottom"]
assert len(image) == image_end - BASE
assert boot_size + text_size + data_size == len(image)
assert word(0x10000100) == symbols["__StackTop"]

memory_rows = [
    ("flash", BASE, flash_end), ("sram", 0x20000000, 0x20042000),
    ("rom", 0, 0x4000), ("image", BASE, image_end),
    ("free", image_end, flash_end),
    ("boot2", symbols["__boot2_start__"], symbols["__boot2_end__"]),
    ("text", symbols["__boot2_end__"], text_end),
    ("data flash", text_end, text_end + data_size),
    ("data ram", symbols["__data_start__"], symbols["__data_end__"]),
    ("bss", symbols["__bss_start__"], symbols["__bss_end__"]),
    ("heap", symbols["__bss_end__"], symbols["__HeapLimit"]),
    ("stack", symbols["__StackBottom"], symbols["__StackTop"]),
]

command_count = word(symbols["command_count"])
commands = []
for i in range(command_count):
    offset = symbols["commands"] + i * 8
    commands.append((string_at(word(offset)), word(offset + 4)))

# Read initialized RAM objects from their flash load image.
def ram_bytes(address, length):
    offset = text_end - BASE + address - symbols["__data_start__"]
    return image[offset:offset + length]

card = ram_bytes(symbols["device_card"], 20)
card_version = struct.unpack_from("<I", card)[0]
card_name = card[4:17].split(b"\0")[0].decode("ascii")
card_revision = card[17]
initial_counter = struct.unpack("<I", ram_bytes(symbols["data_variable"], 4))[0]

def response(command, state):
    if command in ("enable", "disable"):
        state["led"] = int(command == "enable")
        return ["led " + ("on" if state["led"] else "off")]
    if command == "ping":
        return ["pong"]
    if command == "info":
        return ["project: 211-command-usb", "repo: https://github.com/nasir-prog/es-student",
                "board: pico", "serial: unavailable (no physical board)",
                "chip: unavailable (hardware register not read)", "pico-sdk: 2.3.0"]
    if command == "mem_info":
        return ["area       start      end        size"] + [region(*r) for r in memory_rows] + [
            "total", f"  flash image {len(image):8} = boot2 {boot_size} + text {text_size} + data {data_size}",
            f"  flash free  {flash_end-image_end:8} of {flash_end-BASE}",
            f"  ram used    {data_size+bss_size:8} = data {data_size} + bss {bss_size}",
            f"  ram free    {heap_size:8} for heap and {stack_size} for stack"]
    if command == "fw_info":
        state["calls"] += 1
        rows = ["object          address     value"]
        for name in ("main", "fw_info"):
            address = symbols[name] & ~1
            rows.append(f"{name:<15} 0x{address|1:08x}  0x{halfword(address):04x}")
        rows.append(f"commands        0x{symbols['commands']:08x}")
        rows += [f"- {name:<13} 0x{handler:08x}" for name, handler in commands]
        rows += [f"data_variable   0x{symbols['data_variable']:08x}  {initial_counter+state['calls']}",
                 f"bss_variable    0x{symbols['bss_variable']:08x}  {state['calls']}",
                 "stack_variable  runtime address unavailable; initializer 1946",
                 "heap_variable   runtime address unavailable; initializer 1951"]
        return rows
    if command == "dev_info":
        address = symbols["device_card"]
        return ["struct          address     size offset value", f"device_card     0x{address:08x}    20",
                f"- version       0x{address:08x}     4      0 0x{card_version:08x}",
                f"- name          0x{address+4:08x}    13      4 {card_name}",
                f"- revision      0x{address+17:08x}     1     17 {card_revision}",
                "fields 18, sizeof 20, padding 2"]
    if command == "boot_info":
        reset = word(0x10000104)
        return ["vector table   0x10000100", f"  stack top    0x{word(0x10000100):08x}",
                f"  reset        0x{reset:08x}", f"  reset (even) 0x{reset & ~1:08x}",
                "gpio in        0xd0000004", f"  led bit      {state['led']} (modeled)",
                f"  gpio_get     {state['led']} (modeled)"]
    return ["unknown command: " + command]

cases = {
    1: ["enable", "disable", "info", "nosuchcommand"],
    2: ["enable", "disable", "info", "ping", "nosuchcommand"],
    3: ["mem_info"], 4: ["fw_info", "fw_info", "fw_info"],
    5: ["dev_info"], 6: ["boot_info", "enable", "boot_info", "disable", "boot_info"],
}
for task, sequence in cases.items():
    state = {"led": 0, "calls": 0}
    lines = ["источник: расчётная модель без платы; прошивка не исполнялась",
             f"задание: 2.1.{task}", "проект: 211-command-usb",
             "устройство: отсутствует", "серийный номер: недоступен", "порт: отсутствует",
             "время: не измерялось; порядок обозначен номерами шагов",
             "bin sha256: " + hashlib.sha256(image).hexdigest()]
    for step, command in enumerate(sequence, 1):
        lines.append(f"шаг {step} --> {command}")
        lines += [f"шаг {step} <-- {row}" for row in response(command, state)]
    path = OUT / f"expected-2-1-{task}.log"
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(path.name)
print("Binary consistency checks passed; these are not hardware test logs.")
