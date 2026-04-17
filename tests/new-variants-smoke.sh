#!/bin/bash

set -euo pipefail

error() {
  echo "new-variants smoke test failed on line $1"
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-}
if [[ -z "${ENGINE}" ]]; then
  if [[ -x "src/stockfish" ]]; then
    ENGINE="src/stockfish"
  else
    ENGINE="./stockfish"
  fi
fi
VARIANT_PATH=${2:-variants.ini}
if [[ ! -f "${VARIANT_PATH}" && -f "src/variants.ini" ]]; then
  VARIANT_PATH="src/variants.ini"
fi

run_cmds() {
  cat <<EOF | "${ENGINE}" 2>/dev/null
uci
setoption name VariantPath value ${VARIANT_PATH}
$1
quit
EOF
}

variant_available() {
  local v="$1"
  local out
  out=$(run_cmds "setoption name UCI_Variant value ${v}
d")
  if echo "${out}" | grep -q "info string variant ${v} "; then
    return 0
  fi
  return 1
}

echo "new variants smoke testing started"

# 0) PieceTypeBitboardGroup repeated piece clauses are additive, not overwrite.
tmp_ini=$(mktemp)
cat > "${tmp_ini}" <<'INI'
[ptgroup-merge:chess]
promotionRegionWhite = P(a8);P(h8); *(*8)
promotionRegionBlack = P(a1);P(h1); *(*1)
promotionPieceTypes = q
promotionPieceTypesWhite = q
promotionPieceTypesBlack = q
INI
out=$(cat <<EOF | "${ENGINE}"
uci
setoption name VariantPath value ${tmp_ini}
setoption name UCI_Variant value ptgroup-merge
position fen 4k3/P6P/8/8/8/8/8/4K3 w - - 0 1
go perft 1
quit
EOF
)
echo "${out}" | grep -q "^a7a8q: 1$"
echo "${out}" | grep -q "^h7h8q: 1$"
rm -f "${tmp_ini}"

# This smoke suite contains >8x8 and template-dependent variants.
# Run each block only if the underlying variant exists in the current build.

# 0b) Berolina pawns move diagonally, including the initial double-step.
out=$(run_cmds "setoption name UCI_Variant value berolina
position startpos
go perft 1")
echo "${out}" | grep -q "^a2b3: 1$"
echo "${out}" | grep -q "^a2c4: 1$"
! echo "${out}" | grep -q "^a2a3: 1$"
! echo "${out}" | grep -q "^a2a4: 1$"

# 1) Hasami: orthogonal sandwich should capture the middle piece.
if variant_available "hasami"; then
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/R1rR5/9 w - - 0 1 moves a2b2
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1R1R5/9 b - - 1 1"

# 2) Hasami: edge alone is not hostile; moving away must not capture.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/Rr7/9 w - - 0 1 moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1r7/R8 b - - 1 1"
fi

# 3) Achi: pre-connected line is immediate game end (no legal moves).
if variant_available "achi"; then
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PPP/3/3[PPPPpppp] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 4) Achi: non-terminal filled setup still yields legal drops.
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PpP/pPp/3[PpppPPPP] w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 3"
fi

# 5) Checkless: king capture is legal (checks are disabled by variant).
if variant_available "checkless"; then
out=$(run_cmds "setoption name UCI_Variant value checkless
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2e8: 1$"
fi

# 5a) Janggi: non-king moves may not expose the king to a cannon check.
if variant_available "janggi"; then
out=$(run_cmds "setoption name UCI_Variant value janggi
position fen rnba1abnr/4k4/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/4C2C1/4K4/RNBA1ABNR b - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e7f7: 1$"
fi

# 5b) Little Trio: the opening cannon capture over the pawn screen is available.
if variant_available "little-trio"; then
out=$(run_cmds "setoption name UCI_Variant value little-trio
position startpos
go perft 1")
echo "${out}" | grep -q "^f1f6: 1$"
fi

# 5ba) Former built-ins moved to variants.ini still load and preserve core behavior.
for v in balancedalternation2 raazuvaa paradigm joust fox-and-hounds; do
  variant_available "${v}"
done
out=$(run_cmds "setoption name UCI_Variant value balancedalternation2
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 20"
out=$(run_cmds "setoption name UCI_Variant value raazuvaa
position startpos
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value paradigm
position fen 4k3/1P6/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^b7b8b: 1$"
if variant_available "joust"; then
out=$(run_cmds "setoption name UCI_Variant value joust
position startpos
go perft 1")
echo "${out}" | grep -q "^d4b5,b5d4: 1$"
fi
out=$(run_cmds "setoption name UCI_Variant value fox-and-hounds
position startpos
go perft 1")
echo "${out}" | grep -q "^e1d2: 1$"

if variant_available "lewthwaite-swap"; then
out=$(run_cmds "setoption name UCI_Variant value lewthwaite-swap
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched:"
! echo "${out}" | grep -q "s: 1$"
fi

# 5bb) Additional Groups variants load and expose the expected setup-phase drops.
for v in groups groups-fixed groups-setup groups-jump-setup groups-queen-fixed groups-queen-jump-fixed groups-queen-setup groups-queen-jump-setup; do
  variant_available "${v}"
done
out=$(run_cmds "setoption name UCI_Variant value groups-setup
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"
out=$(run_cmds "setoption name UCI_Variant value groups-jump-setup
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"
out=$(run_cmds "setoption name UCI_Variant value groups-queen-setup
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"
out=$(run_cmds "setoption name UCI_Variant value groups-queen-jump-setup
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"

# 5bb1) Mini Hexchess loads on the masked 37-cell hex board and exposes the expected opening moves.
if variant_available "hex-7x7"; then
out=$(run_cmds "setoption name UCI_Variant value hex-7x7
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 49"
fi

if variant_available "hex-10x10"; then
out=$(run_cmds "setoption name UCI_Variant value hex-10x10
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 100"
fi

if variant_available "hex-16x16"; then
out=$(run_cmds "setoption name UCI_Variant value hex-16x16
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 256"
fi

if variant_available "esa-hex"; then
out=$(run_cmds "setoption name UCI_Variant value esa-hex
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 100"
out=$(run_cmds "setoption name UCI_Variant value esa-hex
position startpos moves P@a1
go perft 1")
echo "${out}" | grep -q "^0000: 1$"
fi

if variant_available "misere-hex"; then
out=$(run_cmds "setoption name UCI_Variant value misere-hex
position fen 11/11/11/11/11/11/11/11/11/11/PPPPPPPPPPP[P] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

if variant_available "minihexchess"; then
out=$(run_cmds "setoption name UCI_Variant value minihexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 11"
echo "${out}" | grep -q "^a2d3: 1$"
echo "${out}" | grep -q "^a2b5: 1$"
echo "${out}" | grep -q "^c1d2: 1$"
echo "${out}" | grep -q "^a3b4: 1$"
echo "${out}" | grep -q "^c3d4: 1$"
echo "${out}" | grep -q "^b2d3: 1$"
echo "${out}" | grep -q "^b2c4: 1$"
fi

if variant_available "glinski-chess"; then
out=$(run_cmds "setoption name UCI_Variant value glinski-chess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 48"
echo "${out}" | grep -q "^d1d2: 1$"
echo "${out}" | grep -q "^a4b4: 1$"
echo "${out}" | grep -q "^a1c2: 1$"
echo "${out}" | grep -q "^a5b6: 1$"
echo "${out}" | grep -q "^b1d2: 1$"
fi

if variant_available "glinski-chess-3shift"; then
out=$(run_cmds "setoption name UCI_Variant value glinski-chess-3shift
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 44"
echo "${out}" | grep -q "^c3e4: 1$"
echo "${out}" | grep -q "^c5d6: 1$"
fi

if variant_available "glinski-chess-5shift"; then
out=$(run_cmds "setoption name UCI_Variant value glinski-chess-5shift
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 46"
echo "${out}" | grep -q "^c3e4: 1$"
echo "${out}" | grep -q "^b4c5: 1$"
fi

if variant_available "van-gennip-hexchess"; then
out=$(run_cmds "setoption name UCI_Variant value van-gennip-hexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 36"
echo "${out}" | grep -q "^c3d4: 1$"
fi

if variant_available "van-gennip-small-hexchess"; then
out=$(run_cmds "setoption name UCI_Variant value van-gennip-small-hexchess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 29"
echo "${out}" | grep -q "^c3d4: 1$"
fi

if variant_available "mccooey-chess"; then
out=$(run_cmds "setoption name UCI_Variant value mccooey-chess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 31"
echo "${out}" | grep -q "^c3e4: 1$"
echo "${out}" | grep -q "^c2e1: 1$"
echo "${out}" | grep -q "^a4b5: 1$"
echo "${out}" | grep -q "^a4c6: 1$"
fi

if variant_available "grand-hexachess"; then
out=$(run_cmds "setoption name UCI_Variant value grand-hexachess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 125"
echo "${out}" | grep -q "^i13g12: 1$"
echo "${out}" | grep -q "^a5a6: 1$"
echo "${out}" | grep -q "^k5k6: 1$"
echo "${out}" | grep -q "^c3d4: 1$"
echo "${out}" | grep -q "^e11f10: 1$"
echo "${out}" | grep -q "^j13k12: 1$"
fi

# 5bc) Simplified inheritance stanzas still preserve start positions.
out=$(run_cmds "setoption name UCI_Variant value maharajah
position startpos
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/8/8/4M3 w kq - 0 1"
out=$(run_cmds "setoption name UCI_Variant value pawnsonly
position startpos
d")
echo "${out}" | grep -q "Fen: 8/pppppppp/8/8/8/8/PPPPPPPP/8 w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value pawns
position startpos
d")
echo "${out}" | grep -q "Fen: pppppppp/8/8/8/8/8/8/PPPPPPPP w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value rugby
position startpos
d")
echo "${out}" | grep -q "Fen: pppppppp/8/8/8/8/8/8/PPPPPPPP w - - 0 1"

# 5c) Bombardment: front-rank missiles move forward quietly and can self-destruct.
if variant_available "bombardment"; then
out=$(run_cmds "setoption name UCI_Variant value bombardment
position startpos
go perft 1")
echo "${out}" | grep -q "^a2a3: 1$"
echo "${out}" | grep -q "^a2b3: 1$"
echo "${out}" | grep -q "^a2a2x: 1$"
fi

# 5d) Ko-Oshi: opening setup loads and immediate push-back is illegal.
if variant_available "ko-oshi"; then
out=$(run_cmds "setoption name UCI_Variant value ko-oshi
position startpos
d")
echo "${out}" | grep -q "Fen: b3b/1aaa1/5/1AAA1/B3B w - - 0 1 {0 0}"
out=$(run_cmds "setoption name UCI_Variant value ko-oshi
position fen 5/5/5/1a3/1A3 w - - 0 1 {0 0} moves b1b2
go perft 1")
! echo "${out}" | grep -q "b3b2: 1"
fi

# 5e) Oshi: opening setup loads, push-back is illegal, and scoring/adjudication follow rules.
if variant_available "oshi"; then
out=$(run_cmds "setoption name UCI_Variant value oshi
position startpos
d")
echo "${out}" | grep -q "Fen: c7c/2baaab2/4a4/9/9/9/4A4/2BAAAB2/C7C w - - 0 1 {0 0}"
out=$(run_cmds "setoption name UCI_Variant value oshi
position startpos moves i1f1 i9f9
go perft 1")
echo "${out}" | grep -q "^f1f4: 1$"
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/1a3/1A3 w - - 0 1 {0 0} moves b1b2
go perft 1")
! echo "${out}" | grep -q "b3b2: 1"
out=$(run_cmds "setoption name UCI_Variant value oshi
position startpos moves i1f1 i9f9 f1f4
d")
echo "${out}" | grep -q "Fen: c4c3/2baaab2/4a4/9/5A3/5C3/4A4/2BAA1B2/C8 b - - 3 2 {0 0}"
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/C8/c8 w - - 0 1 {0 0} moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/9/C8 b - - 0 1 {3 0}"
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/C8/C8 w - - 0 1 {0 0} moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/9/C8 b - - 0 1 {0 3}"
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen C8/9/9/9/9/9/9/9/9 b - - 0 1 {7 0}
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
out=$(run_cmds "setoption name UCI_Variant value oshi
position fen 9/9/9/9/9/9/9/9/9 b - - 0 1 {8 7}
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 5f) Aries: opening setup loads and a repetition-losing move is avoided in search.
if variant_available "aries"; then
out=$(run_cmds "setoption name UCI_Variant value aries
position startpos
d")
echo "${out}" | grep -q "Fen: 4rrrr/4rrrr/4rrrr/4rrrr/RRRR4/RRRR4/RRRR4/RRRR4 w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value aries
position fen 8/8/8/8/8/8/7r/R7 w - - 0 1 moves a1a2 h2h1 a2a1 h1h2 a1a2 h2h1 a2a1
go depth 3")
! echo "${out}" | grep -q "^bestmove h1h2$"
fi

# 5g) Ko-app-paw-na: hunter capture and rabbit-pattern goal both end the game.
if variant_available "ko-app-paw-na"; then
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen 5/2R2/2h2/5/5 b - - 0 1 moves c3c5
d")
echo "${out}" | grep -q "Fen: 2h2/5/5/5/5 w - - 0 2 {0 1}"
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen 5/2R2/2h2/5/5 w - - 0 2 {0 1}
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen RRRRR/5/R1h1R/5/5 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 6) Tablut split: edge-escape should end immediately, corner-escape should not.
if variant_available "tablut" && variant_available "tablut-corner-escape"; then
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-corner-escape
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 15"
fi

# 7b) Tawlbwrdd: edge escape should end immediately on 11x11, unlike corner-escape Hnefatafl.
if variant_available "tawlbwrdd"; then
out=$(run_cmds "setoption name UCI_Variant value tawlbwrdd
position fen 5K5/11/11/11/11/5r5/11/11/11/11/11 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 7) Tablut split: throne-adjacent king strength changes capture outcome.
if variant_available "tablut" && variant_available "tablut-throne-adjacent-strong"; then
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-throne-adjacent-strong
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 12"
fi

# 7a) Crossway: alternating 2x2 checker-pattern placements are illegal.
if variant_available "crossway"; then
out=$(run_cmds "setoption name UCI_Variant value crossway
position fen 8/8/8/8/8/8/S7/sS6[Ss] b - - 0 1
go perft 1")
! echo "${out}" | grep -q "@b2: 1"

# Existing edge-to-edge connection should end the game immediately.
out=$(run_cmds "setoption name UCI_Variant value crossway
position fen 8/8/8/SSSSSSSS/8/8/8/8[Ss] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value crossway
position startpos moves S@d4
go perft 1")
echo "${out}" | grep -q "^S@d4: 1$"

out=$(run_cmds "setoption name UCI_Variant value crossway
position startpos moves S@d4 S@d4
go perft 1")
! echo "${out}" | grep -q "^S@d4: 1$"
fi

# 7b) Pathway: enemy-only adjacency drops are illegal, one-friendly drops are legal, and no-placement wins.
if variant_available "pathway"; then
out=$(run_cmds "setoption name UCI_Variant value pathway
position fen 6/6/6/6/3p2/6[Pp] w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^P@c2: 1$"

out=$(run_cmds "setoption name UCI_Variant value pathway
position fen 6/6/6/2P3/3p2/6[Pp] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^P@c2: 1$"

out=$(run_cmds "setoption name UCI_Variant value pathway
position fen PPPPPP/PPPPPP/PPPPPP/pppppp/pppppp/pppppp[Pp] w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 7c) Kopano: mirrored opening swap, reciprocal weak links, crosscut blocks, and no-placement wins.
if variant_available "kopano"; then
out=$(run_cmds "setoption name UCI_Variant value kopano
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 64"

out=$(run_cmds "setoption name UCI_Variant value kopano
position startpos moves P@b1
go perft 1")
echo "${out}" | grep -q "^P@a2: 1$"
! echo "${out}" | grep -q "^P@b1: 1$"

out=$(run_cmds "setoption name UCI_Variant value kopano
position fen 8/8/8/8/8/8/1P6/8[Pp] w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^P@c3: 1$"

out=$(run_cmds "setoption name UCI_Variant value kopano
position fen 8/8/8/8/3p4/8/1P6/8[Pp] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^P@c3: 1$"

out=$(run_cmds "setoption name UCI_Variant value kopano
position fen 8/8/8/8/2pP4/3p4/8/8[Pp] w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^P@c3: 1$"

out=$(run_cmds "setoption name UCI_Variant value kopano
position fen 7p/6p1/5p2/4p3/3p4/2p5/1p6/p7 w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 7d) Hex-family connection variants: Y and Hex load on the expected build sizes.
if variant_available "y"; then
out=$(run_cmds "setoption name UCI_Variant value y
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 55"
fi

if variant_available "hex"; then
out=$(run_cmds "setoption name UCI_Variant value hex
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 121"
fi

# 8) Neutreeko: max-distance move completes a line and ends the game.
if variant_available "neutreeko"; then
out=$(run_cmds "setoption name UCI_Variant value neutreeko
position fen 5/3N1/5/1N3/N4 w - - 0 1 moves d4c3
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 9) Forced en passant: when EP is legal, ordinary king moves are forbidden.
if variant_available "forced-en-passant"; then
out=$(run_cmds "setoption name UCI_Variant value forced-en-passant
position fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1
go perft 1")
echo "${out}" | grep -q "^e5d6: 1$"
echo "${out}" | grep -q "Nodes searched: 1"
fi

# 10) Eurasian: entering the optional promotion band without reserve is still legal.
if variant_available "eurasian"; then
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/10/P9/10/10/10/10/10/10/5K4 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a8a9: 1$"

# 11) Eurasian: entering the last rank without a reserve promotion is forbidden.
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/P9/10/10/10/10/10/10/10/5K4 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a9a10"

# 12) Eurasian: a matching reserve piece enables the last-rank promotion.
out=$(run_cmds "setoption name UCI_Variant value eurasian
position fen 4k5/P9/10/10/10/10/10/10/10/5K4[Q] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a9a10q: 1$"
fi

# 12b) Battery Chess: promotion requires a captured reserve piece and consumes it.
out=$(run_cmds "setoption name UCI_Variant value battery-chess
position fen 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^a7a8"
out=$(run_cmds "setoption name UCI_Variant value battery-chess
position fen 4k3/P7/8/8/8/8/8/4K3[Q] w - - 0 1 moves a7a8q
d")
echo "${out}" | grep -q "Fen: Q~3k3/8/8/8/8/8/8/4K3\\[\\] b - - 0 1"

# 12c) Beast Chess: documented replacement back rank loads.
out=$(run_cmds "setoption name UCI_Variant value beast-chess
position startpos
d")
echo "${out}" | grep -q "Fen: eghqkhge/pppppppp/8/8/8/8/PPPPPPPP/EGHQKHGE w KQkq - 0 1"

# 13) Fatal giveaway: non-pawn capturer dies, pawns survive captures.
if variant_available "fatal-giveaway"; then
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4p3/4R3/8/8/8 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4\\^3/8/8/8/8 b - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3P4/8/8/8/8 b - - 0 1"
fi

# 14) Kamikaze giveaway: any capturer is removed after capture.
if variant_available "kamikaze-giveaway"; then
out=$(run_cmds "setoption name UCI_Variant value kamikaze-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/8 b - - 0 1"
fi

# 15) Kamikaze: plain chess family, capturer is removed.
if variant_available "kamikaze"; then
out=$(run_cmds "setoption name UCI_Variant value kamikaze
position fen 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/4K3 b - - 0 1"

# 15b) Kamikaze (nocheckatomic template): kings are explosion-immune.
out=$(run_cmds "setoption name UCI_Variant value kamikaze
position fen r1bqkbnr/pppp1ppp/8/4pK2/4P3/8/PPPP1PPP/RNBQ2NR w KQkq - 0 3 moves f5e5
d")
echo "${out}" | grep -q "Fen: r1bqkbnr/pppp1ppp/8/4K3/4P3/8/PPPP1PPP/RNBQ2NR b kq - 0 3"
fi

# 16) Fatal giveaway: dead squares can be captured as neutral blockers.
if variant_available "fatal-giveaway"; then
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4\\^3/8/8/8/4Q3 w - - 0 1 moves e1e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4Q3/8/8/8/8 b - - 0 1"
fi

# 17) Progressive: after Black's first move, White must pass before Black's second move.
out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5
go perft 1")
echo "${out}" | grep -q "^0000: 1$"

# 18) British chess: the royal queen may not move through check.
if variant_available "british-chess"; then
out=$(run_cmds "setoption name UCI_Variant value british-chess
position fen 9q/10/10/10/10/10/10/10/1r8/4Q5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1e3: 1$"
echo "${out}" | grep -q "^e1f1: 1$"
fi

# 18b) Hippolyta: captures are stationary, mandatory, and pieces do not move.
if variant_available "hippolyta"; then
out=$(run_cmds "setoption name UCI_Variant value hippolyta
position startpos
go perft 1")
echo "${out}" | grep -q "^a1b2: 1$"
! echo "${out}" | grep -q "^a1a2:"
fi

# 19) iChess: setup starts with non-king drops only.
out=$(run_cmds "setoption name UCI_Variant value ichess
position startpos
go perft 1")
! echo "${out}" | grep -q "^K@"
echo "${out}" | grep -q "^Q@"
# 19b) Half Chess: 4x8 orthodox setup with no pawns.
out=$(run_cmds "setoption name UCI_Variant value half-chess
position startpos
d")
echo "${out}" | grep -q "Fen: rnbq/kbbq/4/4/4/4/KBBQ/RNBQ w - - 0 1"

# 19baa) Los Alamos Chess: 6x6 chess without bishops.
out=$(run_cmds "setoption name UCI_Variant value losalamos
position startpos
d")
echo "${out}" | grep -q "Fen: rnqknr/pppppp/6/6/PPPPPP/RNQKNR w - - 0 1"

# 19bab) Promotion Chess: pawn-only setup with kings in opposite corners.
out=$(run_cmds "setoption name UCI_Variant value promotion-chess
position startpos
d")
echo "${out}" | grep -q "Fen: 7k/pppppppp/8/pppppppp/PPPPPPPP/8/PPPPPPPP/K7 w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value promotion-chess
position fen 7k/P7/8/8/8/8/8/K7 w - - 0 1
go perft 1")
echo "${out}" | grep -q "a7a8q: 1"
echo "${out}" | grep -q "a7a8r: 1"
echo "${out}" | grep -q "a7a8b: 1"
echo "${out}" | grep -q "a7a8n: 1"

# 19bac) Reach Chess: reaching the back rank wins, and the current checkmated case is terminal.
out=$(run_cmds "setoption name UCI_Variant value reach-chess
position fen 4P3/8/8/8/8/8/8/4k3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
out=$(run_cmds "setoption name UCI_Variant value reach-chess
position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 19bad) All Queens Chess: source-backed setup and line-of-four win.
out=$(run_cmds "setoption name UCI_Variant value all-queens-chess
position startpos
d")
echo "${out}" | grep -Eq "Fen: qQqQq/5/Q3q/5/QqQqQ(\\[\\])? w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value all-queens-chess
position fen 5/QQQQ1/5/5/5 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 19bb) Compound Chess: setup and dragon-specific en passant capture.
if variant_available "compound-chess"; then
out=$(run_cmds "setoption name UCI_Variant value compound-chess
position startpos
d")
echo "${out}" | grep -q "Fen: rdcbqkbcdr/ssssssssss/10/10/10/10/SSSSSSSSSS/RDCBQKBCDR w KQkq - 0 1"
fi

# 19ba) Crown Prince Chess: crown prince cannot capture and wins by reaching the back rank.
out=$(run_cmds "setoption name UCI_Variant value crown-prince-chess
position fen 4k3/8/8/8/8/8/4p3/4C3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1e2"
out=$(run_cmds "setoption name UCI_Variant value crown-prince-chess
position fen 4C3/8/8/8/8/8/8/4k3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 19c) Dris at-Talata: setup by drops, then pieces move to any empty square.
out=$(run_cmds "setoption name UCI_Variant value dris-at-talata
position startpos
go perft 1")
echo "${out}" | grep -q "^M@a1: 1$"
out=$(run_cmds "setoption name UCI_Variant value dris-at-talata
position startpos moves M@a1 M@a3 M@b2 M@b1 M@c1 M@c3
go perft 1")
echo "${out}" | grep -q "^a1a2: 1$"
echo "${out}" | grep -q "^a1b3: 1$"

# 19ca) Tic-Tac-Chess: setup by drops, then chess-style motion plus hop-without-capture.
out=$(run_cmds "setoption name UCI_Variant value tictacchess
position startpos
go perft 1")
echo "${out}" | grep -q "^Q@a1: 1$"
out=$(run_cmds "setoption name UCI_Variant value tictacchess
position startpos moves Q@a1 Q@b2 R@a2 R@c2 K@b1 K@c1
go perft 1")
echo "${out}" | grep -q "^b1b3: 1$"
echo "${out}" | grep -q "^a2a3: 1$"

# 19d) Shatranj al-Jawarhiya: source-backed 7x8 setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value shatranj-al-jawarhiya
position startpos
d")
echo "${out}" | grep -q "Fen: rafkanr/ppppppp/7/7/7/7/PPPPPPP/RNAFKAR w - - 0 1"

# 19daa) Shatranj (14x14): source-backed setup should load if the build supports it.
if variant_available "shatranj-14x14"; then
out=$(run_cmds "setoption name UCI_Variant value shatranj-14x14
position startpos
d")
echo "${out}" | grep -q "Fen: rndwbmksbwdnr1/pppppppppppppp/14/14/14/14/14/14/14/14/14/14/PPPPPPPPPPPPPP/RNDWBSKMBWDNR1 w - - 0 1"
fi

# 19da) Shatranj: source-backed setup should load as documented.
if variant_available "shatranj"; then
out=$(run_cmds "setoption name UCI_Variant value shatranj
position startpos
d")
echo "${out}" | grep -q "Fen: rnafkanr/pppppppp/8/8/8/8/PPPPPPPP/RNAFKANR w - - 0 1"
fi

# 19db) Chaturanga: source-backed setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value chaturanga
position startpos
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w - - 0 1"

# 19dc) Chaturanga (Payagunda): source-backed setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value chaturanga-payagunda
position startpos
d")
echo "${out}" | grep -q "Fen: afrkbnfa/pppppppp/8/8/8/8/PPPPPPPP/AFRNBKFA w - - 0 1"

# 19dd) Chaturanga (al-Adli): source-backed setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value chaturanga-al-adli
position startpos
d")
echo "${out}" | grep -q "Fen: brnfknrb/pppppppp/8/8/8/8/PPPPPPPP/BRNFKNRB w - - 0 1"

# 19de) Shatranj (Iraq): source-backed setup and current elephant immunity behavior.
out=$(run_cmds "setoption name UCI_Variant value shatranj-iraq
position startpos
d")
echo "${out}" | grep -q "Fen: rnefkenr/pppppppp/8/8/8/8/PPPPPPPP/RNEFKENR w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value shatranj-iraq
position fen 8/8/8/3e4/4E3/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e4d5: 1$"
echo "${out}" | grep -q "^e4d3: 1$"
echo "${out}" | grep -q "^e4f5: 1$"
echo "${out}" | grep -q "^e4f3: 1$"

# 19de) Shatranj (Turkey): source-backed setup currently loads without first-move Fers leaps.
out=$(run_cmds "setoption name UCI_Variant value shatranj-turkey
position startpos
d")
echo "${out}" | grep -q "Fen: rnafkanr/pppppppp/8/8/8/8/PPPPPPPP/RNAFKANR w - - 0 1"

# 19df) Tsatsarandi: documented title currently maps to orthodox shatranj rules.
out=$(run_cmds "setoption name UCI_Variant value tsatsarandi
position startpos
d")
echo "${out}" | grep -q "Fen: rnbkqbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBKQBNR w - - 0 1"

# 19e) Chess (Siberia): source-backed 9x9 setup should load as documented.
if variant_available "chess-siberia"; then
out=$(run_cmds "setoption name UCI_Variant value chess-siberia
position startpos
d")
echo "${out}" | grep -q "Fen: rnbqk1bnr/ppppppppp/9/9/9/9/9/PPPPPPPPP/RNBQK1BNR w - - 0 1"
fi

# 19dzz) Fart (5x5): two placements per turn, centre excluded, then orthogonal sliding with line-5 win.
out=$(run_cmds "setoption name UCI_Variant value fart-5x5
position startpos
go perft 1")
echo "${out}" | grep -q "^M@a1: 1$"
echo "${out}" | grep -vq "^M@c3: 1$"

# 19e0) English Draughts: documented title matches the supported checkers ruleset.
out=$(run_cmds "setoption name UCI_Variant value english-draughts
position startpos
d")
echo "${out}" | grep -q "Fen: 1m1m1m1m/m1m1m1m1/1m1m1m1m/8/8/M1M1M1M1/1M1M1M1M/M1M1M1M1 w - - 0 1"

# 19ea) HP-minichess: 5x5 orthodox setup with kings on the a-file.
out=$(run_cmds "setoption name UCI_Variant value hp-minichess
position startpos
d")
echo "${out}" | grep -q "Fen: kqbnr/ppppp/5/PPPPP/KQBNR w - - 0 1"

# 19eaa) Jeson Mor: center reach by a knight is an immediate win.
if variant_available "jesonmor"; then
out=$(run_cmds "setoption name UCI_Variant value jesonmor
position startpos
d")
echo "${out}" | grep -q "Fen: nnnnnnnnn/9/9/9/9/9/9/9/NNNNNNNNN w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value jesonmor
position fen 9/9/9/9/4N4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 19eb) Dodgem: classic 3x3 rules on an internal 5x4 board with escape lanes.
out=$(run_cmds "setoption name UCI_Variant value dodgem
position startpos
go perft 1")
echo "${out}" | grep -q "^a2a1: 1$"
echo "${out}" | grep -q "^a2b2: 1$"
echo "${out}" | grep -q "^a3b3: 1$"
out=$(run_cmds "setoption name UCI_Variant value dodgem
position fen 4k/5/2X2/4K w - - 0 1
go perft 1")
echo "${out}" | grep -q "^c2d2: 1$"
out=$(run_cmds "setoption name UCI_Variant value dodgem
position fen 4k/5/3U1/3UK b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 19fzzzz) Gale 15x15: only available on VERY_LARGE_BOARDS builds.
if variant_available "gale-15"; then
  out=$(run_cmds "setoption name UCI_Variant value gale-15
position startpos
go perft 1")
  echo "${out}" | grep -q "Nodes searched: 113"
  echo "${out}" | grep -q "^P@a1: 1$"
fi

# 19g) Apit-Sodok: same reverse/intervention capture as Maak Yek.
out=$(run_cmds "setoption name UCI_Variant value apit-sodok
position fen 8/8/8/2r1r3/3R4/8/8/8 w - - 0 1 moves d4d5
d")
echo "${out}" | grep -q "Fen: 8/8/8/3R4/8/8/8/8 b - - 1 1"

# 19h) Apit: canonical title for the same documented rules family.
out=$(run_cmds "setoption name UCI_Variant value apit
position fen 8/8/8/2r1r3/3R4/8/8/8 w - - 0 1 moves d4d5
d")
echo "${out}" | grep -q "Fen: 8/8/8/3R4/8/8/8/8 b - - 1 1"

# 21) Maak Yek: moving between two enemy pieces captures both of them.
out=$(run_cmds "setoption name UCI_Variant value maak-yek
position fen 8/8/8/8/2r1r3/8/8/3R4 w - - 0 1 moves d1d4
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/3R4/8/8/8 b - - 1 1"

# 22) Troll: drops flip enclosed enemy stones but paths connect only orthogonally.
out=$(run_cmds "setoption name UCI_Variant value troll
position fen 8/8/8/8/8/8/3p4/3P4[PPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPpppppppppppppppppppppppppppppppp] w - - 0 1 moves P@d3
d")
echo "${out}" | grep -q "Fen: 8/8/8/8/8/3P4/3P4/3P4 b - - 0 1"

# 23) Tic-Tac-Toe misere: completing your own line loses immediately.
out=$(run_cmds "setoption name UCI_Variant value tictactoe-misere
position fen PPP/3/3[pppp] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5 e1e1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 29"

# 17b) Progressive: forced pass plies must not increment halfmove clock.
out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5 0000
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2"

# 18) Hindustani baseline: no pawn double-step.
if variant_available "hindustani"; then
out=$(run_cmds "setoption name UCI_Variant value hindustani
position startpos
go perft 1")
! echo "${out}" | grep -q "^e2e4:"
echo "${out}" | grep -q "^e1d3: 1$"
echo "${out}" | grep -q "^e1f3: 1$"

# 19) Hindustani baseline: if all promotion targets are at cap, promotion is forbidden.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position fen 4k3/3P4/8/8/8/8/RNBQKBNR/R6R w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^d7d8"

# 20) Hindustani baseline: if queen is below cap, central-file promotion to queen is legal.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position fen 4k3/3P4/8/8/8/8/RNB1KBNR/R6R w - - 0 1
go perft 1")
echo "${out}" | grep -q "^d7d8q: 1$"
! echo "${out}" | grep -q "^d7d8r:"
fi

# 21) Gala: opening drop phase starts from pockets on own half (white has 10 choices).
out=$(run_cmds "setoption name UCI_Variant value gala
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 10"

# 22) Gala: custodial capture by orthogonal sandwich.
out=$(run_cmds "setoption name UCI_Variant value gala
position fen 5/5/1D1dD/5/5 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: 5/5/2D1D/5/5(\\[\\])? b - - 1 1"

# 23) iChess baseline: opening drops restricted to own half.
out=$(run_cmds "setoption name UCI_Variant value ichess
position startpos
go perft 1")
! echo "${out}" | grep -q "@a8"
echo "${out}" | grep -q "^Q@a1: 1$"

# 24) iChess baseline: pawn is shogi-like (one-step forward only, no initial double-step).
out=$(run_cmds "setoption name UCI_Variant value ichess
position fen 4k3/8/8/8/8/8/4P3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2e3: 1$"
! echo "${out}" | grep -q "^e2e4:"

# 25) iChess baseline: promotion rank is constrained (no unpromoted advance to rank 8).
out=$(run_cmds "setoption name UCI_Variant value ichess
position fen k7/4P3/8/8/8/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e7e8"

# 26) Fianchetto chess: bishops and rooks are swapped in initial setup, castling disabled.
out=$(run_cmds "setoption name UCI_Variant value fianchetto
position startpos
d")
echo "${out}" | grep -q "Fen: bnrqkrnb/pppppppp/8/8/8/8/PPPPPPPP/BNRQKRNB w - - 0 1"
! echo "${out}" | grep -q " KQkq "

# 27) Asymmetrical chess baseline: custom start position, no castling, no pawn double-step.
out=$(run_cmds "setoption name UCI_Variant value asymmetrical
position startpos
d")
echo "${out}" | grep -q "Fen: 3prnbk/4ppqn/5ppb/P5pr/RP5p/BPP5/NQPP4/KBNRP3 w - - 0 1"
! echo "${out}" | grep -q " KQkq "
out=$(run_cmds "setoption name UCI_Variant value asymmetrical
position startpos
go perft 1")
! echo "${out}" | grep -q "d1d3"

# 28) Brotherhood baseline: same-type captures are forbidden.
out=$(run_cmds "setoption name UCI_Variant value brotherhood
position fen 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e4d5:"
echo "${out}" | grep -q "^e4e5: 1$"

# 29) Marseillais baseline: each side gets two moves per turn (with pass separator).
out=$(run_cmds "setoption name UCI_Variant value marseillais
position startpos moves e2e4
go perft 1")
echo "${out}" | grep -q "^0000: 1$"
! echo "${out}" | grep -q "^e7e5:"
out=$(run_cmds "setoption name UCI_Variant value marseillais
position startpos moves e2e4 0000
go perft 1")
echo "${out}" | grep -q "^e4e5: 1$"
! echo "${out}" | grep -q "^e7e5:"

# 30) Antimatter baseline: same-type captures annihilate the capturer.
out=$(run_cmds "setoption name UCI_Variant value antimatter
position fen 4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/4K3 b - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value antimatter
position fen 4k3/8/8/3n4/4P3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3P4/8/8/8/4K3 b - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value antimatter
position startpos moves g2g3
go movetime 50")
echo "${out}" | grep -q "^bestmove "
out=$(run_cmds "setoption name UCI_Variant value antimatter
position fen 4k3/8/8/3p4/4P3/8/8/3K4 b - - 0 1
go depth 1")
echo "${out}" | grep -q "^bestmove "

# 30b) Benedict (capture morph): capturer adopts captured piece type.
out=$(run_cmds "setoption name UCI_Variant value benedict
position fen 4k3/8/8/3n4/4B3/8/8/4K3 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3N4/8/8/8/4K3 b - - 0 1"

# 31) Pawns baseline: pawn-only start and promotion race objective.
out=$(run_cmds "setoption name UCI_Variant value pawns
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 8"
out=$(run_cmds "setoption name UCI_Variant value pawns
position fen 8/P7/8/8/8/8/8/8 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^a7a8p: 1$"

# 32) Rugby baseline: king-like pawn movement without orthogonal captures.
out=$(run_cmds "setoption name UCI_Variant value rugby
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 22"
out=$(run_cmds "setoption name UCI_Variant value rugby
position fen 8/8/8/8/8/4P3/4p3/8 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e3e2:"
echo "${out}" | grep -q "^e3d2: 1$"

# 33) Capped pawns: extra two-step window from rank 6/3 into promotion.
out=$(run_cmds "setoption name UCI_Variant value capped-pawns
position fen k7/8/4P3/8/8/8/8/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e6e8q: 1$"
out=$(run_cmds "setoption name UCI_Variant value capped-pawns
position fen k7/8/8/8/8/4p3/8/7K b - - 0 1
go perft 1")
echo "${out}" | grep -q "^e3e1q: 1$"

# 34) No-castle-10: castling blocked before ply 20, allowed afterwards.
out=$(run_cmds "setoption name UCI_Variant value no-castle-10
position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
! echo "${out}" | grep -q "^e1c1:"
out=$(run_cmds "setoption name UCI_Variant value no-castle-10
position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 11
go perft 1")
echo "${out}" | grep -q "^e1g1: 1$"
echo "${out}" | grep -q "^e1c1: 1$"

# 35) Dueling archbishops baseline: bishops gain knight movement.
out=$(run_cmds "setoption name UCI_Variant value dueling-archbishops
position startpos
go perft 1")
echo "${out}" | grep -q "^c1b3: 1$"
echo "${out}" | grep -q "^f1g3: 1$"

# 36) Dueling archbishops baseline: hand pieces can be dropped.
out=$(run_cmds "setoption name UCI_Variant value dueling-archbishops
position fen 4k3/8/8/8/8/8/8/4K3[P] w - - 0 1
go perft 1")
echo "${out}" | grep -q "^P@a2: 1$"

# 37) Royal race baseline: expected opening move count with custom movers.
if variant_available "royal-race"; then
out=$(run_cmds "setoption name UCI_Variant value royal-race
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 36"

# 38) Royal race baseline: king on goal rank is an immediate game end.
out=$(run_cmds "setoption name UCI_Variant value royal-race
position fen 3K3/7/7/7/7/7/7/7/3k3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
fi

# 39) Spell chess: frozen castling rook blocks castling.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4k3/8/8/8/8/8/8/4K2R[f] b K - 0 1 moves f@h1 e8e7
go perft 1")
! echo "${out}" | grep -q "^e1g1:"

# 40) Spell chess: castling through attack is illegal, but legal if attacker is frozen first.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4kr2/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@f8,e1g1: 1$"

# 41) Spell chess: castling out of check is illegal, but legal if attacker is frozen first.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4r1k1/8/8/8/8/8/8/4K2R[F] w K - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1g1:"
echo "${out}" | grep -q "^f@e8,e1g1: 1$"

# 42) Spell chess: jump potion does not let castling pass through occupied blockers.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 6k1/8/8/8/8/8/8/R2nK3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@d1,e1c1:"

out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 6k1/8/8/8/8/8/8/Rn2K3[J] w Q - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1c1:"
! echo "${out}" | grep -q "^j@b1,e1c1:"

# 43) Spell chess: frozen pawn cannot capture en passant.
out=$(run_cmds "setoption name UCI_Variant value spell-chess
position fen 4k3/3p4/8/4P3/8/8/8/4K3[f] b - - 0 1 moves f@e5 d7d5
go perft 1")
! echo "${out}" | grep -q "^e5d6:"

# 44) Monad baseline (large-board): custom 10x10 setup is loaded.
if variant_available "monad"; then
  out=$(run_cmds "setoption name UCI_Variant value monad
position startpos
go perft 1")
  echo "${out}" | grep -q "Nodes searched: 37"
fi

# 43) Camel-rhino baseline (large-board): setup loads and generates legal moves.
if variant_available "camel-rhino"; then
  out=$(run_cmds "setoption name UCI_Variant value camel-rhino
position startpos
go perft 1")
  echo "${out}" | grep -q "Nodes searched: 68"
fi

# 44) Rifle chess baseline: start position behaves like orthodox chess before captures appear.
out=$(run_cmds "setoption name UCI_Variant value rifle-chess
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 20"

echo "new variants smoke testing OK"
