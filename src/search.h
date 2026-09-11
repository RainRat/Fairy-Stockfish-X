/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <vector>

#ifdef ENABLE_COMPOUND_TURNS
#include "compound_turn.h"
#endif
#include "misc.h"
#include "movepick.h"
#include "types.h"

namespace Stockfish {

class Position;

namespace Search {

/// Threshold used for countermoves based pruning
constexpr int CounterMovePruneThreshold = 0;


/// Stack struct keeps track of the information we need to remember from nodes
/// shallower and deeper in the tree during the search. Each search thread has
/// its own array of Stack objects, indexed by the current ply.

struct Stack {
  Move* pv;
#ifdef ENABLE_COMPOUND_TURNS
  LogicalMove* logicalPv;
  bool pvIsLogical;
#endif
  PieceToHistory* continuationHistory;
  int ply;
  Move currentMove;
  Piece currentMovePiece;
#ifdef ENABLE_COMPOUND_TURNS
  bool currentMoveHistoryCompatible;
  bool currentMoveCapturedOpponent;
#endif
  Move excludedMove;
  Move killers[2];
  Value staticEval;
  int statScore;
  int moveCount;
  bool inCheck;
  bool ttPv;
  bool ttHit;
  int doubleExtensions;

  template<bool Logical>
  auto pv_ptr() const {
#ifdef ENABLE_COMPOUND_TURNS
    if constexpr (Logical)
        return logicalPv;
    else
#endif
        return pv;
  }

  template<bool Logical>
  void set_pv(std::conditional_t<Logical, LogicalMove*, Move*> value) {
#ifdef ENABLE_COMPOUND_TURNS
    if constexpr (Logical)
        logicalPv = value;
    else
#endif
        pv = value;
  }
};


/// RootMove struct is used for moves at the root of the tree. For each root move
/// we store a score and a PV (really a refutation in the case of moves which
/// fail low). Score is normally set at -VALUE_INFINITE for all non-pv moves.

struct RootMove {

#ifdef ENABLE_COMPOUND_TURNS
  explicit RootMove(LogicalMove m, LogicalMoveInfo i = {}) : rootMove(m), info(i) {}
#else
  explicit RootMove(LogicalMove m) : rootMove(m) {}
#endif
  explicit RootMove(Move m) : RootMove(LogicalMove(m)) {}
  bool extract_ponder_from_tt(Position& pos);
  bool operator==(const LogicalMove& m) const { return rootMove == m; }
  bool operator==(const Move& m) const { return rootMove == m; }
  bool operator<(const RootMove& m) const { // Sort in descending order
    return m.score != score ? m.score < score
                            : m.previousScore < previousScore;
  }

  const LogicalMove& first() const { return rootMove; }
  LogicalMove& first() { return rootMove; }
  size_t pv_size() const { return pv.size() + 1; }
  const LogicalMove& pv_at(size_t index) const {
    return index ? pv[index - 1] : rootMove;
  }
  LogicalMove& pv_at(size_t index) {
    return index ? pv[index - 1] : rootMove;
  }
  void clear_pv() { pv.clear(); }
  void append_pv(LogicalMove m) { pv.push_back(m); }
  const std::vector<LogicalMove>& continuation() const { return pv; }
#ifdef ENABLE_COMPOUND_TURNS
  const LogicalMoveInfo& move_info() const { return info; }
#endif
  void set_pv(const std::vector<LogicalMove>& line) {
    if (line.empty())
    {
        rootMove = LogicalMove();
        pv.clear();
        return;
    }
    rootMove = line.front();
    pv.assign(line.begin() + 1, line.end());
  }

  Value score = -VALUE_INFINITE;
  Value previousScore = -VALUE_INFINITE;
  int selDepth = 0;
  int tbRank = 0;
  Value tbScore = VALUE_ZERO;
  LogicalMove rootMove;
#ifdef ENABLE_COMPOUND_TURNS
  LogicalMoveInfo info;
#endif
  std::vector<LogicalMove> pv; // Continuation after rootMove
};

typedef std::vector<RootMove> RootMoves;


/// LimitsType struct stores information sent by GUI about available time to
/// search the current move, maximum depth/time, or if we are in analysis mode.

struct LimitsType {

  LimitsType() { // Init explicitly due to broken value-initialization of non POD in MSVC
    time[WHITE] = time[BLACK] = inc[WHITE] = inc[BLACK] = npmsec = movetime = startTime = TimePoint(0);
    movestogo = depth = mate = perft = infinite = 0;
    nodes = 0;
  }

  bool use_time_management() const {
    return time[WHITE] || time[BLACK];
  }

  std::vector<LogicalMove> searchmoves, banmoves;
  bool searchMovesSpecified = false;
  TimePoint time[COLOR_NB], inc[COLOR_NB], npmsec, movetime, startTime;
  int movestogo, depth, mate, perft, infinite;
  int64_t nodes;
};

extern LimitsType Limits;

void init();
void clear();

} // namespace Search

} // namespace Stockfish

#endif // #ifndef SEARCH_H_INCLUDED
