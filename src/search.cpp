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

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>   // For std::memset
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <thread>

#include "evaluate.h"
#ifdef ENABLE_COMPOUND_TURNS
#include "compound_turn.h"
#endif
#include "misc.h"
#include "movegen.h"
#include "movepick.h"
#include "partner.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"
#include "tt.h"
#include "uci.h"
#include "xboard.h"
#include "syzygy/tbprobe.h"

namespace Stockfish {

namespace Search {

  LimitsType Limits;
}

namespace Tablebases {

  int Cardinality;
  bool RootInTB;
  bool UseRule50;
  Depth ProbeDepth;
}

namespace TB = Tablebases;

using std::string;
using Eval::evaluate;
using namespace Search;

namespace {

  // Different node types, used as a template parameter
  enum NodeType { NonPV, PV, Root };

#ifdef ENABLE_COMPOUND_TURNS
  template <bool Logical>
  struct LogicalMoveSourceStorage {};

  template <>
  struct LogicalMoveSourceStorage<true> {
      std::optional<LogicalMoveSource> source;
  };

#endif

  template <typename T, bool Enabled>
  struct PvBuffer;

  template <typename T>
  struct PvBuffer<T, true> {
      std::array<T, MAX_PLY + 1> moves;
      T* data() { return moves.data(); }
  };

  template <typename T>
  struct PvBuffer<T, false> {
      T* data() { return nullptr; }
  };

  constexpr uint64_t TtHitAverageWindow     = 4096;
  constexpr uint64_t TtHitAverageResolution = 1024;
  constexpr int MaxReductionIndex = MAX_MOVES - 1;

  // Futility margin
  Value futility_margin(Depth d, bool improving) {
    return Value(214 * (d - improving));
  }

  // Reductions lookup table, initialized at startup
  int Reductions[MAX_MOVES]; // [depth or moveNumber]

  Depth reduction(bool i, Depth d, int mn) {
    int r = Reductions[std::clamp(int(d), 0, MaxReductionIndex)]
           * Reductions[std::clamp(mn, 0, MaxReductionIndex)];
    return (r + 534) / 1024 + (!i && r > 904);
  }

  int futility_move_count(bool improving, Depth depth, const Position& pos) {
    return (3 + depth * depth * (1 + pos.walling()) + 2 * pos.blast_on_capture()) / (2 - improving + pos.blast_on_capture());
  }

  // History and stats update bonus, based on depth
  int stat_bonus(Depth d) {
    constexpr int MaxBonus = 10692;
    int bonus = 6 * d * d + 229 * d - 215;
    return std::clamp(bonus, -MaxBonus, MaxBonus);
  }

  // Add a small random component to draw evaluations to avoid 3-fold blindness
  Value value_draw(Thread* thisThread) {
    return VALUE_DRAW + Value(2 * (thisThread->nodes & 1) - 1);
  }

  struct RootTerminal {
    Value value;
    const char* reason;
  };

  RootTerminal compute_root_terminal(const Position& pos) {
    Value variantResult;
    bool variantGameEnd = pos.is_game_end(variantResult);
    bool inCheck = pos.evasion_checkers();
    return { variantGameEnd ? variantResult
                            : inCheck ? pos.checkmate_value()
                                      : pos.stalemate_value(),
             variantGameEnd ? "game_end"
                            : inCheck ? "checkmate"
                                      : "stalemate" };
  }

  const char* xboard_result(const Position& pos, Value value) {
    if (value == VALUE_DRAW)
        return "1/2-1/2 {Draw}";

    Value whiteRelative = pos.side_to_move() == BLACK ? -value : value;
    return whiteRelative > VALUE_DRAW ? "1-0 {White wins}" : "0-1 {Black wins}";
  }

  void print_root_adjudication(const Position& pos, Value result, const char* reason) {
    if (int(Options["Verbosity"]) < 2 || !is_uci_dialect(CurrentProtocol))
        return;

    sync_cout << "info string adjudication reason " << reason
              << " result " << UCI::value(result)
              << " side_to_move " << (pos.side_to_move() == WHITE ? "white" : "black")
              << sync_endl;
  }

  // Skill structure is used to implement strength limit
  struct Skill {
    explicit Skill(int l) : level(l) {}
    bool enabled() const { return level < 20; }
    bool time_to_pick(Depth depth) const { return depth == 1 + std::max(level, 0); }
    LogicalMove pick_best(size_t multiPV);

    int level;
    LogicalMove best;
  };

  struct OrdinaryMoveInfo {
    Piece movedPiece = NO_PIECE;
    bool capturesOpponent = false;
    bool removesMaterial = false;
    bool promotionLike = false;
    bool givesCheck = false;
    bool historyCompatible = true;
    bool reductionEligible = true;
    bool seeReliable = true;
  };

  template<bool Logical, NodeType nodeType>
  Value search_impl(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);
  template<bool Logical, NodeType nodeType>
  Value qsearch_impl(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth = 0);
  template<NodeType nodeType>
  Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode);

  Value value_to_tt(Value v, int ply);
  Value value_from_tt(Value v, int ply, int r50c);
  template<typename SearchMove>
  void update_pv(SearchMove* pv, SearchMove move, SearchMove* childPv);
  void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus);
  void update_quiet_stats(const Position& pos, Stack* ss, Move move, int bonus, int depth);
  void update_all_stats(const Position& pos, Stack* ss, Move bestMove, Value bestValue, Value beta, Square prevSq,
                        Move* quietsSearched, int quietCount, Move* capturesSearched, int captureCount, Depth depth);
  void idle_wait() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  void check_main_thread_time(Thread* thisThread) {
    if (thisThread != Threads.main())
        return;

    static_cast<MainThread*>(thisThread)->check_time();
  }

  template<bool Logical>
  bool history_compatible(const Stack* ss) {
#ifdef ENABLE_COMPOUND_TURNS
    if constexpr (Logical)
        return ss->currentMoveHistoryCompatible;
#endif
    return true;
  }

  template<bool Logical>
  bool captured_opponent(const Stack* ss) {
#ifdef ENABLE_COMPOUND_TURNS
    if constexpr (Logical)
        return ss->currentMoveCapturedOpponent;
#endif
    return false;
  }

  // perft() is our utility to verify move generation. All the leaf nodes up
  // to the given depth are generated and counted, and the sum is returned.
  template<bool Root>
  uint64_t perft(Position& pos, Depth depth) {

#ifdef ENABLE_COMPOUND_TURNS
    if (pos.logical_moves_active())
        return compound_perft(pos, depth, Root);
#endif

    StateInfo st;
    ASSERT_ALIGNED(&st, Eval::NNUE::CacheLineSize);

    uint64_t cnt, nodes = 0;
    const bool leaf = (depth == 2);

    for (const auto& m : MoveList<LEGAL>(pos))
    {
        assert(pos.pseudo_legal(m));
        if (Root && depth <= 1)
            cnt = 1, nodes++;
        else
        {
            pos.do_move(m, st);
            cnt = leaf ? MoveList<LEGAL>(pos).size() : perft<false>(pos, depth - 1);
            nodes += cnt;
            pos.undo_move(m);
        }
        if (Root)
            sync_cout << UCI::move(pos, m) << ": " << cnt << sync_endl;
    }
    return nodes;
  }

} // namespace


/// Search::init() is called at startup to initialize various lookup tables

void Search::init() {

  for (int i = 1; i < MAX_MOVES; ++i)
      Reductions[i] = int(21.9 * std::log(i));
}


/// Search::clear() resets search state to its initial value

void Search::clear() {

  Threads.main()->wait_for_search_finished();

  Time.availableNodes = 0;
  TT.clear();
  Threads.clear();
  Tablebases::init(Options["SyzygyPath"]); // Free mapped files
}


/// MainThread::search() is started when the program receives the UCI 'go'
/// command. It searches from the root position and outputs the "bestmove".

void MainThread::search() {

  if (Limits.perft)
  {
      nodes = perft<true>(rootPos, Limits.perft);
      sync_cout << "\nNodes searched: " << nodes << "\n" << sync_endl;
      return;
  }

  Color us = rootPos.side_to_move();
  Time.init(rootPos, Limits, us, rootPos.game_ply());
  TT.new_search();

  Eval::NNUE::verify();

  const bool lazyRoot = rootPos.logical_moves_active() && rootMoves.empty();
  const bool noRootMove = !lazyRoot
                       && (rootMoves.empty()
                           || (rootMoves.size() == 1 && rootMoves[0].first() == MOVE_NONE));
  const bool optionalRootEnd = CurrentProtocol == XBOARD && rootPos.is_optional_game_end();

  if (noRootMove)
  {
      if (rootMoves.empty())
          rootMoves.emplace_back(MOVE_NONE);

      RootTerminal terminal = compute_root_terminal(rootPos);
      rootMoves[0].score = terminal.value;

      if (CurrentProtocol == XBOARD)
      {
          if (!ponder)
              sync_cout << xboard_result(rootPos, terminal.value) << sync_endl;
      }
      else if (int(Options["Verbosity"]) >= 1)
          sync_cout << "info depth 0 score "
                    << UCI::value(terminal.value)
                    << sync_endl;

      print_root_adjudication(rootPos, terminal.value, terminal.reason);
  }
  else
  {
      Threads.start_searching(); // start non-main threads
      Thread::search();          // main thread start searching
  }

  // Sit in bughouse variants if partner requested it or we are dead
  if (rootPos.two_boards() && !Threads.abort && CurrentProtocol == XBOARD)
  {
      while (!Threads.stop && (Partner.sitRequested || (Partner.weDead && !Partner.partnerDead)) && Time.elapsed() < Limits.time[us] - 1000)
          idle_wait();
  }

  // When we reach the maximum depth, we can arrive here without a raise of
  // Threads.stop. However, if we are pondering or in an infinite search,
  // the UCI protocol states that we shouldn't print the best move before the
  // GUI sends a "stop" or "ponderhit" command. We therefore simply wait here
  // until the GUI sends one of those commands.

  while (!Threads.stop && (ponder || Limits.infinite))
      idle_wait();

  // Stop the threads if not already stopped (also raise the stop if
  // "ponderhit" just reset Threads.ponder).
  Threads.stop = true;

  // Wait until all threads have finished
  Threads.wait_for_search_finished();

  // When playing in 'nodes as time' mode, subtract the searched nodes from
  // the available ones before exiting.
  if (Limits.npmsec)
      Time.availableNodes += Limits.inc[us] - Threads.nodes_searched();

  bestThread = this;

  if (   int(Options["MultiPV"]) == 1
      && !Limits.depth
      && !Limits.mate
      && !(Skill(Options["Skill Level"]).enabled() || int(Options["UCI_LimitStrength"]))
      && rootMoves[0].first() != MOVE_NONE)
      bestThread = Threads.get_best_thread();

  bestPreviousScore = bestThread->rootMoves[0].score;

  if (bestThread->rootMoves[0].first() == MOVE_NONE)
  {
      RootTerminal terminal = compute_root_terminal(rootPos);
      print_root_adjudication(rootPos, terminal.value, terminal.reason);
  }

  bool extractedPonder = false;

  if (!rootPos.compound_turn_active() && bestThread->rootMoves[0].pv_size() == 1)
      extractedPonder = bestThread->rootMoves[0].extract_ponder_from_tt(rootPos);

  // Send again PV info if we have a new best thread or extracted a ponder move.
  if ((bestThread != this || extractedPonder) && int(Options["Verbosity"]) >= 1)
      sync_cout << UCI::pv(bestThread->rootPos, bestThread->completedDepth, -VALUE_INFINITE, VALUE_INFINITE) << sync_endl;

  if (optionalRootEnd && !noRootMove && !ponder)
  {
      RootTerminal terminal = compute_root_terminal(rootPos);
      if (terminal.value >= bestThread->rootMoves[0].score)
      {
          sync_cout << xboard_result(rootPos, terminal.value) << sync_endl;
          return;
      }
  }

  if (CurrentProtocol == XBOARD)
  {
#ifdef ENABLE_COMPOUND_TURNS
      if (rootPos.compound_turn_active())
      {
          const LogicalMove& bestTurn = bestThread->rootMoves[0].first();
          const std::vector<std::string> compoundPv =
              compound_pv_to_strings(rootPos, bestThread->rootMoves[0].first(),
                                     bestThread->rootMoves[0].continuation());
          if (!Limits.infinite && !ponder && !compoundPv.empty()
              && bestTurn.first() != MOVE_NONE
              && !Threads.abort.exchange(true))
          {
              sync_cout << "move " << compoundPv.front() << sync_endl;
              if (XBoard::stateMachine->moveAfterSearch)
              {
                  XBoard::stateMachine->do_compound_move(bestTurn);
                  XBoard::stateMachine->moveAfterSearch = false;
              }
          }
          return;
      }
#endif
      Move bestMove = bestThread->rootMoves[0].first().first();
      // Wait for virtual drop to become real
      if (rootPos.two_boards() && rootPos.virtual_drop(bestMove))
      {
          Partner.ptell("fast");
          while (!Threads.abort && !Partner.partnerDead && !Partner.fast && Limits.time[us] - Time.elapsed() > Partner.opptime)
              idle_wait();
          Partner.ptell("x");
          // Find best real move
          for (const auto& m : bestThread->rootMoves)
              if (!rootPos.virtual_drop(m.first().first()))
              {
                  bestMove = m.first().first();
                  break;
              }
      }
      // Send move only when not in analyze mode and not at game end
      if (!Limits.infinite && !ponder && rootMoves[0].first() != MOVE_NONE && !Threads.abort.exchange(true))
      {
          std::string move = UCI::move(rootPos, bestMove);
          if (rootPos.walling() && move.find(",") != std::string::npos)
          {
              size_t comma = move.find(",");
              sync_cout << "move " << move.substr(0, comma) << "," << sync_endl;
              sync_cout << "move " << move.substr(comma + 1) << sync_endl;
          }
          else
              sync_cout << "move " << move << sync_endl;
          if (XBoard::stateMachine->moveAfterSearch)
          {
              XBoard::stateMachine->do_move(bestMove);
              XBoard::stateMachine->moveAfterSearch = false;
              if (Options["Ponder"] && bestThread->rootMoves[0].pv_size() > 1)
                  XBoard::stateMachine->ponderMove = bestThread->rootMoves[0].pv_at(1).first();
          }
      }
      return;
  }

  SyncCout out;
#ifdef ENABLE_COMPOUND_TURNS
  const bool logicalPv = rootPos.logical_moves_active() || rootPos.may_enter_logical_moves();
  const std::vector<std::string> compoundPv = logicalPv
                                            ? compound_pv_to_strings(rootPos, bestThread->rootMoves[0].first(),
                                                                     bestThread->rootMoves[0].continuation())
                                            : std::vector<std::string>();
  if (rootPos.compound_turn_active()
      && !bestThread->rootMoves.empty()
      && !compoundPv.empty()
      && bestThread->rootMoves[0].first().first() != MOVE_NONE)
      out << "bestmove " << compoundPv.front();
  else
#endif
      out << "bestmove " << UCI::move(rootPos, bestThread->rootMoves[0].first().first());

  if (bestThread->rootMoves[0].pv_size() > 1)
  {
#ifdef ENABLE_COMPOUND_TURNS
      if (compoundPv.size() > 1)
          out << " ponder " << compoundPv[1];
      else
#endif
          out << " ponder " << UCI::move(rootPos, bestThread->rootMoves[0].pv_at(1).first());
  }

  out << sync_endl;
}


