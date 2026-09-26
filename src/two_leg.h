/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef TWO_LEG_H_INCLUDED
#define TWO_LEG_H_INCLUDED

#include "direction_pair.h"

namespace Stockfish {

enum class TwoLegKind { TWO_STEP, HOOK };

namespace detail {
struct TwoLegPath {
  TwoLegKind kind = TwoLegKind::TWO_STEP;
  Square via = SQ_NONE;
  Square to = SQ_NONE;
  Bitboard captures = 0;
  Bitboard transit = 0;
  bool valid = false;

  bool captures_via() const { return is_ok(via) && via != to && bool(captures & square_bb(via)); }
  bool captures_to(Square from) const { return is_ok(to) && to != from && bool(captures & square_bb(to)); }
  Square primary_capture(Square from) const {
      return captures_to(from) ? to : captures_via() ? via : SQ_NONE;
  }
};
} // namespace detail

} // namespace Stockfish


#endif // TWO_LEG_H_INCLUDED
