#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/../../../../tests/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "StateInfo regression test"

CXX=${CXX:-g++}
JOBS=${JOBS:-2}
source "${SCRIPT_DIR}/../../../../tests/lib/harness-build.sh"
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
#include "movegen.h"
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

[stack-transform:chess]
laserGame = true
customPiece1 = a:K
stackedPieceType = r:a
laserEmitters = white@a1:0, black@h8:2
laser_r = D/D/D/D
laser_a = D/D/D/D
laser_k = D/D/D/D
castling = false

[castling-piece-state:chess]
laserGame = true
orientedPieceTypes = r
promotedPieceType = p:r
laserEmitters = white@a1:0, black@h8:2
laser_r = D/D/D/D
laser_k = D/D/D/D
castling = true

[test-lion-state:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
castling = true
promotedPieceType = l:q
promotionLimit = q:1

[test-mandatory-lion:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
promotedPieceType = l:q
mandatoryPromotionRegionWhite = *8
mandatoryPromotionRegionBlack = *1

[test-morph-lion:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
captureMorph = true
promotedPieceType = l:q
castling = true

[test-piece-zone-lion:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
promotedPieceType = l:q p:r
promotionRegionWhite = Q(*7);R(*8)
promotionRegionBlack = Q(*2);R(*1)
castling = false

[test-forbidden-lion:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
mutuallyImmuneTypes = l
castling = true

[test-narrow-lion:chess]
customPiece1 = h:0
twoStepMoves = h:W>W
cylindrical = true
maxFile = b
castling = false
startFen = 1k/2/2/2/2/2/K1/H1 w - - 0 1

[test-blast-lion:chess]
customPiece1 = l:ADNWK
twoStepMoves = l:*
blastOnCapture = true
castling = false

[test-hook-state:chess]
customPiece1 = h:0
hookMoves = h:R-R
castling = false

[test-hook-double-state:chess]
customPiece1 = h:0
hookMoves = h:R-R:2
castling = false

[test-hook-promo-state:chess]
customPiece1 = h:0
hookMoves = h:R-R
promotedPieceType = h:q
castling = false

[test-horned-falcon:chess]
customPiece1 = h:BsRbRfDfW
twoStepMoves = h:N>N,N>S
castling = false

[test-clone-lion:chess]
customPiece1 = l:R
twoStepMoves = l:*
cloneMoveTypes = l
castling = false

[test-opening-lion:chess]
customPiece1 = l:0
twoStepMoves = l:*
openingSelfRemoval = true
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

static void test_stack_piece_transforms_and_undo() {
    const Variant* stackTransform = variants.get("stack-transform");
    PSQT::init(stackTransform);
    StateInfo st{};
    Position pos;
    pos.set(stackTransform, "7k/8/8/8/8/8/1RR5/7K w - - 0 1", false, &st, nullptr);

    Piece result = make_piece(WHITE, CUSTOM_PIECE_1);
    assert(PieceValue[MG][result] == 2 * PieceValue[MG][W_ROOK]);
    assert(PieceValue[EG][result] == 2 * PieceValue[EG][W_ROOK]);
    const Value beforeNpm = pos.non_pawn_material(WHITE);
    const Key beforeKey = pos.key();
    const Key beforeMaterialKey = pos.state()->materialKey;
    std::string stackText = "b2c2+";
    Move stack = UCI::to_move(pos, stackText);
    assert(stack != MOVE_NONE && pos.legal(stack));

    StateInfo stackedState{};
    pos.do_move(stack, stackedState);
    assert(pos.pos_is_ok());
    assert(pos.piece_on(SQ_C2) == result);
    assert(pos.count<ROOK>(WHITE) == 0);
    assert(pos.count(WHITE, CUSTOM_PIECE_1) == 1);
    assert(pos.non_pawn_material(WHITE) == beforeNpm);

    pos.undo_move(stack);
    assert(pos.pos_is_ok());
    assert(pos.piece_on(SQ_B2) == W_ROOK && pos.piece_on(SQ_C2) == W_ROOK);
    assert(pos.non_pawn_material(WHITE) == beforeNpm);
    assert(pos.key() == beforeKey);
    assert(pos.state()->materialKey == beforeMaterialKey);

    StateInfo laserSt{};
    pos.set(stackTransform, "7k/8/8/8/8/A7/8/7K w - - 0 1", false, &laserSt, nullptr);
    assert(pos.piece_on(SQ_A3) == result);
    const Value stackedNpm = pos.non_pawn_material(WHITE);
    const Key stackedKey = pos.key();
    const Key stackedMaterialKey = pos.state()->materialKey;
    std::string triggerText = "h1g1";
    Move trigger = UCI::to_move(pos, triggerText);
    assert(trigger != MOVE_NONE && pos.legal(trigger));

    StateInfo firedState{};
    pos.do_move(trigger, firedState);
    assert(pos.pos_is_ok());
    assert(pos.piece_on(SQ_A3) == W_ROOK);
    assert(pos.non_pawn_material(WHITE) == stackedNpm - PieceValue[MG][W_ROOK]);

    pos.undo_move(trigger);
    assert(pos.pos_is_ok());
    assert(pos.piece_on(SQ_A3) == result);
    assert(pos.non_pawn_material(WHITE) == stackedNpm);
    assert(pos.key() == stackedKey);
    assert(pos.state()->materialKey == stackedMaterialKey);
}

static void test_castling_preserves_piece_state() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("castling-piece-state"),
            "4k3/8/8/8/8/8/8/4K2+P(2) w K - 0 1", false, &st, nullptr);

    const Move castle = make<CASTLING>(SQ_E1, SQ_H1);
    assert(pos.legal(castle));
    assert(pos.is_promoted(SQ_H1));
    assert(pos.unpromoted_piece_on(SQ_H1) == W_PAWN);
    assert(pos.orientation_on(SQ_H1) == 2);

    StateInfo next{};
    pos.do_move(castle, next);
    assert(pos.is_promoted(SQ_F1));
    assert(pos.unpromoted_piece_on(SQ_F1) == W_PAWN);
    assert(pos.orientation_on(SQ_F1) == 2);

    pos.undo_move(castle);
    assert(pos.is_promoted(SQ_H1));
    assert(pos.unpromoted_piece_on(SQ_H1) == W_PAWN);
    assert(pos.orientation_on(SQ_H1) == 2);
}

static void test_gating_special_collision() {
    for (int gate = 0; gate <= int(SQ_MAX); ++gate) {
        for (PieceType pt = PieceType(1); pt < PIECE_TYPE_NB; ++pt) {
            Move m = make_gating<SPECIAL>(SQ_E2, SQ_E4, pt, Square(gate));
            assert(!is_two_step(m));
            assert(is_gating(m));
            assert(special_subtype(m) == SPECIAL_SUBTYPE_NONE);
        }
    }
    for (int via = 0; via <= int(SQ_MAX); ++via) {
        Move m = make_two_step(SQ_E2, Square(via), SQ_E4);
        assert(is_two_step(m));
        assert(!is_gating(m));
        assert(via_sq(m) == Square(via));
    }
}

static void test_two_step_state_transitions() {
    StateInfo st{};
    Position pos;
    // Lion on e4, black rooks on d5 and c6 with black castling rights
    pos.set(variants.get("test-lion-state"),
            "r3k2r/8/2r5/3r4/4L3/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);

    const Key origKey = pos.key();
    const Key origMatKey = pos.state()->materialKey;

    // 1. First-leg-only capture (igui): e4 -> d5 (captures r) -> e4
    Move m_igui = make_two_step(SQ_E4, SQ_D5, SQ_E4);
    assert(pos.legal(m_igui));
    StateInfo next1{};
    pos.do_move(m_igui, next1);
    assert(pos.piece_on(SQ_D5) == NO_PIECE);
    assert(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECE_1));
    assert(pos.count<ROOK>(BLACK) == 3);
    pos.undo_move(m_igui);
    assert(pos.key() == origKey);
    assert(pos.state()->materialKey == origMatKey);
    assert(pos.piece_on(SQ_D5) == B_ROOK);
    assert(pos.count<ROOK>(BLACK) == 4);

    // 2. Double capture: e4 -> d5 (captures r) -> c6 (captures r)
    Move m_double = make_two_step(SQ_E4, SQ_D5, SQ_C6);
    assert(pos.legal(m_double));
    StateInfo next2{};
    pos.do_move(m_double, next2);
    assert(pos.piece_on(SQ_D5) == NO_PIECE);
    assert(pos.piece_on(SQ_C6) == make_piece(WHITE, CUSTOM_PIECE_1));
    assert(pos.count<ROOK>(BLACK) == 2);
    pos.undo_move(m_double);
    assert(pos.key() == origKey);
    assert(pos.state()->materialKey == origMatKey);
    assert(pos.piece_on(SQ_D5) == B_ROOK);
    assert(pos.piece_on(SQ_C6) == B_ROOK);
    assert(pos.count<ROOK>(BLACK) == 4);

    // 3. First-leg capture of a piece with castling rights revokes those rights
    pos.set(variants.get("test-lion-state"),
            "r3k2r/6L1/8/8/8/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);
    const Key rookOrigKey = pos.key();
    const Key rookOrigMatKey = pos.state()->materialKey;
    const int rookOrigCastling = pos.state()->castlingRights;
    assert(rookOrigCastling & (1 << (BLACK * 2))); // Black has kingside castling rights

    Move m_cap_rook = make_two_step(SQ_G7, SQ_H8, SQ_G7);
    assert(pos.legal(m_cap_rook));
    StateInfo next3{};
    pos.do_move(m_cap_rook, next3);
    assert(!(pos.state()->castlingRights & (1 << (BLACK * 2)))); // Black kingside castling revoked!
    pos.undo_move(m_cap_rook);
    assert(pos.state()->castlingRights == rookOrigCastling);
    assert(pos.key() == rookOrigKey);
    assert(pos.state()->materialKey == rookOrigMatKey);
}

static void test_two_step_promotions() {
    StateInfo st{};
    Position pos;
    // In test-lion-state (chess baseline, rank 8 promoZone, promotionLimit q:1)
    // Board has no queen yet:
    pos.set(variants.get("test-lion-state"),
            "8/6L1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);

    Move m_promo = make_two_step(SQ_G7, SQ_F8, SQ_G8, true);
    Move m_nonpromo = make_two_step(SQ_G7, SQ_F8, SQ_G8, false);
    assert(pos.legal(m_promo));
    assert(pos.legal(m_nonpromo));

    const Key origKey = pos.key();
    const Key origMatKey = pos.state()->materialKey;
    StateInfo next{};
    pos.do_move(m_promo, next);
    assert(pos.piece_on(SQ_G8) == W_QUEEN);
    pos.undo_move(m_promo);
    assert(pos.piece_on(SQ_G7) == make_piece(WHITE, CUSTOM_PIECE_1));
    assert(pos.key() == origKey);
    assert(pos.state()->materialKey == origMatKey);

    // Promotion limit test: if White already has a Queen, promoting to Queen is disallowed by promotionLimit = q:1
    pos.set(variants.get("test-lion-state"),
            "8/6L1/8/8/8/8/8/Q3K2k w - - 0 1", false, &st, nullptr);
    assert(!pos.legal(m_promo)); // Disallowed by promotionLimit!
    assert(pos.legal(m_nonpromo));

    // Mandatory promotion test
    pos.set(variants.get("test-mandatory-lion"),
            "8/6L1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);
    assert(pos.legal(m_promo));
    assert(!pos.legal(m_nonpromo)); // Disallowed because entering rank 8 without promoting
}

static void test_capture_squares_and_recapture() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("test-lion-state"),
            "r3k2r/8/2r5/3r4/4L3/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);

    Move m_double = make_two_step(SQ_E4, SQ_D5, SQ_C6);
    Bitboard caps = pos.capture_squares(m_double);
    assert(caps & square_bb(SQ_D5));
    assert(caps & square_bb(SQ_C6));
    assert(!(caps & square_bb(SQ_E4)));
}

static void test_two_step_promotion_ignores_morph() {
    StateInfo st{};
    Position pos;
    // captureMorph must not apply on top of a two-step promotion: a promoting
    // capture ends as the promoted piece, like ordinary PIECE_PROMOTION.
    pos.set(variants.get("test-morph-lion"),
            "6r1/6L1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);

    Move m_promo = make_two_step(SQ_G7, SQ_F8, SQ_G8, true);
    assert(pos.legal(m_promo));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_promo, next);
    assert(pos.piece_on(SQ_G8) == W_QUEEN);
    pos.undo_move(m_promo);
    assert(pos.key() == origKey);

    // A non-promoting single capture still morphs into the victim type.
    Move m_plain = make_two_step(SQ_G7, SQ_F8, SQ_G8, false);
    assert(pos.legal(m_plain));
    StateInfo next2{};
    pos.do_move(m_plain, next2);
    assert(pos.piece_on(SQ_G8) == W_ROOK);
    pos.undo_move(m_plain);
    assert(pos.key() == origKey);
}

static void test_two_step_double_capture_rejects_ambiguous_effects() {
    StateInfo st{};
    Position pos;
    // With captureMorph active, a double capture has two victims but morph is
    // defined for one: reject instead of morphing into an arbitrary victim.
    // Single-leg captures remain well-defined and legal.
    pos.set(variants.get("test-morph-lion"),
            "r3k2r/8/2r5/3r4/4L3/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);

    Move m_double = make_two_step(SQ_E4, SQ_D5, SQ_C6);
    assert(!pos.legal(m_double));

    Move m_igui = make_two_step(SQ_E4, SQ_D5, SQ_E4);
    assert(pos.legal(m_igui));

    Move m_to_only = make_two_step(SQ_E4, SQ_E5, SQ_D5, false);
    assert(pos.legal(m_to_only));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_to_only, next);
    assert(pos.piece_on(SQ_D5) == W_ROOK);
    pos.undo_move(m_to_only);
    assert(pos.key() == origKey);
}

static void test_two_step_piece_specific_promotion_zone() {
    StateInfo st{};
    Position pos;
    // test-piece-zone-lion promotes the lion to Q on *7 but pawns to R on *8,
    // so the global zone (*7|*8) is wider than the lion's own zone (*7).
    // From d6, landing on f8 is outside the lion zone: promotion is illegal
    // even though the landing square is inside the global zone.
    pos.set(variants.get("test-piece-zone-lion"),
            "8/8/3L4/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);

    Move m_promo = make_two_step(SQ_D6, SQ_E7, SQ_F8, true);
    Move m_plain = make_two_step(SQ_D6, SQ_E7, SQ_F8, false);
    assert(!pos.legal(m_promo));
    assert(pos.legal(m_plain));

    // From e7 the origin square is inside the lion zone, so promotion is fine.
    pos.set(variants.get("test-piece-zone-lion"),
            "8/4L3/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);
    Move m_promo2 = make_two_step(SQ_E7, SQ_F8, SQ_F7, true);
    assert(pos.legal(m_promo2));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_promo2, next);
    assert(pos.piece_on(SQ_F7) == W_QUEEN);
    pos.undo_move(m_promo2);
    assert(pos.key() == origKey);
}

static void test_two_step_via_capture_restrictions() {
    StateInfo st{};
    Position pos;
    // mutuallyImmuneTypes = l forbids lion-on-lion captures. captured_piece()
    // only sees the primary (to) victim, so a double capture whose forbidden
    // victim is on the via leg must be rejected explicitly.
    // White lion e4, black lion d5, black rook c6/d4.
    pos.set(variants.get("test-forbidden-lion"),
            "r3k2r/8/2r5/3l4/3rL3/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);

    // Double capture e4->d5(lion)->c6(rook): forbidden via victim.
    Move m_double = make_two_step(SQ_E4, SQ_D5, SQ_C6);
    assert(!pos.legal(m_double));

    // Igui via-capture of the lion is caught by the primary-victim check.
    Move m_igui = make_two_step(SQ_E4, SQ_D5, SQ_E4);
    assert(!pos.legal(m_igui));

    // Single via-capture of an allowed victim stays legal.
    Move m_single = make_two_step(SQ_E4, SQ_D4, SQ_C4, false);
    assert(pos.legal(m_single));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_single, next);
    assert(pos.piece_on(SQ_D4) == NO_PIECE);
    assert(pos.piece_on(SQ_C4) == make_piece(WHITE, CUSTOM_PIECE_1));
    pos.undo_move(m_single);
    assert(pos.key() == origKey);
    assert(pos.piece_on(SQ_D4) == B_ROOK);
}

static void test_hook_state_transitions() {
    StateInfo st{};
    Position pos;
    // Hook rook legs (R-R): bend must be on a rook ray, transit rays empty,
    // sliders cannot jump over pieces.
    // White hook e4, black rooks e5/e6.
    pos.set(variants.get("test-hook-double-state"),
            "4k3/8/4r3/4r3/4H3/8/8/4K3 w - - 0 1", false, &st, nullptr);

    // Double capture e4->e5->e6 with :2 allowed.
    Move m_double = make_hook(SQ_E4, SQ_E5, SQ_E6, false);
    assert(pos.legal(m_double));
    const Key origKey = pos.key();
    const Key origMatKey = pos.state()->materialKey;
    StateInfo next{};
    pos.do_move(m_double, next);
    assert(pos.piece_on(SQ_E5) == NO_PIECE);
    assert(pos.piece_on(SQ_E6) == make_piece(WHITE, CUSTOM_PIECE_1));
    assert(pos.count<ROOK>(BLACK) == 0);
    pos.undo_move(m_double);
    assert(pos.key() == origKey);
    assert(pos.state()->materialKey == origMatKey);
    assert(pos.piece_on(SQ_E5) == B_ROOK);
    assert(pos.piece_on(SQ_E6) == B_ROOK);

    // Igui single capture e4->e5->e4.
    Move m_igui = make_hook(SQ_E4, SQ_E5, SQ_E4, false);
    assert(pos.legal(m_igui));
    StateInfo next2{};
    pos.do_move(m_igui, next2);
    assert(pos.piece_on(SQ_E5) == NO_PIECE);
    assert(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECE_1));
    pos.undo_move(m_igui);
    assert(pos.key() == origKey);

    // Cannot jump over the e6 rook: e4->e5->e7 is not pseudo-legal.
    // (Path geometry is validated in pseudo_legal(), not legal().)
    Move m_jump = make_hook(SQ_E4, SQ_E5, SQ_E7, false);
    assert(!pos.pseudo_legal(m_jump));

    // Diagonal bend is outside the R-R mask.
    Move m_diag = make_hook(SQ_E4, SQ_E5, SQ_F6, false);
    assert(!pos.pseudo_legal(m_diag));
}

static void test_hook_capture_limits() {
    StateInfo st{};
    Position pos;
    // Same geometry under :1: the double is illegal, singles stay legal.
    // Kings stand off the e-file so captures are judged on limits alone.
    pos.set(variants.get("test-hook-state"),
            "7k/8/4r3/4r3/4H3/8/8/K7 w - - 0 1", false, &st, nullptr);

    Move m_double = make_hook(SQ_E4, SQ_E5, SQ_E6, false);
    assert(!pos.legal(m_double));

    Move m_single = make_hook(SQ_E4, SQ_E5, SQ_F5, false);
    assert(pos.legal(m_single));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_single, next);
    assert(pos.piece_on(SQ_E5) == NO_PIECE);
    assert(pos.piece_on(SQ_F5) == make_piece(WHITE, CUSTOM_PIECE_1));
    assert(pos.count<ROOK>(BLACK) == 1);
    pos.undo_move(m_single);
    assert(pos.key() == origKey);
    assert(pos.piece_on(SQ_E5) == B_ROOK);
}

static void test_hook_promotions() {
    StateInfo st{};
    Position pos;
    // Hook promotions use the from|to squares like two-step promotions.
    pos.set(variants.get("test-hook-promo-state"),
            "8/6H1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);

    Move m_promo = make_hook(SQ_G7, SQ_G8, SQ_H8, true);
    Move m_plain = make_hook(SQ_G7, SQ_G8, SQ_H8, false);
    assert(pos.legal(m_promo));
    assert(pos.legal(m_plain));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m_promo, next);
    assert(pos.piece_on(SQ_H8) == W_QUEEN);
    pos.undo_move(m_promo);
    assert(pos.key() == origKey);
    assert(pos.piece_on(SQ_G7) == make_piece(WHITE, CUSTOM_PIECE_1));
}

static void test_quiet_multileg_promotion_is_tactical() {
    StateInfo st{};
    Position pos;
    // A quiet (victimless) two-step promotion must appear in the CAPTURES
    // generation bucket, exactly like an ordinary quiet pawn promotion.
    pos.set(variants.get("test-lion-state"),
            "8/6L1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);
    Move m_quiet_promo = make_two_step(SQ_G7, SQ_F8, SQ_G8, true);
    assert(pos.legal(m_quiet_promo));
    assert(!pos.capture(m_quiet_promo));
    assert(MoveList<CAPTURES>(pos).contains(m_quiet_promo));
    assert(pos.capture_or_promotion(m_quiet_promo));
    // A quiet promotion searched in CAPTURES must not be duplicated in QUIETS.
    assert(!MoveList<QUIETS>(pos).contains(m_quiet_promo));
    Move m_quiet = make_two_step(SQ_G7, SQ_F8, SQ_G8, false);
    assert(!MoveList<CAPTURES>(pos).contains(m_quiet));
    assert(MoveList<QUIETS>(pos).contains(m_quiet));

    pos.set(variants.get("chess"),
            "8/6P1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);
    Move m_pawn_promo = make<PROMOTION>(SQ_G7, SQ_G8, QUEEN);
    // Ordinary pawn quiet promotions are tactical for search ordering even
    // though the fairy pawn generator currently keeps them out of CAPTURES.
    assert(pos.capture_or_promotion(m_pawn_promo));

    // Hook quiet promotions follow the same policy.
    pos.set(variants.get("test-hook-promo-state"),
            "8/6H1/8/8/8/8/8/4K2k w - - 0 1", false, &st, nullptr);
    Move m_hook_quiet_promo = make_hook(SQ_G7, SQ_G8, SQ_H8, true);
    assert(pos.legal(m_hook_quiet_promo));
    assert(!pos.capture(m_hook_quiet_promo));
    assert(MoveList<CAPTURES>(pos).contains(m_hook_quiet_promo));
    assert(pos.capture_or_promotion(m_hook_quiet_promo));
    assert(!MoveList<QUIETS>(pos).contains(m_hook_quiet_promo));
    // Quiet non-promotions stay out of CAPTURES.
    Move m_hook_quiet = make_hook(SQ_G7, SQ_G8, SQ_H8, false);
    assert(!MoveList<CAPTURES>(pos).contains(m_hook_quiet));
    assert(MoveList<QUIETS>(pos).contains(m_hook_quiet));
}

static void test_igui_clears_not_moved() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("test-lion-state"),
            "r3k2r/8/2r5/3r4/4L3/8/8/R3K2R w KQkq - 0 1", false, &st, nullptr);
    assert(pos.not_moved_pieces(WHITE) & SQ_E4);

    // Quiet igui: the board is unchanged but the piece has moved.
    Move m_igui = make_two_step(SQ_E4, SQ_E5, SQ_E4);
    assert(pos.legal(m_igui));
    StateInfo n1{};
    pos.do_move(m_igui, n1);
    assert(!(pos.not_moved_pieces(WHITE) & SQ_E4));
    pos.undo_move(m_igui);
    assert(pos.not_moved_pieces(WHITE) & SQ_E4);

    // Capturing igui.
    Move m_cap = make_two_step(SQ_E4, SQ_D5, SQ_E4);
    StateInfo n2{};
    pos.do_move(m_cap, n2);
    assert(!(pos.not_moved_pieces(WHITE) & SQ_E4));
    pos.undo_move(m_cap);
    assert(pos.not_moved_pieces(WHITE) & SQ_E4);
    assert(pos.piece_on(SQ_D5) == B_ROOK);
}

static void test_narrow_board_direction_alias() {
    StateInfo st{};
    Position pos;
    // 2-file cylinder: E and W both step A1->B1. The encoded move stores
    // squares, not provenance, so pseudo-legality must accept ANY mask pair
    // explaining the geometry, not just the first alias found.
    pos.set(variants.get("test-narrow-lion"),
            "1k/2/2/2/2/2/K1/H1 w - - 0 1", false, &st, nullptr);
    Move m = make_two_step(SQ_A1, SQ_B1, SQ_A1);
    assert(pos.pseudo_legal(m));
    assert(pos.legal(m));
    assert(MoveList<LEGAL>(pos).contains(m));
}

static void test_blast_via_capture() {
    StateInfo st{};
    Position pos;
    // Via-only capture under blastOnCapture: the blast is centered on the
    // via square, removing the victim, the adjacent bystander, and the mover.
    pos.set(variants.get("test-blast-lion"),
            "7k/8/3b4/3r4/4L3/8/8/K7 w - - 0 1", false, &st, nullptr);
    Move m = make_two_step(SQ_E4, SQ_D5, SQ_E4);
    assert(pos.legal(m));
    const Key origKey = pos.key();
    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.piece_on(SQ_D5) == NO_PIECE);
    assert(pos.piece_on(SQ_D6) == NO_PIECE);
    assert(pos.piece_on(SQ_E4) == NO_PIECE);
    pos.undo_move(m);
    assert(pos.key() == origKey);
    assert(pos.piece_on(SQ_D5) == B_ROOK);
    assert(pos.piece_on(SQ_D6) == B_BISHOP);
    assert(pos.piece_on(SQ_E4) == make_piece(WHITE, CUSTOM_PIECE_1));
}

static void test_multileg_special_subtype_isolation() {
    StateInfo st{};
    Position pos;
    // A clone-capable piece using two-step movement must not decode as a
    // clone: multi-leg moves reuse SPECIAL with a subtype, while clones are
    // plain SPECIAL moves.
    pos.set(variants.get("test-clone-lion"),
            "4k3/8/8/8/4L3/8/8/4K3 w - - 0 1", false, &st, nullptr);
    Move m_multi = make_two_step(SQ_E4, SQ_E5, SQ_E6, false);
    assert(pos.pseudo_legal(m_multi));
    assert(!pos.is_clone_move(m_multi));

    // An igui multi-leg move (from == to) must not decode as opening
    // self-removal, which is also a plain SPECIAL from == to move.
    pos.set(variants.get("test-opening-lion"),
            "4k3/8/8/8/4L3/8/8/4K3 w - - 0 1", false, &st, nullptr);
    assert(pos.in_opening_self_removal_phase());
    Move m_igui = make_two_step(SQ_E4, SQ_D5, SQ_E4, false);
    assert(!pos.is_opening_self_removal_move(m_igui));
    assert(!pos.legal(m_igui));
}

static void test_hook_royal_attacks() {
    StateInfo st{};
    Position pos;
    // The h:0 hook piece has no Betza movement, so every attack below must
    // come from the shared multi-leg attack model, not ordinary rays.

    // Bend capture: white hook e4 attacks black king on e5 (bend).
    pos.set(variants.get("test-hook-state"),
            "8/8/8/4k3/4H3/8/8/4K3 b - - 0 1", false, &st, nullptr);
    assert(pos.evasion_checkers());
    assert(pos.requires_full_evasion_filter());
    // King takes the checker.
    assert(pos.legal(make<NORMAL>(SQ_E5, SQ_E4)));

    // Final-leg capture: white hook e4 attacks black king on e6 via e5.
    pos.set(variants.get("test-hook-state"),
            "8/8/4k3/8/4H3/8/8/4K3 b - - 0 1", false, &st, nullptr);
    assert(pos.evasion_checkers());
    assert(pos.requires_full_evasion_filter());

    // King movement into a hook attack is illegal: d5 is checked via e5 and
    // d6 is attacked via e6, so the king cannot flee there.
    pos.set(variants.get("test-hook-state"),
            "8/8/8/3k4/4H3/8/8/4K3 b - - 0 1", false, &st, nullptr);
    assert(pos.evasion_checkers());
    assert(pos.requires_full_evasion_filter());
    assert(!pos.legal(make<NORMAL>(SQ_D5, SQ_D6)));

    // Via royal capture is rejected like a final-square king capture.
    pos.set(variants.get("test-hook-state"),
            "8/8/8/4k3/4H3/8/8/4K3 w - - 0 1", false, &st, nullptr);
    assert(!pos.legal(make_hook(SQ_E4, SQ_E5, SQ_E6, false)));

    // gives_check: quiet hook entry gives check down the file.
    pos.set(variants.get("test-hook-state"),
            "4k3/8/8/8/4H3/8/8/4K3 w - - 0 1", false, &st, nullptr);
    Move m_check = make_hook(SQ_E4, SQ_E5, SQ_E6, false);
    assert(pos.legal(m_check));
    assert(pos.gives_check(m_check));

    // Non-king move exposing the king to a hook attack is illegal: Be2f3
    // vacates e2, opening e4-e3-e1 onto the white king.
    pos.set(variants.get("test-hook-state"),
            "4k3/8/8/8/4h3/8/P3B3/4K3 w - - 0 1", false, &st, nullptr);
    assert(!pos.evasion_checkers());
    assert(!pos.legal(make<NORMAL>(SQ_E2, SQ_F3)));
    assert(pos.legal(make<NORMAL>(SQ_A2, SQ_A3)));
}

static void test_two_step_vs_jump() {
    StateInfo st{};
    Position pos;
    // Horned Falcon: fD jumps over transit square, while two-step visits it
    pos.set(variants.get("test-horned-falcon"),
            "8/8/8/8/4P3/8/4H3/8 w - - 0 1", false, &st, nullptr);
    // e2e4 is a forward Dabbaba jump (fD) over friendly e3:
    Move m_jump = make<NORMAL>(SQ_E2, SQ_E4);
    assert(pos.legal(m_jump));
    // e2e3e4 is a two-step move visiting e3, which is empty here, but if e3 is blocked:
    pos.set(variants.get("test-horned-falcon"),
            "8/8/8/8/4p3/4P3/4H3/8 w - - 0 1", false, &st, nullptr);
    assert(pos.legal(m_jump)); // fD jumps over friendly e3!
    Move m_two_step = make_two_step(SQ_E2, SQ_E3, SQ_E4);
    assert(!pos.legal(m_two_step)); // two-step blocked by friendly e3!
}

int main() {
    init_test_engine();
    load_variants();
    test_blast_center_pawn_promotion_updates_pawn_key();
    test_blast_see_values_enemy_royal_removal_as_win();
    test_null_move_clears_undo_payload();
    test_null_move_preserves_extinction_history();
    test_spell_chess_null_move_decays();
    test_stack_piece_transforms_and_undo();
    test_castling_preserves_piece_state();
    test_gating_special_collision();
    test_two_step_state_transitions();
    test_two_step_promotions();
    test_two_step_promotion_ignores_morph();
    test_two_step_double_capture_rejects_ambiguous_effects();
    test_two_step_piece_specific_promotion_zone();
    test_two_step_via_capture_restrictions();
    test_quiet_multileg_promotion_is_tactical();
    test_igui_clears_not_moved();
    test_narrow_board_direction_alias();
    test_blast_via_capture();
    test_hook_state_transitions();
    test_hook_capture_limits();
    test_hook_promotions();
    test_capture_squares_and_recapture();
    test_two_step_vs_jump();
    test_multileg_special_subtype_isolation();
    test_hook_royal_attacks();
    return 0;
}
EOF

fsx_harness_collect_objects
fsx_harness_build "${HARNESS_CPP}" "${HARNESS_BIN}" \
  "stateinfo-regressions" "${HARNESS_BIN}.sig"

echo "StateInfo regression test started"
"${HARNESS_BIN}"
echo "StateInfo regression test passed"
