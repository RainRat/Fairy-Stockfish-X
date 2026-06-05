#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source "${SCRIPT_DIR}/lib/uci.sh"

init_test_env "${1:-}" "${2:-}" "StateInfo regression test"

ENGINE_BASENAME=$(basename "${ENGINE}")
CXX=${CXX:-g++}
JOBS=${JOBS:-2}
CXX_DEFS=()
case "${ENGINE_BASENAME}" in
  stockfish)
    make -C "${ROOT_DIR}/src" EXE=stockfish objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 EXE=stockfish
    ;;
  stockfish-allvars*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    make -C "${ROOT_DIR}/src" EXE=stockfish-allvars objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes nnue=yes EXE=stockfish-allvars
    ;;
  stockfish-large*)
    CXX_DEFS+=(-DLARGEBOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    make -C "${ROOT_DIR}/src" EXE=stockfish-large objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes all=yes EXE=stockfish-large
    ;;
  stockfish-vlb*)
    CXX_DEFS+=(-DLARGEBOARDS -DVERY_LARGE_BOARDS -DALLVARS -DNNUE_EMBEDDING_OFF)
    make -C "${ROOT_DIR}/src" EXE=stockfish-vlb objclean
    make -C "${ROOT_DIR}/src" -j"${JOBS}" build ARCH=x86-64 largeboards=yes verylargeboards=yes all=yes nnue=yes EXE=stockfish-vlb
    ;;
esac

if [[ ${#CXX_DEFS[@]} -eq 0 && -f "${ROOT_DIR}/src/position.o" ]]; then
  POSITION_O_SIG="$(nm -C "${ROOT_DIR}/src/position.o" 2>/dev/null || true)"
  if ! grep -q 'Position::fen(bool, bool, int, .*unsigned long) const' <<<"${POSITION_O_SIG}"; then
    CXX_DEFS+=(-DLARGEBOARDS -DPRECOMPUTED_MAGICS -DALLVARS -DNNUE_EMBEDDING_OFF)
  fi
fi

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

int main() {
    init_test_engine();
    load_variants();
    test_blast_center_pawn_promotion_updates_pawn_key();
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
  "${CXX}" -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" -I"${ROOT_DIR}/tests/lib" "${CXX_DEFS[@]}" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
)

echo "StateInfo regression test started"
"${HARNESS_BIN}"
echo "StateInfo regression test passed"
