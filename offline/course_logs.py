"""Reproduce the course-format logs using production C logic and modeled peripherals."""

import ast
from datetime import datetime
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import wave
from zoneinfo import ZoneInfo

from generate import ROOT, telemetry_and_rc, synthetic_audio, load_recorder, run_plot, write_csv

TEMPLATE = "231-firmware-template"
ADC = "311-adc"
RECORDER = "321-audio-recorder"
TRACE_TASKS = {
    TEMPLATE: ("2-3-8",),
    ADC: ("3-1-1", "3-1-2", "3-1-3"),
    RECORDER: ("3-2-1", "3-2-2", "3-2-3", "3-2-6", "3-2-7"),
}
MODEL = (
    "режим: учебная программная модель, физическая плата не использовалась\n"
    "примечание: USB ID обозначает модель Pico, нулевой серийный номер — заполнитель\n"
    "примечание: время, АЦП, прерывания, DMA, сброс и сигналы в этих прогонах модельные\n"
)


def step_value(node, constants):
    if isinstance(node, ast.Name):
        return constants[node.id]
    if isinstance(node, (ast.List, ast.Tuple)):
        return [step_value(item, constants) for item in node.elts]
    if isinstance(node, ast.BinOp):
        left = step_value(node.left, constants)
        right = step_value(node.right, constants)
        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Mult):
            return left * right
    return ast.literal_eval(node)


def load_steps(script):
    """Evaluate the literal scenario declarations without running a serial client."""
    module = ast.parse(script.read_text(encoding="utf-8"))
    namespace = {}
    for node in module.body:
        if not isinstance(node, ast.Assign) or not isinstance(node.targets[0], ast.Name):
            continue
        name = node.targets[0].id
        if name in {"BACKSPACE", "RECONNECT"}:
            namespace[name] = ast.literal_eval(node.value)
        elif name == "STEPS":
            return [(frame, seconds if frame is not None else 0.1)
                    for frame, seconds in step_value(node.value, namespace)]
    raise ValueError(f"{script}: no STEPS assignment")


def read_trace(executable, steps):
    data = "".join(
        f"{round(seconds * 1000000)} {frame.encode('ascii').hex() if frame else '-'}\n"
        for frame, seconds in steps
    )
    command = executable if isinstance(executable, list) else [executable]
    run = subprocess.run(
        command, input=data, text=True, capture_output=True, check=True, timeout=15,
    )
    blocks = run.stdout.split("\n@@END@@\n")
    if blocks[-1] or len(blocks) != len(steps) + 1:
        raise ValueError("The model did not return one block for each input step")
    return [block.splitlines() for block in blocks[:-1]]


def header(task, project):
    started = datetime.now(ZoneInfo("Europe/Moscow")).isoformat(timespec="seconds")
    return MODEL + (
        f"задание: {task}\nпроект: {project}\n"
        "устройство: 2e8a:000a\nсерийный номер: 0000000000000000\n"
        f"порт: MODEL (не COM-порт)\nначало: {started}\n"
    )


def printable(line):
    return line.replace("\x7f", "<BS>").replace("\b", "<BS>").replace("\r", "").replace("\n", "")


def write_trace(path, task, project, executable, steps, metadata=()):
    log = [header(task, project).rstrip(), *metadata]
    sent = received = 0
    now = 0.0
    for (frame, seconds), block in zip(steps, read_trace(executable, steps), strict=True):
        if frame is not None:
            log.append(f"{now:8.3f} --> {printable(frame).strip()}".rstrip())
            sent += 1
        for raw in block:
            timing = re.match(r"^@@US (\d+)@@ ", raw)
            if timing:
                raw = raw[timing.end():]
            line = printable(raw).strip()
            if line == "@@RECONNECT@@":
                now += 2.0
                log.append(f"{now:8.3f} --- плата снова на связи")
            elif line:
                timestamp = int(timing[1]) / 1000000 if timing else now + 0.001
                if line.startswith("tm: "):
                    timestamp = max(timestamp, int(line.split()[1]) / 1000000)
                log.append(f"{timestamp:8.3f} <-- {line}")
                received += 1
        now += seconds
    log.append(f"итог: отправлено кадров {sent}, принято строк {received}")
    path.write_text("\n".join(log) + "\n", encoding="utf-8")


