/*
  Fairy-Stockfish, a UCI chess variant playing engine derived from Stockfish
  Copyright (C) 2018-2022 Fabian Fichter

  Fairy-Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Fairy-Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef XBOARD_H_INCLUDED
#define XBOARD_H_INCLUDED

#include <algorithm>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#ifdef ENABLE_COMPOUND_TURNS
#include "compound_turn.h"
#endif
#include "thread_win32_osx.h"
#include "types.h"

namespace Stockfish {

class Position;

namespace XBoard {

/// StateMachine class maintains the states required by XBoard protocol

class StateMachine {
public:
  StateMachine(Position& uciPos, StateListPtr& uciPosStates) : pos(uciPos), states(uciPosStates) {
    history = std::deque<HistoryEntry>();
    moveAfterSearch = false;
    playColor = COLOR_NB;
    ponderHighlight = "";
    shuttingDown = false;
  }
  ~StateMachine();
  void go(Search::LimitsType searchLimits, bool ponder = false);
  void ponder();
  void stop(bool abort = true);
  void shutdown_ponder_worker();
  void setboard(std::string fen = "");
  void do_move(Move m);
#ifdef ENABLE_COMPOUND_TURNS
  void do_compound_move(const LogicalMove& turn);
#endif
  void undo_move();
  std::string highlight(std::string square);
  void process_command(std::string token, std::istringstream& is);
  void launch_ponder_worker();
  void join_ponder_worker();
  void cancel_ponder_worker();
  bool moveAfterSearch;
  std::atomic<Move> ponderMove {MOVE_NONE};

private:
  Position& pos;
  StateListPtr& states;
  // One entry per externally applied move. Compound turns own their undo
  // payload for as long as the move remains undoable; ordinary moves carry
  // no payload. `display` is the physical move shown to the GUI (the first
  // component of a compound turn), recorded at apply time so readers never
  // inspect components.
  struct HistoryEntry {
    LogicalMove complete;
    Move display = MOVE_NONE;
#ifdef ENABLE_COMPOUND_TURNS
    std::unique_ptr<LogicalMoveUndo> undo;
#endif
  };
  std::deque<HistoryEntry> history;
  Search::LimitsType limits;
  Color playColor;
  std::string ponderHighlight;
  std::mutex ponderMutex;
  std::unique_ptr<NativeThread> ponderWorker;
  std::atomic<bool> shuttingDown;
};

extern StateMachine* stateMachine;

} // namespace XBoard

} // namespace Stockfish

#endif // #ifndef XBOARD_H_INCLUDED
