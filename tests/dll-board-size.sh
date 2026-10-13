#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/.local/build/dll-board-size"
mkdir -p "$out"
for width in 128 512 256 128; do
    make -C "$root/src" -f Makefile_dll -j "${JOBS:-2}" boardbits="$width" \
        EXE="$out/libffish.so" 'CXXFLAGS=-O2 -DNDEBUG' >"$out/build-$width.log" 2>&1 || {
        cat "$out/build-$width.log" >&2
        exit 1
    }
    python3 "$root/tests/dll-board-size.py" "$out/libffish.so" "$width"
done
cat >"$out/probe.cpp" <<'CPP'
extern "C" int config_value() { return FSX_CONFIG_TEST; }
CPP
for value in 1 2; do
    make -C "$root/src" -f Makefile_dll SRCS=probe.cpp VPATH="$out" \
        DLL_BUILD_DIR="$out/probe-objects" EXE="$out/probe.so" boardbits=128 \
        "CXXFLAGS=-O2 -DFSX_CONFIG_TEST=$value" >"$out/probe.log" 2>&1 || {
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
echo 'DLL recompiles when explicit compiler flags change'
