#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "quiet-check-special-moves regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/../../../.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
ENGINE=${1:-./stockfish}
ENGINE_BASENAME=$(basename "${ENGINE}")
source "${SCRIPT_DIR}/../../../../tests/lib/harness-build.sh"
fsx_harness_init "${ENGINE}" "${ROOT_DIR}"
CXX_DEFS=("${FSX_HARNESS_CXX_DEFS[@]}")

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/quiet-check-special-moves"
HARNESS_CPP="${BUILD_SIG_DIR}/quiet-check-special-moves.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/quiet-check-special-moves.bin"
HARNESS_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.harness.sig"
mkdir -p "${BUILD_SIG_DIR}"
fsx_harness_prepare_objects "${JOBS}"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <cstring>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "movepick.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "thread.h"
#include "uci.h"
#include "variant.h"
#include "test_engine_init.hpp"

using namespace Stockfish;

#if defined(__GNUC__)
#define FSX_NOINLINE __attribute__((noinline))
#else
#define FSX_NOINLINE
#endif

static void load_variants() {
    std::istringstream ss(R"ini(
[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r

[pairdrop-nocheck:fairy]
pieceDrops = true
symmetricDropTypes = r
checking = false
allowChecks = false

[pull-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = K...A...R...k...b...r...
king = k
customPiece1 = a:mW
customPiece2 = b:mW
customPiece3 = c:mW
pullingStrength = a:3 b:1 c:3
startFen = 5/5/5/5/5 w - - 0 1
[swap-basic:fairy]
maxFile = e
maxRank = 5
castling = false
checking = false
king = -
pieceToCharTable = -
customPiece1 = a:mW
customPiece2 = b:mW
adjacentSwapMoveTypes = a
adjacentSwapRequiresEmptyNeighbor = true
swapNoImmediateReturn = true
startFen = 5/5/5/5/5 w - - 0 1

[pass-quiet-check:fairy]
pass = true
castling = false
startFen = 8/8/8/8/8/8/8/R3K2k w - - 0 1

[wrapped-quiet-check:chess]
cylindrical = true
castling = false
checking = false
startFen = 2k5/8/8/8/8/7p/P7/4K3 w - - 0 1

[promotion-quiet-check:fairy]
castling = false
startFen = 7k/6P1/8/8/8/8/8/7K w - - 0 1

[clone-pseudoroyal-check:chess]
king = -
pseudoRoyalTypes = q
cloneMoveTypes = r
castling = false
startFen = 4q3/8/8/8/8/8/8/R7 w - - 0 1

[firstmove-pseudoroyal-check:chess]
king = -
pseudoRoyalTypes = q
pseudoRoyalCount = 1
gating = true
firstMovePieceTypes = b:n
firstMoveLoseOnCheck = true
castling = false
startFen = 3bq3/8/8/8/8/8/8/R7 w d - 0 1
)ini");
    variants.parse_istream<false>(ss);
}

static FSX_NOINLINE void test_swap_basic() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("swap-basic"), "5/5/2Ab1/5/5 w - - 0 1", false, &st, nullptr);
    const Move m = make<SWAP>(SQ_C3, SQ_D3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_pairdrop() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pairdrop"), "4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1", false, &st, nullptr);
    const Move m = make_drop_pair(SQ_A4, SQ_H4, ROOK, ROOK);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_probe_nodes() {
    Thread thread(0);
    thread.nodes.store(0);

    StateInfo st{};
    Position pos;
    pos.set(variants.get("pairdrop-nocheck"), "4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1", false, &st, &thread);
    const Move m = make_drop_pair(SQ_A1, SQ_B1, ROOK, ROOK);

    const auto before = thread.nodes.load();
    assert(pos.legal(m));
    assert(!pos.gives_check(m));
    assert(!pos.gives_check(m));
    assert(thread.nodes.load() == before);
    assert(MoveList<LEGAL>(pos).contains(m));
    assert(thread.nodes.load() == before);
}

static FSX_NOINLINE void test_pull_basic() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pull-basic"), "5/5/2b2/2A2/5 w - - 0 1", false, &st, nullptr);
    const Move m = make_pull(SQ_C2, SQ_D2, SQ_C3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_pass_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("pass-quiet-check"), "8/8/8/8/8/8/8/R3K2k w - - 0 1", false, &st, nullptr);
    const Move m = make<SPECIAL>(SQ_A1, SQ_A1);
    assert(pos.pass(WHITE));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_wrapped_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("wrapped-quiet-check"), "2k5/8/8/8/8/7p/P7/4K3 w - - 0 1", false, &st, nullptr);
    const Move m = make<NORMAL>(SQ_A2, SQ_A3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_promotion_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("promotion-quiet-check"), "7k/6P1/8/8/8/8/8/7K w - - 0 1", false, &st, nullptr);
    const auto quietChecks = MoveList<QUIET_CHECKS>(pos);
    assert(pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, QUEEN)));
    assert(pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, ROOK)));
    assert(!pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, BISHOP)));
    assert(!pos.gives_check(make<PROMOTION>(SQ_G7, SQ_G8, KNIGHT)));
    assert(quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, QUEEN)));
    assert(quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, ROOK)));
    assert(!quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, BISHOP)));
    assert(!quietChecks.contains(make<PROMOTION>(SQ_G7, SQ_G8, KNIGHT)));
}

