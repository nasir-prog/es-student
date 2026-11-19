#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

build="$(mktemp -d)"
trap 'rm -f -- "$build/trace"; rmdir -- "$build"' EXIT
project=331-uart

gcc -std=c11 -O2 -Wall -Wextra -Werror -include "$project/offline/trace-output.h" -Ioffline -I"$project" -I"$project/logic" \
    -Ilibs/stdio-text-protocol/include -Ilibs/device/include \
    -Ilibs/profiling/include -Ilibs/systime/include \
    "$project"/offline/*.c "$project"/logic/*/*.c \
    libs/{stdio-text-protocol/src/stdio-text-protocol,profiling/src/profiling,systime/src/systime}.c \
    -lm -o "$build/trace"

python3 "$project/offline/generate.py" "$build/trace"
