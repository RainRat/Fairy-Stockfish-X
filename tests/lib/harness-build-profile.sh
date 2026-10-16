#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
source "${ROOT_DIR}/tests/lib/harness-build.sh"

fixture=$(mktemp -d "${ROOT_DIR}/.local/build/harness-profile.XXXXXX")
trap 'rm -rf "${fixture}"' EXIT
mkdir -p "${fixture}/src"
printf 'all:\n\t@:\n' >"${fixture}/src/Makefile"
printf '# board selector fixture\n' >"${fixture}/src/board-size.mk"
printf '#!/usr/bin/env sh\nexit 0\n' >"${fixture}/src/stockfish-allvars"
chmod +x "${fixture}/src/stockfish-allvars"

has_build_arg() {
  local expected="$1" arg
  for arg in "${FSX_HARNESS_BUILD_ARGS[@]}"; do
    [[ "${arg}" == "${expected}" ]] && return 0
  done
  return 1
}

check_profile() {
  local profile="$1" expected_debug="$2" expected_optimize="$3"
  fsx_build_write_signature "${fixture}" "${fixture}/src/stockfish-allvars" \
    fixture "${profile}"
  fsx_harness_init "${fixture}/src/stockfish-allvars" "${fixture}"
  has_build_arg ARCH=x86-64-modern
  has_build_arg "debug=${expected_debug}"
  has_build_arg "optimize=${expected_optimize}"
}

check_profile \
  'arch=x86-64-modern;board=large;all=yes;nnue=yes;debug=yes;optimize=no;compiler=g++;comp=native' \
  yes no
check_profile \
  'arch=x86-64-modern;board=large;all=yes;nnue=yes;debug=no;optimize=yes;compiler=g++;comp=native' \
  no yes

cp "${fixture}/src/stockfish-allvars" "${fixture}/src/stockfish-huge"
fsx_build_write_signature "${fixture}" "${fixture}/src/stockfish-huge" fixture \
  'arch=x86-64-modern;board=huge;all=yes;nnue=no;debug=yes;optimize=no;compiler=g++;comp=native'
fsx_harness_init "${fixture}/src/stockfish-huge" "${fixture}"
has_build_arg ARCH=x86-64-modern
has_build_arg hugeboards=yes
has_build_arg EXE=stockfish-huge
has_build_arg debug=yes
has_build_arg optimize=no
[[ " ${FSX_HARNESS_CXX_DEFS[*]} " == *' -DBOARD_BITS=512 '* ]]

# A recorded all=no profile must stay all=no: harness objects have to match
# the tested executable, not the filename default.
fsx_build_write_signature "${fixture}" "${fixture}/src/stockfish-huge" fixture \
  'arch=x86-64-modern;board=huge;all=no;nnue=no;debug=yes;optimize=no;compiler=g++;comp=native'
fsx_harness_init "${fixture}/src/stockfish-huge" "${fixture}"
has_build_arg all=no
has_build_arg nnue=no
[[ " ${FSX_HARNESS_CXX_DEFS[*]} " != *' -DALLVARS '* ]]

cp "${fixture}/src/stockfish-allvars" "${fixture}/custom-avx512"
for bits in 64 128 256 512; do
  fsx_build_write_signature "$fixture" "$fixture/custom-avx512" fixture \
    "arch=x86-64-modern;board=$(fsx_board_family "$bits");all=yes;nnue=no;debug=yes;optimize=no"
  fsx_harness_init "$fixture/custom-avx512" "$fixture"
  case "$bits" in
    512) has_build_arg hugeboards=yes ;;
    256) has_build_arg verylargeboards=yes ;;
    128) has_build_arg largeboards=yes ;;
    *) has_build_arg largeboards=no ;;
  esac
  has_build_arg EXE=custom-avx512
  has_build_arg ARCH=x86-64-modern
  [[ " ${FSX_HARNESS_CXX_DEFS[*]} " == *" -DBOARD_BITS=$bits "* ]]
done

# Exercise cache invalidation without compiling a second engine in this fixture.
preparations=0
fsx_harness_prepare_objects() {
  ((preparations += 1))
  printf '%s\n' "$preparations" >"$fixture/src/position.o"
}
unset FSX_REUSE_OBJECTS
fsx_harness_prepare_objects_cached "$fixture/cache" >"$fixture/cache.log"
[[ "$preparations" == 1 ]]
fsx_harness_prepare_objects_cached "$fixture/cache" >>"$fixture/cache.log"
[[ "$preparations" == 1 ]]
printf '# changed selector\n' >>"$fixture/src/board-size.mk"
fsx_harness_prepare_objects_cached "$fixture/cache" >>"$fixture/cache.log"
[[ "$preparations" == 2 ]]
fsx_harness_prepare_objects_cached "$fixture/cache" >>"$fixture/cache.log"
[[ "$preparations" == 2 ]]

echo 'harness build profile mapping passed'
