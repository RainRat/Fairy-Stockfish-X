#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "spell potion movegen test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
ENGINE=${1:-./stockfish}
ENGINE_BASENAME=$(basename "${ENGINE}")
CXX_DEFS=(-DIS_64BIT -DUSE_PTHREADS)
case "${ENGINE_BASENAME}" in
  stockfish-allvars*)
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-large*)
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-vlb*)
    CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
esac

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/spell-potion-movegen"
BUILD_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.sig"
HARNESS_CPP="${BUILD_SIG_DIR}/spell-potion-movegen.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/spell-potion-movegen.bin"
HARNESS_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.harness.sig"
mkdir -p "${BUILD_SIG_DIR}"

if command -v sha256sum >/dev/null 2>&1; then
  MAKEFILE_HASH="$(cd "${ROOT_DIR}/src" && sha256sum Makefile | cut -d' ' -f1)"
elif command -v shasum >/dev/null 2>&1; then
  MAKEFILE_HASH="$(cd "${ROOT_DIR}/src" && shasum -a 256 Makefile | cut -d' ' -f1)"
else
  MAKEFILE_HASH="no-hash-tool"
fi

BUILD_SIG="$(printf '%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}")"
if [[ ! -f "${BUILD_SIG_FILE}" || "$(cat "${BUILD_SIG_FILE}" 2>/dev/null || true)" != "${BUILD_SIG}" ]]; then
  printf '%s\n' "${BUILD_SIG}" > "${BUILD_SIG_FILE}"
fi

cat > "${HARNESS_CPP}" <<'EOF'
#include <cstdlib>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>

#include "bitboard.h"
#include "apiutil.h"
#include "endgame.h"
#include "movegen.h"
#include "movepick.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

static void init_engine() {
    UCI::init(Options);
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
}

static void expect(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << msg << std::endl;
        std::exit(1);
    }
}

static void load_test_variants() {
    static bool loaded = false;
    if (loaded)
        return;

    std::istringstream in(R"INI(
[commitgate-test:chess]
commitGates = true
castling = false
startFen = n7/4k3/8/8/8/8/8/8/4K3/4R3 w - - 0 1

[spell-sacred:spell-chess]
checkedRoyalsIgnoreFreeze = true
)INI");
    variants.parse_istream<false>(in);
    loaded = true;
}

static void test_jump_lists() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "7k/8/8/p7/8/p7/8/R3K3[J] w - - 0 1", false, &st, nullptr);

    std::string quietJumpStr = "j@a3,a1a4";
    std::string captureJumpStr = "j@a3,a1a5";
    const Move quietJump = UCI::to_move(pos, quietJumpStr);
    const Move captureJump = UCI::to_move(pos, captureJumpStr);
    expect(quietJump != MOVE_NONE, "jump potion quiet move failed to parse");
    expect(captureJump != MOVE_NONE, "jump potion capture failed to parse");

    const auto legalMoves = MoveList<LEGAL>(pos);

    expect(legalMoves.contains(quietJump), "jump potion quiet move missing from LEGAL");
    expect(legalMoves.contains(captureJump), "jump potion capture missing from LEGAL");
}

static void test_jump_checks() {
    StateInfo st{};
    Position captureCheckPos;
    captureCheckPos.set(variants.get("spell-chess"), "7K/8/8/8/8/8/8/R1p3pk[J] w - - 0 1", false, &st, nullptr);
    std::string captureCheckStr = "j@c1,a1g1";
    const Move captureCheck = UCI::to_move(captureCheckPos, captureCheckStr);
    expect(captureCheck != MOVE_NONE, "jump potion checking capture failed to parse");

    const auto legalMoves = MoveList<LEGAL>(captureCheckPos);

    expect(legalMoves.contains(captureCheck), "jump potion checking capture missing from LEGAL");
}

static void test_empty_destination_capture_predicate() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "4k3/8/8/3pP3/8/8/8/4K3[F] w - d6 0 1", false, &st, nullptr);

    const Move epCapture = make<EN_PASSANT>(SQ_E5, SQ_D6);
    const bool oldStyleCapture = !pos.empty(to_sq(epCapture));
    const bool newStyleCapture = pos.capture(epCapture);
    expect(newStyleCapture, "en passant base move should be a capture");
    expect(pos.empty(to_sq(epCapture)), "en passant destination should be empty");
    expect(oldStyleCapture != newStyleCapture, "empty-destination capture did not expose the misclassification condition");
}