def telemetry_file(executable, out):
    trace = read_trace(executable, [("tm_start\r", 10.1), ("tm_stop\r", 0.1)])
    rows = [line.split()[1:] for line in trace[0] if line.startswith("tm: ")]
    if len(rows) < 100:
        raise ValueError("The telemetry task must produce at least 100 records")
    first = int(rows[0][0])
    path = out / "telemetry.csv"
    write_csv(path, (
        (f"{(int(timestamp) - first) / 1000000:.6f}", voltage, temperature)
        for timestamp, voltage, temperature in rows
    ))
    run_plot(out, [ROOT / ADC / "get_telemetry.py", "--plot", path], "telemetry")
    (out / "model-telemetry.png").replace(out / "telemetry.png")


def mark_wav(path):
    # Preserve the PCM data and record provenance in a standard RIFF INFO comment.
    data = path.read_bytes()
    comment = b"Educational model: synthesized audio; no physical Pico or microphone.\0"
    info = b"INFOICMT" + struct.pack("<I", len(comment)) + comment
    if len(comment) % 2:
        info += b"\0"
    chunk = b"LIST" + struct.pack("<I", len(info)) + info
    path.write_bytes(data[:4] + struct.pack("<I", len(data) + len(chunk) - 8) + data[8:] + chunk)


def capture_log(path, task, bits, count, elapsed, wav_path=None, melody_count=None):
    frame_size = 512 if bits == 16 else 1024
    frames, remainder = divmod(count, frame_size)
    if remainder:
        raise ValueError("The capture must contain whole frames")
    command = "stream_start" if bits == 16 else "stream_start 8"
    lines = [
        header(task, RECORDER).rstrip(),
        f"--> {command}",
        f"<-- stream: started, 44100 Hz, {bits} bit",
        f"принято: кадров {frames}, отсчётов {count}, пропущено кадров 0, байтов вне кадров 0",
        f"скорость: {count / elapsed:.0f} отсчётов в секунду за {elapsed:.2f} с",
    ]
    if wav_path:
        duration = count / 44100
        lines.append(f"файл: {wav_path.name}, {bits} бит, 44100 Гц, {duration:.2f} с")
    if melody_count is not None:
        lines.append(f"--> generate_tone: {melody_count} команд")
    lines.extend(["--> stream_stop", f"<-- stream: stopped, {frames} frames, {count} samples, 0 overruns"])
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def recording_files(scratch, out):
    recorder = load_recorder()
    synthetic_audio(scratch, recorder)
    audio = (scratch / "model-audio.log").read_text(encoding="utf-8")
    stream = re.search(r"Stream model: \d+ frames, (\d+) samples in (\d+) s", audio)
    capture_log(out / "stream.log", "3.2.4", 16, int(stream[1]), float(stream[2]))
    tones, _ = recorder.melody_events()
    for source, target, task in (
        ("melody", "melody", "3.2.5"),
        ("recording-16bit", "recording", "3.2.7"),
        ("recording-8bit", "recording-8bit", "3.2.7д"),
    ):
        wav_path = out / f"{target}.wav"
        shutil.copyfile(scratch / f"model-{source}.wav", wav_path)
        with wave.open(str(wav_path), "rb") as file:
            bits = file.getsampwidth() * 8
            frames = file.getnframes()
        mark_wav(wav_path)
        capture_log(
            out / f"{target}.log", task, bits, frames, frames / recorder.SAMPLE_RATE, wav_path,
            len(tones) if target == "melody" else None,
        )


def main():
    binaries = Path(sys.argv[1])
    scratch = Path(sys.argv[2]).resolve()
    for project, tasks in TRACE_TASKS.items():
        for task in tasks:
            script = ROOT / project / f"check-{task}.py"
            steps = load_steps(script)
            write_trace(
                ROOT / project / f"device-{task}.log", task.replace("-", "."),
                project, binaries / project, steps,
            )

    telemetry_file(binaries / ADC, ROOT / ADC)
    telemetry_and_rc(scratch)
    for extension in ("csv", "png", "log"):
        shutil.copyfile(scratch / f"model-rc.{extension}", ROOT / ADC / f"rc.{extension}")
    recording_files(scratch, ROOT / RECORDER)


if __name__ == "__main__":
    main()
