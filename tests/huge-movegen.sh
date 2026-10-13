#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/.local/build/huge-movegen"
mkdir -p "$out/src"
while IFS= read -r file; do
    relative=${file#"$root/src/"}
    mkdir -p "$out/src/$(dirname "$relative")"
    cp -p "$file" "$out/src/$relative"
done < <(rg --files "$root/src" -g '*.cpp' -g '*.h' -g '*.mk' -g Makefile)
source "$root/tests/lib/build-signature.sh"
args=(ARCH=x86-64 boardbits=512 all=no nnue=no debug=yes optimize=no
      'sanitize=address undefined' 'CXXFLAGS=-O1 -fno-omit-frame-pointer'
      "CXX=${CXX:-g++}" "COMP=${COMP:-gcc}")
if ! fsx_build_object_config_matches "$out" "${args[@]}"; then
    make -C "$out/src" objclean >"$out/build.log" 2>&1
fi
make -C "$out/src" -j "${JOBS:-2}" objects "${args[@]}" >>"$out/build.log" 2>&1 || {
    cat "$out/build.log" >&2
    exit 1
}
fsx_build_write_object_config "$out" "${args[@]}"
objects=("$out/src/"*.o)
for i in "${!objects[@]}"; do
    [[ ${objects[$i]##*/} != main.o ]] || unset 'objects[i]'
done
"${CXX:-g++}" -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
    -DBOARD_BITS=512 -DNNUE_EMBEDDING_OFF -DIS_64BIT -DUSE_PTHREADS -I"$out/src" \
    "$root/tests/native/huge-movegen.cpp" "${objects[@]}" -pthread -o "$out/test"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" UBSAN_OPTIONS=halt_on_error=1 "$out/test"
