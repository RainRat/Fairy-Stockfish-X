#!/usr/bin/env bash
set -euo pipefail

ENGINE="${1:-/home/chris/Fairy-Stockfish-X/src/stockfish}"
TMP_VARIANTS="$(mktemp /tmp/stationary-castling.XXXXXX.ini)"
trap 'rm -f "${TMP_VARIANTS}"' EXIT

cat > "${TMP_VARIANTS}" <<'INI'
[stationary-castling-safe:chess]
castling = true
castlingKingFile = e
castlingKingsideFile = g
castlingQueensideFile = e
castlingRookKingsideFile = h
castlingRookQueensideFile = d
startFen = 8/8/8/8/8/8/8/3RK2R w KQ - 0 1

[stationary-castling-exposed:chess]
castling = true
castlingKingFile = e
castlingKingsideFile = g
castlingQueensideFile = e
castlingRookKingsideFile = h
castlingRookQueensideFile = d
startFen = 8/8/8/8/8/8/8/r2RK2R w KQ - 0 1
INI

run_cmds() {
  local variant="$1"
  local cmds="$2"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "${TMP_VARIANTS}" "${variant}" "${cmds}" | "${ENGINE}"
}

echo "stationary castling regression started"

out=$(run_cmds stationary-castling-safe "position startpos
go perft 1")
echo "${out}" | grep -q "^e1e1: 1$"

out=$(run_cmds stationary-castling-safe "position startpos moves e1e1
d")
echo "${out}" | grep -Fq "Fen: 8/8/8/8/8/8/8/4KR1R b - - 1 1"

out=$(run_cmds stationary-castling-exposed "position startpos
go perft 1")
! echo "${out}" | grep -q "^e1e1: 1$"
echo "${out}" | grep -q "^e1g1: 1$"

echo "stationary castling regression passed"
