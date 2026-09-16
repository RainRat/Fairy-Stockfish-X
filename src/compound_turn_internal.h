/*
  Fairy-Stockfish-X compound turn support
*/

#ifndef COMPOUND_TURN_INTERNAL_H_INCLUDED
#define COMPOUND_TURN_INTERNAL_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

#include <array>
#include <memory>
#include <vector>

#include "compound_turn.h"
#include "position.h"

namespace Stockfish {

class CompoundTurnBuilder {
 public:
  static void push_back(LogicalMove& move, Move component) { move.push_back(component); }
  static void pop_back(LogicalMove& move) { move.pop_back(); }
  static void set_component(LogicalMove& move, int index, Move component) {
      move.set_component(index, component);
  }
};

/// Scratch owned by the compound-turn provider, not by Position's state API.
struct LogicalMoveUndo {
  alignas(Eval::NNUE::CacheLineSize)
  std::array<StateInfo, LogicalMove::MAX_COMPONENTS> components;
  std::array<bool, LogicalMove::MAX_COMPONENTS> previousTurnReset{};
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
                           bool& previousTurnReset, bool countNode = true);
  static void undo_component(Position& pos, Move move, bool previousTurnReset);
  static void do_component(Position& pos, Move move, LogicalMoveUndo& transaction,
                           int index, bool countNode = true) {
      do_component(pos, move, transaction.components[index],
                   transaction.previousTurnReset[index], countNode);
  }
  static void undo_component(Position& pos, Move move,
                             const LogicalMoveUndo& transaction, int index) {
      undo_component(pos, move, transaction.previousTurnReset[index]);
  }
};

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // COMPOUND_TURN_INTERNAL_H_INCLUDED
