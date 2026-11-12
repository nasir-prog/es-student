#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
build="$(mktemp -d)"
trap 'rm -r -- "$build"' EXIT
libs=libs
sources=(
    "$libs/stdio-text-protocol/src/stdio-text-protocol.c"
    "$libs/systime/src/systime.c"
    "$libs/profiling/src/profiling.c"
)
includes=(
    -Ioffline
    -I"$libs/stdio-text-protocol/include"
    -I"$libs/systime/include"
    -I"$libs/device/include"
    -I"$libs/profiling/include"
)

for project in 231-firmware-template 311-adc 321-audio-recorder; do
    flags=()
    case "$project" in
        311-adc) flags+=(-DMODEL_ADC) ;;
        321-audio-recorder) flags+=(-DMODEL_ADC -DMODEL_RECORDER) ;;
    esac
    gcc -std=c11 -O2 -Wall -Wextra -Werror "${includes[@]}" "${flags[@]}" -I"$project" -I"$project/logic" \
        offline/trace.c "${sources[@]}" "$project"/logic/*/*.c -lm -o "$build/$project"
done
python3 offline/course_logs.py "$build" "$build"
