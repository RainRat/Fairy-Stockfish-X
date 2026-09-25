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

#include "types.h"

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

// Direction-pair masks use king-step order N, NE, E, SE, S, SW, W, NW.
constexpr Direction KingDirections[8] = {
    NORTH, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST
};
constexpr const char* KingDirectionNames[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

constexpr uint8_t KingForwardDirections = (1 << 0) | (1 << 1) | (1 << 7);
constexpr uint8_t KingBackwardDirections = (1 << 3) | (1 << 4) | (1 << 5);
constexpr uint8_t KingSidewaysDirections = (1 << 2) | (1 << 6);
constexpr uint8_t KingOrthogonalDirections = (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6);
constexpr uint8_t KingDiagonalDirections = (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7);

constexpr int reflect_king_direction(int d) { return (d + 4) & 7; }
inline const char* king_step_name(int idx) { return KingDirectionNames[idx & 7]; }
constexpr uint64_t two_leg_direction_pair_bit(int d1, int d2) {
    return uint64_t(1) << (d1 * 8 + d2);
}
constexpr int two_leg_pair_first(int pair) { return pair / 8; }
constexpr int two_leg_pair_second(int pair) { return pair % 8; }

inline uint64_t reflect_direction_pairs(uint64_t mask) {
    uint64_t result = 0;
    for (int d1 = 0; d1 < 8; ++d1)
        for (int d2 = 0; d2 < 8; ++d2)
            if (mask & two_leg_direction_pair_bit(d1, d2))
                result |= two_leg_direction_pair_bit(reflect_king_direction(d1), reflect_king_direction(d2));
    return result;
}

} // namespace Stockfish


#endif // TWO_LEG_H_INCLUDED
