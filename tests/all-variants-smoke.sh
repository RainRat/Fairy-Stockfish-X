#!/bin/bash

# Verify every variant available to this engine can search its start position.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "all variants smoke test"

SMOKE_TIMEOUT="${VARIANT_SMOKE_TIMEOUT:-5m}"

variant_list() {
  awk '
    /^option name UCI_Variant type combo / { line = $0 }
    END {
      sub(/^.* default [^ ]+ /, "", line)
      count = split(line, fields, " ")
      for (i = 1; i < count; ++i)
        if (fields[i] == "var")
          print fields[i + 1]
    }
  '
}

get_variants() {
  local variant_path="${1:-}"

  {
    printf 'uci\n'
    if [[ -n "${variant_path}" ]]; then
      printf 'setoption name VariantPath value %s\nuci\n' "${variant_path}"
    fi
    printf 'quit\n'
  } | uci_timeout "${ENGINE}" | variant_list
}

smoke_variants() {
  local label="$1"
  local variant_path="$2"
  shift 2
  local variants=("$@")
  local output i failed=0
  local -a bestmoves

  [[ ${#variants[@]} -gt 0 ]] || {
    echo "no ${label} variants found" >&2
    return 1
  }

  output=$(mktemp)
  fsx_add_exit_cleanup "rm -f '${output}'"

  if ! {
    printf 'uci\n'
    if [[ -n "${variant_path}" ]]; then
      printf 'setoption name VariantPath value %s\n' "${variant_path}"
    fi
    for variant in "${variants[@]}"; do
      printf 'setoption name UCI_Variant value %s\nposition startpos\ngo depth 1\n' "${variant}"
    done
    printf 'quit\n'
  } | timeout "${SMOKE_TIMEOUT}" "${ENGINE}" >"${output}" 2>&1; then
    echo "${label} variant smoke run failed or timed out" >&2
    tail -40 "${output}" >&2
    return 1
  fi

  mapfile -t bestmoves < <(sed -n 's/^bestmove //p' "${output}")
  if [[ ${#bestmoves[@]} -ne ${#variants[@]} ]]; then
    echo "${label} variant smoke run returned ${#bestmoves[@]} bestmoves for ${#variants[@]} variants" >&2
    tail -80 "${output}" >&2
    return 1
  fi

  for i in "${!variants[@]}"; do
    if [[ "${bestmoves[$i]}" == "0000" || "${bestmoves[$i]}" == "(none)"* ]]; then
      echo "${label} variant '${variants[$i]}' starts in a terminal position (${bestmoves[$i]})" >&2
      failed=1
    fi
  done

  [[ ${failed} -eq 0 ]] || return 1
  echo "ok: ${label} variants (${#variants[@]})"
}

mapfile -t internal_variants < <(get_variants)
mapfile -t configured_variants < <(get_variants "${VARIANT_PATH}")

echo "all variants smoke testing started"
smoke_variants "internal" "" "${internal_variants[@]}"
smoke_variants "variants.ini" "${VARIANT_PATH}" "${configured_variants[@]}"
echo "all variants smoke test passed"
