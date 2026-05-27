#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "hex connection variants regression failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

ENGINE="${1:-}"
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "${ROOT_DIR}/src/stockfish-vlb" ]]; then
    ENGINE="${ROOT_DIR}/src/stockfish-vlb"
  else
    ENGINE=$(default_engine)
  fi
fi
VARIANT_PATH=$(default_variants "${2:-}")

variant_available() {
  local v="$1"
  local out
  out=$(run_uci "$ENGINE" "$VARIANT_PATH" "$v" <<<'d')
  grep -Fq "info string variant ${v} " <<<"${out}"
}

if ! variant_available "hex"; then
  echo "hex connection variants regression requires a very-large-board capable engine"
  exit 1
fi

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "hex" <<<'position startpos
go perft 1')
assert_nodes "$out" 121

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "hex-7x7" <<<'position startpos
go perft 1')
assert_nodes "$out" 49

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "hex-10x10" <<<'position startpos
go perft 1')
assert_nodes "$out" 100

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "hex-16x16" <<<'position startpos
go perft 1')
assert_nodes "$out" 256

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos
go perft 1')
assert_nodes "$out" 100

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos moves P@a1
go perft 1')
assert_contains "$out" "^0000: 1$"
assert_nodes "$out" 1

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos moves P@a1 0000 p@b1 0000
go perft 1')
assert_nodes "$out" 99

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "hex" <<<'position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP b - - 0 1
go perft 1')
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "misere-hex" <<<'position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP[P] b - - 0 1
go perft 1')
assert_nodes "$out" 0

out=$(run_uci "$ENGINE" "$VARIANT_PATH" "y" <<<'position startpos
go perft 1')
assert_nodes "$out" 55

echo "hex connection variants regression passed"
