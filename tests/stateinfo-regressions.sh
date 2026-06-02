#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "StateInfo regression test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-src/stockfish}
SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)

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

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[blast-center-pawn-promotion:chess]
promotedPieceType = p:q
blastOnMove = true
blastPromotion = true
)ini");
    variants.parse_istream<false>(ss);
}

static void init_engine() {
    UCI::init(Options);
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
    load_variants();
}

static void assert_recomputed_state_matches(const Position& pos) {
    StateInfo recomputed = *pos.state();
    pos.set_state(&recomputed);
    assert(recomputed.key == pos.state()->key);
    assert(recomputed.boardKey == pos.state()->boardKey);
    assert(recomputed.pawnKey == pos.state()->pawnKey);
    assert(recomputed.materialKey == pos.state()->materialKey);
    assert(recomputed.nonPawnMaterial[WHITE] == pos.state()->nonPawnMaterial[WHITE]);
    assert(recomputed.nonPawnMaterial[BLACK] == pos.state()->nonPawnMaterial[BLACK]);
}

static void assert_no_undo_payload(const StateInfo* st) {
    assert(st->bycatchSquares == Bitboard(0));
    assert(st->promotedBycatch == Bitboard(0));
    assert(st->demotedBycatch == Bitboard(0));
    assert(st->blastPromotedSquares == Bitboard(0));
    assert(!st->captured);
    assert(st->captureSquare == SQ_NONE);
    assert(!st->dead);
    assert(st->promotionPawn == NO_PIECE);
    assert(st->consumedPromotionHandPiece == NO_PIECE);
    assert(st->flippedPieces == Bitboard(0));
    assert(st->claimedSquares == Bitboard(0));
    assert(st->dropHandColor == COLOR_NB);
    assert(st->forcedJumpSquare == SQ_NONE);
    assert(st->forcedJumpStep == 0);
    assert(st->removedGatingType == NO_PIECE_TYPE);
    assert(st->removedCastlingGatingType == NO_PIECE_TYPE);
    assert(st->capturedGatingType == NO_PIECE_TYPE);
    assert(!st->transforms.morphedFrom);
    assert(st->transforms.morphSquare == SQ_NONE);
    assert(!st->transforms.colorChanged);
    assert(st->transforms.colorChangeSquare == SQ_NONE);
    assert(st->pushTailSquare == SQ_NONE);
    assert(st->pushStepF == 0);
    assert(st->pushStepR == 0);
    assert(st->pushCount == 0);
    assert(st->pushSnapshotCount == 0);
    assert(st->pushTransferCount == 0);
    assert(st->pullFromSquare == SQ_NONE);
    assert(!st->pulled);
    assert(!st->suppressedCaptureTransfer);
    assert(!st->pass);
    assert(!st->pendingClaimPass);
    assert(!st->forcedJumpHasFollowup);
    assert(!st->didPush);
    assert(!st->didPull);
    assert(!st->pushStepwise);
    assert(!st->pushEjected);
    assert(!st->pushBlockedCapture);
}

static void test_blast_center_pawn_promotion_updates_pawn_key() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("blast-center-pawn-promotion"),
            "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", false, &st, nullptr);

    const Move m = make<NORMAL>(SQ_E2, SQ_E3);
    assert(pos.legal(m));

    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.piece_on(SQ_E3) == W_QUEEN);
    assert_recomputed_state_matches(pos);
}

static void test_clear_dirty_piece_clears_unused_slots() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"), "startpos", false, &st, nullptr);

    StateInfo next{};
    std::memset(&next.dirtyPiece, 0x7f, sizeof(next.dirtyPiece));

    const Move m = make<NORMAL>(SQ_E2, SQ_E4);
    assert(pos.legal(m));
    pos.do_move(m, next);

    assert(pos.state()->dirtyPiece.dirty_num <= DIRTY_PIECE_MAX);
    for (int i = pos.state()->dirtyPiece.dirty_num; i < DIRTY_PIECE_MAX; ++i)
    {
        assert(pos.state()->dirtyPiece.piece[i] == NO_PIECE);
        assert(pos.state()->dirtyPiece.handPiece[i] == NO_PIECE);
        assert(pos.state()->dirtyPiece.handCount[i] == 0);
        assert(pos.state()->dirtyPiece.from[i] == SQ_NONE);
        assert(pos.state()->dirtyPiece.to[i] == SQ_NONE);
    }
}

static void poison_undo_payload(StateInfo& st) {
    st.bycatchSquares = square_bb(SQ_A1);
    st.promotedBycatch = square_bb(SQ_A1);
    st.demotedBycatch = square_bb(SQ_A1);
    st.blastPromotedSquares = square_bb(SQ_A1);
    st.captured.set(W_PAWN, false);
    st.captureSquare = SQ_A1;
    st.dead.set(B_PAWN, false);
    st.promotionPawn = W_PAWN;
    st.consumedPromotionHandPiece = W_KNIGHT;
    st.flippedPieces = square_bb(SQ_A1);
    st.claimedSquares = square_bb(SQ_A1);
    st.dropHandColor = WHITE;
    st.forcedJumpSquare = SQ_A1;
    st.forcedJumpStep = 1;
    st.removedGatingType = PAWN;
    st.removedCastlingGatingType = PAWN;
    st.capturedGatingType = PAWN;
    st.transforms.morphedFrom.set(W_PAWN, false);
    st.transforms.morphSquare = SQ_A1;
    st.transforms.colorChanged.set(B_PAWN, false);
    st.transforms.colorChangeSquare = SQ_A1;
    st.pushTailSquare = SQ_A1;
    st.pushStepF = 1;
    st.pushStepR = 1;
    st.pushCount = 1;
    st.pushSnapshotCount = 1;
    st.pushTransferCount = 1;
    st.pullFromSquare = SQ_A1;
    st.pulled.set(W_PAWN, false);
    st.suppressedCaptureTransfer = true;
    st.pass = true;
    st.pendingClaimPass = true;
    st.forcedJumpHasFollowup = true;
    st.didPush = true;
    st.didPull = true;
    st.pushStepwise = true;
    st.pushEjected = true;
    st.pushBlockedCapture = true;
}

static void test_null_move_clears_undo_payload() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("chess"), "startpos", false, &st, nullptr);

    const std::string before = pos.fen();
    StateInfo next{};
    poison_undo_payload(next);
    pos.do_null_move(next);

    assert(pos.state()->move == MOVE_NULL);
    assert_no_undo_payload(pos.state());
    assert_recomputed_state_matches(pos);

    pos.undo_null_move();
    assert(pos.fen() == before);
}

int main() {
    init_engine();
    test_blast_center_pawn_promotion_updates_pawn_key();
    test_clear_dirty_piece_clears_unused_slots();
    test_null_move_clears_undo_payload();
    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

if (( ${#OBJ_FILES[@]} == 0 )); then
  echo "no src/*.o objects found; build ${ENGINE} before running this test" >&2
  exit 1
fi

(
  cd "${ROOT_DIR}/src"
  g++ -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
)

echo "StateInfo regression test started"
"${HARNESS_BIN}"
echo "StateInfo regression test passed"
