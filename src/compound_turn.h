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
struct StateInfo;
struct LogicalMoveState;

/// Materialize complete compound moves from a turn-boundary position.
/// Intermediate positions are used only while traversing the legal tree.
std::vector<LogicalMove> generate_compound_moves(Position& pos);
bool has_any_compound_move(Position& pos);
bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn);

/// Apply and undo one complete logical move. Component state is transaction
/// scratch; only the supplied StateInfo enters the persistent history chain.
void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state,
                      LogicalMoveState& transaction);
void undo_compound_move(Position& pos, const LogicalMove& turn,
                        LogicalMoveState& transaction);

uint64_t compound_perft(Position& pos, int depth, bool root);
std::string compound_move_to_string(Position& pos, const LogicalMove& turn);

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // #ifndef COMPOUND_TURN_H_INCLUDED
