#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/.local/build/dll-board-size"
mkdir -p "$out"
check_dll_capacity() {
    python3 - "$1" "$2" <<'PY'
import ctypes
import pathlib
import sys

dll = ctypes.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
bits = int(sys.argv[2])
width, height, banner = {64: (8, 8, ""), 128: (12, 10, " LB "),
                       256: (16, 16, " VLB "), 512: (22, 22, " HB 512 ")}[bits]


def function(name, result, *args):
    fn = getattr(dll, name)
    fn.restype, fn.argtypes = result, args
    return fn


ptr, string = ctypes.c_void_p, ctypes.c_char_p
free = function("fsf_free", None, ptr)


def text(name, *args):
    value = function(name, ptr, *([ptr, ctypes.c_bool, ctypes.c_int] if args else []))(*args)
    try:
        return ctypes.string_at(value).decode()
    finally:
        free(value)


info = text("fsf_info")
assert banner in info, info
if bits == 64:
    assert not any(marker in info for marker in (" LB ", " VLB ", " HB ")), info
start = f"{width - 1}k/{width - 2}R1/" + f"{width}/" * (height - 3) + f"K{width - 1} w - - 0 1"
load = function("fsf_try_load_variant_config", ctypes.c_int, string)
new = function("fsf_new_board", ptr, string, string, ctypes.c_bool)
push = function("fsf_push", ctypes.c_bool, ptr, string)
pop = function("fsf_pop", None, ptr)
release = function("fsf_free_board", None, ptr)
assert load((f"[dll-capacity:chess]\nmaxFile = {width}\nmaxRank = {height}\n"
             f"castling = false\nallowChecks = true\nstartFen = {start}\n").encode()) == 1
board = new(b"dll-capacity", b"", False)
assert board, info
try:
    move = f"{chr(97 + width - 2)}{height - 1}{chr(97 + width - 2)}{height}"
    assert push(board, move.encode()), move
    expected = f"{width - 2}Rk/" + f"{width}/" * (height - 2) + f"K{width - 1} b - - 1 1"
    assert text("fsf_fen", board, False, 0) == expected
    pop(board)
    assert text("fsf_fen", board, False, 0) == start
finally:
    release(board)
assert load((f"[dll-oversized:chess]\nmaxFile = {width + 1}\nmaxRank = {height}\n"
             "castling = false\n").encode()) == 0
assert not new(b"dll-oversized", b"", False), info
print(f"DLL {bits}-bit banner, maximum geometry, boundary move/undo and size rejection passed")
PY
}
for width in 64 128 512 256 128; do
    case "$width" in
        512) capacity=hugeboards=yes ;;
        256) capacity=verylargeboards=yes ;;
        128) capacity=largeboards=yes ;;
        *) capacity=largeboards=no ;;
    esac
    make -C "$root/src" -f Makefile_dll -j "${JOBS:-2}" "$capacity" \
        EXE="$out/libffish.so" 'EXTRACXXFLAGS=-O2 -DNDEBUG' >"$out/build-$width.log" 2>&1 || {
        cat "$out/build-$width.log" >&2
        exit 1
    }
    check_dll_capacity "$out/libffish.so" "$width"
done
cat >"$out/probe.cpp" <<'CPP'
#include "probe.h"
extern "C" int config_value() { return FSX_CONFIG_TEST + PROBE_HEADER_VALUE; }
CPP
printf '#define PROBE_HEADER_VALUE 0\n' >"$out/probe.h"
for value in 1 2; do
    make -C "$root/src" -f Makefile_dll SRCS=probe.cpp VPATH="$out" \
        DLL_BUILD_DIR="$out/probe-objects" EXE="$out/probe.so" largeboards=yes \
        "EXTRACXXFLAGS=-O2 -DFSX_CONFIG_TEST=$value" >"$out/probe.log" 2>&1 || {
        cat "$out/probe.log" >&2
        exit 1
    }
    python3 - "$out/probe.so" "$value" <<'PY'
import ctypes, sys
dll = ctypes.CDLL(sys.argv[1])
dll.config_value.restype = ctypes.c_int
assert dll.config_value() == int(sys.argv[2])
PY
done
before=$(python3 - "$out" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
print([p.stat().st_mtime_ns for p in (root / 'probe-objects/probe.o', root / 'probe.so')])
PY
)
make -C "$root/src" -f Makefile_dll SRCS=probe.cpp VPATH="$out" \
    DLL_BUILD_DIR="$out/probe-objects" EXE="$out/probe.so" largeboards=yes \
    'EXTRACXXFLAGS=-O2 -DFSX_CONFIG_TEST=2' >"$out/probe-unchanged.log" 2>&1
after=$(python3 - "$out" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
print([p.stat().st_mtime_ns for p in (root / 'probe-objects/probe.o', root / 'probe.so')])
PY
)
[[ "$before" == "$after" ]] || { cat "$out/probe-unchanged.log" >&2; exit 1; }
printf '#define PROBE_HEADER_VALUE 4\n' >"$out/probe.h"
make -C "$root/src" -f Makefile_dll SRCS=probe.cpp VPATH="$out" \
    DLL_BUILD_DIR="$out/probe-objects" EXE="$out/probe.so" largeboards=yes \
    'EXTRACXXFLAGS=-O2 -DFSX_CONFIG_TEST=2' >"$out/probe-header.log" 2>&1
python3 - "$out/probe.so" <<'PY'
import ctypes, sys
dll = ctypes.CDLL(sys.argv[1])
dll.config_value.restype = ctypes.c_int
assert dll.config_value() == 6
PY
echo 'DLL rebuilds for compiler flags and headers, and unchanged builds do no compiling or linking'