static void test_jump_evasions() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("spell-chess"), "k6r/8/8/8/8/8/8/R1p4K[J] w - - 0 1", false, &st, nullptr);
    std::string nonEvasionStr = "j@c1,a1g1";
    const Move nonEvasion = UCI::to_move(pos, nonEvasionStr);
    expect(nonEvasion != MOVE_NONE, "non-evasion jump potion move failed to parse");

    const auto legalMoves = MoveList<LEGAL>(pos);
    expect(legalMoves.contains(nonEvasion), "non-evasion jump potion move missing from LEGAL");
}

static void test_persistent_jump_and_short_cast() {
    const char* fen = "K7/6b1/8/4P3/8/8/4R3/7k[JJFFFFFjjfffff] w - - 0 1";

    // A Jump potion may accompany an ordinary move that stops before its
    // target.  It must not force the caster to jump on that same move.
    StateInfo shortState{};
    Position shortPos;
    shortPos.set(variants.get("spell-chess"), fen, false, &shortState, nullptr);
    std::string shortCastStr = "j@e5,e2e4";
    const Move shortCast = UCI::to_move(shortPos, shortCastStr);
    expect(shortCast != MOVE_NONE, "short jump-potion cast failed to parse");
    expect(MoveList<LEGAL>(shortPos).contains(shortCast),
           "jump potion incorrectly requires its casting move to cross the target");

    // The target persists through the opponent's reply and can be used by
    // that opponent.  This is the GUI regression reported from g7 to c3.
    StateInfo firstState{};
    Position pos;
    pos.set(variants.get("spell-chess"), fen, false, &firstState, nullptr);
    std::string castStr = "j@e5,e2e6";
    const Move cast = UCI::to_move(pos, castStr);
    expect(cast != MOVE_NONE, "persistent jump-potion cast failed to parse");
    expect(MoveList<LEGAL>(pos).contains(cast), "persistent jump-potion cast missing from LEGAL");

    StateInfo replyState{};
    pos.do_move(cast, replyState);
    std::string bishopJumpStr = "g7c3";
    const Move bishopJump = UCI::to_move(pos, bishopJumpStr);
    expect(bishopJump != MOVE_NONE, "opponent jump reply failed to parse");
    expect(MoveList<LEGAL>(pos).contains(bishopJump),
           "opponent cannot use the previous turn's jump-potion target");

    // A newly cast Jump must retain the previous player's persistent target
    // while its accompanying slider move is revalidated. Both occupied path
    // squares below are jump targets for the rook.
    StateInfo mergedJumpState{};
    Position mergedJump;
    mergedJump.set(variants.get("spell-chess"),
                   "7k/8/8/8/8/r7/r7/R6K[JJFFFFFjjfffff] w - - 0 1 bj:a2",
                   false, &mergedJumpState, nullptr);
    const Move mergedJumpMove = make_gating<NORMAL>(
        SQ_A1, SQ_A4, mergedJump.potion_piece(Variant::POTION_JUMP), SQ_A3);
    expect(MoveList<LEGAL>(mergedJump).contains(mergedJumpMove),
           "new Jump cast discarded the previous persistent target");
}

template<GenType Type>
static void expect_split_potion_generation_matches(const Position& pos, const char* label) {
    ExtMove complete[MOVEGEN_OVERFLOW_CAPACITY];
    ExtMove split[MOVEGEN_OVERFLOW_CAPACITY];
    const ExtMove* completeEnd = generate<Type>(pos, complete);
    ExtMove* baseEnd = generate_without_potions<Type>(pos, split);
    const ExtMove* splitEnd = append_potions<Type>(pos, split, baseEnd);

    std::set<std::string> completeMoves;
    std::set<std::string> splitMoves;
    for (const ExtMove* move = complete; move != completeEnd; ++move)
        completeMoves.insert(UCI::move(pos, *move));
    for (const ExtMove* move = split; move != splitEnd; ++move)
        splitMoves.insert(UCI::move(pos, *move));
    expect(completeMoves == splitMoves, label);
}

