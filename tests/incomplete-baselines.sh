#!/bin/bash

set -euo pipefail

ENGINE=${1:-./stockfish}
VARIANT_PATH=${2:-variants-incomplete.ini}

run_uci() {
  local variant=$1
  local commands=$2

  cat <<UCI | "${ENGINE}"
uci
setoption name VariantPath value ${VARIANT_PATH}
setoption name UCI_Variant value ${variant}
isready
${commands}
quit
UCI
}

assert_contains() {
  local output=$1
  local needle=$2
  local label=$3

  if ! grep -Fq "${needle}" <<<"${output}"; then
    echo "incomplete baseline test failed: ${label}" >&2
    echo "Expected to find: ${needle}" >&2
    echo "${output}" >&2
    exit 1
  fi
}

dots_out=$(run_uci dots-boxes-2x2 "position startpos
go perft 1")
assert_contains "${dots_out}" "variant dots-boxes-2x2" "dots-boxes-2x2 loads"
assert_contains "${dots_out}" "Nodes searched: 12" "dots-boxes-2x2 wall placements"

camel_out=$(run_uci camel-rhino "position startpos
go perft 1")
if grep -Fq "unknown variant 'camel-rhino'; keeping 'chess'" <<<"${camel_out}"; then
  echo "camel-rhino skipped by this engine build"
else
  assert_contains "${camel_out}" "variant camel-rhino" "camel-rhino loads in large-board builds"
  assert_contains "${camel_out}" "Nodes searched:" "camel-rhino perft runs"
fi

echo "incomplete baselines ok"
