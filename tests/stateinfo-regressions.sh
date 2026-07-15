#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "StateInfo regression test"

CXX=${CXX:-g++}
JOBS=${JOBS:-2}
source "${SCRIPT_DIR}/lib/harness-build.sh"
fsx_harness_init "${ENGINE}" "${ROOT_DIR}"
fsx_harness_prepare_objects "${JOBS}"

BUILD_DIR="${ROOT_DIR}/.local/build/stateinfo-regressions"
mkdir -p "${BUILD_DIR}"

HARNESS_CPP="${BUILD_DIR}/stateinfo-regressions.cpp"
HARNESS_BIN="${BUILD_DIR}/stateinfo-regressions.bin"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <cstring>
#include <sstream>

#include "bitboard.h"
#define private public
#include "endgame.h"
#include "piece.h"
#include "position.h"
#undef private
#include "psqt.h"
#include "types.h"
#include "uci.h"
#include "variant.h"
#include "test_engine_init.hpp"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[blast-center-pawn-promotion:chess]
promotedPieceType = p:q
blastOnMove = true
blastPromotion = true

[royal-blast-see:chess]
blastOnCapture = true
castling = false
)ini");
    variants.parse_istream<false>(ss);
}

static void test_blast_center_pawn_promotion_updates_pawn_key() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("blast-center-pawn-promotion"),
            "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", false, &st, nullptr);

    const Key beforePawnKey = pos.pawn_key();
    const Move m = make<NORMAL>(SQ_E2, SQ_E3);
    assert(pos.legal(m));

    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.piece_on(SQ_E3) == W_QUEEN);
    assert(pos.pawn_key() != beforePawnKey);
}

static void test_blast_see_values_enemy_royal_removal_as_win() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("royal-blast-see"),
            "8/8/8/8/4k3/4p3/3Q4/K7 w - - 0 1", false, &st, nullptr);

    const Move m = make<NORMAL>(SQ_D2, SQ_E3);
    assert(pos.legal(m));
    assert(pos.see_ge(m, VALUE_ZERO + 1));
}

static void test_null_move_clears_undo_payload() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"), "startpos", false, &st, nullptr);

    const Key beforeKey = pos.state()->key;
    const Key beforeBoardKey = pos.state()->boardKey;
    const Key beforePawnKey = pos.state()->pawnKey;
    const Key beforeMaterialKey = pos.state()->materialKey;
    StateInfo next{};
    pos.do_null_move(next);

    assert(pos.pos_is_ok());

    pos.undo_null_move();
    assert(pos.pos_is_ok());
    assert(pos.state()->key == beforeKey);
    assert(pos.state()->boardKey == beforeBoardKey);
    assert(pos.state()->pawnKey == beforePawnKey);
    assert(pos.state()->materialKey == beforeMaterialKey);
}

static void test_null_move_preserves_extinction_history() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"), "startpos", false, &st, nullptr);

    pos.state()->extinctionSeen[WHITE] = piece_set(PAWN);
    pos.state()->extinctionSeen[BLACK] = piece_set(KNIGHT);

    StateInfo next{};
    pos.do_null_move(next);

    assert(pos.state()->extinctionSeen[WHITE] == piece_set(PAWN));
    assert(pos.state()->extinctionSeen[BLACK] == piece_set(KNIGHT));

    pos.undo_null_move();
    assert(pos.state()->extinctionSeen[WHITE] == piece_set(PAWN));
    assert(pos.state()->extinctionSeen[BLACK] == piece_set(KNIGHT));
}

static void test_spell_chess_null_move_decays() {
    const Variant* spellChess = variants.get("spell-chess");
    if (!spellChess)
        return;

    StateInfo st{};
    Position pos;
    pos.set(spellChess, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 f:e4 <3 1 2 0>", false, &st, nullptr);

    assert(pos.state()->potionZones[BLACK][Variant::POTION_FREEZE] != Bitboard(0));
    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_FREEZE] == 3);
    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_JUMP] == 1);
    assert(pos.state()->potionCooldown[BLACK][Variant::POTION_FREEZE] == 2);
    assert(pos.state()->potionCooldown[BLACK][Variant::POTION_JUMP] == 0);

    StateInfo next{};
    pos.do_null_move(next);

    assert(pos.state()->potionZones[WHITE][Variant::POTION_FREEZE] == Bitboard(0));
    assert(pos.state()->potionZones[WHITE][Variant::POTION_JUMP] == Bitboard(0));
    assert(pos.state()->potionZones[BLACK][Variant::POTION_FREEZE] == Bitboard(0));
    assert(pos.state()->potionZones[BLACK][Variant::POTION_JUMP] == Bitboard(0));

    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_FREEZE] == 2);
    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_JUMP] == 0);
    assert(pos.state()->potionCooldown[BLACK][Variant::POTION_FREEZE] == 2);
    assert(pos.state()->potionCooldown[BLACK][Variant::POTION_JUMP] == 0);

    Position expectedPos;
    StateInfo expectedSt{};
    expectedPos.set(spellChess, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 1 1 - <2 0 2 0>", false, &expectedSt, nullptr);

    assert(pos.state()->key == expectedPos.state()->key);

    pos.undo_null_move();
    assert(pos.state()->potionZones[BLACK][Variant::POTION_FREEZE] != Bitboard(0));
    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_FREEZE] == 3);
    assert(pos.state()->potionCooldown[WHITE][Variant::POTION_JUMP] == 1);
}

int main() {
    init_test_engine();
    load_variants();
    test_blast_center_pawn_promotion_updates_pawn_key();
    test_blast_see_values_enemy_royal_removal_as_win();
    test_null_move_clears_undo_payload();
    test_null_move_preserves_extinction_history();
    test_spell_chess_null_move_decays();
    return 0;
}
EOF

fsx_harness_collect_objects
fsx_harness_build "${HARNESS_CPP}" "${HARNESS_BIN}" \
  "stateinfo-regressions" "${HARNESS_BIN}.sig"

echo "StateInfo regression test started"
"${HARNESS_BIN}"
echo "StateInfo regression test passed"