/// Thread::search() is the main iterative deepening loop. It calls search()
/// repeatedly with increasing depth until the allocated thinking time has been
/// consumed, the user stops the search, or the maximum search depth is reached.

void Thread::search() {
  // To allow access to (ss-7) up to (ss+2), the stack must be oversized.
  // The former is needed to allow update_continuation_histories(ss-1, ...),
  // which accesses its argument at ss-6, also near the root.
  // The latter is needed for statScore and killer initialization.
  Stack stack[MAX_PLY+10], *ss = stack+7;
  Move ordinaryPv[MAX_PLY+1];
#ifdef ENABLE_COMPOUND_TURNS
  const bool logicalPvRequired = rootPos.logical_moves_active()
                              || rootPos.may_enter_logical_moves();
  std::unique_ptr<LogicalMove[]> logicalPv;
  if (logicalPvRequired)
      logicalPv = std::make_unique<LogicalMove[]>(MAX_PLY + 1);
#endif
  Value bestValue, alpha, beta, delta;
  LogicalMove lastBestMove(MOVE_NONE);
  Value lastBestScore = -VALUE_INFINITE;
  Depth lastBestMoveDepth = 0;
  std::vector<LogicalMove> lastBestPV;
  MainThread* mainThread = (this == Threads.main() ? Threads.main() : nullptr);
  double timeReduction = 1, totBestMoveChanges = 0;
  Color us = rootPos.side_to_move();
  int iterIdx = 0;

  std::memset(ss-7, 0, 10 * sizeof(Stack));
  for (int i = -7; i <= 2; ++i)
      (ss+i)->currentMovePiece = NO_PIECE;

  for (int i = 7; i > 0; i--)
      (ss-i)->continuationHistory = &this->continuationHistory[0][0][NO_PIECE][0]; // Use as a sentinel

  for (int i = 0; i <= MAX_PLY + 2; ++i)
      (ss+i)->ply = i;

#ifdef ENABLE_COMPOUND_TURNS
  if (logicalPvRequired)
      ss->set_pv<true>(logicalPv.get());
  else
#endif
      ss->set_pv<false>(ordinaryPv);

  bestValue = delta = alpha = -VALUE_INFINITE;
  beta = VALUE_INFINITE;

  if (mainThread)
  {
      if (mainThread->bestPreviousScore == VALUE_INFINITE)
          for (int i = 0; i < 4; ++i)
              mainThread->iterValue[i] = VALUE_ZERO;
      else
          for (int i = 0; i < 4; ++i)
              mainThread->iterValue[i] = mainThread->bestPreviousScore;
  }

  std::copy(&lowPlyHistory[2][0], &lowPlyHistory.back().back() + 1, &lowPlyHistory[0][0]);
  std::fill(&lowPlyHistory[MAX_LPH - 2][0], &lowPlyHistory.back().back() + 1, 0);

  size_t multiPV = size_t(Options["MultiPV"]);

  // Pick integer skill levels, but non-deterministically round up or down
  // such that the average integer skill corresponds to the input floating point one.
  // UCI_Elo is converted to a suitable fractional skill level, using anchoring
  // to CCRL Elo (goldfish 1.13 = 2000) and a fit through Ordo derived Elo
  // for match (TC 60+0.6) results spanning a wide range of k values.
  PRNG rng(now());
  double shiftedElo = Options["UCI_Elo"] - 1346.6;
  double floatLevel = Options["UCI_LimitStrength"] ?
                      std::clamp(shiftedElo > 0 ? std::pow(shiftedElo / 143.4, 1 / 0.806)
                                                : shiftedElo / 143.4 + std::pow(shiftedElo / 500, 5),
                                 -20.0, 20.0) :
                        double(Options["Skill Level"]);
  int intLevel = int(floatLevel) +
                 ((floatLevel - int(floatLevel)) * 1024 > rng.rand<unsigned>() % 1024  ? 1 : 0);
  Skill skill(intLevel);

  const bool lazyRoot = rootMoves.empty()
                     && rootPos.logical_moves_active()
                     && multiPV == 1
                     && !skill.enabled()
                     && !Limits.searchMovesSpecified
                     && Limits.banmoves.empty();


  // When playing with strength handicap enable MultiPV search that we will
  // use behind the scenes to retrieve a set of possible moves.
  if (skill.enabled())
      multiPV = std::max(multiPV, (size_t)4);

  if (!lazyRoot)
      multiPV = std::min(multiPV, rootMoves.size());
  ttHitAverage = TtHitAverageWindow * TtHitAverageResolution / 2;

  trend = SCORE_ZERO;

  int searchAgainCounter = 0;

  // Iterative deepening loop until requested to stop or the target depth is reached
  while (   ++rootDepth < MAX_PLY
         && !Threads.stop
         && !(Limits.depth && mainThread && rootDepth > Limits.depth))
  {
      // Age out PV variability metric
      if (mainThread)
          totBestMoveChanges /= 2;

      // Save the last iteration's scores before first PV line is searched and
      // all the move scores except the (new) PV are set to -VALUE_INFINITE.
      for (RootMove& rm : rootMoves)
          rm.previousScore = rm.score;

      size_t pvFirst = 0;
      pvLast = 0;

      if (!Threads.increaseDepth)
         searchAgainCounter++;

      // MultiPV loop. We perform a full root search for each PV line
      for (pvIdx = 0; pvIdx < multiPV && !Threads.stop; ++pvIdx)
      {
          if (pvIdx == pvLast)
          {
              pvFirst = pvLast;
              for (pvLast++; pvLast < rootMoves.size(); pvLast++)
                  if (rootMoves[pvLast].tbRank != rootMoves[pvFirst].tbRank)
                      break;
          }

          // Reset UCI info selDepth for each depth and each PV line
          selDepth = 0;

          // Reset aspiration window starting size
          if (rootDepth >= 4)
          {
              Value prev = rootMoves[pvIdx].previousScore;
              delta = Value(17 * (1 + rootPos.captures_to_hand()));
              alpha = std::max(prev - delta,-VALUE_INFINITE);
              beta  = std::min(prev + delta, VALUE_INFINITE);

              // Adjust trend based on root move's previousScore (dynamic contempt)
              int tr = 113 * prev / (abs(prev) + 147);

              trend = (us == WHITE ?  make_score(tr, tr / 2)
                                   : -make_score(tr, tr / 2));
          }

          // Start with a small aspiration window and, in the case of a fail
          // high/low, re-search with a bigger window until we don't fail
          // high/low anymore.
          int failedHighCnt = 0;
          while (true)
          {
              Depth adjustedDepth = std::max(1, rootDepth - failedHighCnt - searchAgainCounter);
              bestValue = Stockfish::search<Root>(rootPos, ss, alpha, beta, adjustedDepth, false);

              // Bring the best move to the front. It is critical that sorting
              // is done with a stable algorithm because all the values but the
              // first and eventually the new best one are set to -VALUE_INFINITE
              // and we want to keep the same order for all the moves except the
              // new PV that goes to the front. Note that in case of MultiPV
              // search the already searched PV lines are preserved.
              std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast);

              // If search has been stopped, we break immediately. Sorting is
              // safe because RootMoves is still valid, although it refers to
              // the previous iteration.
              if (Threads.stop)
                  break;

              // When failing high/low give some update (without cluttering
              // the UI) before a re-search.
              if (   mainThread
                  && multiPV == 1
                  && (bestValue <= alpha || bestValue >= beta)
                  && Time.elapsed() > 3000)
                  if (int(Options["Verbosity"]) >= 1)
                      sync_cout << UCI::pv(rootPos, rootDepth, alpha, beta) << sync_endl;

              // In case of failing low/high increase aspiration window and
              // re-search, otherwise exit the loop.
              if (bestValue <= alpha)
              {
                  beta = (alpha + beta) / 2;
                  alpha = std::max(bestValue - delta, -VALUE_INFINITE);

                  failedHighCnt = 0;
                  if (mainThread)
                      mainThread->stopOnPonderhit = false;
              }
              else if (bestValue >= beta)
              {
                  beta = std::min(bestValue + delta, VALUE_INFINITE);
                  ++failedHighCnt;
              }
              else
                  break;

              delta += delta / 4 + 5;

              assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
          }

          // Sort the PV lines searched so far and update the GUI
          std::stable_sort(rootMoves.begin() + pvFirst, rootMoves.begin() + pvIdx + 1);

          if (    mainThread
              && (Threads.stop || pvIdx + 1 == multiPV || Time.elapsed() > 3000))
              // If search stopped mid-iteration, an exact mate / TB-loss score
              // at the front can be unproven for this thread. Suppress that PV here
              // and below roll back to the last completed best line.
              if (!(Threads.stop && completedDepth != rootDepth
                    && (std::abs(rootMoves[0].score) >= VALUE_MATE_IN_MAX_PLY
                        || rootMoves[0].score <= VALUE_TB_LOSS_IN_MAX_PLY)))
                  if (int(Options["Verbosity"]) >= 1)
                      sync_cout << UCI::pv(rootPos, rootDepth, alpha, beta) << sync_endl;
      }

      if (   Threads.stop
          && completedDepth != rootDepth
          && rootMoves[0].score != -VALUE_INFINITE
          && (std::abs(rootMoves[0].score) >= VALUE_MATE_IN_MAX_PLY
              || rootMoves[0].score <= VALUE_TB_LOSS_IN_MAX_PLY))
      {
          if (!lastBestPV.empty())
          {
              auto it = std::find_if(rootMoves.begin(), rootMoves.end(), [&lastBestPV](const RootMove& rm) {
                  return rm == lastBestPV[0];
              });
              if (it != rootMoves.end())
                  std::rotate(rootMoves.begin(), it, it + 1);
              rootMoves[0].set_pv(lastBestPV);
              rootMoves[0].score = lastBestScore;
          }
          else
              rootMoves[0].score = VALUE_DRAW;
      }

      if (!Threads.stop)
          completedDepth = rootDepth;

      if (rootMoves[0].first() != lastBestMove) {
         lastBestMove = rootMoves[0].first();
         lastBestScore = rootMoves[0].score;
         lastBestPV.clear();
         lastBestPV.reserve(rootMoves[0].pv_size());
         for (size_t i = 0; i < rootMoves[0].pv_size(); ++i)
             lastBestPV.push_back(rootMoves[0].pv_at(i));
         lastBestMoveDepth = rootDepth;
      }

      // Have we found a "mate in x" after a completed iteration?
      if (   Limits.mate
          && !Threads.stop
          && (   (rootMoves[0].score >= VALUE_MATE_IN_MAX_PLY
                && VALUE_MATE - rootMoves[0].score <= 2 * Limits.mate)
              || (rootMoves[0].score <= VALUE_MATED_IN_MAX_PLY
                && VALUE_MATE + rootMoves[0].score <= 2 * Limits.mate)))
          Threads.stop = true;

      if (!mainThread)
          continue;

      // If skill level is enabled and time is up, pick a sub-optimal best move
      if (skill.enabled() && skill.time_to_pick(rootDepth))
          skill.pick_best(multiPV);

      // Do we have time for the next iteration? Can we stop searching now?
      if (    Limits.use_time_management()
          && !Threads.stop
          && !mainThread->stopOnPonderhit)
      {
          double fallingEval = (318 + 6 * (mainThread->bestPreviousScore - bestValue)
                                    + 6 * (mainThread->iterValue[iterIdx] - bestValue)) / 825.0;
          fallingEval = std::clamp(fallingEval, 0.5, 1.5);

          // If the bestMove is stable over several iterations, reduce time accordingly
          timeReduction = lastBestMoveDepth + 9 < completedDepth ? 1.92 : 0.95;
          double reduction = (1.47 + mainThread->previousTimeReduction) / (2.32 * timeReduction);

          // Use part of the gained time from a previous stable move for the current move
          for (Thread* th : Threads)
          {
              totBestMoveChanges += th->bestMoveChanges;
              th->bestMoveChanges = 0;
          }
          double bestMoveInstability = 1.073 + std::max(1.0, 2.25 - 9.9 / rootDepth)
                                              * totBestMoveChanges / Threads.size();
          double totalTime = Time.optimum() * fallingEval * reduction * bestMoveInstability;

          // Cap used time in case of a single legal move for a better viewer experience in tournaments
          // yielding correct scores and sufficiently fast moves.
          if (rootMoves.size() == 1)
              totalTime = std::min(500.0, totalTime);

          // Update partner in bughouse variants
          if (completedDepth >= 8 && rootPos.two_boards() && CurrentProtocol == XBOARD)
          {
              // Communicate clock times relevant for sitting decisions
              if (Limits.time[us])
                  Partner.ptell<FAIRY>("time " + std::to_string((Limits.time[us] - Time.elapsed()) / 10));
              if (Limits.time[~us])
                  Partner.ptell<FAIRY>("otim " + std::to_string(Limits.time[~us] / 10));
              // We are dead and need to sit
              if (!Partner.weDead && bestValue <= VALUE_MATED_IN_MAX_PLY)
              {
                  Partner.ptell("dead");
                  Partner.weDead = true;
              }
              // We were dead but are fine again
              else if (Partner.weDead && bestValue > VALUE_MATED_IN_MAX_PLY)
              {
                  Partner.ptell("x");
                  Partner.weDead = false;
              }
              // We win by force, so partner should sit
              else if (!Partner.weWin && bestValue >= VALUE_MATE_IN_MAX_PLY && Limits.time[~us] < Partner.time)
              {
                  Partner.ptell("sit");
                  Partner.weWin = true;
              }
              // We are no longer winning
              else if (Partner.weWin && (bestValue < VALUE_MATE_IN_MAX_PLY || Limits.time[~us] > Partner.time))
              {
                  Partner.ptell("x");
                  Partner.weWin = false;
              }
              // We can win if partner delivers required material quickly
              else if (  !Partner.weVirtualWin
                       && bestValue >= VALUE_VIRTUAL_MATE_IN_MAX_PLY
                       && bestValue <= VALUE_VIRTUAL_MATE
                       && Limits.time[us] - Time.elapsed() > Partner.opptime)
              {
                  Partner.ptell("fast");
                  Partner.weVirtualWin = true;
              }
              // Virtual mate is gone
              else if (   Partner.weVirtualWin
                       && (bestValue < VALUE_VIRTUAL_MATE_IN_MAX_PLY || bestValue > VALUE_VIRTUAL_MATE || Limits.time[us] - Time.elapsed() < Partner.opptime))
              {
                  Partner.ptell("slow");
                  Partner.weVirtualWin = false;
              }
              // We need to survive a virtual mate and play fast
              else if (  !Partner.weVirtualLoss
                       && (bestValue <= -VALUE_VIRTUAL_MATE_IN_MAX_PLY && bestValue >= -VALUE_VIRTUAL_MATE)
                       && Limits.time[~us] > Partner.time)
              {
                  Partner.ptell("sit");
                  Partner.weVirtualLoss = true;
                  Partner.fast = true;
              }
              // Virtual mate threat is over
              else if (   Partner.weVirtualLoss
                       && (bestValue > -VALUE_VIRTUAL_MATE_IN_MAX_PLY || bestValue < -VALUE_VIRTUAL_MATE || Limits.time[~us] < Partner.time))
              {
                  Partner.ptell("x");
                  Partner.weVirtualLoss = false;
                  Partner.fast = false;
              }
          }

          // Stop the search if we have exceeded the totalTime
          if (Time.elapsed() > totalTime)
          {
              // If we are allowed to ponder do not stop the search now but
              // keep pondering until the GUI sends "ponderhit" or "stop".
              if (mainThread->ponder)
                  mainThread->stopOnPonderhit = true;
              else if (!(rootPos.two_boards() && (Partner.sitRequested || Partner.weDead)))
                  Threads.stop = true;
          }
          else if (   Threads.increaseDepth
                   && !mainThread->ponder
                   && Time.elapsed() > totalTime * 0.58)
                   Threads.increaseDepth = false;
          else
                   Threads.increaseDepth = true;
      }

      mainThread->iterValue[iterIdx] = bestValue;
      iterIdx = (iterIdx + 1) & 3;
  }

  if (rootMoves.empty())
      rootMoves.emplace_back(MOVE_NONE);

  if (!mainThread)
      return;

  mainThread->previousTimeReduction = timeReduction;

  // If skill level is enabled, swap best PV line with the sub-optimal one
  if (skill.enabled())
      std::swap(rootMoves[0], *std::find(rootMoves.begin(), rootMoves.end(),
                skill.best.empty() ? skill.pick_best(multiPV) : skill.best));
}


