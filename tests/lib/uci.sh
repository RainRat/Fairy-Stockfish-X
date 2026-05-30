#!/bin/bash

# Detect project root directory relative to this script
UCI_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "${UCI_LIB_DIR}/../.." && pwd)

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

