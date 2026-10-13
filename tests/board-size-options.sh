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
    check '512 yes yes yes' boardbits=512
    check '512 yes yes yes' hugeboards=yes boardbits=512 largeboards=yes verylargeboards=yes
    check '256 yes yes no' hugeboards=no verylargeboards=yes
    reject hugeboards=yes boardbits=256
    reject hugeboards=no boardbits=512
    reject hugeboards=yes largeboards=no
    reject hugeboards=yes verylargeboards=no
    reject hugeboards=maybe
    reject 'boardbits=256 512'
    for width in 64 128 256 512; do
        flags=$(make --no-print-directory -s -C "$root/src" -f "$makefile" \
            -f "$out/print.mk" print-flags ARCH=x86-64 boardbits="$width" \
            'CXXFLAGS=-O2 -DFSX_USER_FLAGS')
        [[ " $flags " == *" -DBOARD_BITS=$width "* && " $flags " == *' -DFSX_USER_FLAGS '* ]]
        [[ " $flags " == *' -std=c++17 '* && " $flags " == *' -DNNUE_EMBEDDING_OFF '* ]]
        if [[ "$makefile" == Makefile_js ]]; then
            [[ " $flags " == *' --bind '* && " $flags " == *' -DNO_THREADS '* ]]
        elif [[ "$makefile" == Makefile_dll ]]; then
            [[ " $flags " == *' -DALLVARS '* ]]
            case $(uname -s) in Linux|Darwin) [[ " $flags " == *' -fPIC '* ]] ;; esac
        fi
    done
done
[[ $(fsx_build_profile hugeboards=yes) == *';board=512;'* ]]
[[ $(fsx_build_profile hugeboards=yes) == "$(fsx_build_profile boardbits=512)" ]]
source <(sed -n '/^engine_family() {/,/^}/p' "$root/tests/run.sh")
ROOT_DIR="$out/profile-fixture"
unset FSX_ENGINE_FAMILY
[[ $(engine_family stockfish-avx512) == normal ]]
[[ $(engine_family stockfish-huge) == 512 ]]
[[ $(engine_family stockfish-512-debug) == 512 ]]
mkdir -p "$ROOT_DIR/src"
printf '#!/bin/sh\nexit 0\n' > "$ROOT_DIR/src/stockfish-avx512"
fsx_build_write_signature "$ROOT_DIR" "$ROOT_DIR/src/stockfish-avx512" fixture \
    'board=large;all=yes'
[[ $(engine_family "$ROOT_DIR/src/stockfish-avx512") == large ]]
source <(sed -n '/^native() {/,/^}/p' "$root/tests/lib/suite.sh")
if FSX_ALLOW_SMALL_BOARD=0 ENGINE=stockfish-avx512 native royal >"$out/native-avx512.log" 2>&1; then
    echo 'native suite accepted an AVX-512 filename as a large-board build' >&2
    exit 1
fi
echo 'Board selectors and Huge boards profiles passed'
