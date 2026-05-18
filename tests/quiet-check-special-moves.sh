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

TMP_CPP=""
TMP_BIN=""
trap 'rm -f "${TMP_CPP}" "${TMP_BIN}"' EXIT

TMP_CPP=$(mktemp /tmp/quiet-check-special-moves-XXXXXX.cpp)
TMP_BIN=$(mktemp /tmp/quiet-check-special-moves-XXXXXX)

cat > "${TMP_CPP}" <<'EOF'
#include <cassert>
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
)ini");
    variants.parse_istream<false>(ss);
}

int main() {
    pieceMap.init();
    variants.init();
    PSQT::init(variants.get("fairy"));
    Bitboards::init();
    Position::init();
    Bitbases::init();
    Endgames::init();
    load_variants();

    {
        StateInfo st{};
        Position pos;
        pos.set(variants.get("pairdrop"), "4k3/8/8/8/8/8/8/4K3[RR] w - - 0 1", false, &st, nullptr);
        const Move m = make_drop_pair(SQ_A4, SQ_H4, ROOK, ROOK);
        assert(!pos.gives_check(m));
        assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
    }

    {
        StateInfo st{};
        Position pos;
        pos.set(variants.get("pull-basic"), "5/5/2b2/2A2/5 w - - 0 1", false, &st, nullptr);
        const Move m = make_pull(SQ_C2, SQ_D2, SQ_C3);
        assert(!pos.gives_check(m));
        assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
    }

    {
        StateInfo st{};
        Position pos;
        pos.set(variants.get("swap-basic"), "5/5/2Ab1/5/5 w - - 0 1", false, &st, nullptr);
        const Move m = make<SWAP>(SQ_C3, SQ_D3);
        assert(!pos.gives_check(m));
        assert(!MoveList<QUIET_CHECKS>(pos).contains(m));
    }

    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

"${CXX}" -std=c++17 -O2 -Wall -Wextra -I"${ROOT_DIR}/src" "${TMP_CPP}" "${OBJ_FILES[@]}" -pthread -o "${TMP_BIN}"
"${TMP_BIN}"

echo "quiet-check-special-moves ok"
