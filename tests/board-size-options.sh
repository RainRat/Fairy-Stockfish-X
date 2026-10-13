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
echo 'Board selectors and Huge boards profiles passed'
