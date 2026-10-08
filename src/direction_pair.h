/* Direction-pair configuration for two-leg moves (parser/API/Variant only). */
#ifndef DIRECTION_PAIR_H_INCLUDED
#define DIRECTION_PAIR_H_INCLUDED

#include "types.h"

namespace Stockfish {

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

#endif // DIRECTION_PAIR_H_INCLUDED
