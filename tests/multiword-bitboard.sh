#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/.local/build/multiword-bitboard"
mkdir -p "$out"
for width in 256 512; do
    for mode in table popcnt portable; do
        flags=()
        case "$mode" in
            popcnt) flags=(-DUSE_POPCNT -mpopcnt) ;;
            portable) flags=(-U__SIZEOF_INT128__) ;;
        esac
        "${CXX:-g++}" -std=c++17 -O2 -fsanitize=undefined -DIS_64BIT \
            -DBOARD_BITS="$width" "${flags[@]}" "$root/tests/multiword-bitboard.cpp" \
            -o "$out/test-$width-$mode"
        "$out/test-$width-$mode"
    done
done