static void test_split_potion_generation_uses_persistent_jump() {
    const char* fen = "K7/6b1/8/4P3/8/8/4R3/7k[JJFFFFFjjfffff] w - - 0 1";
    StateInfo initialState{};
    Position pos;
    pos.set(variants.get("spell-chess"), fen, false, &initialState, nullptr);
    std::string castStr = "j@e5,e2e6";
    const Move cast = UCI::to_move(pos, castStr);
    expect(cast != MOVE_NONE, "persistent jump generation cast failed to parse");
    StateInfo replyState{};
    pos.do_move(cast, replyState);

    expect_split_potion_generation_matches<CAPTURES>(pos, "split CAPTURES potion generation lost persistent Jump context");
    expect_split_potion_generation_matches<QUIETS>(pos, "split QUIETS potion generation lost persistent Jump context");
    expect_split_potion_generation_matches<QUIET_CHECKS>(pos, "split QUIET_CHECKS potion generation lost persistent Jump context");
    expect_split_potion_generation_matches<NON_EVASIONS>(pos, "split NON_EVASIONS potion generation lost persistent Jump context");
}

static void test_qsearch_keeps_freeze_capture_defense() {
    StateInfo state{};
    Position pos;
    pos.set(variants.get("spell-chess"),
            "r1b2k2/ppp2ppp/2n5/4p3/2B1K3/P1P2qrP/2PP2P1/1RB3NR[JJFFFFj] w - - 1 15 bf:g2 <0 0 2 1>",
            false, &state, nullptr);
    std::string defenseStr = "f@g4,e4f3";
    const Move defense = UCI::to_move(pos, defenseStr);
    expect(defense != MOVE_NONE, "freeze-capture defense failed to parse");
    expect(MoveList<LEGAL>(pos).contains(defense), "freeze-capture defense is not legal");

    GateHistory gateHistory{};
    CapturePieceToHistory captureHistory{};
    MovePicker picker(pos, MOVE_NONE, DEPTH_QS_CHECKS, nullptr, &gateHistory,
                      &captureHistory, nullptr, SQ_NONE);
    bool foundDefense = false;
    for (Move move; (move = picker.next_move()) != MOVE_NONE; )
        foundDefense |= move == defense;
    expect(foundDefense,
           "quiescence search pruned a legal freeze-potion capture defense");
}

static void test_main_search_keeps_freeze_on_previously_frozen_enemy() {
    StateInfo state{};
    Position pos;
    pos.set(variants.get("spell-chess"),
            "r1b2k2/ppp2ppp/2n5/4p3/2B1K3/P1P2qrP/2PP2P1/1RB3NR[JJFFFFj] w - - 1 15 bf:g2 <0 0 2 1>",
            false, &state, nullptr);
    std::string defenseStr = "f@g4,e4d4";
    const Move defense = UCI::to_move(pos, defenseStr);
    expect(defense != MOVE_NONE, "quiet freeze defense failed to parse");
    expect(MoveList<LEGAL>(pos).contains(defense), "quiet freeze defense is not legal");

    auto mainHistory = std::make_unique<ButterflyHistory>();
    auto gateHistory = std::make_unique<GateHistory>();
    auto lowPlyHistory = std::make_unique<LowPlyHistory>();
    auto captureHistory = std::make_unique<CapturePieceToHistory>();
    std::array<std::unique_ptr<PieceToHistory>, 6> continuationStorage;
    std::array<const PieceToHistory*, 6> continuationHistory{};
    for (size_t i = 0; i < continuationStorage.size(); ++i)
    {
        continuationStorage[i] = std::make_unique<PieceToHistory>();
        continuationHistory[i] = continuationStorage[i].get();
    }
    const Move killers[2] = {MOVE_NONE, MOVE_NONE};
    MovePicker picker(pos, MOVE_NONE, 1, mainHistory.get(), gateHistory.get(),
                      lowPlyHistory.get(), captureHistory.get(),
                      continuationHistory.data(), MOVE_NONE, killers, 0);
    bool foundDefense = false;
    for (Move move; (move = picker.next_move()) != MOVE_NONE; )
        foundDefense |= move == defense;
    expect(foundDefense,
           "main search pruned a freeze cast because its enemy was frozen before the cast");
}