static FSX_NOINLINE void test_clone_pseudoroyal_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("clone-pseudoroyal-check"),
            "4q3/8/8/8/8/8/8/R7 w - - 0 1", false, &st, nullptr);
    const Move m = make<SPECIAL>(SQ_A1, SQ_E1);
    assert(pos.is_clone_move(m));
    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.piece_on(SQ_E1) != NO_PIECE);
    assert(pos.attackers_to(SQ_E8, pos.pieces(), WHITE));
    assert(pos.checked_pseudo_royals(BLACK));
    pos.undo_move(m);
    assert(pos.gives_check(m));
    assert(MoveList<QUIET_CHECKS>(pos).contains(m));
}

static FSX_NOINLINE void test_pseudoroyal_state_rebuild() {
    StateInfo initialState{};
    Position initial;
    initial.set(variants.get("clone-pseudoroyal-check"),
                "4q3/8/8/8/8/8/8/4R3 b - - 0 1", false, &initialState, nullptr);
    assert(initial.checkers() & SQ_E8);

    StateInfo st{};
    Position pos;
    pos.set(variants.get("clone-pseudoroyal-check"),
            "4q3/8/8/8/8/8/8/4R3 w - - 0 1", false, &st, nullptr);
    assert(!pos.checkers());

    StateInfo nullState{};
    pos.do_null_move(nullState);
    assert(pos.checkers() & SQ_E8);
    pos.undo_null_move();
}

static FSX_NOINLINE void test_firstmove_pseudoroyal_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("firstmove-pseudoroyal-check"),
            "3bq3/8/8/8/8/8/8/R7 w d - 0 1", false, &st, nullptr);
    assert(pos.gates(BLACK) & SQ_D8);

    StateInfo next{};
    pos.do_move(make<NORMAL>(SQ_A1, SQ_E1), next);
    assert(pos.checkers());
    assert(!(pos.gates(BLACK) & SQ_D8));
    pos.undo_move(make<NORMAL>(SQ_A1, SQ_E1));
}

static FSX_NOINLINE void test_qsearch_rejects_quiet_tt_move() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"),
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            false, &st, nullptr);

    static ButterflyHistory mainHistory{};
    static GateHistory gateHistory{};
    static CapturePieceToHistory captureHistory{};
    static PieceToHistory histories[6]{};
    const PieceToHistory* continuationHistory[] = {
        &histories[0], &histories[1], &histories[2],
        &histories[3], &histories[4], &histories[5]
    };

    MovePicker picker(pos, make<NORMAL>(SQ_E2, SQ_E4), DEPTH_QS_NO_CHECKS,
                      &mainHistory, &gateHistory, &captureHistory,
                      continuationHistory, SQ_NONE);
    assert(picker.next_move() == MOVE_NONE);
}

static FSX_NOINLINE void test_probcut_accepts_quiet_promotion_tt_move() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("promotion-quiet-check"),
            "7k/6P1/8/8/8/8/8/7K w - - 0 1", false, &st, nullptr);

    static GateHistory gateHistory{};
    static CapturePieceToHistory captureHistory{};
    const Move promotion = make<PROMOTION>(SQ_G7, SQ_G8, QUEEN);
    MovePicker picker(pos, promotion, Value(100), &gateHistory, &captureHistory);
    assert(picker.next_move() == promotion);
}

static FSX_NOINLINE void test_gate_history_square_validation() {
    const Move valid = make_gating<NORMAL>(SQ_E1, SQ_E2, ROOK, SQ_D1);
    assert(gate_history_square(valid) == SQ_D1);

    const Move ordinary = make<NORMAL>(SQ_E2, SQ_E4);
    assert(!is_gating(ordinary));
    assert(gate_history_square(ordinary) == SQ_NONE);
}

static FSX_NOINLINE void test_non_laser_quiet_checks_are_checks() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"),
            "4k3/8/8/8/8/8/8/R3K3 w - - 0 1", false, &st, nullptr);

    for (const auto& em : MoveList<QUIET_CHECKS>(pos))
        assert(pos.gives_check(em));
}

static FSX_NOINLINE void test_discovered_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"),
            "4k3/8/8/8/8/8/4B3/K3R3 w - - 0 1", false, &st, nullptr);

    const Move discovered = make<NORMAL>(SQ_E2, SQ_D3);
    assert(pos.gives_check(discovered));
    assert(MoveList<QUIET_CHECKS>(pos).contains(discovered));
}