namespace {

  // search_impl<>() is the main search function for both PV and non-PV nodes.
  // The ordinary instantiation keeps a compact Move PV even in a compound
  // enabled binary.

  template <bool Logical, NodeType nodeType>
  Value search_impl(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) {

    constexpr bool PvNode = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;
    using SearchMove = std::conditional_t<Logical, LogicalMove, Move>;
    using SearchMoveInfo = std::conditional_t<Logical, LogicalMoveInfo, OrdinaryMoveInfo>;
    const Depth maxNextDepth = rootNode ? depth : depth + 1;

    // Check if we have an upcoming move which draws by repetition, or
    // if the opponent had an alternative move earlier to this position.
    if (   !rootNode
        && pos.rule50_count() >= 3
        && alpha < VALUE_DRAW
        && pos.has_game_cycle(ss->ply))
    {
        alpha = value_draw(pos.this_thread());
        if (alpha >= beta)
            return alpha;
    }

    // Dive into quiescence search when the depth reaches zero
    if (depth <= 0)
        return qsearch_impl<Logical, PvNode ? PV : NonPV>(pos, ss, alpha, beta);

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(0 < depth && depth < MAX_PLY);
    assert(!(PvNode && cutNode));

    Thread* thisThread = pos.this_thread();
    using LocalPv = PvBuffer<SearchMove, PvNode && !Logical>;
    LocalPv localPv{};
    SearchMove* pv = nullptr;
    if constexpr (PvNode)
    {
#ifdef ENABLE_COMPOUND_TURNS
        if constexpr (Logical)
            pv = thisThread->logical_pv(ss->ply);
        else
#endif
            pv = localPv.data();
    }
    Move capturesSearched[32], quietsSearched[64];
    StateInfo st;
    ASSERT_ALIGNED(&st, Eval::NNUE::CacheLineSize);

    TTEntry* tte;
    Key posKey;
    Move ttMove, move, excludedMove, bestMove;
    SearchMove bestSearchMove = SearchMove(MOVE_NONE);
    Depth extension, newDepth;
    Value bestValue, value, ttValue, eval, maxValue, probCutBeta;
    bool givesCheck, improving, didLMR, priorCapture;
    bool captureOrPromotion, doFullDepthSearch, moveCountPruning,
         ttCapture, singularQuietLMR;
    Piece movedPiece;
    int moveCount, captureCount, quietCount;

    // Step 1. Initialize node
    PieceToHistory* neutralContinuationHistory = &thisThread->continuationHistory[0][0][NO_PIECE][0];
    const LogicalMoveCapabilities moveCapabilities = [&] {
        if constexpr (Logical)
            return pos.logical_move_capabilities();
        return LogicalMoveCapabilities{};
    }();
    const bool logicalMovePosition = [&] {
        if constexpr (Logical)
            return pos.logical_moves_active();
        return false;
    }();
    const bool exactTtMoveIdentity = !logicalMovePosition;
    const bool lazyRoot = rootNode && logicalMovePosition
                       && thisThread->rootMoves.empty();
    ss->inCheck        = pos.evasion_checkers();
    priorCapture       = [&] {
        if constexpr (Logical)
            return captured_opponent<Logical>(ss-1);
        return bool(pos.captured_piece());
    }();
    Color us           = pos.side_to_move();
    moveCount          = captureCount = quietCount = ss->moveCount = 0;
    bestValue          = -VALUE_INFINITE;
    maxValue           = VALUE_INFINITE;

    // Check for the available remaining time
    check_main_thread_time(thisThread);

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && thisThread->selDepth < ss->ply + 1)
        thisThread->selDepth = ss->ply + 1;

    if (!rootNode)
    {
        Value variantResult;
        if (pos.is_game_end(variantResult, ss->ply))
            return variantResult;

        // Step 2. Check for aborted search and immediate draw
        if (   Threads.stop.load(std::memory_order_relaxed)
            || ss->ply >= MAX_PLY)
            return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos)
                                                        : value_draw(pos.this_thread());

