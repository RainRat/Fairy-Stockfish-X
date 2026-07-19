#pragma once

#include "bitboard.h"
#include "endgame.h"
#include "piece.h"
#include "position.h"
#include "psqt.h"
#include "uci.h"
#include "variant.h"

namespace Stockfish {

inline void init_test_engine() {
  pieceMap.init();
  variants.init();
  UCI::init(Options);
  PSQT::init(variants.get("fairy"));
  Bitboards::init();
  Position::init();
  Bitbases::init();
  Endgames::init();
}

} // namespace Stockfish
