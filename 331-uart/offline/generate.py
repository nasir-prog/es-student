"""Run the official command scenarios with a modeled RC circuit and UART."""

from pathlib import Path
import re
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parent / "offline"))

from course_logs import load_steps, write_trace


def plot_scope(log):
    captures = {}
    samples = None
    for line in log.read_text(encoding="utf-8").splitlines():
        _, separator, reply = line.partition(" <-- ")
        if not separator:
            continue
        match = re.match(r"scope: (\d+) samples, (\d+) Hz, (\d+) baud,", reply)
        if match:
            count, rate, baud = map(int, match.groups())
            samples = []
        elif reply == "scope: end" and samples is not None:
            if len(samples) != count:
                raise ValueError(f"Scope returned {len(samples)} samples, expected {count}")
            captures[baud] = rate, samples
            samples = None
        elif samples is not None:
            samples.extend(map(int, reply.split()))

    figure, axes = plt.subplots(2, figsize=(10, 6), layout="constrained")
    figure.suptitle("UART на GP0 · учебная модель, без физической платы")
    for axis, baud in zip(axes, (115200, 9600), strict=True):
        rate, codes = captures[baud]
        time = [i * 1e6 / rate for i in range(len(codes))]
        voltage = [sample * 3.3 / 4095 for sample in codes]
        start = next(i for i in range(1, len(codes)) if codes[i - 1] >= 2048 > codes[i])
        axis.step(time, voltage, where="post", linewidth=1.4)
        axis.set(
            title=f"{baud:,} бит/с · {1e6 / baud:.2f} мкс на бит".replace(",", " "),
            xlim=(0, time[start] + 11e6 / baud),
            ylim=(-0.25, 3.55),
            ylabel="Напряжение, В",
        )
        axis.axvline(time[start], color="tab:orange", linestyle="--", linewidth=0.8)
        axis.grid(alpha=0.25)
    axes[-1].set_xlabel("Время от начала захвата, мкс")
    figure.savefig(PROJECT / "scope-3-3-5.png", dpi=130)
    plt.close(figure)


def main():
    trace = Path(sys.argv[1]).resolve()
    for task in range(1, 6):
        script = PROJECT / f"check-3-3-{task}.py"
        log = PROJECT / f"device-3-3-{task}.log"
        wiring = "jumper" if task == 2 else "crossed"
        extra = ["конденсатор, мкФ: 4.7"] if task == 1 else []
        write_trace(log, f"3.3.{task}", PROJECT.name, [trace, wiring], load_steps(script), extra)
    plot_scope(PROJECT / "device-3-3-5.log")


if __name__ == "__main__":
    main()