        // Step 3. Mate distance pruning. Even if we mate at the next move our score
        // would be at best mate_in(ss->ply+1), but if alpha is already bigger because
        // a shorter mate was found upward in the tree then there is no need to search
        // because we will never beat the current alpha. Same logic but with reversed
        // signs applies also in the opposite condition of being mated instead of giving
        // mate. In this case return a fail-high score.
        alpha = std::max(mated_in(ss->ply), alpha);
        beta = std::min(mate_in(ss->ply+1), beta);
        if (alpha >= beta)
            return alpha;
    }

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    (ss+1)->ttPv         = false;
    (ss+1)->excludedMove = bestMove = MOVE_NONE;
    (ss+2)->killers[0]   = (ss+2)->killers[1] = MOVE_NONE;
    ss->doubleExtensions = (ss-1)->doubleExtensions;
    const bool previousMoveHistoryCompatible = history_compatible<Logical>(ss-1);
    Square prevSq = previousMoveHistoryCompatible ? to_sq((ss-1)->currentMove) : SQ_NONE;

    // Initialize statScore to zero for the grandchildren of the current position.
    // So statScore is shared between all grandchildren and only the first grandchild
    // starts with statScore = 0. Later grandchildren start with the last calculated
    // statScore of the previous grandchild. This influences the reduction rules in
    // LMR which are based on the statScore of parent position.
    if (!rootNode)
        (ss+2)->statScore = 0;

    // Step 4. Transposition table lookup. We don't want the score of a partial
    // search to overwrite a previous full search TT value, so we use a different
    // position key in case of an excluded move.
    excludedMove = ss->excludedMove;
    posKey = excludedMove == MOVE_NONE ? pos.key() : pos.key() ^ make_key(excludedMove);
    tte = TT.probe(posKey, ss->ttHit);
    ttValue = ss->ttHit ? value_from_tt(tte->value(), ss->ply, pos.rule50_count()) : VALUE_NONE;
    if constexpr (Logical)
        ttMove = rootNode ? (lazyRoot ? MOVE_NONE
                                      : thisThread->rootMoves[thisThread->pvIdx].first().first())
                          : ss->ttHit ? tte->move() : MOVE_NONE;
    else
        ttMove = rootNode ? thisThread->rootMoves[thisThread->pvIdx].first().first()
                          : ss->ttHit ? tte->move() : MOVE_NONE;
    if (!excludedMove)
        ss->ttPv = PvNode || (ss->ttHit && tte->is_pv());

    // Update low ply history for previous move if we are near root and position is or has been in PV
    if (   ss->ttPv
        && depth > 12
        && ss->ply - 1 < MAX_LPH
        && !priorCapture
        && previousMoveHistoryCompatible
        && is_ok((ss-1)->currentMove))
        thisThread->lowPlyHistory[ss->ply - 1][from_to((ss-1)->currentMove)] << stat_bonus(depth - 5);

    // thisThread->ttHitAverage can be used to approximate the running average of ttHit
    thisThread->ttHitAverage =   (TtHitAverageWindow - 1) * thisThread->ttHitAverage / TtHitAverageWindow
                                + TtHitAverageResolution * ss->ttHit;

    // At non-PV nodes we check for an early TT cutoff
    if (  !PvNode
        && ss->ttHit
        && tte->depth() >= depth
        && ttValue != VALUE_NONE // Possible in case of TT access race
        && (ttValue >= beta ? (tte->bound() & BOUND_LOWER)
                            : (tte->bound() & BOUND_UPPER)))
    {
        // If ttMove is quiet, update move sorting heuristics on TT hit
        if (ttMove && exactTtMoveIdentity)
        {
            if (ttValue >= beta)
            {
                // Bonus for a quiet ttMove that fails high
                if (!pos.capture_or_promotion(ttMove))
                    update_quiet_stats(pos, ss, ttMove, stat_bonus(depth), depth);

                // Extra penalty for early quiet moves of the previous ply
                if (   previousMoveHistoryCompatible
                    && (ss-1)->moveCount <= 2
                    && !priorCapture)
                    update_continuation_histories(ss-1, pos.piece_on(prevSq), prevSq, -stat_bonus(depth + 1));
            }
            // Penalty for a quiet ttMove that fails low
            else if (!pos.capture_or_promotion(ttMove))
            {
                int penalty = -stat_bonus(depth);
                thisThread->mainHistory[us][from_to(ttMove)] << penalty;
                const Square gate = gate_history_square(ttMove);
                if (pos.walling() && gate != SQ_NONE)
                    thisThread->gateHistory[us][gate] << penalty;
                update_continuation_histories(ss, pos.moved_piece(ttMove), to_sq(ttMove), penalty);
            }
        }

        // Partial workaround for the graph history interaction problem
        // For high rule50 counts don't produce transposition table cutoffs.
        if (pos.rule50_count() < 90)
            return ttValue;
    }

    // Step 5. Tablebases probe
    if (!rootNode && TB::Cardinality)
    {
        int piecesCount = pos.count<ALL_PIECES>();

        if (    piecesCount <= TB::Cardinality
            && (piecesCount <  TB::Cardinality || depth >= TB::ProbeDepth)
            &&  pos.rule50_count() == 0
            &&  Options["UCI_Variant"] == "chess"
            && !pos.can_castle(ANY_CASTLING))
        {
            TB::ProbeState err;
            TB::WDLScore wdl = Tablebases::probe_wdl(pos, &err);

            // Force check of time on the next occasion
            if (thisThread == Threads.main())
                static_cast<MainThread*>(thisThread)->callsCnt = 0;

            if (err != TB::ProbeState::FAIL)
            {
                thisThread->tbHits.fetch_add(1, std::memory_order_relaxed);

                int drawScore = TB::UseRule50 ? 1 : 0;

                // use the range VALUE_MATE_IN_MAX_PLY to VALUE_TB_WIN_IN_MAX_PLY to score
                value =  wdl < -drawScore ? VALUE_MATED_IN_MAX_PLY + ss->ply + 1
                       : wdl >  drawScore ? VALUE_MATE_IN_MAX_PLY - ss->ply - 1
                                          : VALUE_DRAW + 2 * wdl * drawScore;

                Bound b =  wdl < -drawScore ? BOUND_UPPER
                         : wdl >  drawScore ? BOUND_LOWER : BOUND_EXACT;

                if (    b == BOUND_EXACT
                    || (b == BOUND_LOWER ? value >= beta : value <= alpha))
                {
                    tte->save(posKey, value_to_tt(value, ss->ply), ss->ttPv, b,
                              std::min(MAX_PLY - 1, depth + 6),
                              MOVE_NONE, VALUE_NONE);

                    return value;
                }

                if (PvNode)
                {
                    if (b == BOUND_LOWER)
                        bestValue = value, alpha = std::max(alpha, bestValue);
                    else
                        maxValue = value;
                }
            }
        }
    }

    CapturePieceToHistory& captureHistory = thisThread->captureHistory;

    // Step 6. Static evaluation of the position
    if (ss->inCheck)
    {
        // Skip early pruning when in check
        ss->staticEval = eval = VALUE_NONE;
        improving = false;
        goto moves_loop;
    }
    else if (ss->ttHit)
    {
        // Never assume anything about values stored in TT
        ss->staticEval = eval = tte->eval();
        if (eval == VALUE_NONE)
            ss->staticEval = eval = evaluate(pos);

        // Randomize draw evaluation
        if (eval == VALUE_DRAW)
            eval = value_draw(thisThread);

        // Can ttValue be used as a better position evaluation?
        if (    ttValue != VALUE_NONE
            && (tte->bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttValue;
    }
    else
    {
        // In case of null move search use previous static eval with a different sign
        // and addition of two tempos
        if ((ss-1)->currentMove != MOVE_NULL)
            ss->staticEval = eval = evaluate(pos);
        else
            ss->staticEval = eval = -(ss-1)->staticEval;

        // Save static evaluation into transposition table
        tte->save(posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, DEPTH_NONE, MOVE_NONE, eval);
    }

    // Use static evaluation difference to improve quiet move ordering
    if (   previousMoveHistoryCompatible
        && is_ok((ss-1)->currentMove)
        && !(ss-1)->inCheck
        && !priorCapture)
    {
        int bonus = std::clamp(-depth * 4 * int((ss-1)->staticEval + ss->staticEval), -1000, 1000);
        thisThread->mainHistory[~us][from_to((ss-1)->currentMove)] << bonus;
    }

    // Set up improving flag that is used in various pruning heuristics
    // We define position as improving if static evaluation of position is better
    // Than the previous static evaluation at our turn
    // In case of us being in check at our previous move we look at move prior to it
    improving =  (ss-2)->staticEval == VALUE_NONE
               ? ss->staticEval > (ss-4)->staticEval || (ss-4)->staticEval == VALUE_NONE
               : ss->staticEval > (ss-2)->staticEval;

    // Skip early pruning in case of mandatory capture
    if (pos.must_capture() && pos.has_capture())
        goto moves_loop;

    // Step 7. Futility pruning: child node (~50 Elo)
    if (   !PvNode
        && moveCapabilities.futilityPruning
        &&  depth < 9 - 3 * pos.blast_on_capture()
        &&  eval - futility_margin(depth, improving) * (1 + pos.check_counting() + 2 * pos.must_capture() + pos.extinction_single_piece() + !pos.checking_permitted()) >= beta
        &&  eval < VALUE_KNOWN_WIN) // Do not return unproven wins
        return eval;

    // Step 8. Null move search with verification search (~40 Elo)
    if (   !PvNode
        && moveCapabilities.nullMovePruning
        && (ss-1)->currentMove != MOVE_NULL
        && (ss-1)->statScore < 23767
        && !pos.multimove_pass(pos.game_ply())
        &&  eval >= beta
        &&  eval >= ss->staticEval
        &&  ss->staticEval >= beta - 20 * depth - 22 * improving + 168 * ss->ttPv + 159 + 200 * (((ss - 1)->currentMovePiece == NO_PIECE || !pos.double_step_region((ss - 1)->currentMovePiece)) && (pos.piece_types() & PAWN))
        && !excludedMove
        &&  pos.non_pawn_material(us)
        &&  pos.count<ALL_PIECES>(~us) != pos.count<PAWN>(~us)
        && !pos.flip_enclosed_pieces()
        && (ss->ply >= thisThread->nmpMinPly || us != thisThread->nmpColor))
    {
        assert(eval - beta >= 0);

        // Null move dynamic reduction based on depth and value
        Depth R = (1090 - 300 * pos.must_capture() - 250 * !pos.checking_permitted() + 81 * depth) / 256 + std::min(int(eval - beta) / 205, pos.must_capture() || pos.blast_on_capture() ? 0 : 3);

        ss->currentMove = MOVE_NULL;
        ss->currentMovePiece = NO_PIECE;
#ifdef ENABLE_COMPOUND_TURNS
        ss->currentMoveHistoryCompatible = false;
        ss->currentMoveCapturedOpponent = false;
#endif
        ss->continuationHistory = neutralContinuationHistory;

        pos.do_null_move(st);

        // Some variants can make a pass/null move unsafe because the unchanged
        // board leaves the opponent in an immediate evasion state (for example
        // antimatter blast semantics). Null-move pruning assumes the null
        // position is quiet enough for an ordinary search, so skip it here.
        if (pos.evasion_checkers())
            pos.undo_null_move();
        else
        {
            Value nullValue = -search_impl<Logical, NonPV>(pos, ss+1, -beta, -beta+1, depth-R, !cutNode);

            pos.undo_null_move();

            if (nullValue >= beta)
            {
                // Do not return unproven mate or TB scores
                if (nullValue >= VALUE_TB_WIN_IN_MAX_PLY)
                    nullValue = beta;

                if (thisThread->nmpMinPly || (abs(beta) < VALUE_KNOWN_WIN && depth < 14))
                    return nullValue;

                assert(!thisThread->nmpMinPly); // Recursive verification is not allowed

                // Do verification search at high depths, with null move pruning disabled
                // for us, until ply exceeds nmpMinPly.
                thisThread->nmpMinPly = ss->ply + 3 * (depth-R) / 4;
                thisThread->nmpColor = us;

                Value v = search_impl<Logical, NonPV>(pos, ss, beta-1, beta, depth-R, false);

                thisThread->nmpMinPly = 0;

                if (v >= beta)
                    return nullValue;
            }
        }
    }

    probCutBeta = beta + (209 + 20 * !!pos.flag_region(~pos.side_to_move()) + 50 * pos.captures_to_hand()) * (1 + pos.check_counting() + pos.extinction_single_piece()) - 44 * improving;

    // Step 9. ProbCut (~4 Elo)
    // If we have a good enough capture and a reduced search returns a value
    // much above beta, we can (almost) safely prune the previous move.
    if (   !PvNode
        && moveCapabilities.probCut
        &&  depth > 4
        && !pos.see_pruning_unreliable()
        &&  abs(beta) < VALUE_TB_WIN_IN_MAX_PLY
        // if value from transposition table is lower than probCutBeta, don't attempt probCut
        // there and in further interactions with transposition table cutoff depth is set to depth - 3
        // because probCut search has depth set to depth - 4 but we also do a move before it
        // so effective depth is equal to depth - 3
        && !(   ss->ttHit
             && tte->depth() >= depth - 3
             && ttValue != VALUE_NONE
             && ttValue < probCutBeta))
    {
        assert(probCutBeta < VALUE_INFINITE);

        MovePicker mp(pos, ttMove, probCutBeta - ss->staticEval, &thisThread->gateHistory, &captureHistory);
        int probCutCount = 0;
        bool ttPv = ss->ttPv;
        ss->ttPv = false;

        while (   (move = mp.next_move()) != MOVE_NONE
               && probCutCount < 2 + 2 * cutNode)
            if (move != excludedMove && pos.legal(move))
            {
                if (!pos.capture_or_promotion(move))
                    continue;
                assert(depth >= 5);

                captureOrPromotion = true;
                probCutCount++;

                ss->currentMove = move;
#ifdef ENABLE_COMPOUND_TURNS
                ss->currentMoveHistoryCompatible = true;
                Piece victim = captured_piece_or_on(pos, move);
                ss->currentMoveCapturedOpponent = victim != NO_PIECE
                                                && color_of(victim) != pos.side_to_move();
#endif
                ss->currentMovePiece = pos.moved_piece(move);
                ss->continuationHistory = &thisThread->continuationHistory[ss->inCheck]
                                                                          [captureOrPromotion]
                                                                          [history_slot(pos.moved_piece(move))]
                                                                          [to_sq(move)];

                pos.do_move(move, st);

                // Perform a preliminary qsearch to verify that the move holds
                value = -qsearch_impl<Logical, NonPV>(pos, ss+1, -probCutBeta, -probCutBeta+1);

                // If the qsearch held, perform the regular search
                if (value >= probCutBeta)
                    value = -search_impl<Logical, NonPV>(pos, ss+1, -probCutBeta, -probCutBeta+1, depth - 4, !cutNode);

                pos.undo_move(move);

                if (value >= probCutBeta)
                {
                    // if transposition table doesn't have equal or more deep info write probCut data into it
                    if ( !(ss->ttHit
                       && tte->depth() >= depth - 3
                       && ttValue != VALUE_NONE))
                        tte->save(posKey, value_to_tt(value, ss->ply), ttPv,
                            BOUND_LOWER,
                            depth - 3, move, ss->staticEval);
                    return value;
                }
            }
         ss->ttPv = ttPv;
    }

    // Step 10. If the position is not in TT, decrease depth by 2
    if (   PvNode
        && depth >= 6
        && !ttMove)
        depth -= 2;

moves_loop: // When in check, search starts from here

    ttCapture = ttMove && pos.capture_or_promotion(ttMove);

    // Step 11. A small Probcut idea, when we are in check
    probCutBeta = beta + 409;
    if (   ss->inCheck
        && !PvNode
        && depth >= 4
        && ttCapture
        && (tte->bound() & BOUND_LOWER)
        && tte->depth() >= depth - 3
        && ttValue >= probCutBeta
        && abs(ttValue) <= VALUE_KNOWN_WIN
        && abs(beta) <= VALUE_KNOWN_WIN
       )
        return probCutBeta;


    const PieceToHistory* contHist[] = { previousMoveHistoryCompatible ? (ss-1)->continuationHistory
                                                                       : neutralContinuationHistory,
                                          (ss-2)->continuationHistory,
                                          nullptr                   , (ss-4)->continuationHistory,
                                          nullptr                   , (ss-6)->continuationHistory };

    Move countermove = previousMoveHistoryCompatible
                     ? thisThread->counterMoves[pos.piece_on(prevSq)][prevSq]
                     : MOVE_NONE;

    MovePicker mp(pos, ttMove, depth, &thisThread->mainHistory,
                                      &thisThread->gateHistory,
                                      &thisThread->lowPlyHistory,
                                      &captureHistory,
                                      contHist,
                                      countermove,
                                      ss->killers,
                                      ss->ply);

    value = bestValue;
    singularQuietLMR = moveCountPruning = false;
    bool doubleExtension = false;

    // Indicate PvNodes that will probably fail low if the node was searched
    // at a depth equal or greater than the current depth, and the result of this search was a fail low.
    bool likelyFailLow =    PvNode
                         && ttMove
                         && (tte->bound() & BOUND_UPPER)
                         && tte->depth() >= depth;

#ifdef ENABLE_COMPOUND_TURNS
    LogicalMoveSourceStorage<Logical> logicalSource;
    if constexpr (Logical)
        if (logicalMovePosition && (!rootNode || lazyRoot))
        {
            const LogicalMove* preferredTurn = thisThread->logical_move_hint(pos.key());
            if (preferredTurn && ttMove && preferredTurn->first() != ttMove)
                preferredTurn = nullptr;
            logicalSource.source.emplace(pos, thisThread, thisThread->logical_move_state(ss->ply),
                                         true, ttMove, preferredTurn);
        }
#endif

    // Step 12. Loop through all pseudo-legal moves until no moves remain
    // or a beta cutoff occurs.
    bool bestMoveHistoryCompatible = false;
#ifdef ENABLE_COMPOUND_TURNS
    size_t rootMoveIndex = thisThread->pvIdx;
#endif
    while (true)
    {
      SearchMove logicalMove;
      SearchMoveInfo moveInfo;
      auto next_ordinary_move = [&] {
          if ((move = mp.next_move(moveCountPruning)) == MOVE_NONE)
              return false;
          logicalMove = move;
          if constexpr (Logical)
          {
              moveInfo.representative = move;
              moveInfo.historyCompatible = true;
          }
          return true;
      };
#ifdef ENABLE_COMPOUND_TURNS
      if constexpr (Logical)
      {
          if (logicalMovePosition && rootNode)
          {
              if (lazyRoot)
              {
                  if (!logicalSource.source->next(logicalMove, moveInfo))
                      break;
                  move = moveInfo.representative;
                  thisThread->rootMoves.emplace_back(logicalMove, moveInfo);
                  rootMoveIndex = thisThread->rootMoves.size();
              }
              else
              {
                  if (rootMoveIndex >= thisThread->pvLast)
                      break;
                  RootMove& rootMove = thisThread->rootMoves[rootMoveIndex++];
                  logicalMove = rootMove.first();
                  move = logicalMove.first();
                  moveInfo = rootMove.move_info();
              }
          }
          else if (logicalMovePosition)
          {
              if (!logicalSource.source->next(logicalMove, moveInfo))
                  break;
              move = moveInfo.representative;
          }
          else if (!next_ordinary_move())
              break;
      }
      else
#endif
          if (!next_ordinary_move())
              break;
      assert(is_ok(move));

      if (move == excludedMove)
          continue;

      // At root obey the "searchmoves" option and skip moves not listed in Root
      // Move List. As a consequence any illegal move is also skipped. In MultiPV
      // mode we also skip PV moves which have been already searched and those
      // of lower "TB rank" if we are in a TB root position.
      // Logical root moves are selected directly from rootMoves, so this
      // membership check is only needed for the ordinary MovePicker path.
      if constexpr (Logical)
      {
          if (!logicalMovePosition && rootNode
              && !std::count(thisThread->rootMoves.begin() + thisThread->pvIdx,
                             thisThread->rootMoves.begin() + thisThread->pvLast, logicalMove))
              continue;
      }
      else if (rootNode
               && !std::count(thisThread->rootMoves.begin() + thisThread->pvIdx,
                              thisThread->rootMoves.begin() + thisThread->pvLast, logicalMove))
          continue;

      // Check for legality
      if constexpr (Logical)
      {
          if (!logicalMovePosition && !rootNode && !pos.legal(move))
              continue;
      }
      else if (!rootNode && !pos.legal(move))
          continue;

      if constexpr (Logical)
      {
          if (!logicalMovePosition)
          {
              moveInfo.movedPiece = pos.moved_piece(move);
              moveInfo.removesMaterial = pos.capture(move);
              Piece captured = captured_piece_or_on(pos, move);
              moveInfo.capturesOpponent = captured != NO_PIECE && color_of(captured) != us;
              moveInfo.losesOwnMaterial = captured != NO_PIECE && color_of(captured) == us;
              moveInfo.promotionLike = is_promotion_move(move);
              moveInfo.givesCheck = pos.gives_check(move);
              moveInfo.seeReliable = true;
          }
      }
      else
      {
          moveInfo.movedPiece = pos.moved_piece(move);
          moveInfo.removesMaterial = pos.capture(move);
          Piece captured = captured_piece_or_on(pos, move);
          moveInfo.capturesOpponent = captured != NO_PIECE && color_of(captured) != us;
          moveInfo.promotionLike = is_promotion_move(move);
          moveInfo.givesCheck = pos.gives_check(move);
          moveInfo.seeReliable = true;
      }

      ss->moveCount = ++moveCount;

      if (rootNode && thisThread == Threads.main() && Time.elapsed() > 3000
          && is_uci_dialect(CurrentProtocol) && int(Options["Verbosity"]) >= 1)
      {
          sync_cout << "info depth " << depth << " currmove ";
#ifdef ENABLE_COMPOUND_TURNS
          if (logicalMovePosition)
          {
              if constexpr (Logical)
                  sync_cout << compound_move_to_string(pos, logicalMove);
          }
          else
              sync_cout << UCI::move(pos, move);
#else
          sync_cout << UCI::move(pos, move);
#endif
          sync_cout << " currmovenumber " << moveCount + thisThread->pvIdx << sync_endl;
      }
      if (PvNode)
          (ss+1)->set_pv<Logical>(nullptr);

      extension = 0;
      if constexpr (Logical)
          captureOrPromotion = logicalMovePosition ? moveInfo.capturesOpponent || moveInfo.promotionLike
                                                   : moveInfo.removesMaterial || moveInfo.promotionLike;
      else
          captureOrPromotion = moveInfo.removesMaterial || moveInfo.promotionLike;
      const bool materialChange = moveInfo.removesMaterial || moveInfo.promotionLike;
      movedPiece = moveInfo.movedPiece;
      givesCheck = moveInfo.givesCheck;

      // Calculate new depth for this move
      newDepth = depth - 1;

      // A forced-jump continuation pass is bookkeeping between two captures,
      // so it should not consume a search ply.
      if constexpr (Logical)
      {
          if (!logicalMovePosition && is_pass(move)
              && pos.forced_jump_continuation()
              && pos.has_forced_jump_followup())
          {
              Square forcedSquare = pos.forced_jump_square();
              Piece forcedPiece = pos.piece_on(forcedSquare);
              if (forcedPiece != NO_PIECE && color_of(forcedPiece) != us)
                  ++newDepth;
          }
      }
      else if (is_pass(move) && pos.forced_jump_continuation()
               && pos.has_forced_jump_followup())
      {
          Square forcedSquare = pos.forced_jump_square();
          Piece forcedPiece = pos.piece_on(forcedSquare);
          if (forcedPiece != NO_PIECE && color_of(forcedPiece) != us)
              ++newDepth;
      }

      // Step 13. Pruning at shallow depth (~200 Elo)
      if (  moveInfo.historyCompatible
          && !rootNode
          && (pos.non_pawn_material(us) || pos.count<ALL_PIECES>(us) == pos.count<PAWN>(us))
          && bestValue > VALUE_TB_LOSS_IN_MAX_PLY)
      {
          // Skip quiet moves if movecount exceeds our FutilityMoveCount threshold
          moveCountPruning = moveCount >= futility_move_count(improving, depth, pos);

          // Reduced depth of the next LMR search
          int lmrDepth = std::max(newDepth - reduction(improving, depth, moveCount), 0);

          if (pos.must_capture() && pos.attackers_to(to_sq(move), ~us))
          {}
          else

          if (   materialChange
              || givesCheck)
          {
              // Capture history based pruning when the move doesn't give check
              if (   !givesCheck
                  && lmrDepth < 1
                  && captureHistory[movedPiece][to_sq(move)][captured_type(pos, move)] < 0)
                  continue;

              // SEE based pruning
              if (moveInfo.seeReliable
                  && !pos.see_pruning_unreliable(move)
                  && !pos.see_ge(move, Value(-218 - 120 * pos.captures_to_hand()) * depth)) // (~25 Elo)
                  continue;
          }
          else
          {
              // Continuation history based pruning (~20 Elo)
              if (   lmrDepth < 5
                  && (*contHist[0])[history_slot(movedPiece)][to_sq(move)] < CounterMovePruneThreshold
                  && (*contHist[1])[history_slot(movedPiece)][to_sq(move)] < CounterMovePruneThreshold)
                  continue;

              // Futility pruning: parent node (~5 Elo)
              if (   lmrDepth < 7
                  && !ss->inCheck
                  && !pos.extinction_single_piece()
                  && ss->staticEval + (174 + 157 * lmrDepth) * (1 + pos.check_counting()) <= alpha
                  &&  (*contHist[0])[history_slot(movedPiece)][to_sq(move)]
                    + (*contHist[1])[history_slot(movedPiece)][to_sq(move)]
                    + (*contHist[3])[history_slot(movedPiece)][to_sq(move)]
                    + (*contHist[5])[history_slot(movedPiece)][to_sq(move)] / 3 < 28255)
                  continue;

              // Prune moves with negative SEE (~20 Elo)
              if (!(pos.walling_rule() == DUCK)
                  && moveInfo.seeReliable
                  && !pos.see_pruning_unreliable(move)
                  && !pos.see_ge(move, Value(-(30 - std::min(lmrDepth, 18) + 10 * !!pos.flag_region(pos.side_to_move())) * lmrDepth * lmrDepth)))
                  continue;
          }
      }

      // Step 14. Extensions (~75 Elo)

      // Singular extension search (~70 Elo). If all moves but one fail low on a
      // search of (alpha-s, beta-s), and just one fails high on (alpha, beta),
      // then that move is singular and should be extended. To verify this we do
      // a reduced search on all the other moves but the ttMove and if the
      // result is lower than ttValue minus a margin, then we will extend the ttMove.
      if (   exactTtMoveIdentity
          && moveInfo.historyCompatible
          && !rootNode
          &&  depth >= 7 - 2 * (pos.count<KING>() == 1)
          &&  move == ttMove
          && !excludedMove // Avoid recursive singular search
       /* &&  ttValue != VALUE_NONE Already implicit in the next condition */
          &&  abs(ttValue) < VALUE_KNOWN_WIN
          && (tte->bound() & BOUND_LOWER)
          &&  tte->depth() >= depth - 3)
      {
          Value singularBeta = ttValue - 2 * depth;
          Depth singularDepth = (depth - 1) / 2;

          ss->excludedMove = move;
          value = search_impl<Logical, NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
          ss->excludedMove = MOVE_NONE;

          if (value < singularBeta)
          {
              extension = 1;
              singularQuietLMR = !ttCapture;

              // Avoid search explosion by limiting the number of double extensions to at most 3
              if (   !PvNode
                  && value < singularBeta - 93
                  && ss->doubleExtensions < 3)
              {
                  extension = 2;
                  doubleExtension = true;
              }
          }

          // Multi-cut pruning
          // Our ttMove is assumed to fail high, and now we failed high also on a reduced
          // search without the ttMove. So we assume this expected Cut-node is not singular,
          // that multiple moves fail high, and we can prune the whole subtree by returning
          // a soft bound.
          else if (singularBeta >= beta)
              return singularBeta;

          // If the eval of ttMove is greater than beta we try also if there is another
          // move that pushes it over beta, if so also produce a cutoff.
          else if (ttValue >= beta)
          {
              ss->excludedMove = move;
              value = search_impl<Logical, NonPV>(pos, ss, beta - 1, beta, (depth + 3) / 2, cutNode);
              ss->excludedMove = MOVE_NONE;

              if (value >= beta)
                  return beta;
          }
      }
      else if (   givesCheck
               && depth > 6
               && abs(ss->staticEval) > Value(100))
          extension = 1;

      // Losing chess capture extension
      else if (   pos.must_capture()
               && moveInfo.historyCompatible
               && pos.capture(move)
               && (ss->inCheck || MoveList<CAPTURES>(pos).size() == 1))
          extension = 1;

      // Add extension to new depth
      newDepth += extension;
      ss->doubleExtensions = (ss-1)->doubleExtensions + (extension == 2);

      // Speculative prefetch as early as possible
      // Update the current move (this must be done after singular extension search)
#ifdef ENABLE_COMPOUND_TURNS
      ss->currentMoveHistoryCompatible = moveInfo.historyCompatible;
      ss->currentMoveCapturedOpponent = Logical ? moveInfo.capturesOpponent
                                                : false;
#endif
      ss->currentMove = moveInfo.historyCompatible ? move : MOVE_NONE;
      ss->currentMovePiece = moveInfo.historyCompatible ? movedPiece : NO_PIECE;
      ss->continuationHistory = moveInfo.historyCompatible
                              ? &thisThread->continuationHistory[ss->inCheck]
                                                                     [captureOrPromotion]
                                                                     [history_slot(movedPiece)]
                                                                     [to_sq(move)]
                              : neutralContinuationHistory;

      // Step 15. Make the move
#ifdef ENABLE_COMPOUND_TURNS
      LogicalMoveUndo* transaction = nullptr;
      if constexpr (Logical)
      {
          if (logicalMovePosition)
          {
              transaction = &thisThread->logical_move_state(ss->ply);
              pos.do_move(logicalMove, st, *transaction);
          }
          else
              pos.do_move(move, st);
      }
      else
          pos.do_move(move, st);
#endif
#ifndef ENABLE_COMPOUND_TURNS
      pos.do_move(move, st);
#endif

      // Step 16. Late moves reduction / extension (LMR, ~200 Elo)
      // We use various heuristics for the sons of a node after the first son has
      // been searched. In general we would like to reduce them, but there are many
      // cases where we extend a son if it has good chances to be "interesting".
      if (    depth >= 3
          && moveInfo.reductionEligible
          &&  moveCount > 1 + 2 * rootNode
          && !(pos.must_capture() && pos.has_capture())
          && (  !materialChange
              || (cutNode && (ss-1)->moveCount > 1)
              || !ss->ttPv)
          && (!PvNode || ss->ply > 1 || thisThread->id() % 4 != 3))
      {
          Depth r = reduction(improving, depth, moveCount);

          if (PvNode)
              r--;

          // Decrease reduction if the ttHit running average is large (~0 Elo)
          if (thisThread->ttHitAverage > 537 * TtHitAverageResolution * TtHitAverageWindow / 1024)
              r--;

          // Decrease reduction if position is or has been on the PV
          // and node is not likely to fail low. (~3 Elo)
          if (   ss->ttPv
              && !likelyFailLow)
              r -= 2;

          // Increase reduction at root and non-PV nodes when the best move does not change frequently
          if (   (rootNode || !PvNode)
              && thisThread->bestMoveChanges <= 2)
              r++;

          // Decrease reduction if opponent's move count is high (~1 Elo)
          if ((ss-1)->moveCount > 13)
              r--;

          // Decrease reduction if ttMove has been singularly extended (~1 Elo)
          if (singularQuietLMR)
              r--;

          // Increase reduction for cut nodes (~3 Elo)
          if (cutNode)
              r += 1 + !materialChange;

          if (!captureOrPromotion && moveInfo.historyCompatible)
          {
              // Increase reduction if ttMove is a capture (~3 Elo)
              if (ttCapture)
                  r++;

              const Square gate = gate_history_square(move);
              ss->statScore =  thisThread->mainHistory[us][from_to(move)]
                             + (gate != SQ_NONE ? thisThread->gateHistory[us][gate] * 2 : 0)
                             + (*contHist[0])[history_slot(movedPiece)][to_sq(move)]
                             + (*contHist[1])[history_slot(movedPiece)][to_sq(move)]
                             + (*contHist[3])[history_slot(movedPiece)][to_sq(move)]
                             - 4923;

              // Decrease/increase reduction for moves with a good/bad history (~30 Elo)
              if (!ss->inCheck)
                  r -= ss->statScore / (14721 - 4434 * pos.captures_to_hand());
          }

          if (!moveInfo.historyCompatible)
              r = 1;

          // In general we want to cap the LMR depth search at newDepth. But if
          // reductions are really negative and movecount is low, we allow this move
          // to be searched deeper than the first move, unless ttMove was extended by 2.
          Depth d = std::clamp(newDepth - r, 1, newDepth + (r < -1 && moveCount <= 5 && !doubleExtension));

          value = -search_impl<Logical, NonPV>(pos, ss+1, -(alpha+1), -alpha, d, true);

          // If the son is reduced and fails high it will be re-searched at full depth
          doFullDepthSearch = value > alpha && d < newDepth;
          didLMR = true;
      }
      else
      {
          doFullDepthSearch = !PvNode || moveCount > 1;
          didLMR = false;
      }

      // Step 17. Full depth search when LMR is skipped or fails high
      if (doFullDepthSearch)
      {
          value = -search_impl<Logical, NonPV>(pos, ss+1, -(alpha+1), -alpha, newDepth, !cutNode);

          // If the move passed LMR update its stats
          if (didLMR && moveInfo.historyCompatible && !captureOrPromotion)
          {
              int bonus = value > alpha ?  stat_bonus(newDepth)
                                        : -stat_bonus(newDepth);

              update_continuation_histories(ss, movedPiece, to_sq(move), bonus);
          }
      }

      // For PV nodes only, do a full PV search on the first move or after a fail
      // high (in the latter case search only if value < beta), otherwise let the
      // parent node fail low with value <= alpha and try another move.
      if (PvNode && (moveCount == 1 || (value > alpha && (rootNode || value < beta))))
      {
          (ss+1)->set_pv<Logical>(pv);
          pv[0] = SearchMove(MOVE_NONE);

          value = -search_impl<Logical, PV>(pos, ss+1, -beta, -alpha,
                                            std::min(maxNextDepth, newDepth), false);
      }

      // Step 18. Undo move
#ifdef ENABLE_COMPOUND_TURNS
      if constexpr (Logical)
      {
          if (logicalMovePosition)
          {
              pos.undo_move(logicalMove, *transaction);
          }
          else
              pos.undo_move(move);
      }
      else
          pos.undo_move(move);
#endif
#ifndef ENABLE_COMPOUND_TURNS
      pos.undo_move(move);
#endif

      assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

      // Step 19. Check for a new best move
      // Finished searching the move. If a stop occurred, the return value of
      // the search cannot be trusted, and we return immediately without
      // updating best move, PV and TT.
      if (Threads.stop.load(std::memory_order_relaxed))
          return VALUE_ZERO;

      if (rootNode)
      {
          RootMove& rm = [&]() -> RootMove& {
#ifdef ENABLE_COMPOUND_TURNS
              if (logicalMovePosition)
                  return thisThread->rootMoves[rootMoveIndex - 1];
#endif
              return *std::find(thisThread->rootMoves.begin(),
                                thisThread->rootMoves.end(), logicalMove);
          }();

          // PV move or new best move?
          if (moveCount == 1 || value > alpha)
          {
              rm.score = value;
              rm.selDepth = thisThread->selDepth;
              rm.clear_pv();

              assert((ss+1)->pv_ptr<Logical>());

              for (SearchMove* m = (ss+1)->pv_ptr<Logical>();
                   *m != MOVE_NONE; ++m)
                  rm.append_pv(*m);

              // We record how often the best move has been changed in each
              // iteration. This information is used for time management and LMR
              if (moveCount > 1)
                  ++thisThread->bestMoveChanges;
          }
          else
              // All other moves but the PV are set to the lowest value: this
              // is not a problem when sorting because the sort is stable and the
              // move position in the list is preserved - just the PV is pushed up.
              rm.score = -VALUE_INFINITE;
      }

      if (value > bestValue)
      {
          bestValue = value;

          if (value > alpha)
          {
              bestMove = move;
              bestSearchMove = logicalMove;
              bestMoveHistoryCompatible = moveInfo.historyCompatible;

              if (PvNode && !rootNode) // Update pv even in fail-high case
                  update_pv(ss->pv_ptr<Logical>(), logicalMove,
                            (ss+1)->pv_ptr<Logical>());

              if (PvNode && value < beta) // Update alpha! Always alpha < beta
                  alpha = value;
              else
              {
                  assert(value >= beta); // Fail high
                  break;
              }
          }
      }

      // If the move is worse than some previously searched move, remember it to update its stats later
      if (logicalMove != bestSearchMove)
      {
          if (moveInfo.historyCompatible && captureOrPromotion && captureCount < 32)
              capturesSearched[captureCount++] = move;

          else if (moveInfo.historyCompatible && !captureOrPromotion && quietCount < 64)
              quietsSearched[quietCount++] = move;
      }
    }

    if (lazyRoot)
    {
        if (thisThread->rootMoves.empty())
            thisThread->rootMoves.emplace_back(MOVE_NONE);
        thisThread->pvLast = thisThread->rootMoves.size();
    }

    // The following condition would detect a stop only after move loop has been
    // completed. But in this case bestValue is valid because we have fully
    // searched our subtree, and we can anyhow save the result in TT.
    /*
       if (Threads.stop)
        return VALUE_DRAW;
    */

    // Step 20. Check for mate and stalemate
    // All legal moves have been searched and if there are no legal moves, it
    // must be a mate or a stalemate. If we are in a singular extension search then
    // return a fail low score.

    assert(moveCount || !ss->inCheck || excludedMove
           || logicalMovePosition || !MoveList<LEGAL>(pos).size());

    if (!moveCount)
    {
        if (!excludedMove && !logicalMovePosition && pos.has_legal_move())
        {
            assert(false && "MovePicker missed a legal move");
            return ss->inCheck ? value_draw(pos.this_thread()) : evaluate(pos);
        }
        else
            bestValue = excludedMove ? alpha :
                        ss->inCheck  ? pos.checkmate_value(ss->ply)
                                     : pos.stalemate_value(ss->ply);
    }

    // If there is a move which produces search value greater than alpha we update stats of searched moves
    else if (bestMove && bestMoveHistoryCompatible)
        update_all_stats(pos, ss, bestMove, bestValue, beta, prevSq,
                         quietsSearched, quietCount, capturesSearched, captureCount, depth);

    // Bonus for prior countermove that caused the fail low
    else if (   previousMoveHistoryCompatible
             && (depth >= 3 || PvNode)
             && !priorCapture)
        update_continuation_histories(ss-1, pos.piece_on(prevSq), prevSq, stat_bonus(depth));

    if (PvNode)
        bestValue = std::min(bestValue, maxValue);

    // If no good move is found and the previous position was ttPv, then the previous
    // opponent move is probably good and the new position is added to the search tree.
    if (bestValue <= alpha)
        ss->ttPv = ss->ttPv || ((ss-1)->ttPv && depth > 3);
    // Otherwise, a counter move has been found and if the position is the last leaf
    // in the search tree, remove the position from the search tree.
    else if (depth > 3)
        ss->ttPv = ss->ttPv && (ss+1)->ttPv;

    // Write gathered information in transposition table
    const Move ttStoredMove = [&] {
        if constexpr (Logical)
            return logicalMovePosition ? bestSearchMove.first() : bestMove;
        return bestMove;
    }();
#ifdef ENABLE_COMPOUND_TURNS
    if constexpr (Logical)
        if (logicalMovePosition && !bestSearchMove.empty())
            thisThread->store_logical_move_hint(pos.key(), bestSearchMove);
#endif
    if (!excludedMove && !(rootNode && thisThread->pvIdx))
        tte->save(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                  bestValue >= beta ? BOUND_LOWER :
                  PvNode && bestMove ? BOUND_EXACT : BOUND_UPPER,
                  depth, ttStoredMove, ss->staticEval);

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
  }


  template <NodeType nodeType>
  Value search(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) {
#ifdef ENABLE_COMPOUND_TURNS
    if (pos.logical_moves_active() || pos.may_enter_logical_moves())
        return search_impl<true, nodeType>(pos, ss, alpha, beta, depth, cutNode);
#endif
    return search_impl<false, nodeType>(pos, ss, alpha, beta, depth, cutNode);
  }

  // qsearch_impl() is the quiescence search function, which is called by the
  // main search function with zero depth, or recursively with further
  // decreasing depth per call.
  template <bool Logical, NodeType nodeType>
  Value qsearch_impl(Position& pos, Stack* ss, Value alpha, Value beta, Depth depth) {

    static_assert(nodeType != Root);
    constexpr bool PvNode = nodeType == PV;

    const bool providerStaticOnly = [&] {
        if constexpr (Logical)
            return pos.logical_move_capabilities().quiescence == QuiescenceSupport::STATIC_ONLY;
        return false;
    }();
    if (providerStaticOnly)
    {
        Value result;
        if (pos.is_game_end(result, ss->ply))
            return result;
        if (!pos.has_legal_logical_move())
            return pos.stalemate_value(ss->ply);
        return Eval::evaluate(pos);
    }

    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(depth <= 0);

    using SearchMove = std::conditional_t<Logical, LogicalMove, Move>;
    Thread* thisThread = pos.this_thread();
    using LocalPv = PvBuffer<SearchMove, PvNode && !Logical>;
    LocalPv localPv{};
    SearchMove* pv = nullptr;
    if constexpr (PvNode)
    {
#ifdef ENABLE_COMPOUND_TURNS
        if constexpr (Logical)
            pv = thisThread->logical_pv(ss->ply);
        else
#endif
            pv = localPv.data();
    }
    StateInfo st;
    ASSERT_ALIGNED(&st, Eval::NNUE::CacheLineSize);

    TTEntry* tte;
    Key posKey;
    Move ttMove, move, bestMove;
    Depth ttDepth;
    Value bestValue, value, ttValue, futilityValue, futilityBase, oldAlpha;
    bool pvHit, givesCheck, captureOrPromotion, legalMoveFound;
    int moveCount;

    if (PvNode)
    {
        oldAlpha = alpha; // To flag BOUND_EXACT when eval above alpha and no available moves
        (ss+1)->set_pv<Logical>(pv);
        ss->pv_ptr<Logical>()[0] = MOVE_NONE;
    }

    bestMove = MOVE_NONE;
    ss->inCheck = pos.evasion_checkers();
    legalMoveFound = false;
    moveCount = 0;

    check_main_thread_time(thisThread);

    Value gameResult;
    if (pos.is_game_end(gameResult, ss->ply))
        return gameResult;

    // Check for maximum ply reached
    if (ss->ply >= MAX_PLY)
        return !ss->inCheck ? evaluate(pos) : VALUE_DRAW;

    // Safeguard against too deep recursions in quiescence search
    if (depth < DEPTH_QS_MAX && !ss->inCheck)
        return evaluate(pos);

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    // Decide whether or not to include checks: this fixes also the type of
    // TT entry depth that we are going to use. Note that in qsearch we use
    // only two types of depth in TT: DEPTH_QS_CHECKS or DEPTH_QS_NO_CHECKS.
    ttDepth = ss->inCheck || depth >= DEPTH_QS_CHECKS ? DEPTH_QS_CHECKS
                                                  : DEPTH_QS_NO_CHECKS;
    // Transposition table lookup
    posKey = pos.key();
    tte = TT.probe(posKey, ss->ttHit);
    ttValue = ss->ttHit ? value_from_tt(tte->value(), ss->ply, pos.rule50_count()) : VALUE_NONE;
    ttMove = ss->ttHit ? tte->move() : MOVE_NONE;
    pvHit = ss->ttHit && tte->is_pv();

    if (  !PvNode
        && ss->ttHit
        && tte->depth() >= ttDepth
        && ttValue != VALUE_NONE // Only in case of TT access race
        && (ttValue >= beta ? (tte->bound() & BOUND_LOWER)
                            : (tte->bound() & BOUND_UPPER)))
        return ttValue;

    // Evaluate the position statically
    if (ss->inCheck)
    {
        ss->staticEval = VALUE_NONE;
        bestValue = futilityBase = -VALUE_INFINITE;
    }
    else
    {
        if (ss->ttHit)
        {
            // Never assume anything about values stored in TT
            if ((ss->staticEval = bestValue = tte->eval()) == VALUE_NONE)
                ss->staticEval = bestValue = evaluate(pos);

            // Can ttValue be used as a better position evaluation?
            if (    ttValue != VALUE_NONE
                && (tte->bound() & (ttValue > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttValue;
        }
        else
            // In case of null move search use previous static eval with a different sign
            // and addition of two tempos
            ss->staticEval = bestValue =
            (ss-1)->currentMove != MOVE_NULL ? evaluate(pos)
                                             : -(ss-1)->staticEval;

        // Stand pat. Return immediately if static value is at least beta
        if (bestValue >= beta)
        {
            // Save gathered info in transposition table
            if (!ss->ttHit)
                tte->save(posKey, value_to_tt(bestValue, ss->ply), false, BOUND_LOWER,
                          DEPTH_NONE, MOVE_NONE, ss->staticEval);

            return bestValue;
        }

        if (PvNode && bestValue > alpha)
            alpha = bestValue;

        futilityBase = bestValue + 155;
    }

    const PieceToHistory* contHist[] = { history_compatible<Logical>(ss-1) ? (ss-1)->continuationHistory
                                                                             : &thisThread->continuationHistory[0][0][NO_PIECE][0],
                                          (ss-2)->continuationHistory,
                                          nullptr                   , (ss-4)->continuationHistory,
                                          nullptr                   , (ss-6)->continuationHistory };

    // Initialize a MovePicker object for the current position, and prepare
    // to search the moves. Because the depth is <= 0 here, only captures,
    // queen promotions, and other checks (only if depth >= DEPTH_QS_CHECKS)
    // will be generated.
    MovePicker mp(pos, ttMove, depth, &thisThread->mainHistory,
                                      &thisThread->gateHistory,
                                      &thisThread->captureHistory,
                                      contHist,
                                      history_compatible<Logical>(ss-1) ? to_sq((ss-1)->currentMove) : SQ_NONE);

    // Loop through the moves until no moves remain or a beta cutoff occurs
    while ((move = mp.next_move()) != MOVE_NONE)
    {
      assert(is_ok(move));

      if (Threads.stop.load(std::memory_order_relaxed))
          break;

      if (!pos.legal(move))
          continue;

      legalMoveFound = true;
      givesCheck = pos.gives_check(move);
      captureOrPromotion = pos.capture_or_promotion(move);
      Piece victim = captured_piece_or_on(pos, move);

      moveCount++;

      // Futility pruning and moveCount pruning
      if (    bestValue > VALUE_TB_LOSS_IN_MAX_PLY
          && !givesCheck
          && !(   pos.extinction_value(~pos.side_to_move()) == -VALUE_MATE
               && victim != NO_PIECE
               && (pos.extinction_piece_types(~pos.side_to_move()) & type_of(victim)))
          &&  futilityBase > -VALUE_KNOWN_WIN
          &&  type_of(move) != PROMOTION)
      {

          if (moveCount > 2)
              continue;

          futilityValue = futilityBase + PieceValue[EG][victim];

          if (futilityValue <= alpha)
          {
              bestValue = std::max(bestValue, futilityValue);
              continue;
          }

          if (futilityBase <= alpha
              && !pos.see_pruning_unreliable(move)
              && !pos.see_ge(move, VALUE_ZERO + 1))
          {
              bestValue = std::max(bestValue, futilityBase);
              continue;
          }
      }

      // Do not search moves with negative SEE values
      if (    bestValue > VALUE_TB_LOSS_IN_MAX_PLY
          && !pos.see_pruning_unreliable(move)
          && !pos.see_ge(move))
          continue;

      ss->currentMove = move;
#ifdef ENABLE_COMPOUND_TURNS
      ss->currentMoveHistoryCompatible = true;
      ss->currentMoveCapturedOpponent = victim != NO_PIECE
                                      && color_of(victim) != pos.side_to_move();
#endif
      ss->currentMovePiece = pos.moved_piece(move);
      ss->continuationHistory = &thisThread->continuationHistory[ss->inCheck]
                                                                [captureOrPromotion]
                                                                [history_slot(pos.moved_piece(move))]
                                                                [to_sq(move)];

      // Continuation history based pruning
      if (  !captureOrPromotion
          && bestValue > VALUE_TB_LOSS_IN_MAX_PLY
          && (*contHist[0])[history_slot(pos.moved_piece(move))][to_sq(move)] < CounterMovePruneThreshold
          && (*contHist[1])[history_slot(pos.moved_piece(move))][to_sq(move)] < CounterMovePruneThreshold)
          continue;

      // Make and search the move
      pos.do_move(move, st);
      value = -qsearch_impl<Logical, nodeType>(pos, ss+1, -beta, -alpha, depth - 1);
      pos.undo_move(move);

      assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

      // Check for a new best move
      if (value > bestValue)
      {
          bestValue = value;

          if (value > alpha)
          {
              bestMove = move;

              if (PvNode) // Update pv even in fail-high case
              {
                  if constexpr (Logical)
                      update_pv(ss->pv_ptr<Logical>(), LogicalMove(move),
                                (ss+1)->pv_ptr<Logical>());
                  else
                      update_pv(ss->pv_ptr<Logical>(), move,
                                (ss+1)->pv_ptr<Logical>());
              }

              if (PvNode && value < beta) // Update alpha here!
                  alpha = value;
              else
                  break; // Fail high
          }
       }
    }

    if (Threads.stop.load(std::memory_order_relaxed) && bestValue == -VALUE_INFINITE)
        return ss->inCheck ? value_draw(pos.this_thread()) : evaluate(pos);

    if (!legalMoveFound && ss->inCheck)
    {
        assert(!pos.has_legal_move());

        Value result;
        if (pos.is_game_end(result, ss->ply))
            return result;
        return pos.checkmate_value(ss->ply);
    }

    if (!legalMoveFound && bestValue == -VALUE_INFINITE)
        return evaluate(pos);

    // Save gathered info in transposition table
    tte->save(posKey, value_to_tt(bestValue, ss->ply), pvHit,
              bestValue >= beta ? BOUND_LOWER :
              PvNode && bestValue > oldAlpha  ? BOUND_EXACT : BOUND_UPPER,
              ttDepth, bestMove, ss->staticEval);

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
  }


  // value_to_tt() adjusts a mate or TB score from "plies to mate from the root" to
  // "plies to mate from the current position". Standard scores are unchanged.
  // The function is called before storing a value in the transposition table.

  Value value_to_tt(Value v, int ply) {

    assert(v != VALUE_NONE);

    return  v >= VALUE_TB_WIN_IN_MAX_PLY  ? v + ply
          : v <= VALUE_TB_LOSS_IN_MAX_PLY ? v - ply : v;
  }


  // value_from_tt() is the inverse of value_to_tt(): it adjusts a mate or TB score
  // from the transposition table (which refers to the plies to mate/be mated from
  // current position) to "plies to mate/be mated (TB win/loss) from the root". However,
  // for mate scores, to avoid potentially false mate scores related to the 50 moves rule
  // and the graph history interaction, we return an optimal TB score instead.

  Value value_from_tt(Value v, int ply, int r50c) {

    if (v == VALUE_NONE)
        return VALUE_NONE;

    if (v >= VALUE_TB_WIN_IN_MAX_PLY)  // TB win or better
    {
        if (v >= VALUE_MATE_IN_MAX_PLY && VALUE_MATE - v > 99 - r50c)
            return VALUE_MATE_IN_MAX_PLY - 1; // do not return a potentially false mate score

        return v - ply;
    }

    if (v <= VALUE_TB_LOSS_IN_MAX_PLY) // TB loss or worse
    {
        if (v <= VALUE_MATED_IN_MAX_PLY && VALUE_MATE + v > 99 - r50c)
            return VALUE_MATED_IN_MAX_PLY + 1; // do not return a potentially false mate score

        return v + ply;
    }

    return v;
  }


  // update_pv() adds current move and appends child pv[]

  template<typename SearchMove>
  void update_pv(SearchMove* pv, SearchMove move, SearchMove* childPv) {

    for (*pv++ = move; childPv && *childPv != MOVE_NONE; )
        *pv++ = *childPv++;
    *pv = MOVE_NONE;
  }


  // update_all_stats() updates stats at the end of search() when a bestMove is found

  void update_all_stats(const Position& pos, Stack* ss, Move bestMove, Value bestValue, Value beta, Square prevSq,
                        Move* quietsSearched, int quietCount, Move* capturesSearched, int captureCount, Depth depth) {

    int bonus1, bonus2;
    Color us = pos.side_to_move();
    Thread* thisThread = pos.this_thread();
    CapturePieceToHistory& captureHistory = thisThread->captureHistory;
    Piece moved_piece = pos.moved_piece(bestMove);
    PieceType captured = captured_type(pos, bestMove);

    bonus1 = stat_bonus(depth + 1);
    bonus2 = bestValue > beta + PawnValueMg ? bonus1                                 // larger bonus
                                            : std::min(bonus1, stat_bonus(depth));   // smaller bonus

    if (!pos.capture_or_promotion(bestMove))
    {
        // Increase stats for the best move in case it was a quiet move
        update_quiet_stats(pos, ss, bestMove, bonus2, depth);

        // Decrease stats for all non-best quiet moves
        for (int i = 0; i < quietCount; ++i)
        {
            if (!(pos.walling() && from_to(quietsSearched[i]) == from_to(bestMove)))
                thisThread->mainHistory[us][from_to(quietsSearched[i])] << -bonus2;
            const Square gate = gate_history_square(quietsSearched[i]);
            if (pos.walling() && gate != SQ_NONE)
                thisThread->gateHistory[us][gate] << -bonus2;
            update_continuation_histories(ss, pos.moved_piece(quietsSearched[i]), to_sq(quietsSearched[i]), -bonus2);
        }
    }
    else
    {
        // Increase stats for the best move in case it was a capture move
        captureHistory[moved_piece][to_sq(bestMove)][captured] << bonus1;
        const Square gate = gate_history_square(bestMove);
        if (pos.walling() && gate != SQ_NONE)
            thisThread->gateHistory[us][gate] << bonus1;
    }

    // Extra penalty for a quiet early move that was not a TT move or
    // main killer move in previous ply when it gets refuted.
#ifdef ENABLE_COMPOUND_TURNS
    if (   (ss-1)->currentMoveHistoryCompatible
        && ((ss-1)->moveCount == 1 + (ss-1)->ttHit || ((ss-1)->currentMove == (ss-1)->killers[0]))
        && !(ss-1)->currentMoveCapturedOpponent)
#else
    if (   ((ss-1)->moveCount == 1 + (ss-1)->ttHit || ((ss-1)->currentMove == (ss-1)->killers[0]))
        )
#endif
            update_continuation_histories(ss-1, pos.piece_on(prevSq), prevSq, -bonus1);

    // Decrease stats for all non-best capture moves
    for (int i = 0; i < captureCount; ++i)
    {
        moved_piece = pos.moved_piece(capturesSearched[i]);
        captured = captured_type(pos, capturesSearched[i]);
        if (!(pos.walling() && from_to(capturesSearched[i]) == from_to(bestMove)))
            captureHistory[moved_piece][to_sq(capturesSearched[i])][captured] << -bonus1;
        const Square gate = gate_history_square(capturesSearched[i]);
        if (pos.walling() && gate != SQ_NONE)
            thisThread->gateHistory[us][gate] << -bonus1;
    }
  }


  // update_continuation_histories() updates histories of the move pairs formed
  // by moves at ply -1, -2, -4, and -6 with current move.

  void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {

    for (int i : {1, 2, 4, 6})
    {
        // Only update first 2 continuation histories if we are in check
        if (ss->inCheck && i > 2)
            break;
        if (
#ifdef ENABLE_COMPOUND_TURNS
            (ss-i)->currentMoveHistoryCompatible &&
#endif
            is_ok((ss-i)->currentMove))
            (*(ss-i)->continuationHistory)[history_slot(pc)][to] << bonus;
    }
  }


  // update_quiet_stats() updates move sorting heuristics

  void update_quiet_stats(const Position& pos, Stack* ss, Move move, int bonus, int depth) {

    // Update killers
    if (ss->killers[0] != move)
    {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = move;
    }

    Color us = pos.side_to_move();
    Thread* thisThread = pos.this_thread();
    thisThread->mainHistory[us][from_to(move)] << bonus;
    const Square gate = gate_history_square(move);
    if (pos.walling() && gate != SQ_NONE)
        thisThread->gateHistory[us][gate] << bonus;
    update_continuation_histories(ss, pos.moved_piece(move), to_sq(move), bonus);

    // Penalty for reversed move in case of moved piece not being a pawn
    if (type_of(pos.moved_piece(move)) != PAWN && !is_drop_move(move))
        thisThread->mainHistory[us][from_to(reverse_move(move))] << -bonus;

    // Update countermove history
    if (
#ifdef ENABLE_COMPOUND_TURNS
        (ss-1)->currentMoveHistoryCompatible &&
#endif
        is_ok((ss-1)->currentMove))
    {
        Square prevSq = to_sq((ss-1)->currentMove);
        thisThread->counterMoves[pos.piece_on(prevSq)][prevSq] = move;
    }

    // Update low ply history
    if (depth > 11 && ss->ply < MAX_LPH)
        thisThread->lowPlyHistory[ss->ply][from_to(move)] << stat_bonus(depth - 7);
  }

  // When playing with strength handicap, choose best move among a set of RootMoves
  // using a statistical rule dependent on 'level'. Idea by Heinz van Saanen.

  LogicalMove Skill::pick_best(size_t multiPV) {

    const RootMoves& rootMoves = Threads.main()->rootMoves;
    static PRNG rng(now()); // PRNG sequence should be non-deterministic

    // RootMoves are already sorted by score in descending order
    Value topScore = rootMoves[0].score;
    int delta = std::min(topScore - rootMoves[multiPV - 1].score, PawnValueMg);
    int weakness = 120 - 2 * level;
    int maxScore = -VALUE_INFINITE;

    // Choose best move. For each move score we add two terms, both dependent on
    // weakness. One is deterministic and bigger for weaker levels, and one is
    // random. Then we choose the move with the resulting highest score.
    for (size_t i = 0; i < multiPV; ++i)
    {
        // This is our magic formula
        int push = (  weakness * int(topScore - rootMoves[i].score)
                    + delta * (rng.rand<unsigned>() % weakness)) / 128;

        if (rootMoves[i].score + push >= maxScore)
        {
            maxScore = rootMoves[i].score + push;
            best = rootMoves[i].first();
        }
    }

    return best;
  }

} // namespace


/// MainThread::check_time() is used to print debug info and, more importantly,
/// to detect when we are out of available time and thus stop the search.

void MainThread::check_time() {

  if (--callsCnt > 0)
      return;

  // When using nodes, ensure checking rate is not lower than 0.1% of nodes
  callsCnt = Limits.nodes ? std::min(1024, int(Limits.nodes / 1024)) : 1024;

  static TimePoint lastInfoTime = now();

  TimePoint elapsed = Time.elapsed();
  TimePoint tick = Limits.startTime + elapsed;

  if (tick - lastInfoTime >= 1000)
  {
      lastInfoTime = tick;
      dbg_print();
  }

  // We should not stop pondering until told so by the GUI
  if (ponder)
      return;

  if (   rootPos.two_boards()
      && Time.elapsed() < Limits.time[rootPos.side_to_move()] - 1000
      && (Partner.sitRequested || (Partner.weDead && !Partner.partnerDead) || Partner.weVirtualWin))
      return;

  if (   (Limits.use_time_management() && (elapsed > Time.maximum() - 10 || stopOnPonderhit))
      || (Limits.movetime && elapsed >= Limits.movetime)
      || (Limits.nodes && Threads.nodes_searched() >= (uint64_t)Limits.nodes))
      Threads.stop = true;
}


/// UCI::pv() formats PV information according to the UCI protocol. UCI requires
/// that all (if any) unsearched PV lines are sent using a previous search score.

string UCI::pv(const Position& pos, Depth depth, Value alpha, Value beta) {

  std::stringstream ss;
  TimePoint elapsed = Time.elapsed() + 1;
  const RootMoves& rootMoves = pos.this_thread()->rootMoves;
  size_t pvIdx = pos.this_thread()->pvIdx;
  size_t multiPV = std::min((size_t)Options["MultiPV"], rootMoves.size());
  uint64_t nodesSearched = Threads.nodes_searched();
  uint64_t tbHits = Threads.tb_hits() + (TB::RootInTB ? rootMoves.size() : 0);

  for (size_t i = 0; i < multiPV; ++i)
  {
      bool updated = rootMoves[i].score != -VALUE_INFINITE;

      if (depth == 1 && !updated && i > 0)
          continue;

      Depth d = updated ? depth : std::max(1, depth - 1);
      Value v = updated ? rootMoves[i].score : rootMoves[i].previousScore;

#ifdef ENABLE_COMPOUND_TURNS
      const bool logicalPv = pos.logical_moves_active() || pos.may_enter_logical_moves();
      const std::vector<std::string> compoundPv = logicalPv
                                                ? compound_pv_to_strings(pos, rootMoves[i].first(),
                                                                         rootMoves[i].continuation())
                                                : std::vector<std::string>();
#endif

      if (v == -VALUE_INFINITE)
          v = VALUE_ZERO;

      bool tb = TB::RootInTB && abs(v) < VALUE_MATE_IN_MAX_PLY;
      v = tb ? rootMoves[i].tbScore : v;

      if (ss.rdbuf()->in_avail()) // Not at first line
          ss << "\n";

      if (CurrentProtocol == XBOARD)
      {
          ss << d << " "
             << UCI::value(v) << " "
             << elapsed / 10 << " "
             << nodesSearched << " "
             << rootMoves[i].selDepth << " "
             << nodesSearched * 1000 / elapsed << " "
             << tbHits << "\t";

          // Do not print PVs with virtual drops in bughouse variants
          if (!pos.two_boards())
          {
#ifdef ENABLE_COMPOUND_TURNS
              if (logicalPv)
                  for (const std::string& move : compoundPv)
                      ss << " " << move;
              else
#endif
                  for (size_t j = 0; j < rootMoves[i].pv_size(); ++j)
                      ss << " " << UCI::move(pos, rootMoves[i].pv_at(j).first());
          }
      }
      else
      {
      ss << "info"
         << " depth "    << d
         << " seldepth " << rootMoves[i].selDepth
         << " multipv "  << i + 1
         << " score "    << UCI::value(v);

      if (Options["UCI_ShowWDL"])
          ss << UCI::wdl(v, pos.game_ply());

      if (!tb && i == pvIdx)
          ss << (v >= beta ? " lowerbound" : v <= alpha ? " upperbound" : "");

      ss << " nodes "    << nodesSearched
         << " nps "      << nodesSearched * 1000 / elapsed;

      if (elapsed > 1000) // Earlier makes little sense
          ss << " hashfull " << TT.hashfull();

      ss << " tbhits "   << tbHits
         << " time "     << elapsed
         << " pv";

#ifdef ENABLE_COMPOUND_TURNS
      if (logicalPv)
          for (const std::string& move : compoundPv)
              ss << " " << move;
      else
#endif
          for (size_t j = 0; j < rootMoves[i].pv_size(); ++j)
              ss << " " << UCI::move(pos, rootMoves[i].pv_at(j).first());
      }
  }

  return ss.str();
}


/// RootMove::extract_ponder_from_tt() is called in case we have no ponder move
/// before exiting the search, for instance, in case we stop the search during a
/// fail high at root. We try hard to have a ponder move to return to the GUI,
/// otherwise in case of 'ponder on' we have nothing to think on.

bool RootMove::extract_ponder_from_tt(Position& pos) {

    StateInfo st;
    ASSERT_ALIGNED(&st, Eval::NNUE::CacheLineSize);

    bool ttHit;

    assert(pv_size() == 1);

    if (first() == MOVE_NONE)
        return false;

    pos.do_move(first().first(), st);

    if (!pos.is_draw(1))
    {
        TTEntry* tte = TT.probe(pos.key(), ttHit);

        if (ttHit)
        {
            Move m = tte->move(); // Local copy to be SMP safe
            if (MoveList<LEGAL>(pos).contains(m))
                append_pv(LogicalMove(m));
        }
    }

    pos.undo_move(first().first());
    return pv_size() > 1;
}

void Tablebases::rank_root_moves(Position& pos, Search::RootMoves& rootMoves) {

    RootInTB = false;
    UseRule50 = bool(Options["Syzygy50MoveRule"]);
    ProbeDepth = int(Options["SyzygyProbeDepth"]);
    Cardinality = int(Options["SyzygyProbeLimit"]);
    bool dtz_available = true;

    // Tables with fewer pieces than SyzygyProbeLimit are searched with
    // ProbeDepth == DEPTH_ZERO
    if (Cardinality > MaxCardinality)
    {
        Cardinality = MaxCardinality;
        ProbeDepth = 0;
    }

    if (Cardinality >= popcount(pos.pieces()) && !pos.can_castle(ANY_CASTLING))
    {
        // Rank moves using DTZ tables
        RootInTB = root_probe(pos, rootMoves);

        if (!RootInTB)
        {
            // DTZ tables are missing; try to rank moves using WDL tables
            dtz_available = false;
            RootInTB = root_probe_wdl(pos, rootMoves);
        }
    }

    if (RootInTB)
    {
        // Sort moves according to TB rank
        std::stable_sort(rootMoves.begin(), rootMoves.end(),
                  [](const RootMove &a, const RootMove &b) { return a.tbRank > b.tbRank; } );

        // Probe during search only if DTZ is not available and we are winning
        if (dtz_available || rootMoves[0].tbScore <= VALUE_DRAW)
            Cardinality = 0;
    }
    else
    {
        // Clean up if root_probe() and root_probe_wdl() have failed
        for (auto& m : rootMoves)
            m.tbRank = 0;
    }
}

} // namespace Stockfish
