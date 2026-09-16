/*
  Fairy-Stockfish-X compound turn support
*/

#ifndef COMPOUND_TURN_H_INCLUDED
#define COMPOUND_TURN_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

#include <cstdint>
#include <string>
#include <vector>

#include "types.h"

namespace Stockfish {

class Position;
struct StateInfo;
struct LogicalMoveUndo;

/// Materialize complete compound moves from a turn-boundary position.
/// Intermediate positions are used only while traversing the legal tree.
std::vector<LogicalMove> generate_compound_moves(Position& pos);
bool has_any_compound_move(Position& pos, bool checkGameEnd = true);
bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn);

/// Apply and undo one complete logical move. Component state is transaction
/// scratch; only the supplied StateInfo enters the persistent history chain.
void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state);
void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state,
                      LogicalMoveUndo& transaction);
void undo_compound_move(Position& pos, const LogicalMove& turn,
                        LogicalMoveUndo& transaction);

uint64_t compound_perft(Position& pos, int depth, bool root);
std::string compound_move_to_string(Position& pos, const LogicalMove& turn);
std::vector<std::string> compound_pv_to_strings(const Position& pos,
                                                const LogicalMove& first,
                                                const std::vector<LogicalMove>& continuation);

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // #ifndef COMPOUND_TURN_H_INCLUDED
