/*
  Fairy-Stockfish-X compound turn support
*/

#ifndef COMPOUND_TURN_H_INCLUDED
#define COMPOUND_TURN_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

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

/// A complete compound move made from one to four ordinary FSX step moves.
/// The steps are ordered and are interpreted against successive positions.
struct CompoundMove {
  static constexpr int MAX_STEPS = 4;

  std::array<Move, MAX_STEPS> steps{};
  uint8_t length = 0;

  bool operator==(const CompoundMove& other) const {
      return length == other.length
          && std::equal(steps.begin(), steps.begin() + length, other.steps.begin());
  }

  bool operator!=(const CompoundMove& other) const {
      return !(*this == other);
  }
};

using CompoundTurn = CompoundMove;

/// Materialize complete compound moves from a turn-boundary position.
/// Intermediate positions are used only while traversing the legal tree.
std::vector<CompoundMove> generate_compound_moves(Position& pos);
bool has_any_compound_move(Position& pos);
bool parse_compound_move(Position& pos, const std::string& text, CompoundMove& turn);

/// Apply and undo one complete compound move. The caller supplies at least five
/// suitably aligned StateInfo objects; the fifth is used only when the turn
/// ends before the configured step maximum.
void do_compound_move(Position& pos, const CompoundMove& turn, StateInfo* states);
void undo_compound_move(Position& pos, const CompoundMove& turn);

uint64_t compound_perft(Position& pos, int depth, bool root);
std::string compound_move_to_string(Position& pos, const CompoundMove& turn);
void search_compound(Thread& thread);

std::vector<CompoundMove> parse_root_compound_moves(Position& pos, const std::vector<std::string>& texts);
bool root_compound_move_allowed(const CompoundMove& turn,
                                const std::vector<CompoundMove>& searchMoves,
                                bool searchMovesSpecified,
                                const std::vector<CompoundMove>& banMoves);

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // #ifndef COMPOUND_TURN_H_INCLUDED