static void test_jump_move_picker_keeps_persistent_targets() {
    const char* shortFen = "K7/6b1/8/4P3/8/8/4R3/7k[JJFFFFFjjfffff] w - - 0 1";
    StateInfo shortState{};
    Position shortPos;
    shortPos.set(variants.get("spell-chess"), shortFen, false, &shortState, nullptr);
    std::string shortCastStr = "j@e5,e2e4";
    const Move shortCast = UCI::to_move(shortPos, shortCastStr);
    expect(shortCast != MOVE_NONE, "short Jump cast failed to parse for MovePicker");

    auto mainHistory = std::make_unique<ButterflyHistory>();
    auto gateHistory = std::make_unique<GateHistory>();
    auto lowPlyHistory = std::make_unique<LowPlyHistory>();
    auto captureHistory = std::make_unique<CapturePieceToHistory>();
    std::array<std::unique_ptr<PieceToHistory>, 6> continuationStorage;
    std::array<const PieceToHistory*, 6> continuationHistory{};
    for (size_t i = 0; i < continuationStorage.size(); ++i)
    {
        continuationStorage[i] = std::make_unique<PieceToHistory>();
        continuationHistory[i] = continuationStorage[i].get();
    }
    const Move killers[2] = {MOVE_NONE, MOVE_NONE};
    MovePicker shortPicker(shortPos, MOVE_NONE, 1, mainHistory.get(), gateHistory.get(),
                           lowPlyHistory.get(), captureHistory.get(),
                           continuationHistory.data(), MOVE_NONE, killers, 0);
    bool foundShort = false;
    for (Move move; (move = shortPicker.next_move()) != MOVE_NONE; )
        foundShort |= move == shortCast;
    expect(foundShort, "main search pruned a short Jump cast");

    const char* recastFen =
        "7k/8/8/8/8/r7/r7/R6K[JJFFFFFjjfffff] w - - 0 1 bj:a2";
    StateInfo recastState{};
    Position recastPos;
    recastPos.set(variants.get("spell-chess"), recastFen, false, &recastState, nullptr);
    std::string recastStr = "j@a2,h1h2";
    const Move recast = UCI::to_move(recastPos, recastStr);
    expect(recast != MOVE_NONE, "Jump recast failed to parse for MovePicker");
    MovePicker recastPicker(recastPos, MOVE_NONE, 1, mainHistory.get(), gateHistory.get(),
                            lowPlyHistory.get(), captureHistory.get(),
                            continuationHistory.data(), MOVE_NONE, killers, 0);
    bool foundRecast = false;
    for (Move move; (move = recastPicker.next_move()) != MOVE_NONE; )
        foundRecast |= move == recast;
    expect(foundRecast, "main search pruned a Jump recast of the active target");

    const char* captureFen =
        "r3k3/8/4p3/8/8/8/8/R3K3[JJFFFFFjjfffff] w - - 0 1";
    StateInfo captureState{};
    Position capturePos;
    capturePos.set(variants.get("spell-chess"), captureFen, false, &captureState, nullptr);
    std::string captureStr = "j@e6,a1a8";
    const Move capture = UCI::to_move(capturePos, captureStr);
    expect(capture != MOVE_NONE, "short Jump capture failed to parse for MovePicker");
    GateHistory captureGateHistory{};
    CapturePieceToHistory captureHistoryForQsearch{};
    MovePicker capturePicker(capturePos, MOVE_NONE, DEPTH_QS_CHECKS, nullptr,
                             &captureGateHistory, &captureHistoryForQsearch,
                             nullptr, SQ_NONE);
    bool foundCapture = false;
    for (Move move; (move = capturePicker.next_move()) != MOVE_NONE; )
        foundCapture |= move == capture;
    expect(foundCapture, "quiescence search pruned a short Jump capture");
}

static void test_potion_root_move_undo_integrity() {
    StateInfo rootState{};
    Position pos;
    pos.set(variants.get("spell-chess"),
            "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/3B1N2/PPPP1PPP/RNBQK2R[JJFFFFjjffff] b KQkq - 1 3 - <0 0 2 0>",
            false, &rootState, nullptr);
    const std::string before = pos.fen();
    const auto legalMoves = MoveList<LEGAL>(pos);

    for (const auto& extMove : legalMoves) {
        const Move move = extMove;
        StateInfo nextState{};
        pos.do_move(move, nextState);
        pos.undo_move(move);
        if (pos.fen() != before) {
            std::cerr << "potion move failed do/undo round trip: "
                      << UCI::move(pos, move) << std::endl;
            std::exit(1);
        }
    }
}

