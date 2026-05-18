#!/usr/bin/env bash

set -euo pipefail

error() {
  echo "quiet-check-special-moves regression failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
ENGINE=${1:-./stockfish}
ENGINE_BASENAME=$(basename "${ENGINE}")
CXX_DEFS=()
case "${ENGINE_BASENAME}" in
  stockfish-allvars*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-large*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
  stockfish-vlb*)
    CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    ;;
esac

case "${ENGINE_BASENAME}" in
  stockfish)
    # The harness links against the object files in src/. Rebuild the standard
    # object set so the test is independent of any previous largeboard build.
    make -C "${ROOT_DIR}/src" EXE=stockfish objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 EXE=stockfish
    ;;
  stockfish-allvars*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-allvars objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars
    ;;
  stockfish-large*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-large objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large
    ;;
  stockfish-vlb*)
    make -C "${ROOT_DIR}/src" EXE=stockfish-vlb objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
    ;;
esac

HARNESS_CPP=$(mktemp /tmp/quiet-check-special-moves-XXXXXX.cpp)
trap 'rm -f "${HARNESS_CPP}"' EXIT

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <cstring>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "movegen.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "variant.h"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[pairdrop:fairy]
pieceDrops = true
symmetricDropTypes = r
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

[wrapped-quiet-check:chess]
cylindrical = true
castling = false
checking = false
startFen = 2k5/8/8/8/8/7p/P7/4K3 w - - 0 1
)ini");
    variants.parse_istream<false>(ss);
}

static void init_engine() {
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
    load_variants();
}

static void test_swap_basic() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("swap-basic"), "5/5/2Ab1/5/5 w - - 0 1", false, &st, nullptr);
    const Move m = make<SWAP>(SQ_C3, SQ_D3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

static void test_wrapped_quiet_check() {
    StateInfo st{};
    Position pos;
    pos.set(variants.get("wrapped-quiet-check"), "2k5/8/8/8/8/7p/P7/4K3 w - - 0 1", false, &st, nullptr);
    const Move m = make<NORMAL>(SQ_A2, SQ_A3);
    assert(!pos.gives_check(m));
    assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
}

int main(int argc, char** argv) {
    init_engine();

    auto run_case = [&](const char* which) {
        if (!which || !std::strcmp(which, "swap"))
            test_swap_basic();
        if (!which || !std::strcmp(which, "wrapped"))
            test_wrapped_quiet_check();
    };

    if (argc > 1)
        run_case(argv[1]);
    else
        run_case(nullptr);

    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

run_case() {
  local which="$1"
  local tmp_bin
  tmp_bin=$(mktemp /tmp/quiet-check-special-moves-XXXXXX)
  (
    cd "${ROOT_DIR}/src"
    "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${tmp_bin}"
    "${tmp_bin}" "${which}"
  )
  rm -f "${tmp_bin}"
}

run_case swap
run_case pull
run_case wrapped

echo "quiet-check-special-moves ok"
