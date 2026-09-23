/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef MULTILEG_H_INCLUDED
#define MULTILEG_H_INCLUDED

#include "types.h"

namespace Stockfish {

// Direction-pair masks use king-step order N, NE, E, SE, S, SW, W, NW.
constexpr Direction KingDirections[8] = {
    NORTH, NORTH_EAST, EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST, NORTH_WEST
};

constexpr int reflect_king_direction(int d) { return (d + 4) & 7; }
constexpr uint64_t multileg_direction_pair_bit(int d1, int d2) {
    return uint64_t(1) << (d1 * 8 + d2);
}
constexpr int multileg_pair_first(int pair) { return pair / 8; }
constexpr int multileg_pair_second(int pair) { return pair % 8; }

} // namespace Stockfish

#endif // MULTILEG_H_INCLUDED
