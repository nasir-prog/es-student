"""Generate and verify educational fixtures with modeled clocks and signals."""

import csv
import importlib.util
import math
from pathlib import Path
import re
import struct
import subprocess
import sys
import wave

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parent.parent
MODEL = "Учебная модель: данные рассчитаны программой без платы Raspberry Pi Pico."
CSV_FIELDS = ("time_s", "voltage_v", "temp_c")


def load_recorder():
    spec = importlib.util.spec_from_file_location(
        "rec_audio", ROOT / "321-audio-recorder" / "rec_audio.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        writer.writerow(CSV_FIELDS)
        writer.writerows(rows)


def watermark_plot(source, destination):
    image = plt.imread(source)
    height, width = image.shape[:2]
    figure = plt.figure(figsize=(width / 100, (height + 35) / 100), dpi=100)
    axes = figure.add_axes([0, 0, 1, height / (height + 35)])
    axes.imshow(image)
    axes.axis("off")
    figure.text(0.5, 0.982, MODEL, ha="center", va="top", fontsize=9)
    figure.savefig(destination)
    plt.close(figure)
    source.unlink()


def run_plot(out, command, basename):
    subprocess.run([sys.executable, *map(str, command)], cwd=out, check=True)
    watermark_plot(out / f"{basename}.png", out / f"model-{basename}.png")


def telemetry_and_rc(out):
    adc = ROOT / "311-adc"
    csv_path = out / "model-telemetry.csv"
    write_csv(csv_path, (
        (f"{i / 10:.1f}", f"{1.625 + 0.025 * math.sin(i / 6):.3f}", "25.0")
        for i in range(101)
    ))
    run_plot(out, [adc / "get_telemetry.py", "--plot", csv_path], "telemetry")

    # Ideal 10 kΩ and 100 μF network, sampled every 10 ms with 12-bit quantization.
    csv_path = out / "model-rc.csv"
    peak = 3.3 * (1 - math.exp(-5))

    def voltage_at(t):
        if t < 0.5:
            return 0.0
        if t < 5.5:
            return 3.3 * (1 - math.exp(-(t - 0.5)))
        return peak * math.exp(-(t - 5.5))

    write_csv(csv_path, (
        (f"{i / 100:.2f}", f"{round(voltage_at(i / 100) * 4095 / 3.3) * 3.3 / 4095:.4f}", "25.0")
        for i in range(1101)
    ))
    run_plot(out, [adc / "rc.py", "--plot", csv_path, "--r", "10", "--c", "100"], "rc")
    rc_log = out / "rc.log"
    analysis = rc_log.read_text(encoding="utf-8")
    assert "result = ok" in analysis
    for phase in ("charge", "discharge"):
        estimate = float(re.search(rf"tau_{phase}_s = (\d+\.\d+)", analysis).group(1))
        assert abs(estimate - 1.0) < 0.05
    (out / "model-rc.log").write_text(MODEL + "\n" + analysis, encoding="utf-8")
    rc_log.unlink()


def verify_serialized_frames(recorder, path):
    data = path.read_bytes()
    assert len(data) == 2 * 1030
    for bits, count, frame in [(16, 512, data[:1030]), (8, 1024, data[1030:])]:
        assert frame[:6] == struct.pack("<HHH", 0x5AA5, 0, count)
        assert frame[6:] == bytes([10]) * 1024
        receiver = recorder.Receiver(bits)
        receiver.feed(b"modeled serial text\r\n" + frame[:1])
        receiver.feed(frame[1:300])
        receiver.feed(frame[300:])
        assert receiver.frames == 1 and len(receiver.samples) == count
        assert receiver.lost == 0
        skipped_frame = struct.pack("<HHH", 0x5AA5, 2, count) + frame[6:]
        receiver.feed(skipped_frame)
        assert receiver.lost == 1


def feed_codes(recorder, codes, bits):
    receiver = recorder.Receiver(bits)
    for index in range(0, len(codes), receiver.expected):
        batch = codes[index:index + receiver.expected]
        if len(batch) < receiver.expected:
            break
        number = index // receiver.expected
        if bits == 16:
            payload = struct.pack(f"<{len(batch)}H", *batch)
        else:
            payload = bytes(value >> 4 for value in batch)
        frame = struct.pack("<HHH", 0x5AA5, number % 65536, len(batch)) + payload
        receiver.feed(frame[:7])
        receiver.feed(frame[7:])
    assert receiver.lost == 0 and receiver.frames == len(codes) // receiver.expected
    return receiver


def save_audio(out, recorder, name, receiver, melody=False):
    path = out / f"model-{name}.wav"
    recorder.save_wav(str(path), receiver.samples, receiver.bits, melody)
    with wave.open(str(path), "rb") as wav:
        assert wav.getnchannels() == 1
        assert wav.getframerate() == recorder.SAMPLE_RATE
        assert wav.getsampwidth() == receiver.sample_bytes
        assert wav.getnframes() == len(receiver.samples)
    return len(receiver.samples) / recorder.SAMPLE_RATE


def synthetic_audio(out, recorder):
    rate = recorder.SAMPLE_RATE
    tones, seconds = recorder.melody_events()
    margin = round(recorder.MELODY_MARGIN_S * rate)
    melody = [0] * margin
    for (when, hz), (next_time, _) in zip(tones, tones[1:]):
        count = round(next_time * rate) - round(when * rate)
        period_us = round(1000000 / hz) if hz else 0
        if hz:
            actual_hz = 1000000 / period_us
            assert abs(actual_hz / hz - 1) < 0.01
            melody.extend(4095 if (i * actual_hz / rate) % 1 < 0.5 else 0 for i in range(count))
        else:
            melody.extend([0] * count)
    assert abs((len(melody) - margin) / rate - seconds) < 1 / rate
    melody.extend([0] * margin)
    rx = feed_codes(recorder, melody, 16)
    duration = save_audio(out, recorder, "melody", rx, melody=True)
    report = [f"PWM model: {rx.frames} frames, {duration:.3f} s, {rx.lost} missing frames"]

    # Synthetic tones through a 1.25 V biased ADC model.
    bias = 1.25 * 4096 / 3.3
    signal = [
        round(bias + (0.4 + 0.6 * math.sin(math.pi * i / (3 * rate)) ** 2) *
              (120 * math.sin(2 * math.pi * 440 * i / rate) +
               55 * math.sin(2 * math.pi * 700 * i / rate)))
        for i in range(5 * rate)
    ]
    for bits in (16, 8):
        rx = feed_codes(recorder, signal[:3 * rate], bits)
        duration = save_audio(out, recorder, f"recording-{bits}bit", rx)
        assert 2.97 < duration <= 3
        report.append(f"ADC model, {bits} bit: {rx.frames} frames, {duration:.3f} s, "
                      f"{rx.lost} missing frames")
    stream = feed_codes(recorder, signal, 16)
    assert 43000 < len(stream.samples) / 5 < 45500
    report.append(f"Stream model: {stream.frames} frames, {len(stream.samples)} samples in 5 s")
    (out / "model-audio.log").write_text(MODEL + "\n" + "\n".join(report) + "\n", encoding="utf-8")


def verify_command_logs(out):
    core = (out / "model-core.log").read_text(encoding="utf-8")
    for reply in (
        "error: pi is busy", "pi: running", "pi: 3.141591654",
        "error: command longer than 63 characters or 4 arguments",
        "error: usage led_period <period_ms>", "error: period_ms must be greater than 0",
    ):
        assert reply in core, reply

    recorder = (out / "model-recorder.log").read_text(encoding="utf-8")
    for reply in (
        "timer_blink: stopped, 24 toggles", "capture: 4 samples, period 125 us",
        "error: adc busy", "dma_cap: 4410 samples in 100000 us",
        "stream: stopped, 1 frames, 512 samples, 0 overruns",
        "stream: stopped, 1 frames, 1024 samples, 0 overruns",
        "tone: 440 Hz", "tone: off", "reset: watchdog, restarts 1",
        "wdt: enabled, timeout 2000 ms", "mic: bias 1.250 V",
        "mic: level 9.7 mV rms, peak 9.7 mV, 100 ms",
    ):
        assert reply in recorder, reply


if __name__ == "__main__":
    output = Path(sys.argv[1]).resolve()
    recorder = load_recorder()
    verify_command_logs(output)
    verify_serialized_frames(recorder, Path(sys.argv[2]))
    telemetry_and_rc(output)
    synthetic_audio(output, recorder)
    (output / "README.txt").write_text(
        MODEL + "\nCSV и WAV содержат модельные сигналы.\n"
        "Логи команд получены при запуске C-модулей прошивки на компьютере.\n"
        "RP2040, электрическая цепь, звук и USB-обмен проверяются здесь моделями.\n",
        encoding="utf-8",
    )
    print("Учебный прогон пройден: протокол, таймеры задач, телеметрия, RC, кадры, WAV.")
