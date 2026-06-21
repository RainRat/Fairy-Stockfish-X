#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "hex boards regression"
VARIANT_PATH=${VARIANTS}
VLB_ENGINE="${ENGINE}"

ensure_vlb_engine() {
  local probe_variant="$1"

  if variant_available "$VLB_ENGINE" "$probe_variant" "$VARIANT_PATH"; then
    return 0
  fi
  if [[ -x "${ROOT_DIR}/src/stockfish-vlb" && "$VLB_ENGINE" != "${ROOT_DIR}/src/stockfish-vlb" ]]; then
    VLB_ENGINE="${ROOT_DIR}/src/stockfish-vlb"
  fi
  variant_available "$VLB_ENGINE" "$probe_variant" "$VARIANT_PATH"
}

load_inline_variants <<'INI'
[hex-display:fairy]
maxRank = 5
maxFile = e
hexBoard = true
checking = false
king = -
pieceToCharTable = -
startFen = 5/5/5/5/5 w - - 0 1

[hex-rook-test:fairy]
maxRank = 5
maxFile = e
hexBoard = true
pieceToCharTable = RKBQNPX.rkbqnp.x
king = -
customPiece1 = r:RrfBlbB
customPiece2 = k:WrfFlbF
customPiece3 = b:flBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
customPiece4 = q:RrfBlbBflBrbBrf(2,1)lb(2,1)fr(2,1)bl(2,1)
customPiece5 = n:fl(2,1)lb(2,1)fr(2,1)rb(2,1)rf(2,1)bl(2,1)fl(1,2)lb(1,2)fr(1,2)rb(1,2)rf(1,2)bl(1,2)
customPiece6 = p:mfWclFcrF
customPiece7 = x:WrfFlbF
startFen = 5/5/2R2/5/5 w - - 0 1

[hex-king-test:hex-rook-test]
startFen = 5/5/2K2/5/5 w - - 0 1

[hex-bishop-test:hex-rook-test]
startFen = 5/5/2B2/5/5 w - - 0 1

[hex-queen-test:hex-rook-test]
startFen = 5/5/2Q2/5/5 w - - 0 1

[hex-knight-test:hex-rook-test]
maxRank = 7
maxFile = g
startFen = 7/7/7/3N3/7/7/7 w - - 0 1

[hex-pawn-test:hex-rook-test]
maxRank = 7
maxFile = g
startFen = 7/7/7/3P3/2x1x2/7/7 w - - 0 1

[hex-royal-king-test:hex-rook-test]
maxRank = 7
maxFile = g
king = k:WrfFlbFflFrbFrf(2,1)lb(2,1)fr(2,1)bl(2,1)
startFen = 6k/7/7/3K3/7/7/7 w - - 0 1

[bad-hex:fairy]
hexBoard = true
cylindrical = true
startFen = 8/8/8/8/8/8/8/8 w - - 0 1
INI
tmp_ini="${FSX_TMP_INI}"

echo "hex board regression tests started"

out=$(run_uci "$ENGINE" "$tmp_ini" hex-display <<'UCI'
position startpos
d
UCI
)
assert_contains_literal "$out" "   a    b    c    d    e"
assert_contains_literal "$out" " [  ] [  ] [  ] [  ] [  ] 5"
assert_contains_literal "$out" "         [  ] [  ] [  ] [  ] [  ] 1 *"
assert_contains_literal "$out" "           a    b    c    d    e"
assert_contains_literal "$out" "Fen: 5/5/5/5/5 w - - 0 1"

bad_out=$("${ENGINE}" check "${tmp_ini}" 2>&1 || true)
assert_contains "$bad_out" "hexBoard is not supported together with cylindrical or toroidal topology."
assert_contains "$bad_out" "Variant 'bad-hex' has invalid configuration. Skipping."

if ensure_vlb_engine minihexchess \
  && variant_available "$VLB_ENGINE" "glinski-chess" \
  && variant_available "$VLB_ENGINE" "glinski-chess-3shift" \
  && variant_available "$VLB_ENGINE" "glinski-chess-5shift" \
  && variant_available "$VLB_ENGINE" "van-gennip-hexchess" \
  && variant_available "$VLB_ENGINE" "van-gennip-small-hexchess" \
  && variant_available "$VLB_ENGINE" "mccooey-chess" \
  && variant_available "$VLB_ENGINE" "grand-hexachess"; then
  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 9
  dump_out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'d')
  assert_contains_literal "$dump_out" "startpos ***1prb/**2pkn/*3ppp/7/PPP3*/NKP2**/BRP1*** w - - 0 1"
  assert_contains "$out" "^a2d3: 1$"
  assert_contains "$out" "^a2d4: 1$"
  assert_contains "$out" "^a2b5: 1$"
  assert_contains "$out" "^a2c5: 1$"
  assert_contains "$out" "^c1d2: 1$"
  assert_contains "$out" "^c2d3: 1$"
  assert_contains "$out" "^a3b4: 1$"
  assert_contains "$out" "^b3c4: 1$"
  assert_contains "$out" "^c3d4: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position startpos moves c3d4 g6d5
go perft 1')
  assert_contains "$out" "^b2c3: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position fen ***3K/**2p2/*4p1/3P3/6*/5**/3k*** w - - 0 1
go perft 1')
  assert_contains "$out" "^d4e5: 1$"
  assert_contains "$out" "^d4e6: 1$"
  assert_contains "$out" "^d4f5: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position fen ***3k/**5/*6/3N3/7/7/K6 w - - 0 1