static void test_potion_fen_zone_validation() {
    const Variant* spell = variants.get("spell-chess");
    const char* prefix = "4k3/8/8/8/8/8/8/4K3[JJFFFFFjjfffff] w - - 0 1 ";
    expect(FEN::validate_fen(std::string(prefix) + "wf:e4,wj:d5,bf:c4,bj:b3", spell) == FEN::FEN_OK,
           "explicit multi-potion FEN was rejected");
    expect(FEN::validate_fen(std::string(prefix) + "wf:e4,wf:d5", spell) != FEN::FEN_OK,
           "duplicate potion-zone FEN was accepted");
    expect(FEN::validate_fen(std::string(prefix) + "wf:e4,", spell) != FEN::FEN_OK,
           "trailing potion-zone separator was accepted");

    // Position loading must not leave a partially parsed zone behind when a
    // later entry makes the token invalid.
    StateInfo invalidState{};
    Position invalid;
    invalid.set(spell, std::string(prefix) + "wf:e4,wf:d5", false,
                &invalidState, nullptr);
    expect(!invalid.potion_zone(WHITE, Variant::POTION_FREEZE),
           "invalid potion-zone FEN committed a partial zone");
}

static void test_sacred_royal() {
    const char* fen = "4r2k/8/8/8/8/8/8/4K3[JJFFFFFjjfffff] w - - 0 1 bf:e1";
    std::string moveStr = "e1d1";

    StateInfo sacredState{};
    Position sacred;
    sacred.set(variants.get("spell-sacred"), fen, false, &sacredState, nullptr);
    const Move move = UCI::to_move(sacred, moveStr);
    expect(move != MOVE_NONE, "Sacred Royal move failed to parse");
    expect(MoveList<LEGAL>(sacred).contains(move),
           "attacked Spell Chess king remained frozen with Sacred Royal enabled");

    const Variant* standardSpell = variants.get("spell-chess");
    expect(standardSpell != nullptr, "standard Spell Chess variant failed to load");
    StateInfo ordinaryState{};
    Position ordinary;
    ordinary.set(standardSpell, fen, false, &ordinaryState, nullptr);
    expect(!MoveList<LEGAL>(ordinary).contains(UCI::to_move(ordinary, moveStr)),
           "standard Spell Chess unexpectedly enabled Sacred Royal");

    // A Freeze zone cast by the checking side must not hide its own
    // attacker from Sacred Royal's attack test. The black rook is checking
    // from e2 while black's zone also freezes the white king on e1.
    const char* attackerOwnZoneFen =
        "7k/8/8/8/8/8/4r3/4K3[JJFFFFFjjfffff] w - - 0 1 bf:e2";
    StateInfo attackerOwnZoneState{};
    Position attackerOwnZone;
    attackerOwnZone.set(variants.get("spell-sacred"), attackerOwnZoneFen,
                        false, &attackerOwnZoneState, nullptr);
    const Move escape = UCI::to_move(attackerOwnZone, moveStr);
    expect(MoveList<LEGAL>(attackerOwnZone).contains(escape),
           "attacker's own Freeze zone incorrectly kept the royal frozen");

    // Conversely, if the royal owner's zone freezes the checking piece, the
    // attacker remains absent from the attack map and does not thaw the king.
    const char* ownerZoneFen =
        "7k/8/8/8/8/8/4r3/4K3[JJFFFFFjjfffff] w - - 0 1 wf:e2";
    StateInfo ownerZoneState{};
    Position ownerZone;
    ownerZone.set(variants.get("spell-sacred"), ownerZoneFen, false,
                  &ownerZoneState, nullptr);
    expect(!MoveList<LEGAL>(ownerZone).contains(UCI::to_move(ownerZone, moveStr)),
           "royal owner's Freeze zone incorrectly thawed a frozen attacker");
}

