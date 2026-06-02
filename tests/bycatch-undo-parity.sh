#!/usr/bin/env bash
set -euo pipefail

error() {
  echo "bycatch undo parity test failed on line $1" >&2
  exit 1
}
trap 'error ${LINENO}' ERR

ENGINE=${1:-./stockfish}
DEFAULT_VARIANT_PATH="variants.ini"
if [[ ! -f "${DEFAULT_VARIANT_PATH}" && -f "src/variants.ini" ]]; then
  DEFAULT_VARIANT_PATH="src/variants.ini"
fi
VARIANT_PATH=${2:-${DEFAULT_VARIANT_PATH}}
SCRIPT_DIR=$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd -- "${SCRIPT_DIR}/.." && pwd)

BUILD_DIR="${ROOT_DIR}/.local/build/bycatch-undo-parity"
mkdir -p "${BUILD_DIR}"

HARNESS_CPP="${BUILD_DIR}/bycatch-undo-parity.cpp"
HARNESS_BIN="${BUILD_DIR}/bycatch-undo-parity.bin"

cat > "${HARNESS_CPP}" <<'EOF'
#include <cassert>
#include <sstream>

#include "bitboard.h"
#include "endgame.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "uci.h"
#include "variant.h"

using namespace Stockfish;

static void load_variants() {
    std::istringstream ss(R"ini(
[surround-color:chess]
castling = false
surroundCaptureIntervene = true
changingColorTrigger = capture
changingColorPieceTypes = *
startFen = 4k3/8/8/8/8/3p1p2/4R3/4K3 w - - 0 1
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

int main() {
    init_engine();

    StateInfo st{};
    Position pos;
    pos.set(variants.get("surround-color"), "4k3/8/8/8/8/3p1p2/4R3/4K3 w - - 0 1", false, &st, nullptr);

    const Move m = make<NORMAL>(SQ_E2, SQ_E3);
    assert(pos.legal(m));

    const std::string before = pos.fen();
    StateInfo next{};
    pos.do_move(m, next);
    assert(pos.pos_is_ok());

    pos.undo_move(m);
    assert(pos.pos_is_ok());
    assert(pos.fen() == before);
    return 0;
}
EOF

OBJ_FILES=()
while IFS= read -r -d '' obj; do
  OBJ_FILES+=("${obj}")
done < <(find "${ROOT_DIR}/src" -maxdepth 1 -name '*.o' ! -name 'main.o' -print0 | sort -z)

(
  cd "${ROOT_DIR}/src"
  g++ -std=c++17 -O2 -Wall -Wextra -flto -I"${ROOT_DIR}/src" "${HARNESS_CPP}" "${OBJ_FILES[@]}" -pthread -o "${HARNESS_BIN}"
)

echo "bycatch undo parity test started"
"${HARNESS_BIN}"
echo "bycatch undo parity test passed"
