#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "wrapping promotion movegen test failed on line $1" >&2
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

BUILD_SIG_DIR="${ROOT_DIR}/.local/build/wrapping-promotion-movegen"
BUILD_SIG_FILE="${BUILD_SIG_DIR}/${ENGINE_BASENAME}.sig"
HARNESS_CPP="${BUILD_SIG_DIR}/wrapping-promotion-movegen.cpp"
HARNESS_BIN="${BUILD_SIG_DIR}/wrapping-promotion-movegen.bin"
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
#include <iostream>
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

static void load_test_variants() {
    static bool loaded = false;
    if (loaded)
        return;

    std::istringstream in(R"INI(
[promo-normal:chess]
castling = false
promotionPieceTypes = q
startFen = 4k3/P7/8/8/8/8/8/4K3 w - - 0 1

[promo-wrap:chess]
toroidal = true
castling = false
promotionPieceTypes = q
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
}

int main() {
    init_engine();
    load_test_variants();
    test_wrapped_promotion_partition();
    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

${CXX} -std=c++17 -O2 -pipe -Wall -Wextra -pedantic \
  -I"${ROOT_DIR}/src" \
  "${HARNESS_CPP}" \
  "${OBJ_FILES[@]}" \
  -o "${HARNESS_BIN}" \
  "${CXX_DEFS[@]}" \
  -lpthread

"${HARNESS_BIN}"

echo "wrapping promotion movegen test passed"