static void test_sacred_royal_compound_freeze() {
    const Variant* sacredVariant = variants.get("spell-sacred");
    const Variant* standardVariant = variants.get("spell-chess");
    std::string moveStr = "f@e1,e1d1";

    // An unfrozen checked king may cast Freeze on itself and move. The
    // temporary self-freeze must be evaluated through Sacred Royal rather
    // than rejected as a raw potion-zone hit on the origin square.
    const char* selfFreezeFen =
        "K3R3/8/8/8/8/8/8/4k3[JJFFFFFjjfffff] b - - 0 1";
    StateInfo selfFreezeState{};
    Position selfFreeze;
    selfFreeze.set(sacredVariant, selfFreezeFen, false, &selfFreezeState, nullptr);
    const Move selfFreezeMove = UCI::to_move(selfFreeze, moveStr);
    expect(selfFreezeMove != MOVE_NONE, "Sacred compound self-freeze failed to parse");
    expect(MoveList<LEGAL>(selfFreeze).contains(selfFreezeMove),
           "Sacred Royal rejected an unfrozen checked king's self-freeze move");

    StateInfo ordinaryState{};
    Position ordinary;
    ordinary.set(standardVariant, selfFreezeFen, false, &ordinaryState, nullptr);
    expect(!MoveList<LEGAL>(ordinary).contains(UCI::to_move(ordinary, moveStr)),
           "ordinary Spell Chess accepted a king move through its Freeze zone");

    // If the king was already frozen, freezing the checking piece in the
    // first half of the compound move must not thaw the king for its second
    // half.
    const char* frozenKingFen =
        "K7/8/8/8/8/4R3/8/4k3[JJFFFFFjjfffff] b - - 0 1 wf:e1";
    StateInfo frozenKingState{};
    Position frozenKing;
    frozenKing.set(sacredVariant, frozenKingFen, false, &frozenKingState, nullptr);
    const Move freezeCheckerMove = make_gating<NORMAL>(
        SQ_E1, SQ_D1, frozenKing.potion_piece(Variant::POTION_FREEZE), SQ_E2);
    expect(!MoveList<LEGAL>(frozenKing).contains(freezeCheckerMove),
           "Sacred Royal thawed a king after freezing its checking piece");

    // The same post-cast situation must remain frozen when the king starts
    // unfrozen: the king and checker are both inside the caster's zone.
    StateInfo checkerZoneState{};
    Position checkerZone;
    const char* checkerZoneFen =
        "K7/8/8/8/8/4R3/8/4k3[JJFFFFFjjfffff] b - - 0 1";
    checkerZone.set(sacredVariant, checkerZoneFen, false, &checkerZoneState, nullptr);
    const Move checkerZoneMove = make_gating<NORMAL>(
        SQ_E1, SQ_D1, checkerZone.potion_piece(Variant::POTION_FREEZE), SQ_E2);
    expect(!MoveList<LEGAL>(checkerZone).contains(checkerZoneMove),
           "Sacred Royal thawed a king whose zone also froze its checker");
}

static void test_committed_piece_type_helpers() {
    StateInfo st{};
    const Variant* v = variants.get("commitgate-test");
    expect(v != nullptr, "commitgate-test variant failed to load");

    Position pos;
    pos.set(v, "n7/4k3/8/8/8/8/8/8/4K3/4R3 w - - 0 1", false, &st, nullptr);

    const Move fakeCastling = make<CASTLING>(SQ_E1, SQ_A8);
    expect(pos.committed_piece_type(fakeCastling, false) == ROOK, "king-side committed piece lookup failed");
    expect(pos.committed_piece_type(fakeCastling, true) == KNIGHT, "rook-side committed piece lookup failed");
    expect(pos.committed_piece_type(make<NORMAL>(SQ_E2, SQ_E4), true) == NO_PIECE_TYPE, "non-castling lookup should be empty");
}

static void test_committed_gate_overflow_parse() {
    StateInfo st{};
    const Variant* v = variants.get("commitgate-test");
    expect(v != nullptr, "commitgate-test variant failed to load");

    Position pos;
    pos.set(v, "nnnnnnnnn/4k3/8/8/8/8/8/8/4K3/RRRRRRRRR w - - 0 1", false, &st, nullptr);
    expect(pos.pos_is_ok(), "overlong committed-gate FEN did not parse cleanly");
}

