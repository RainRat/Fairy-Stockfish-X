#!/usr/bin/env bash
set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "battleotk regression"

echo "battleotk regression started"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position startpos
go perft 1')
assert_contains "$out" "^e2e4n: 1$"
assert_not_contains "$out" "^e2e4: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position startpos moves e2e4n
d')
assert_contains_literal "$out" "Fen: 8/pppppppp/8/8/4P3/8/PPPPNPPP/8 b - - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/6P1/8/8/8/8/8/8 w - - 0 1 moves g7g8n
d')
assert_contains_literal "$out" "Fen: 6N1/6N1/8/8/8/8/8/8 b - - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/ppnppppp/8/2n5/2pP4/4PP2/PPPNNNPP/8 b - d3 0 3
go perft 1')
assert_contains "$out" "^c4d3: 1$"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/ppnppppp/8/2n5/2pP4/4PP2/PPPNNNPP/8 b - d3 0 3 moves c4d3
d')
assert_contains_literal "$out" "Fen: 8/ppnppppp/8/2n5/8/3pPP2/PPPNNNPP/8 w - - 0 4"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position startpos
go depth 1')
assert_contains "$out" "^bestmove "
assert_not_contains_literal "$out" "bestmove (none)"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1 moves a1b2
d')
assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/1K6/8 b - - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 1')
assert_contains "$out" "^bestmove "
assert_not_contains_literal "$out" "bestmove (none)"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2')
assert_contains "$out" "^bestmove "
assert_not_contains_literal "$out" "bestmove (none)"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1k6/K7 w - - 0 1
go depth 2 searchmoves a1b1 a1a2 a1b2')
assert_contains_literal "$out" "bestmove a1b2"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1kk5/K7 w - - 0 1 moves a1b2
d')
assert_contains_literal "$out" "Fen: 8/8/8/8/8/8/1kk5/K7 w - - 0 1"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen 8/8/8/8/8/8/1kk5/K7 w - - 0 1
go depth 1')
assert_contains_literal "$out" "bestmove (none)"

out=$(run_uci "$ENGINE" "$VARIANTS" "battleotk" <<<'position fen K7/R6q/7r/8/8/8/6Q1/8 b - - 0 1 moves h7h8k
d
go perft 1')
assert_contains_literal "$out" "Fen: K7/R6q/7r/8/8/8/6Q1/8 b - - 0 1"
assert_not_contains "$out" "^h7h8k:"
assert_contains "$out" "^h7g7k: 1$"

echo "battleotk regression passed"