go perft 1')
  assert_contains "$out" "^d4b1: 1$"
  assert_contains "$out" "^d4c1: 1$"
  assert_contains "$out" "^d4a2: 1$"
  assert_contains "$out" "^d4e2: 1$"
  assert_contains "$out" "^d4a3: 1$"
  assert_contains "$out" "^d4f3: 1$"
  assert_contains "$out" "^d4b5: 1$"
  assert_contains "$out" "^d4g5: 1$"
  assert_contains "$out" "^d4c6: 1$"
  assert_contains "$out" "^d4g6: 1$"
  assert_contains "$out" "^d4e7: 1$"
  assert_contains "$out" "^d4f7: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "minihexchess" <<<'position startpos moves c2d3
go movetime 100
go movetime 400')
  assert_contains "$out" "^bestmove [a-g][1-7][a-g][1-7]"
  assert_contains "$out" "score cp"
  assert_not_contains "$out" "score mate"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "glinski-chess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 43
  assert_contains "$out" "^d1d2: 1$"
  assert_contains "$out" "^a4b4: 1$"
  assert_contains "$out" "^a1c2: 1$"
  assert_contains "$out" "^a5b6: 1$"
  assert_contains "$out" "^b1d2: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "glinski-chess-3shift" <<<'position startpos
go perft 1')
  assert_nodes "$out" 38
  assert_contains "$out" "^a2b3: 1$"
  assert_contains "$out" "^a2b4: 1$"
  assert_contains "$out" "^b1c2: 1$"
  assert_contains "$out" "^b1d2: 1$"
  assert_contains "$out" "^c5d6: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "glinski-chess-5shift" <<<'position startpos
go perft 1')
  assert_nodes "$out" 37
  assert_contains "$out" "^a2b3: 1$"
  assert_contains "$out" "^b1c2: 1$"
  assert_contains "$out" "^a5b6: 1$"
  assert_contains "$out" "^b4c5: 1$"
  assert_contains "$out" "^d2e3: 1$"
  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "van-gennip-hexchess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 29
  assert_contains "$out" "^a2b3: 1$"
  assert_contains "$out" "^c2a4: 1$"
  assert_contains "$out" "^c3d4: 1$"
  assert_contains "$out" "^d3e4: 1$"
  assert_contains "$out" "^c2b3: 1$"
  assert_contains "$out" "^e2g3: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "van-gennip-small-hexchess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 29
  assert_contains "$out" "^c2b3: 1$"
  assert_contains "$out" "^a2b3: 1$"
  assert_contains "$out" "^g2h3: 1$"
  assert_contains "$out" "^c3d4: 1$"
  assert_contains "$out" "^f3g4: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "mccooey-chess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 25
  assert_contains "$out" "^c3e4: 1$"
  assert_contains "$out" "^c2e1: 1$"
  assert_contains "$out" "^a4b5: 1$"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "grand-hexachess" <<<'position startpos
go perft 1')
  assert_nodes "$out" 139
  assert_contains "$out" "^i13g12: 1$"
  assert_contains "$out" "^a5a6: 1$"
  assert_contains "$out" "^k5k6: 1$"
  assert_contains "$out" "^c3d4: 1$"
  assert_contains "$out" "^e11f10: 1$"
  assert_contains "$out" "^j13k12: 1$"
else
  echo "hex chess variants regression requires a very-large-board capable engine. Skipping."
fi

if ensure_vlb_engine hex \
  && variant_available "$VLB_ENGINE" "hex" \
  && variant_available "$VLB_ENGINE" "hex-7x7" \
  && variant_available "$VLB_ENGINE" "hex-10x10" \
  && variant_available "$VLB_ENGINE" "hex-16x16" \
  && variant_available "$VLB_ENGINE" "esa-hex" \
  && variant_available "$VLB_ENGINE" "misere-hex" \
  && variant_available "$VLB_ENGINE" "y"; then
  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex" <<<'position startpos
go perft 1')
  assert_nodes "$out" 121

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex-7x7" <<<'position startpos
go perft 1')
  assert_nodes "$out" 49

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex-10x10" <<<'position startpos
go perft 1')
  assert_nodes "$out" 100

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex-16x16" <<<'position startpos
go perft 1')
  assert_nodes "$out" 256

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos
go perft 1')
  assert_nodes "$out" 100

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos moves P@a1
go perft 1')
  assert_contains "$out" "^0000: 1$"
  assert_nodes "$out" 1

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "esa-hex" <<<'position startpos moves P@a1 0000 p@b1 0000
go perft 1')
  assert_nodes "$out" 99

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex" <<<'position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP b - - 0 1
go perft 1')
  assert_nodes "$out" 0

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "hex" <<<'position fen 11/11/11/11/11/4PPP4/11/11/11/11/11[p] b - - 0 1
go perft 1')
  assert_nodes "$out" 118

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "misere-hex" <<<'position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP[P] b - - 0 1
go perft 1')
  assert_nodes "$out" 0

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "y" <<<'position startpos
go perft 1')
  assert_nodes "$out" 55

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "y" <<<'position startpos moves P@h6 P@g4 P@g6 P@h3 P@i6
go perft 1')
  assert_nodes "$out" 50

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "y" <<<'position startpos
go depth 14')
  assert_not_contains "$out" "score mate"

  out=$(run_uci "$VLB_ENGINE" "$VARIANT_PATH" "y" <<<'position startpos moves P@h6
go depth 14')
  assert_not_contains "$out" "score mate"
else
  echo "hex connection variants regression requires a very-large-board capable engine. Skipping."
fi

echo "hex board regression tests passed"
