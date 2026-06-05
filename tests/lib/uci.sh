#!/bin/bash

# Detect project root directory relative to this script
UCI_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${UCI_LIB_DIR}/../.." && pwd)

fsx_error() {
  local test_name="$1"
  local line="$2"
  echo "${test_name} failed on line ${line}" >&2
  exit 1
}

FSX_EXIT_CLEANUPS=()

fsx_run_exit_cleanups() {
  local status=$?
  local cleanup

  set +e
  for cleanup in "${FSX_EXIT_CLEANUPS[@]}"; do
    eval "${cleanup}"
  done

  return "${status}"
}

fsx_add_exit_cleanup() {
  local cleanup="${1:-}"

  if [[ -z "${cleanup}" ]]; then
    return
  fi

  if [[ ${#FSX_EXIT_CLEANUPS[@]} -eq 0 ]]; then
    trap fsx_run_exit_cleanups EXIT
  fi

  FSX_EXIT_CLEANUPS+=("${cleanup}")
}

init_test_env() {
  local engine_arg="${1:-}"
  local variants_arg="${2:-}"
  local test_name="${3:-${BASH_SOURCE[1]##*/}}"

  if [[ -z "${SCRIPT_DIR:-}" ]]; then
    SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)
  fi
  if [[ -z "${ROOT_DIR:-}" ]]; then
    ROOT_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)
  fi

  ENGINE="${ENGINE:-$(default_engine "${engine_arg}")}"
  VARIANTS="${VARIANTS:-$(default_variants "${variants_arg}")}"
  export SCRIPT_DIR ROOT_DIR ENGINE VARIANTS

  FSX_TEST_NAME="${test_name}"
  export FSX_TEST_NAME
  set -E
  trap 'fsx_error "${FSX_TEST_NAME}" "${LINENO}"' ERR
}

default_engine() {
  local custom_engine="${1:-}"
  if [[ -n "$custom_engine" ]]; then
    echo "$custom_engine"
  elif [[ -x "${ROOT_DIR}/src/stockfish" ]]; then
    echo "${ROOT_DIR}/src/stockfish"
  else
    echo "${ROOT_DIR}/stockfish"
  fi
}

default_variants() {
  local custom_variants="${1:-}"
  if [[ -n "$custom_variants" ]]; then
    echo "$custom_variants"
  else
    echo "${ROOT_DIR}/src/variants.ini"
  fi
}

assert_contains_literal() {
  local haystack="$1"
  local needle="$2"
  local context="${3:-contains}"

  if ! grep -Fq "$needle" <<<"$haystack"; then
    echo "expected output to ${context}: $needle" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_not_contains_literal() {
  local haystack="$1"
  local needle="$2"
  local context="${3:-not contain}"

  if grep -Fq "$needle" <<<"$haystack"; then
    echo "expected output to ${context}: $needle" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

uci_timeout() {
  timeout "${UCI_TIMEOUT:-60s}" "$@"
}

run_uci() {
  local engine="$1"
  local variant_path="$2"
  local variant="$3"
  shift 3

  {
    printf 'uci\n'
    printf 'setoption name VariantPath value %s\n' "$variant_path"
    printf 'setoption name UCI_Variant value %s\n' "$variant"
    cat
    printf 'quit\n'
  } | uci_timeout "$engine"
}

run_uci_cmds() {
  local engine="$1"
  local variant_path="$2"
  local variant="$3"
  local cmds="$4"
  run_uci "$engine" "$variant_path" "$variant" <<< "$cmds"
}

probe_variant_available() {
  local engine="$1"
  local variant="$2"
  local variant_path="${3:-${VARIANTS}}"
  local out

  out=$(run_uci "$engine" "$variant_path" "$variant" <<<'d')
  grep -Fq "info string variant ${variant} " <<<"$out"
}

variant_available() {
  probe_variant_available "$@"
}

cleanup_tmp_ini() {
  if [[ -n "${FSX_TMP_INI:-}" && -e "${FSX_TMP_INI}" ]]; then
    rm -f "${FSX_TMP_INI}"
  fi
  FSX_TMP_INI=
  TMP_VARIANTS=
}

create_tmp_ini() {
  cleanup_tmp_ini
  FSX_TMP_INI=$(mktemp "${TMPDIR:-/tmp}/fsx-uci-XXXXXX.ini")
  export FSX_TMP_INI
  TMP_VARIANTS="${FSX_TMP_INI}"
  export TMP_VARIANTS
}

init_tmp_ini() {
  create_tmp_ini
  fsx_add_exit_cleanup cleanup_tmp_ini
}

load_inline_variants() {
  create_tmp_ini
  cat >"${FSX_TMP_INI}"
  fsx_add_exit_cleanup cleanup_tmp_ini
}

uci_position_command() {
  local fen_or_startpos="$1"
  shift

  if [[ "$fen_or_startpos" == "startpos" ]]; then
    printf 'position startpos'
  else
    printf 'position fen %s' "$fen_or_startpos"
  fi

  if (($#)); then
    printf ' moves %s' "$*"
  fi
  printf '\n'
}

run_perft() {
  local variant="$1"
  local fen_or_startpos="$2"
  local depth="$3"

  run_uci "${ENGINE}" "${VARIANTS}" "${variant}" <<UCI
$(uci_position_command "${fen_or_startpos}")
go perft ${depth}
UCI
}

run_display() {
  local variant="$1"
  local fen_or_startpos="$2"
  shift 2

  run_uci "${ENGINE}" "${VARIANTS}" "${variant}" <<UCI
$(uci_position_command "${fen_or_startpos}" "$@")
d
UCI
}

run_pyffish_test() {
  PYTHONPATH="${ROOT_DIR}${PYTHONPATH:+:${PYTHONPATH}}" "${PYTHON:-python3}" "$@"
}

engine_config_output() {
  if [[ -z "${FSX_ENGINE_CONFIG_OUTPUT:-}" ]]; then
    FSX_ENGINE_CONFIG_OUTPUT=$(make -C "${ROOT_DIR}/src" -s config-sanity)
    export FSX_ENGINE_CONFIG_OUTPUT
  fi
  printf '%s\n' "${FSX_ENGINE_CONFIG_OUTPUT}"
}

engine_config_value() {
  local key="$1"
  engine_config_output | sed -n "s/^${key}: //p" | tail -n1
}

run_engine_stdin() {
  local engine="$1"
  local input="$2"

  printf '%s' "$input" | uci_timeout "$engine" 2>&1
}

bench_nodes() {
  awk '/Nodes searched  : / {print $4}' | tail -n1
}

expect_engine_setup() {
  local spawn_args="${1:-}"

  printf '   set engine [lindex $argv 0]\n'
  printf '   spawn $engine%s\n' "${spawn_args:+ ${spawn_args}}"
}

run_expect() {
  local timeout_seconds="${EXPECT_TIMEOUT:-20}"
  local exp_file
  local status

  exp_file=$(mktemp "${TMPDIR:-/tmp}/fsx-expect-XXXXXX.exp")
  cat >"${exp_file}"

  if timeout "${timeout_seconds}" expect "${exp_file}" "$@"; then
    status=0
  else
    status=$?
  fi

  rm -f "${exp_file}"
  return "${status}"
}


assert_contains() {
  local haystack="$1"
  local pattern="$2"
  local context="${3:-contains}"

  if ! grep -Eq "$pattern" <<<"$haystack"; then
    echo "expected output to ${context}: $pattern" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_not_contains() {
  local haystack="$1"
  local pattern="$2"
  local context="${3:-not contain}"

  if grep -Eq "$pattern" <<<"$haystack"; then
    echo "expected output to ${context}: $pattern" >&2
    echo "actual output:" >&2
    printf '%s\n' "$haystack" >&2
    return 1
  fi
}

assert_nodes() {
  local haystack="$1"
  local expected="$2"

  assert_contains "$haystack" "^Nodes searched: ${expected}$" "have exact node count"
}
