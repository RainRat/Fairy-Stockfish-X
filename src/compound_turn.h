/*
  Fairy-Stockfish-X compound turn support
*/

#ifndef COMPOUND_TURN_H_INCLUDED
#define COMPOUND_TURN_H_INCLUDED

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "movegen.h"
#include "types.h"

namespace Stockfish {

class Position;
struct StateInfo;
struct LogicalMoveUndo;
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

enum class QuiescenceSupport : uint8_t {
  STANDARD,
  STATIC_ONLY
};

/// Search capabilities supplied by the logical-move provider. These describe
/// the assumptions of a search heuristic, rather than how many components a
/// logical move happens to contain.
struct LogicalMoveCapabilities {
  bool futilityPruning = true;
  bool nullMovePruning = true;
  bool probCut = true;
  QuiescenceSupport quiescence = QuiescenceSupport::STANDARD;
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
  bool next_applied(LogicalMove& move, LogicalMoveInfo& info, StateInfo& committedState);
  void undo_yielded();
  const std::string& yielded_string() const { return yieldedString; }

 private:
  bool next_impl(LogicalMove& move, LogicalMoveInfo* info,
                 StateInfo* committedState);
  bool repetition_illegal(Key layoutKey, int pliesFromNull) const;

  struct Frame {
      size_t current = 0;
      int usedCost = 0;
      LogicalMoveInfo info;
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
  std::vector<Key> previousBoundaryLayoutKeys;
  int repetitionLimit = 0;
  std::array<std::string, LogicalMove::MAX_COMPONENTS> componentStrings;
  std::string yieldedString;
};

/// Materialize complete compound moves from a turn-boundary position.
/// Intermediate positions are used only while traversing the legal tree.
std::vector<LogicalMove> generate_compound_moves(Position& pos);
bool has_any_compound_move(Position& pos, bool checkGameEnd = true);
bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn);

/// Apply and undo one complete logical move. Component state is transaction
/// scratch; only the supplied StateInfo enters the persistent history chain.
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
