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
class Thread;
struct StateInfo;
struct LogicalMoveState;

/// Lazily yield complete legal logical moves without materializing the turn tree.
/// The component recursion and its temporary position changes stay private to
/// this provider; callers always observe the position at a turn boundary.
class LogicalMoveSource {
 public:
  LogicalMoveSource(Position& pos, Thread* thread, LogicalMoveState& transaction,
                    bool checkGameEnd = true, Move preferredMove = MOVE_NONE,
                    const LogicalMove* preferredTurn = nullptr);
  ~LogicalMoveSource();

  LogicalMoveSource(const LogicalMoveSource&) = delete;
  LogicalMoveSource& operator=(const LogicalMoveSource&) = delete;

  bool next(LogicalMove& move);
  bool next(LogicalMove& move, LogicalMoveInfo& info);

 private:
  bool next_impl(LogicalMove& move, LogicalMoveInfo* info);

  struct Frame {
      size_t current = 0;
      int usedCost = 0;
  };

  void initialize_frame(int depth);
  void apply_path(int length);
  void unwind_prefix();

  Position& pos;
  Thread* thread;
  LogicalMoveState& transaction;
  StateInfo* logicalRoot = nullptr;
  Move preferredMove = MOVE_NONE;
  LogicalMove preferredTurn;
  std::array<Frame, LogicalMove::MAX_COMPONENTS> frames{};
  LogicalMove turn;
  Key startBoundaryKey = 0;
  int depth = 0;
  bool initialized = false;
  bool descend = false;
  bool prefixApplied = false;
  bool finished = false;
  Piece firstMovedPiece = NO_PIECE;
  bool firstSeeReliable = false;
  bool firstGivesCheck = false;
};

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
std::vector<std::string> compound_pv_to_strings(const Position& pos,
                                                const LogicalMove& first,
                                                const std::vector<LogicalMove>& continuation);

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS

#endif // #ifndef COMPOUND_TURN_H_INCLUDED
