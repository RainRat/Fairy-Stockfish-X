#!/usr/bin/env bash

set -euo pipefail

source "$(cd "$(dirname "$0")/../../../.." && pwd)/tests/lib/uci.sh"
setup_test_context "${1:-}" "${2:-}" "Mortal Chessgi capture demotion"

capture_fen() {
    local captured="$1" expected="$2" label="$3"
    local out
    out=$(run_uci "$ENGINE" "$VARIANTS" mortalchessgi <<UCI
position fen 4k3/8/8/8/3${captured}R3/8/8/4K3 w - - 0 1 moves e4d4
d
UCI
)
    assert_contains_literal "$out" "Fen: 4k3/8/8/8/3R4/8/8/4K3${expected} b - - 0 1" "$label"
}

capture_fen q "[R]" "queen demotes to rook"
capture_fen r "[B]" "rook demotes to bishop"
capture_fen b "[N]" "bishop demotes to knight"
capture_fen n "[P]" "knight demotes to pawn"
capture_fen p "[]" "captured pawn is removed"

out=$(run_uci "$ENGINE" "$VARIANTS" mortalchessgi <<'UCI'
position fen 4k3/8/8/8/3qR3/8/8/4K3 w - - 0 1 moves e4d4 e8f8
go perft 1
UCI
)
assert_contains_literal "$out" "R@a1: 1" "demoted rook can be dropped"

out=$(run_uci "$ENGINE" "$VARIANTS" mortalchessgi <<'UCI'
position fen 4k3/8/8/8/3qR3/8/8/4K3 w - - 0 1
go perft 2
UCI
)
assert_contains_literal "$out" "e4d4:" "capture participates in make/undo perft"
