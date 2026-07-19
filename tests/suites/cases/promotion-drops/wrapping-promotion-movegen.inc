#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "wrapping promotion movegen test failed on line $1" >&2
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
fsx_harness_prepare_objects "${JOBS}"

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/wrapping-promotion-movegen"
HARNESS_CPP="${BUILD_SIG_DIR}/wrapping-promotion-movegen.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/wrapping-promotion-movegen.bin"
HARNESS_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.harness.sig"
mkdir -p "${BUILD_SIG_DIR}"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
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

static void expect_promotion_partition(const Position& pos, const char* label) {
    const auto captures = MoveList<CAPTURES>(pos);
    const auto quiets = MoveList<QUIETS>(pos);
    const auto all = MoveList<NON_EVASIONS>(pos);
    std::set<Move> seen;
    for (const auto& extMove : all)
    {
        Move move = extMove;
        expect(seen.insert(move).second, label);
        if (!is_promotion_move(move) && type_of(move) != PIECE_PROMOTION)
            continue;
        bool inCaptures = captures.contains(move);
        bool inQuiets = quiets.contains(move);
        expect(inCaptures != inQuiets, label);
    }
}

static void load_test_variants() {
    static bool loaded = false;
    if (loaded)
        return;

    std::istringstream in(R"INI(
[promo-normal:chess]
castling = false
promotionPieceTypes = q r b n
startFen = 4k3/P7/8/8/8/8/8/4K3 w - - 0 1

[promo-wrap:chess]
toroidal = true
castling = false
promotionPieceTypes = q r b n
startFen = 4k3/P7/8/8/8/8/8/4K3 w - - 0 1

[promo-mapped:chess]
castling = false
promotionPieceTypes = q r b n
promotedPieceType = p:q
startFen = 4k3/P7/8/8/8/8/8/4K3 w - - 0 1
)INI");
    variants.parse_istream<false>(in);
    loaded = true;
}

static void test_wrapped_promotion_partition() {
    StateInfo st{};
    const Move quietPromo = make<PROMOTION>(SQ_A7, SQ_A8, QUEEN);

    Position normalPos;
    normalPos.set(variants.get("promo-normal"), "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", false, &st, nullptr);
    const auto normalNonEvasions = MoveList<NON_EVASIONS>(normalPos);
    const auto normalQuiets = MoveList<QUIETS>(normalPos);
    expect(normalNonEvasions.contains(quietPromo), "normal NON_EVASIONS should include quiet promotion");
    expect(normalQuiets.contains(quietPromo), "normal QUIETS should include quiet promotion");

    Position wrappedPos;
    wrappedPos.set(variants.get("promo-wrap"), "4k3/P7/8/8/8/8/8/4K3 w - - 0 1", false, &st, nullptr);
    const auto wrappedNonEvasions = MoveList<NON_EVASIONS>(wrappedPos);
    const auto wrappedQuiets = MoveList<QUIETS>(wrappedPos);
    expect(wrappedNonEvasions.contains(quietPromo), "wrapped NON_EVASIONS should include quiet promotion");
    expect(wrappedQuiets.contains(quietPromo), "wrapped QUIETS should include quiet promotion");

    expect_promotion_partition(normalPos, "normal promotion appeared in both search buckets");
    expect_promotion_partition(wrappedPos, "wrapped promotion appeared in both search buckets");

    StateInfo captureState{};
    Position capturePos;
    capturePos.set(variants.get("promo-normal"), "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1", false,
                   &captureState, nullptr);
    expect_promotion_partition(capturePos, "capture promotion appeared in both search buckets");

    StateInfo mappedState{};
    Position mappedPos;
    mappedPos.set(variants.get("promo-mapped"), "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1", false,
                  &mappedState, nullptr);
    expect_promotion_partition(mappedPos, "mapped promotion appeared in both search buckets");
}

int main() {
    init_engine();
    load_test_variants();
    test_wrapped_promotion_partition();
    return 0;
}
EOF

fsx_harness_collect_objects
fsx_harness_build "${HARNESS_CPP}" "${HARNESS_BIN}" \
  "wrapping-promotion-movegen" "${HARNESS_SIG_FILE}"

"${HARNESS_BIN}"

echo "wrapping promotion movegen test passed"
