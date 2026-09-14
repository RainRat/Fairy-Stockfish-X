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

#include <memory>
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
  PieceToHistory* continuationHistory;
  int ply;
  Move currentMove;
  Piece currentMovePiece;
  Move excludedMove;
  Move killers[2];
  Value staticEval;
  int statScore;
  int moveCount;
  bool inCheck;
  bool ttPv;
  bool ttHit;
  int doubleExtensions;

  void set_pv(Move* value) { pv = value; }
};


/// RootMove struct is used for moves at the root of the tree. For each root move
/// we store a score and a PV (really a refutation in the case of moves which
/// fail low). Score is normally set at -VALUE_INFINITE for all non-pv moves.

struct RootMove {

#ifdef ENABLE_COMPOUND_TURNS
  struct LogicalRootData {
      LogicalMove move;
      LogicalMoveInfo info;
      std::vector<LogicalMove> pv;
  };

  explicit RootMove(LogicalMove m, LogicalMoveInfo i = {})
      : rootMove(m.first()), logical(std::make_unique<LogicalRootData>(LogicalRootData{m, i, {}})) {}
#else
  explicit RootMove(LogicalMove m) : rootMove(m.first()) {}
#endif
  explicit RootMove(Move m) : rootMove(m) {}

#ifdef ENABLE_COMPOUND_TURNS
  RootMove(const RootMove& other)
      : score(other.score), previousScore(other.previousScore), selDepth(other.selDepth),
        tbRank(other.tbRank), tbScore(other.tbScore), rootMove(other.rootMove), pv(other.pv) {
      if (other.logical)
          logical = std::make_unique<LogicalRootData>(*other.logical);
  }

  RootMove& operator=(const RootMove& other) {
      if (this == &other)
          return *this;
      score = other.score;
      previousScore = other.previousScore;
      selDepth = other.selDepth;
      tbRank = other.tbRank;
      tbScore = other.tbScore;
      rootMove = other.rootMove;
      pv = other.pv;
      logical = other.logical ? std::make_unique<LogicalRootData>(*other.logical) : nullptr;
      return *this;
  }

  RootMove(RootMove&&) noexcept = default;
  RootMove& operator=(RootMove&&) noexcept = default;
#endif

  bool extract_ponder_from_tt(Position& pos);
#ifdef ENABLE_COMPOUND_TURNS
  bool is_logical() const { return bool(logical); }
#endif
  bool operator==(const LogicalMove& m) const {
#ifdef ENABLE_COMPOUND_TURNS
      return logical ? logical->move == m : LogicalMove(rootMove) == m;
#else
      return LogicalMove(rootMove) == m;
#endif
  }
  bool operator==(const Move& m) const { return rootMove == m; }
  bool operator<(const RootMove& m) const { // Sort in descending order
    return m.score != score ? m.score < score
                            : m.previousScore < previousScore;
  }

  LogicalMove first() const {
#ifdef ENABLE_COMPOUND_TURNS
      return logical ? logical->move : LogicalMove(rootMove);
#else
      return LogicalMove(rootMove);
#endif
  }
  size_t pv_size() const {
#ifdef ENABLE_COMPOUND_TURNS
      return (logical ? logical->pv.size() : pv.size()) + 1;
#else
      return pv.size() + 1;
#endif
  }
  LogicalMove pv_at(size_t index) const {
#ifdef ENABLE_COMPOUND_TURNS
      if (logical)
          return index ? logical->pv[index - 1] : logical->move;
#endif
      return LogicalMove(index ? pv[index - 1] : rootMove);
  }
  void clear_pv() {
#ifdef ENABLE_COMPOUND_TURNS
      if (logical)
          logical->pv.clear();
      else
#endif
          pv.clear();
  }
  void append_pv(Move m) {
#ifdef ENABLE_COMPOUND_TURNS
      assert(!logical);
#endif
      pv.push_back(m);
  }
#ifdef ENABLE_COMPOUND_TURNS
  void append_pv(LogicalMove m) {
      if (logical)
          logical->pv.push_back(m);
      else
          pv.push_back(m.first());
  }
  const std::vector<LogicalMove>& continuation() const {
      assert(logical);
      return logical->pv;
  }
  const LogicalMoveInfo& move_info() const {
      assert(logical);
      return logical->info;
  }
#endif
  void set_pv(const std::vector<LogicalMove>& line) {
    if (line.empty())
    {
        rootMove = MOVE_NONE;
#ifdef ENABLE_COMPOUND_TURNS
        if (logical)
        {
            logical->move = LogicalMove(MOVE_NONE);
            logical->pv.clear();
        }
        else
#endif
        clear_pv();
        return;
    }
    rootMove = line.front().first();
#ifdef ENABLE_COMPOUND_TURNS
    if (logical)
    {
        logical->move = line.front();
        logical->pv.assign(line.begin() + 1, line.end());
    }
    else
#endif
    {
        pv.clear();
        for (auto it = line.begin() + 1; it != line.end(); ++it)
            pv.push_back(it->first());
    }
  }
  void set_pv(const std::vector<Move>& line) {
      if (line.empty())
      {
          rootMove = MOVE_NONE;
#ifdef ENABLE_COMPOUND_TURNS
          if (logical)
          {
              logical->move = LogicalMove(MOVE_NONE);
              logical->pv.clear();
          }
          else
#endif
          clear_pv();
          return;
      }
      rootMove = line.front();
#ifdef ENABLE_COMPOUND_TURNS
      if (logical)
      {
          logical->move = LogicalMove(rootMove);
          logical->pv.clear();
          for (auto it = line.begin() + 1; it != line.end(); ++it)
              logical->pv.emplace_back(*it);
      }
      else
#endif
      {
          pv.assign(line.begin() + 1, line.end());
      }
  }

  Value score = -VALUE_INFINITE;
  Value previousScore = -VALUE_INFINITE;
  int selDepth = 0;
  int tbRank = 0;
  Value tbScore = VALUE_ZERO;
  Move rootMove = MOVE_NONE;
#ifdef ENABLE_COMPOUND_TURNS
  std::unique_ptr<LogicalRootData> logical;
#endif
  std::vector<Move> pv; // Ordinary continuation after rootMove
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
