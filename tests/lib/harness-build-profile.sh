#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
source "${ROOT_DIR}/tests/lib/harness-build.sh"

fixture=$(mktemp -d "${ROOT_DIR}/.local/build/harness-profile.XXXXXX")
trap 'rm -rf "${fixture}"' EXIT
mkdir -p "${fixture}/src"
printf 'all:\n\t@:\n' >"${fixture}/src/Makefile"
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

echo 'harness build profile mapping passed'