int main() {
    init_engine();
    load_test_variants();
    test_jump_lists();
    test_jump_checks();
    test_empty_destination_capture_predicate();
    test_jump_evasions();
    test_persistent_jump_and_short_cast();
    test_split_potion_generation_uses_persistent_jump();
    test_qsearch_keeps_freeze_capture_defense();
    test_main_search_keeps_freeze_on_previously_frozen_enemy();
    test_jump_move_picker_keeps_persistent_targets();
    test_potion_root_move_undo_integrity();
    test_potion_fen_zone_validation();
    test_sacred_royal();
    test_sacred_royal_compound_freeze();
    test_committed_piece_type_helpers();
    test_committed_gate_overflow_parse();
    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

if command -v sha256sum >/dev/null 2>&1; then
  HASHER=(sha256sum)
elif command -v shasum >/dev/null 2>&1; then
  HASHER=(shasum -a 256)
else
  HASHER=()
fi

hash_text() {
  if [[ ${#HASHER[@]} -gt 0 ]]; then
    printf '%s' "$1" | "${HASHER[@]}" | awk '{print $1}'
  else
    printf '%s' "$1" | wc -c | awk '{print $1}'
  fi
}

object_signature() {
  local sig
  sig=$(
    for obj in "${OBJ_FILES[@]}"; do
      stat -c '%n %Y %s' "${obj}" 2>/dev/null || stat -f '%N %m %z' "${obj}"
    done
  )
  hash_text "${sig}"
}

HARNESS_SIG="$(printf '%s|%s|%s|%s|%s|%s\n' \
    "${ENGINE_BASENAME}" \
    "${CXX}" \
    "${MAKEFILE_HASH}" \
    "${CXX_DEFS[*]}" \
    "$(object_signature)" \
    "$(hash_text "$(cat "${HARNESS_CPP}")")")"
if [[ ! -x "${HARNESS_BIN}" || ! -f "${HARNESS_SIG_FILE}" || "$(cat "${HARNESS_SIG_FILE}")" != "${HARNESS_SIG}" ]]; then
  rm -f "${HARNESS_BIN}"
  (
    cd "${ROOT_DIR}/src"
    "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
  )
  printf '%s\n' "${HARNESS_SIG}" > "${HARNESS_SIG_FILE}"
fi

echo "spell potion movegen test started"
"${HARNESS_BIN}"

# Repeated searches for the same GUI position must retain the caller's state
# chain.  Send each `go` only after the previous UCI search completed.
ENGINE_READ_TIMEOUT=${ENGINE_READ_TIMEOUT:-10}
ENGINE_LAST_LINE=""
coproc ENGINE_PROCESS { "${ENGINE}"; }
cleanup_engine() {
  if [[ -n "${ENGINE_PROCESS_PID:-}" ]]; then
    kill "${ENGINE_PROCESS_PID}" 2>/dev/null || true
    wait "${ENGINE_PROCESS_PID}" 2>/dev/null || true
  fi
}
trap cleanup_engine EXIT
engine_send() {
  printf '%s\n' "$1" >&"${ENGINE_PROCESS[1]}"
}
engine_read_until() {
  local pattern=$1 line
  while IFS= read -r -t "${ENGINE_READ_TIMEOUT}" line <&"${ENGINE_PROCESS[0]}"; do
    ENGINE_LAST_LINE=${line}
    [[ "${line}" =~ ${pattern} ]] && return 0
  done
  echo "timed out waiting for ${pattern}; last engine output: ${ENGINE_LAST_LINE}" >&2
  return 1
}
engine_read_bestmove() {
  local line
  while IFS= read -r -t "${ENGINE_READ_TIMEOUT}" line <&"${ENGINE_PROCESS[0]}"; do
    ENGINE_LAST_LINE=${line}
    if [[ "${line}" =~ ^bestmove[[:space:]]+([^[:space:]]+) ]]; then
      [[ "${BASH_REMATCH[1]}" != "(none)" ]]
      return
    fi
  done
  echo "timed out waiting for bestmove; last engine output: ${ENGINE_LAST_LINE}" >&2
  return 1
}
engine_send 'uci'
engine_read_until '^uciok$'
engine_send 'setoption name UCI_Variant value spell-chess'
engine_send 'position fen rnbqkb1r/pppp1ppp/5n2/4p3/4P3/3B1N2/PPPP1PPP/RNBQK2R[JJFFFFjjffff] b KQkq - 1 3 - <0 0 2 0>'
for _ in 1 2 3; do
  engine_send 'go depth 1'
  if ! engine_read_bestmove; then
    echo "repeated Spell Chess search did not return a best move" >&2
    exit 1
  fi
  engine_send 'd'
  if ! engine_read_until '^Fen: rnbqkb1r/pppp1ppp/5n2/4p3/4P3/3B1N2/PPPP1PPP/RNBQK2R\[JJFFFFjjffff\] b KQkq - 1 3 - <0 0 2 0>$'; then
    echo "repeated Spell Chess search changed the root position" >&2
    exit 1
  fi
done
engine_send 'quit'
wait "${ENGINE_PROCESS_PID}"
echo "spell potion movegen test passed"
