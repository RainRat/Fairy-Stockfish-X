#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
source "$root/tests/lib/build-signature.sh"
out="$root/.local/build/board-size-options"
mkdir -p "$out"
cat >"$out/print.mk" <<'MAKE'
.PHONY: print-board-size
print-board-size:
	@echo $(boardbits) $(largeboards) $(verylargeboards) $(hugeboards)
print-flags:
	@echo $(CXXFLAGS)
MAKE
for makefile in Makefile Makefile_js Makefile_dll; do
    check() {
        local expected="$1"
        shift
        local actual
        actual=$(make --no-print-directory -s -C "$root/src" -f "$makefile" \
            -f "$out/print.mk" print-board-size "$@")
        [[ "$actual" == "$expected" ]] || {
            echo "$makefile $*: expected '$expected', got '$actual'" >&2
            return 1
        }
    }
    reject() {
        if make --no-print-directory -s -C "$root/src" -f "$makefile" \
            -f "$out/print.mk" print-board-size "$@" >"$out/rejected.log" 2>&1; then
            echo "$makefile accepted conflicting/invalid options: $*" >&2
            return 1
        fi
    }
    if [[ "$makefile" == Makefile ]]; then
        check '64 no no no'
    else
        check '128 yes no no'
    fi
    check '128 yes no no' largeboards=yes
    check '256 yes yes no' verylargeboards=yes
    check '512 yes yes yes' hugeboards=yes
    check '512 yes yes yes' hugeboards=yes largeboards=yes verylargeboards=yes
    check '256 yes yes no' hugeboards=no verylargeboards=yes
    reject boardbits=512
    reject boardbits=256
    reject hugeboards=yes largeboards=no
    reject hugeboards=yes verylargeboards=no
    reject hugeboards=maybe
    for width in 64 128 256 512; do
        case "$width" in
            512) capacity=hugeboards=yes ;;
            256) capacity=verylargeboards=yes ;;
            128) capacity=largeboards=yes ;;
            *) capacity=largeboards=no ;;
        esac
        # Upstream semantics: command-line CXXFLAGS replace the makefile's
        # flags, so the capacity selector is dropped along with them.
        flags=$(make --no-print-directory -s -C "$root/src" -f "$makefile" \
            -f "$out/print.mk" print-flags ARCH=x86-64 "$capacity" \
            'CXXFLAGS=-O2 -DFSX_USER_FLAGS')
        [[ " $flags " == *' -DFSX_USER_FLAGS '* ]]
        [[ " $flags " != *' -DBOARD_BITS='* && " $flags " != *' -std=c++17 '* ]]
        # EXTRACXXFLAGS is the additive route: extras merge with the full
        # makefile flags, including the capacity selector.
        flags=$(make --no-print-directory -s -C "$root/src" -f "$makefile" \
            -f "$out/print.mk" print-flags ARCH=x86-64 "$capacity" \
            'EXTRACXXFLAGS=-O2 -DFSX_USER_FLAGS')
        [[ " $flags " == *" -DBOARD_BITS=$width "* && " $flags " == *' -DFSX_USER_FLAGS '* ]]
        [[ " $flags " == *' -std=c++17 '* ]]
    done
done
[[ $(fsx_build_profile hugeboards=yes) == *';board=huge;'* ]]
[[ $(fsx_build_profile hugeboards=yes) == "$(fsx_build_profile hugeboards=yes largeboards=yes verylargeboards=yes)" ]]
ROOT_DIR="$out/profile-fixture"
unset FSX_ENGINE_FAMILY
[[ $(fsx_engine_family "$ROOT_DIR" stockfish-avx512) == normal ]]
[[ $(fsx_engine_family "$ROOT_DIR" stockfish-huge) == huge ]]
[[ $(fsx_engine_family "$ROOT_DIR" stockfish-512-debug) == huge ]]
mkdir -p "$ROOT_DIR/src"
printf '#!/bin/sh\nexit 0\n' > "$ROOT_DIR/src/stockfish-avx512"
for bits in 64 128 256 512; do
    family=$(fsx_board_family "$bits")
    fsx_build_write_signature "$ROOT_DIR" "$ROOT_DIR/src/stockfish-avx512" fixture \
        "board=$family;all=yes"
    [[ $(fsx_engine_family "$ROOT_DIR" "$ROOT_DIR/src/stockfish-avx512") == "$family" ]]
    [[ $(fsx_board_bits "$family") == "$bits" ]]
done
# Exercise the public suite interface instead of importing private functions.
if FSX_CASE_FILTER=native-royal FSX_ALLOW_SMALL_BOARD=0 \
    bash "$root/tests/suites/royal-legality.sh" stockfish-avx512 >"$out/native-avx512.log" 2>&1; then
    echo 'native suite accepted an AVX-512 filename as a large-board build' >&2
    exit 1
fi
grep -q 'requires a large-board engine' "$out/native-avx512.log"

compile() {
    printf '#include "types.h"\n' | "${CXX:-g++}" -std=c++17 -fsyntax-only -x c++ \
        -I"$root/src" "$@" -
}
for bits in 64 128 256 512; do compile "-DBOARD_BITS=$bits"; done
compile -DLARGEBOARDS
compile -DVERY_LARGE_BOARDS
compile -DHUGEBOARDS
compile -DBOARD_BITS=256 -DLARGEBOARDS -DVERY_LARGE_BOARDS
compile -DBOARD_BITS=512 -DHUGEBOARDS -DLARGEBOARDS -DVERY_LARGE_BOARDS
for flags in '-DBOARD_BITS=64 -DLARGEBOARDS' '-DBOARD_BITS=64 -DVERY_LARGE_BOARDS' \
             '-DBOARD_BITS=128 -DVERY_LARGE_BOARDS' '-DBOARD_BITS=256 -DHUGEBOARDS'; do
    read -r -a args <<<"$flags"
    if compile "${args[@]}" >"$out/conflicting-macros.log" 2>&1; then
        echo "compiler accepted conflicting macros: $flags" >&2
        exit 1
    fi
    grep -q 'Legacy board macros conflict with BOARD_BITS' "$out/conflicting-macros.log"
done
echo 'Board selectors, profiles, legacy compiler flags and conflict rejection passed'
