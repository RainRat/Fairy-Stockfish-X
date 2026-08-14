/*
  Fairy-Stockfish-X Arimaa turn support
*/

#ifndef ARIMAA_H_INCLUDED
#define ARIMAA_H_INCLUDED

#ifdef ENABLE_ARIMAA

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace Stockfish {

class Position;
class Thread;
struct StateInfo;

/// A complete Arimaa turn made from one to four ordinary FSX step moves.
/// The steps are ordered and are interpreted against successive positions.
struct ArimaaTurn {
  static constexpr int MAX_STEPS = 4;

  std::array<Move, MAX_STEPS> steps{};
  uint8_t length = 0;

  bool operator==(const ArimaaTurn& other) const {
      return length == other.length && std::equal(steps.begin(), steps.begin() + length,
                                                   other.steps.begin());
  }
};

/// Materialize complete turns from an Arimaa turn-boundary position.
/// Intermediate positions are used only while traversing the legal tree.
std::vector<ArimaaTurn> generate_arimaa_turns(Position& pos);
bool parse_arimaa_turn(Position& pos, const std::string& text, ArimaaTurn& turn);

/// Apply and undo one complete turn. The caller supplies at least five
/// suitably aligned StateInfo objects; the fifth is used only when the turn
/// ends before the configured step maximum.
void do_arimaa_turn(Position& pos, const ArimaaTurn& turn, StateInfo* states);
void undo_arimaa_turn(Position& pos, const ArimaaTurn& turn);

uint64_t arimaa_perft(Position& pos, int depth, bool root);
std::string arimaa_turn_to_string(Position& pos, const ArimaaTurn& turn);
void search_arimaa(Thread& thread);

} // namespace Stockfish

#endif // ENABLE_ARIMAA

#endif // #ifndef ARIMAA_H_INCLUDED
