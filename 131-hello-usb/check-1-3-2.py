# Проверка задания п1.3.2 на устройстве: слушает COM-порт платы
# и записывает всё принятое в файл device-1-3-2.log.

import time
from datetime import datetime

import serial
from serial.tools import list_ports

VENDOR_ID = 0x2E8A
PRODUCT_ID = 0x000A

TASK = "1.3.2"
PROJECT = "131-hello-usb"
LOG_NAME = "device-1-3-2.log"
DURATION_S = 10


def find_board():
    for port in list_ports.comports():
        if port.vid == VENDOR_ID and port.pid == PRODUCT_ID:
            return port
    return None


def listen(board):
    lines = []
    with serial.Serial(board.device, timeout=1) as port:
        time.sleep(0.2)
        port.reset_input_buffer()
        started = time.monotonic()
        while time.monotonic() - started < DURATION_S:
            line = port.readline().decode("ascii", "replace").strip()
            if line:
                lines.append((time.monotonic() - started, line))
    return lines


def write_log(board, lines):
    with open(LOG_NAME, "w", encoding="utf-8") as log:
        log.write("задание: " + TASK + "\n")
        log.write("проект: " + PROJECT + "\n")
        log.write("устройство: %04x:%04x\n" % (board.vid, board.pid))
        log.write("серийный номер: " + str(board.serial_number) + "\n")
        log.write("порт: " + board.device + "\n")
        log.write("начало: " + datetime.now().isoformat(timespec="seconds") + "\n")
        for moment, line in lines:
            log.write("%8.3f <-- %s\n" % (moment, line))
        log.write("итог: принято строк %d за %.1f с\n" % (len(lines), DURATION_S))


board = find_board()

if board is None:
    print("Плата не найдена. Проверьте кабель и запишите на плату прошивку задания.", end="\r\n")
else:
    print("Плата на порту " + board.device + ", слушаю %d секунд" % DURATION_S, end="\r\n")
    lines = listen(board)
    write_log(board, lines)
    print("Принято строк: %d, лог записан в %s" % (len(lines), LOG_NAME), end="\r\n")
