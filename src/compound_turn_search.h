/*
  Fairy-Stockfish-X compound turn search support
*/

#ifndef COMPOUND_TURN_SEARCH_H_INCLUDED
#define COMPOUND_TURN_SEARCH_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

#include <array>
#include <cassert>
#include <memory>
#include <string>

#include "compound_turn.h"

namespace Stockfish {

struct LogicalMoveWorkspace;

/// Properties of a completed logical move used by generic search heuristics.
/// A property is false when the provider cannot establish its physical-move
/// semantics for the complete transition.
struct LogicalMoveInfo {
  Move representative = MOVE_NONE;
  Piece movedPiece = NO_PIECE;
  bool capturesOpponent = false;
  bool losesOwnMaterial = false;
  bool removesMaterial = false;
  bool promotionLike = false;
  bool givesCheck = false;
  bool historyCompatible = false;
  bool reductionEligible = true;
  bool seeReliable = false;
};

struct LogicalMoveOrder {
  Move preferredFirst = MOVE_NONE;
  const LogicalMove* preferredTurn = nullptr;
};

/// Lazily yield complete legal logical moves without materializing the turn tree.
/// The component recursion and its temporary position changes stay private to
/// this provider; callers always observe the position at a turn boundary.
class LogicalMoveSource {
 public:
  LogicalMoveSource(Position& pos, LogicalMoveWorkspace& workspace,
                    LogicalMoveOrder order = {}, bool checkGameEnd = true);
  ~LogicalMoveSource();

  LogicalMoveSource(const LogicalMoveSource&) = delete;
  LogicalMoveSource& operator=(const LogicalMoveSource&) = delete;

  bool next(LogicalMove& move);
  bool next(LogicalMove& move, LogicalMoveInfo& info);
  bool next_applied(LogicalMove& move, LogicalMoveInfo& info, StateInfo& committedState,
                    bool captureNotation = false);
  void undo_yielded();
  const std::string& yielded_string() const {
      assert(notation);
      return notation->yielded;
  }

 private:
  bool next_impl(LogicalMove& move, LogicalMoveInfo* info,
                 StateInfo* committedState, bool captureNotation = false);

  struct Frame {
      size_t current = 0;
      int usedCost = 0;
      LogicalMoveInfo info;
  };

  struct Notation {
      std::array<std::string, LogicalMove::MAX_COMPONENTS> components;
      std::string yielded;
  };

  void initialize_frame(int depth);
  void apply_path(int length);
  void unwind_prefix();

  Position& pos;
  LogicalMoveWorkspace* workspace;
  LogicalMoveUndo* transaction;
  std::unique_ptr<LogicalMoveWorkspace> nestedWorkspace;
  bool ownsWorkspace = false;
  StateInfo* logicalRoot = nullptr;
  LogicalMoveOrder order;
  std::array<Frame, LogicalMove::MAX_COMPONENTS> frames{};
  LogicalMove turn;
  Key startBoundaryKey = 0;
  int depth = 0;
  bool initialized = false;
  bool descend = false;
  bool prefixApplied = false;
  bool finished = false;
  bool yielded = false;
  Piece firstMovedPiece = NO_PIECE;
  bool firstSeeReliable = false;
  bool firstGivesCheck = false;
  std::unique_ptr<Notation> notation;
};

bool compound_move_info(Position& pos, const LogicalMove& turn, LogicalMoveInfo& info);

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // COMPOUND_TURN_SEARCH_H_INCLUDED
