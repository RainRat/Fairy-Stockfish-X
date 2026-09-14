/*
  Fairy-Stockfish-X compound turn support
*/

#ifndef COMPOUND_TURN_INTERNAL_H_INCLUDED
#define COMPOUND_TURN_INTERNAL_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

#include <array>
#include <memory>
#include <vector>

namespace Stockfish {

/// Scratch owned by the compound-turn provider, not by Position's state API.
struct LogicalMoveUndo {
  alignas(Eval::NNUE::CacheLineSize)
  std::array<StateInfo, LogicalMove::MAX_COMPONENTS> components;
  StateInfo* previous = nullptr;
  int usedCost = 0;
  bool syntheticBoundary = false;
};

struct LogicalMoveWorkspace {
  std::array<std::vector<Move>, LogicalMove::MAX_COMPONENTS> moveLists;
  LogicalMoveUndo undo;
  bool inUse = false;
};

/// The only bridge from the compound provider to Position's component executor.
struct CompoundTurnAdapter {
  static void do_component(Position& pos, Move move, StateInfo& state,
                           bool countNode = true);
  static void undo_component(Position& pos, Move move);
};

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // COMPOUND_TURN_INTERNAL_H_INCLUDED