int main(int argc, char** argv) {
    init_test_engine();
    load_variants();

    auto run_case = [&](const char* which) {
        if (!which || !std::strcmp(which, "non-laser"))
            test_non_laser_quiet_checks_are_checks();
        if (!which || !std::strcmp(which, "discovered"))
            test_discovered_quiet_check();
        if (!which || !std::strcmp(which, "pairdrop"))
            test_pairdrop();
        if (!which || !std::strcmp(which, "probe-nodes"))
            test_probe_nodes();
        if (!which || !std::strcmp(which, "swap"))
            test_swap_basic();
        if (!which || !std::strcmp(which, "pull"))
            test_pull_basic();
        if (!which || !std::strcmp(which, "pass"))
            test_pass_quiet_check();
        if (!which || !std::strcmp(which, "wrapped"))
            test_wrapped_quiet_check();
        if (!which || !std::strcmp(which, "promotion"))
            test_promotion_quiet_check();
        if (!which || !std::strcmp(which, "clone-pseudoroyal"))
            test_clone_pseudoroyal_check();
        if (!which || !std::strcmp(which, "pseudoroyal-state"))
            test_pseudoroyal_state_rebuild();
        if (!which || !std::strcmp(which, "firstmove-pseudoroyal"))
            test_firstmove_pseudoroyal_check();
        if (!which || !std::strcmp(which, "qsearch-tt"))
            test_qsearch_rejects_quiet_tt_move();
        if (!which || !std::strcmp(which, "probcut-promotion"))
            test_probcut_accepts_quiet_promotion_tt_move();
        if (!which || !std::strcmp(which, "gate-history-square"))
            test_gate_history_square_validation();
    };

    if (argc > 1)
        run_case(argv[1]);
    else
        run_case(nullptr);

    return 0;
}
EOF

fsx_harness_collect_objects
fsx_harness_build "${HARNESS_CPP}" "${HARNESS_BIN}" \
  "quiet-check-special-moves" "${HARNESS_SIG_FILE}"

run_case() {
  local which="$1"
  "${HARNESS_BIN}" "${which}"
}

run_raw_cmds() {
  local ini="$1"
  local variant="$2"
  local cmds="$3"
  printf 'uci\nsetoption name VariantPath value %s\nsetoption name UCI_Variant value %s\n%s\nquit\n' \
    "$ini" "$variant" "$cmds" | "${ENGINE}" 2>&1
}

test_passive_blast() {
  local tmp_ini out
  tmp_ini=$(mktemp)
  cat > "${tmp_ini}" <<'INI'
[passive-blast-test:chess]
customPiece1 = f:W
blastPassiveTypes = f
pieceToCharTable = ...............F....K...............f....k
INI

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen 4k3/8/8/4f3/8/4K3/8/8 w - - 0 1\ngo perft 1')
  if grep -q "Can not use kings with blastPassiveTypes." <<<"${out}" || \
     grep -q "unknown variant 'passive-blast-test'" <<<"${out}"; then
    echo "skip: passive blast regression not supported by this build"
    rm -f "${tmp_ini}"
    return 0
  fi
  ! grep -q "^e3e4: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen r3k3/4f3/8/8/8/8/4K3/8 b - - 0 1\ngo perft 1')
  ! grep -q "^a8a7: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen r3k3/8/4f3/8/8/8/4K3/8 w - - 0 1 moves e6e7\ngo perft 1')
  ! grep -q "^a8a7: 1$" <<<"${out}"

  out=$(run_raw_cmds "${tmp_ini}" passive-blast-test $'position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4\nd')
  grep -q "Fen: 4k3/8/8/4f3/8/8/4K3/8 b - - 1 1" <<<"${out}"

  cat > "${tmp_ini}" <<'INI'
[passive-blast-immune:chess]
customPiece1 = f:W
blastPassiveTypes = f
blastImmuneTypes = r
pieceToCharTable = ....R..........F....K....r..........f....k
INI
  out=$(run_raw_cmds "${tmp_ini}" passive-blast-immune $'position fen 4k3/8/8/4f3/8/4R3/4K3/8 w - - 0 1 moves e3e4\nd')
  grep -q "Fen: 4k3/8/8/4f3/4R3/8/4K3/8 b - - 1 1" <<<"${out}"

  rm -f "${tmp_ini}"
}

test_crazyhouse_multi_pawn_promo() {
  local tmp_ini out
  tmp_ini=$(mktemp "${TMPDIR:-/tmp}/fsx-crazyhouse-multi-pawn-promo-XXXXXX.ini")
  cat >"${tmp_ini}" <<'VAR'
[newvariant:crazyhouse]
promotionPawnTypes=pb
promotionPieceTypes=qn
VAR

  out=$(run_raw_cmds "${tmp_ini}" newvariant $'position fen r7/7P/8/8/8/8/8/k1K5 w - - 0 1 moves h7h8q a8h8\nd')
  grep -Fq "Fen: 7r/8/8/8/8/8/8/k1K5[p] w - - 0 2" <<<"${out}"
  rm -f "${tmp_ini}"
}

run_case pairdrop
run_case swap
run_case pull
run_case wrapped
run_case gate-history-square
run_case clone-pseudoroyal
run_case pseudoroyal-state
run_case firstmove-pseudoroyal
test_passive_blast
test_crazyhouse_multi_pawn_promo

echo "quiet-check-special-moves ok"
