#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
out="${1:-offline/model-output}"
mkdir -p "$out"
out="$(cd "$out" && pwd)"
build="$(mktemp -d)"
trap 'rm -f -- "$build/core" "$build/recorder" "$build/two" "$build/frames.bin"; rmdir -- "$build"' EXIT
lib=libs
common=(
    -Ioffline
    -I"$lib/stdio-text-protocol/include"
    -I"$lib/systime/include"
    -I"$lib/device/include"
    -I"$lib/profiling/include"
    "$lib/stdio-text-protocol/src/stdio-text-protocol.c"
    "$lib/systime/src/systime.c"
    "$lib/profiling/src/profiling.c"
)

gcc -std=c11 -O2 -Wall -Wextra -Werror "${common[@]}" -I311-adc -I311-adc/logic \
    offline/check-core.c 311-adc/logic/*/*.c -lm -o "$build/core"
{
    printf '%s\n' 'Учебный прогон 2.3 и 3.1. Время и уровни подаются программной моделью.'
    "$build/core"
} | tee "$out/model-core.log"

gcc -std=c11 -O2 -Wall -Wextra -Werror "${common[@]}" -I321-audio-recorder -I321-audio-recorder/logic \
    offline/check-recorder.c 321-audio-recorder/logic/*/*.c -lm -o "$build/recorder"
"$build/recorder" "$build/frames.bin" | tee "$out/model-recorder.log"
gcc -std=c11 -O2 -Wall -Wextra -Werror -Ioffline -I239-two-buttons -I239-two-buttons/logic -Ilibs/systime/include \
    offline/check-two.c libs/systime/src/systime.c \
    239-two-buttons/logic/{button-task/button-task,led-task/led-task}.c -o "$build/two"
"$build/two" | tee "$out/model-two-buttons.log"
python3 offline/generate.py "$out" "$build/frames.bin"
