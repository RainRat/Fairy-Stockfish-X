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
  cat <<EOF | "${ENGINE}"
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
pieceSpecificPromotionRegion = true
whitePiecePromotionRegion = P(a8);P(h8);
blackPiecePromotionRegion = P(a1);P(h1);
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
# On constrained builds, skip gracefully if any required variant is unavailable.
for required in hasami eurasian hindustani gala ichess british-chess crown-prince-chess compound-chess half-chess losalamos promotion-chess reach-chess dris-at-talata tictacchess shatranj shatranj-al-jawarhiya chaturanga chaturanga-payagunda chaturanga-al-adli chess-siberia konane tawlbwrdd kharebga maak-yek apit-sodok apit troll tictactoe-misere all-queens-chess; do
  if ! variant_available "${required}"; then
    echo "new variants smoke skipped: required variant '${required}' is unavailable in this build"
    exit 0
  fi
done

# 1) Hasami: orthogonal sandwich should capture the middle piece.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/R1rR5/9 w - - 0 1 moves a2b2
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1R1R5/9 b - - 1 1"

# 2) Hasami: edge alone is not hostile; moving away must not capture.
out=$(run_cmds "setoption name UCI_Variant value hasami
position fen 9/9/9/9/9/9/9/Rr7/9 w - - 0 1 moves a2a1
d")
echo "${out}" | grep -q "Fen: 9/9/9/9/9/9/9/1r7/R8 b - - 1 1"

# 3) Achi: pre-connected line is immediate game end (no legal moves).
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PPP/3/3[PPPPpppp] b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 4) Achi: non-terminal filled setup still yields legal drops.
out=$(run_cmds "setoption name UCI_Variant value achi
position fen PpP/pPp/3[PpppPPPP] w - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 3"

# 5) Checkless: king capture is legal (checks are disabled by variant).
out=$(run_cmds "setoption name UCI_Variant value checkless
position fen 4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1
go perft 1")
echo "${out}" | grep -q "^e2e8: 1$"

# 6) Tablut split: edge-escape should end immediately, corner-escape should not.
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-corner-escape
position fen 4K4/9/9/9/4r4/9/9/9/9 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 15"

# 7b) Tawlbwrdd: edge escape should end immediately on 11x11, unlike corner-escape Hnefatafl.
out=$(run_cmds "setoption name UCI_Variant value tawlbwrdd
position fen 5K5/11/11/11/11/5r5/11/11/11/11/11 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 7) Tablut split: throne-adjacent king strength changes capture outcome.
out=$(run_cmds "setoption name UCI_Variant value tablut
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

out=$(run_cmds "setoption name UCI_Variant value tablut-throne-adjacent-strong
position fen 9/9/9/9/3K5/2r6/9/9/9 b - - 0 1 moves c4c5
go perft 1")
echo "${out}" | grep -q "Nodes searched: 12"

# 8) Neutreeko: max-distance move completes a line and ends the game.
out=$(run_cmds "setoption name UCI_Variant value neutreeko
position fen 5/3N1/5/1N3/N4 w - - 0 1 moves d4c3
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 9) Forced en passant: when EP is legal, ordinary king moves are forbidden.
out=$(run_cmds "setoption name UCI_Variant value forced-en-passant
position fen 4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1
go perft 1")
echo "${out}" | grep -q "^e5d6: 1$"
echo "${out}" | grep -q "Nodes searched: 1"

# 10) Eurasian: entering the optional promotion band without reserve is still legal.
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

# 13) Fatal giveaway: non-pawn capturer dies, pawns survive captures.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4p3/4R3/8/8/8 w - - 0 1 moves e4e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4\\^3/8/8/8/8 b - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/3P4/8/8/8/8 b - - 0 1"

# 14) Kamikaze giveaway: any capturer is removed after capture.
out=$(run_cmds "setoption name UCI_Variant value kamikaze-giveaway
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/8 b - - 0 1"

# 15) Kamikaze: plain chess family, capturer is removed.
out=$(run_cmds "setoption name UCI_Variant value kamikaze
position fen 4k3/8/8/3p4/4P3/8/8/8 w - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/8/8/8/8/8 b - - 0 1"

# 15b) Kamikaze: kings are exempt from self-destruction on capture.
out=$(run_cmds "setoption name UCI_Variant value kamikaze
position fen 8/8/8/3P4/4k3/8/8/4K3 b - - 0 1 moves e4d5
d")
echo "${out}" | grep -q "Fen: 8/8/8/3k4/8/8/8/4K3 w - - 0 2"

# 16) Fatal giveaway: dead squares can be captured as neutral blockers.
out=$(run_cmds "setoption name UCI_Variant value fatal-giveaway
position fen 4k3/8/8/4\\^3/8/8/8/4Q3 w - - 0 1 moves e1e5
d")
echo "${out}" | grep -q "Fen: 4k3/8/8/4Q3/8/8/8/8 b - - 0 1"

# 17) Progressive: after Black's first move, White must pass before Black's second move.
out=$(run_cmds "setoption name UCI_Variant value progressive
position startpos moves e2e4 e7e5
go perft 1")
echo "${out}" | grep -q "^e1e1: 1$"

# 18) British chess: the royal queen may not move through check.
out=$(run_cmds "setoption name UCI_Variant value british-chess
position fen 9q/10/10/10/10/10/10/10/1r8/4Q5 w - - 0 1
go perft 1")
! echo "${out}" | grep -q "^e1e3: 1$"
echo "${out}" | grep -q "^e1f1: 1$"

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

# 19bac) Reach Chess: reaching the back rank wins, and checkmate only forces a pass.
out=$(run_cmds "setoption name UCI_Variant value reach-chess
position fen 4P3/8/8/8/8/8/8/4k3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"
out=$(run_cmds "setoption name UCI_Variant value reach-chess
position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1
go perft 1")
echo "${out}" | grep -q "^h8h8: 1$"

# 19bad) All Queens Chess: source-backed setup and line-of-four win.
out=$(run_cmds "setoption name UCI_Variant value all-queens-chess
position startpos
d")
echo "${out}" | grep -q "Fen: qQqQq/5/Q3q/5/QqQqQ w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value all-queens-chess
position fen 5/QQQQ1/5/5/5 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

# 19bb) Compound Chess: setup and dragon-specific en passant capture.
out=$(run_cmds "setoption name UCI_Variant value compound-chess
position startpos
d")
echo "${out}" | grep -q "Fen: rdcbqkbcdr/ssssssssss/10/10/10/10/SSSSSSSSSS/RDCBQKBCDR w KQkq - 0 1"
out=$(run_cmds "setoption name UCI_Variant value compound-chess
position fen 10/10/10/3sD5/10/10/10/5k4 w - d6 0 1
go perft 1")
echo "${out}" | grep -q "^e5d6: 1$"

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
echo "${out}" | grep -q "Fen: rndwbmkqsbwdnr/pppppppppppppp/14/14/14/14/14/14/14/14/14/14/PPPPPPPPPPPPPP/RNDWBSQKMBWDNR w - - 0 1"
fi

# 19da) Shatranj: source-backed setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value shatranj
position startpos
d")
echo "${out}" | grep -q "Fen: rnafkanr/pppppppp/8/8/8/8/PPPPPPPP/RNAFKANR w - - 0 1"

# 19db) Chaturanga: source-backed setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value chaturanga
position startpos
d")
echo "${out}" | grep -q "Fen: rnbkfbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBFKBNR w - - 0 1"

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

# 19de) Shatranj (Turkey): source-backed setup and first-move Fers leap.
out=$(run_cmds "setoption name UCI_Variant value shatranj-turkey
position startpos
d")
echo "${out}" | grep -q "Fen: rnafkanr/pppppppp/8/8/8/8/PPPPPPPP/RNAFKANR w - - 0 1"
out=$(run_cmds "setoption name UCI_Variant value shatranj-turkey
position startpos
go perft 1")
echo "${out}" | grep -q "^d1d3: 1$"
echo "${out}" | grep -q "^d1b3: 1$"
echo "${out}" | grep -q "^d1f3: 1$"

# 19e) Chess (Siberia): source-backed 9x9 setup should load as documented.
out=$(run_cmds "setoption name UCI_Variant value chess-siberia
position startpos
d")
echo "${out}" | grep -q "Fen: rnbqk1bnr/ppppppppp/9/9/9/9/9/PPPPPPPPP/RNBQK1BNR w - - 0 1"

# 19f) Konane: opening self-removals and orthogonal jump capture sequence.
out=$(run_cmds "setoption name UCI_Variant value konane
position startpos
go perft 1")
echo "${out}" | grep -q "^a10a10: 1$"
echo "${out}" | grep -q "^e6e6: 1$"
out=$(run_cmds "setoption name UCI_Variant value konane
position startpos moves a10a10
go perft 1")
echo "${out}" | grep -q "^a9a9: 1$"
echo "${out}" | grep -q "^b10b10: 1$"
out=$(run_cmds "setoption name UCI_Variant value konane
position startpos moves a10a10 a9a9
go perft 1")
echo "${out}" | grep -q "^c10a10: 1$"

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


# 20) Kharebga: setup is two drops per turn with the centre excluded.
out=$(run_cmds "setoption name UCI_Variant value kharebga
position startpos moves R@a1
go perft 1")
echo "${out}" | grep -q "^a1a1: 1$"
out=$(run_cmds "setoption name UCI_Variant value kharebga
position startpos moves R@a1 a1a1
go perft 1")
! echo "${out}" | grep -q "^R@c3"
echo "${out}" | grep -q "^R@b1: 1$"

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
echo "${out}" | grep -q "Fen: rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2"

# 18) Hindustani baseline: no pawn double-step.
out=$(run_cmds "setoption name UCI_Variant value hindustani
position startpos
go perft 1")
! echo "${out}" | grep -q "^e2e4:"

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
echo "${out}" | grep -q "Fen: 3prnbk/4ppqn/5ppb/6pr/7p/Bpp5/NQpp4/KBNRP3 w - - 0 1"
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
echo "${out}" | grep -q "^e8e8: 1$"
! echo "${out}" | grep -q "^e7e5:"
out=$(run_cmds "setoption name UCI_Variant value marseillais
position startpos moves e2e4 e8e8
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
out=$(run_cmds "setoption name UCI_Variant value royal-race
position startpos
go perft 1")
echo "${out}" | grep -q "Nodes searched: 29"

# 38) Royal race baseline: king on goal rank is an immediate game end.
out=$(run_cmds "setoption name UCI_Variant value royal-race
position fen 3K3/7/7/7/7/7/7/7/3k3 b - - 0 1
go perft 1")
echo "${out}" | grep -q "Nodes searched: 0"

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
  echo "${out}" | grep -q "Nodes searched: 27"
fi

# 43) Camel-rhino baseline (large-board): setup loads and generates legal moves.
if variant_available "camel-rhino"; then
  out=$(run_cmds "setoption name UCI_Variant value camel-rhino
position startpos
go perft 1")
  echo "${out}" | grep -q "Nodes searched: 68"
fi

# 44) Seega baseline: opening setup excludes the center square.
out=$(run_cmds "setoption name UCI_Variant value seega
position startpos
go perft 1")
! echo "${out}" | grep -q "^D@c3:"

# 45) Seega baseline: custodial capture removes the sandwiched piece.
out=$(run_cmds "setoption name UCI_Variant value seega
position fen 5/5/1D1dD/5/5 w - - 0 1 moves b3c3
d")
echo "${out}" | grep -Eq "Fen: 5/5/2D1D/5/5(\\[\\])? b - - 1 1"

# 46) Ko-app-paw-na baseline: hunter can hop-capture over one adjacent rabbit.
out=$(run_cmds "setoption name UCI_Variant value ko-app-paw-na
position fen 5/2R2/2h2/5/5 b - - 0 1 moves c3c5
d")
echo "${out}" | grep -q "Fen: 2h2/5/5/5/5 w - - 0 2"

# 47) Null baseline: move strings include mandatory wall placement on vacated square.
out=$(run_cmds "setoption name UCI_Variant value null
position startpos
go perft 1")
echo "${out}" | grep -q "^e2e4,e4e2: 1$"

# 48) Null baseline: after move+wall, vacated square is petrified.
out=$(run_cmds "setoption name UCI_Variant value null
position startpos moves e2e4,e4e2
d")
echo "${out}" | grep -q "Fen: rnbqkbnr/pppppppp/8/8/4P3/8/PPPP\\*PPP/RNBQKBNR b KQkq - 0 1"

echo "new variants smoke testing OK"
