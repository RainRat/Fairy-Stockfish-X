/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/

#ifndef MULTILEG_MOVE_H_INCLUDED
#define MULTILEG_MOVE_H_INCLUDED

#include "types.h"

namespace Stockfish {

// Kept as a source-compatible name for existing move-encoding tests/tools.
constexpr uint64_t MultiLegFlag = ExtendedSpecialFlag;

enum MultiLegSubtype : int {
    MULTILEG_SUBTYPE_NONE = 0,
    MULTILEG_SUBTYPE_TWO_STEP = 1,
    MULTILEG_SUBTYPE_TWO_STEP_PROMOTION = 2,
    MULTILEG_SUBTYPE_HOOK = 3,
    MULTILEG_SUBTYPE_HOOK_PROMOTION = 4,
};

inline MultiLegSubtype multileg_subtype(Move m) {
    if (!has_extended_special_payload(m))
        return MULTILEG_SUBTYPE_NONE;
    int sub = (move_bits(m) >> (2 * SQUARE_BITS + MOVE_TYPE_BITS)) & (PIECE_TYPE_NB - 1);
    return sub >= MULTILEG_SUBTYPE_TWO_STEP && sub <= MULTILEG_SUBTYPE_HOOK_PROMOTION
         ? MultiLegSubtype(sub) : MULTILEG_SUBTYPE_NONE;
}

inline bool is_two_step(Move m) {
    MultiLegSubtype sub = multileg_subtype(m);
    return sub == MULTILEG_SUBTYPE_TWO_STEP || sub == MULTILEG_SUBTYPE_TWO_STEP_PROMOTION;
}

inline bool two_step_promotes(Move m) {
    return multileg_subtype(m) == MULTILEG_SUBTYPE_TWO_STEP_PROMOTION;
}

inline bool is_hook(Move m) {
    MultiLegSubtype sub = multileg_subtype(m);
    return sub == MULTILEG_SUBTYPE_HOOK || sub == MULTILEG_SUBTYPE_HOOK_PROMOTION;
}

inline bool hook_promotes(Move m) {
    return multileg_subtype(m) == MULTILEG_SUBTYPE_HOOK_PROMOTION;
}

inline bool is_multileg(Move m) { return is_two_step(m) || is_hook(m); }
inline bool is_multileg_promotion(Move m) { return two_step_promotes(m) || hook_promotes(m); }
inline bool is_any_promotion(Move m) {
    return is_promotion_move(m) || type_of(m) == PIECE_PROMOTION || is_multileg_promotion(m);
}

inline Square via_sq(Move m) {
    assert(is_multileg(m));
    return special_payload_square(m);
}

constexpr Move make_two_step(Square from, Square via, Square to, bool promotes = false) {
    return Move(
        ExtendedSpecialFlag
      + (static_cast<uint64_t>(via) << (2 * SQUARE_BITS + MOVE_TYPE_BITS + PIECE_TYPE_BITS))
      + (static_cast<uint64_t>(promotes ? MULTILEG_SUBTYPE_TWO_STEP_PROMOTION : MULTILEG_SUBTYPE_TWO_STEP) << (2 * SQUARE_BITS + MOVE_TYPE_BITS))
      + static_cast<uint64_t>(SPECIAL)
      + (static_cast<uint64_t>(from) << SQUARE_BITS)
      + static_cast<uint64_t>(to)
    );
}

constexpr Move make_hook(Square from, Square via, Square to, bool promotes = false) {
    return Move(
        ExtendedSpecialFlag
      + (static_cast<uint64_t>(via) << (2 * SQUARE_BITS + MOVE_TYPE_BITS + PIECE_TYPE_BITS))
      + (static_cast<uint64_t>(promotes ? MULTILEG_SUBTYPE_HOOK_PROMOTION : MULTILEG_SUBTYPE_HOOK) << (2 * SQUARE_BITS + MOVE_TYPE_BITS))
      + static_cast<uint64_t>(SPECIAL)
      + (static_cast<uint64_t>(from) << SQUARE_BITS)
      + static_cast<uint64_t>(to)
    );
}
static_assert(int(MULTILEG_SUBTYPE_HOOK_PROMOTION) < int(PIECE_TYPE_NB),
              "Hook subtypes must fit the SPECIAL subtype field");


} // namespace Stockfish

#endif // MULTILEG_MOVE_H_INCLUDED
