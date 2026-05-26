#!/bin/bash

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

