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

[stack-material:chess]
laserGame = true
stackingPieceTypes = r
laserEmitters = white@a1:0, black@h8:2
laser_r = D/D/D/D
laser_k = D/D/D/D
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

static void test_stacked_piece_counts_as_two_material_units() {
    const Variant* stackMaterial = variants.get("stack-material");
    StateInfo st{};
    Position pos;
    pos.set(stackMaterial, "7k/8/8/8/8/8/1RR5/7K w - - 0 1", false, &st, nullptr);

    const Value beforeNpm = pos.non_pawn_material(WHITE);
    const Score beforePsq = pos.psq_score();
    const Key beforeMaterialKey = pos.state()->materialKey;
    std::string stackText = "b2c2+";
    Move stack = UCI::to_move(pos, stackText);
    assert(stack != MOVE_NONE && pos.legal(stack));

    StateInfo stackedState{};
    pos.do_move(stack, stackedState);
    assert(pos.pos_is_ok());
    assert(pos.count<ROOK>(WHITE) == 1);
    assert(pos.count_with_stacks(WHITE, ROOK) == 2);
    assert(pos.non_pawn_material(WHITE) == beforeNpm);
    assert(pos.psq_score() == beforePsq + PSQT::psq[W_ROOK][SQ_C2] - PSQT::psq[W_ROOK][SQ_B2]);

    pos.undo_move(stack);
    assert(pos.pos_is_ok());
    assert(pos.count<ROOK>(WHITE) == 2);
    assert(pos.count_with_stacks(WHITE, ROOK) == 2);
    assert(pos.non_pawn_material(WHITE) == beforeNpm);
    assert(pos.psq_score() == beforePsq);
    assert(pos.state()->materialKey == beforeMaterialKey);

    StateInfo laserSt{};
    pos.set(stackMaterial, "7k/8/8/8/8/R+7/8/7K w - - 0 1", false, &laserSt, nullptr);
    const Value stackedNpm = pos.non_pawn_material(WHITE);
    const Score stackedPsq = pos.psq_score();
    const Key stackedMaterialKey = pos.state()->materialKey;
    std::string triggerText = "h1g1";
    Move trigger = UCI::to_move(pos, triggerText);
    assert(trigger != MOVE_NONE && pos.legal(trigger));

    StateInfo firedState{};
    pos.do_move(trigger, firedState);
    assert(pos.pos_is_ok());
    assert(!pos.is_stacked(SQ_A3));
    assert(pos.count_with_stacks(WHITE, ROOK) == 1);
    assert(pos.non_pawn_material(WHITE) == stackedNpm - PieceValue[MG][W_ROOK]);
    assert(pos.psq_score() == stackedPsq - PSQT::psq[W_ROOK][SQ_A3]
                                      + PSQT::psq[W_KING][SQ_G1] - PSQT::psq[W_KING][SQ_H1]);

    pos.undo_move(trigger);
    assert(pos.pos_is_ok());
    assert(pos.is_stacked(SQ_A3));
    assert(pos.count_with_stacks(WHITE, ROOK) == 2);
    assert(pos.non_pawn_material(WHITE) == stackedNpm);
    assert(pos.psq_score() == stackedPsq);
    assert(pos.state()->materialKey == stackedMaterialKey);
}

int main() {
    init_test_engine();
    load_variants();
    test_blast_center_pawn_promotion_updates_pawn_key();
    test_blast_see_values_enemy_royal_removal_as_win();
    test_null_move_clears_undo_payload();
    test_null_move_preserves_extinction_history();
    test_spell_chess_null_move_decays();
    test_stacked_piece_counts_as_two_material_units();
    return 0;
}
EOF

fsx_harness_collect_objects
fsx_harness_build "${HARNESS_CPP}" "${HARNESS_BIN}" \
  "stateinfo-regressions" "${HARNESS_BIN}.sig"

echo "StateInfo regression test started"
"${HARNESS_BIN}"
echo "StateInfo regression test passed"
