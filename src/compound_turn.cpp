/*
  Fairy-Stockfish-X compound turn support
*/

#include "compound_turn.h"

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>
#include <functional>
#include <unordered_map>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "timeman.h"
#include "uci.h"

namespace Stockfish {

namespace {

bool compound_should_stop(Thread& thread) {
    if (Threads.stop.load(std::memory_order_relaxed))
        return true;

    if (&thread == Threads.main())
        Threads.main()->check_time();

    // MainThread::check_time() enforces the hard limit. Recursive turn
    // search also needs the ordinary time-management target so that one
    // very large turn cannot run all the way to Time.maximum().
    // Match MainThread::check_time()'s normal sampling cadence rather than
    // reading the clock for every recursive candidate.
    const bool sampleTime = (thread.nodes.load(std::memory_order_relaxed) & 1023) == 0;
    if (sampleTime && !Threads.main()->ponder.load(std::memory_order_relaxed)
        && Search::Limits.use_time_management()
        && Time.elapsed() >= Time.optimum())
        Threads.stop = true;

    return Threads.stop.load(std::memory_order_relaxed);
}

int compound_move_cost(const Position& pos, Move move) {
    return pos.compound_turn_step_cost(move);
}

std::string compound_step_to_string(Position& pos, Move move) {
    if (!is_encoded_push(move))
        return UCI::move(pos, move);

    return UCI::square(pos, from_sq(move))
         + UCI::square(pos, to_sq(move))
         + "," + UCI::square(pos, encoded_push_square(move));
}

using CompoundMoveCallback = std::function<bool(const CompoundMove&)>;

bool generate_turns(Position& pos,
                    const CompoundMoveCallback& callback,
                    CompoundMove& turn,
                    StateInfo* states,
                    int depth,
                    int usedSteps,
                    Key startBoardKey)
{
    MoveList<LEGAL> moves(pos);
    const int turnSteps = pos.compound_turn_steps();

    for (const auto& move : moves)
    {
        // A pass completes the compound turn and cannot follow another step.
        if (depth != 0 && is_pass(move))
            continue;

        const int moveCost = compound_move_cost(pos, move);
        if (usedSteps + moveCost > turnSteps)
            continue;

        turn.steps[depth] = move;
        turn.length = uint8_t(depth + 1);

        pos.do_move(move, states[depth], false);
        bool repetitionIllegal = false;
        if (!is_pass(move) && usedSteps + moveCost < turnSteps)
        {
            StateInfo boundaryState;
            pos.end_compound_turn(boundaryState);
            repetitionIllegal = pos.compound_repetition_illegal();
            pos.undo_compound_turn();
        }
        else
            repetitionIllegal = pos.compound_repetition_illegal();

        bool keepGenerating = true;
        if ((is_pass(move) || pos.board_layout_key() != startBoardKey)
            && !repetitionIllegal)
            keepGenerating = callback(turn);

        if (keepGenerating && !is_pass(move)
            && usedSteps + moveCost < turnSteps
            && pos.compound_turn_active())
            keepGenerating = generate_turns(pos, callback, turn, states, depth + 1,
                                            usedSteps + moveCost, startBoardKey);

        pos.undo_move(move);
        if (!keepGenerating)
            return false;
    }

    return true;
}

} // namespace

std::vector<CompoundMove> generate_compound_moves(Position& pos) {

  std::vector<CompoundMove> turns;
  if (!pos.compound_turn_active())
      return turns;

  Value result;
  if (pos.is_game_end(result))
      return turns;

  CompoundMove turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
  generate_turns(pos,
                 [&](const CompoundMove& candidate) {
                     turns.push_back(candidate);
                     return true;
                 },
                 turn, states, 0, 0, pos.board_layout_key());
  return turns;
}

bool has_any_compound_move(Position& pos) {

  if (!pos.compound_turn_active())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  CompoundMove turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
  bool found = false;
  generate_turns(pos,
                 [&](const CompoundMove&) {
                     found = true;
                     return false;
                 },
                 turn, states, 0, 0, pos.board_layout_key());
  return found;
}

std::vector<CompoundMove> parse_root_compound_moves(Position& pos, const std::vector<std::string>& texts) {

  std::vector<CompoundMove> turns;
  for (const std::string& text : texts)
  {
      CompoundMove turn;
      if (pos.compound_turn_active())
      {
          if (parse_compound_move(pos, text, turn))
              turns.push_back(turn);
          continue;
      }

      std::string moveText = text;
      Move move = UCI::to_move(pos, moveText);
      for (const auto& legalMove : MoveList<LEGAL>(pos))
          if (move == legalMove)
          {
              turn.steps[0] = move;
              turn.length = 1;
              turns.push_back(turn);
              break;
          }
  }
  return turns;
}

bool root_compound_move_allowed(const CompoundMove& turn,
                                const std::vector<CompoundMove>& searchMoves,
                                bool searchMovesSpecified,
                                const std::vector<CompoundMove>& banMoves) {

  return (!searchMovesSpecified
          || (!searchMoves.empty()
              && std::find(searchMoves.begin(), searchMoves.end(), turn) != searchMoves.end()))
      && std::find(banMoves.begin(), banMoves.end(), turn) == banMoves.end();
}

bool parse_compound_move(Position& pos, const std::string& text, CompoundMove& turn) {

  if (!pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  CompoundMove parsed;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
  const Key startBoardKey = pos.board_layout_key();

  std::function<bool(size_t, int)> parse = [&](size_t offset, int usedSteps) {
      if (offset >= text.size())
          return false;
      if (parsed.length >= CompoundMove::MAX_STEPS)
          return false;

      for (const auto& move : MoveList<LEGAL>(pos))
      {
          if (is_pass(move) && parsed.length != 0)
              continue;

          const std::string moveText = compound_step_to_string(pos, move);
          if (text.compare(offset, moveText.size(), moveText) != 0)
              continue;

          const int moveCost = compound_move_cost(pos, move);
          if (usedSteps + moveCost > pos.compound_turn_steps())
              continue;

          const size_t next = offset + moveText.size();
          if (next != text.size() && text[next] != ',' && text[next] != ';')
              continue;

          const int index = parsed.length++;
          parsed.steps[index] = move;
          pos.do_move(move, states[index], false);
          const int nextUsedSteps = usedSteps + moveCost;

          bool accepted = false;
          if (next == text.size())
              accepted = is_pass(move) || pos.board_layout_key() != startBoardKey;
          else
              accepted = !is_pass(move) && parse(next + 1, nextUsedSteps);

          if (accepted)
          {
              pos.undo_move(move);
              return true;
          }

          pos.undo_move(move);
          --parsed.length;
      }

      return false;
  };

  if (!parse(0, 0))
      return false;

  int turnCost = 0;
  for (int i = 0; i < parsed.length; ++i)
  {
      turnCost += compound_move_cost(pos, parsed.steps[i]);
      pos.do_move(parsed.steps[i], states[i], false);
  }

  const bool partialTurn = !is_pass(parsed.steps[parsed.length - 1])
                         && turnCost < pos.compound_turn_steps();
  bool repetitionIllegal = false;
  if (partialTurn)
  {
      StateInfo boundaryState;
      pos.end_compound_turn(boundaryState);
      repetitionIllegal = pos.compound_repetition_illegal();
      pos.undo_compound_turn();
  }
  else
      repetitionIllegal = pos.compound_repetition_illegal();

  for (int i = parsed.length - 1; i >= 0; --i)
      pos.undo_move(parsed.steps[i]);

  if (repetitionIllegal)
      return false;

  turn = parsed;
  return true;
}

void do_compound_move(Position& pos, const CompoundMove& turn, StateInfo* states) {

  assert(pos.compound_turn_active());
  assert(turn.length > 0 && turn.length <= CompoundMove::MAX_STEPS);

  int turnCost = 0;

  for (int i = 0; i < turn.length; ++i)
  {
      assert(pos.legal(turn.steps[i]));
      turnCost += compound_move_cost(pos, turn.steps[i]);
      pos.do_move(turn.steps[i], states[i], false);
  }

  if (!is_pass(turn.steps[turn.length - 1])
      && turnCost < pos.compound_turn_steps())
      pos.end_compound_turn(states[turn.length]);
}

void undo_compound_move(Position& pos, const CompoundMove& turn) {

  assert(turn.length > 0 && turn.length <= CompoundMove::MAX_STEPS);

  int turnCost = 0;
  for (int i = 0; i < turn.length; ++i)
      turnCost += compound_move_cost(pos, turn.steps[i]);

  if (!is_pass(turn.steps[turn.length - 1])
      && turnCost < pos.compound_turn_steps())
      pos.undo_compound_turn();

  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_move(turn.steps[i]);
}

std::string compound_move_to_string(Position& pos, const CompoundMove& turn) {

  std::string result;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
  int turnCost = 0;

  for (int i = 0; i < turn.length; ++i)
  {
      if (i)
          result += ',';
      result += compound_step_to_string(pos, turn.steps[i]);
      turnCost += compound_move_cost(pos, turn.steps[i]);
      pos.do_move(turn.steps[i], states[i], false);
  }

  if (!is_pass(turn.steps[turn.length - 1])
      && turnCost < pos.compound_turn_steps())
      pos.end_compound_turn(states[turn.length]);

  if (!is_pass(turn.steps[turn.length - 1])
      && turnCost < pos.compound_turn_steps())
      pos.undo_compound_turn();
  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_move(turn.steps[i]);

  return result;
}

uint64_t compound_perft(Position& pos, int depth, bool root) {

  if (depth <= 0)
      return 1;

  uint64_t nodes = 0;
  for (const CompoundMove& turn : generate_compound_moves(pos))
  {
      uint64_t count;
      if (depth <= 1)
          count = 1;
      else
      {
          alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
          do_compound_move(pos, turn, states);
          count = compound_perft(pos, depth - 1, false);
          undo_compound_move(pos, turn);
      }
      nodes += count;
      if (root)
          sync_cout << compound_move_to_string(pos, turn) << ": " << count << sync_endl;
  }
  return nodes;
}

namespace {

Value search_compound_turns(Position& pos,
                            Thread& thread,
                            int depth,
                            int ply,
                            Value alpha,
                            Value beta,
                            bool& aborted,
                            std::unordered_map<Key, bool>* leafLegalCache,
                            std::unordered_map<Key, Value>* leafEvalCache);

bool search_compound_turn_candidates(Position& pos,
                                     Thread& thread,
                                     int depth,
                                     int ply,
                                     Value alpha,
                                     Value beta,
                                     bool& aborted,
                                     Value& best,
                                     CompoundMove& bestTurn,
                                     bool& foundTurn,
                                     uint64_t& visitedMoves,
                                     CompoundMove& turn,
                                     StateInfo* states,
                                     int stepDepth,
                                     int usedSteps,
                                     Key startBoardKey,
                                     const std::vector<CompoundMove>* rootSearchMoves,
                                     bool rootSearchMovesSpecified,
                                     const std::vector<CompoundMove>* rootBanMoves,
                                     std::unordered_map<Key, bool>* leafLegalCache,
                                     std::unordered_map<Key, Value>* leafEvalCache) {

  MoveList<LEGAL> moves(pos);

  for (const auto& move : moves)
  {
      // A pass completes the compound turn and cannot follow another step.
      if (stepDepth != 0 && is_pass(move))
          continue;

      ++visitedMoves;
      if ((rootSearchMoves == nullptr || foundTurn || visitedMoves > 1)
          && compound_should_stop(thread))
      {
          aborted = true;
          return false;
      }

      const int moveCost = compound_move_cost(pos, move);
      if (usedSteps + moveCost > pos.compound_turn_steps())
          continue;

      turn.steps[stepDepth] = move;
      turn.length = uint8_t(stepDepth + 1);
      pos.do_move(move, states[stepDepth], false);

      bool repetitionIllegal = false;
      if (!is_pass(move) && usedSteps + moveCost < pos.compound_turn_steps())
      {
          StateInfo boundaryState;
          pos.end_compound_turn(boundaryState);
          repetitionIllegal = pos.compound_repetition_illegal();
          pos.undo_compound_turn();
      }
      else
          repetitionIllegal = pos.compound_repetition_illegal();

      bool keepSearching = true;
      const bool rootTurn = rootSearchMoves != nullptr;
      const bool allowed = !rootTurn
                        || root_compound_move_allowed(turn, *rootSearchMoves,
                                                      rootSearchMovesSpecified, *rootBanMoves);
      if (allowed
          && (is_pass(move) || pos.board_layout_key() != startBoardKey)
          && !repetitionIllegal)
      {
          foundTurn = true;
          if (!bestTurn.length)
              bestTurn = turn;
          Value value;
          if (!is_pass(move) && usedSteps + moveCost < pos.compound_turn_steps())
          {
              StateInfo boundaryState;
              pos.end_compound_turn(boundaryState);
              thread.nodes.fetch_add(1, std::memory_order_relaxed);
              value = -search_compound_turns(pos, thread, depth - 1, ply + 1,
                                             -beta, -alpha, aborted, leafLegalCache,
                                             leafEvalCache);
              pos.undo_compound_turn();
          }
          else
          {
              thread.nodes.fetch_add(1, std::memory_order_relaxed);
              value = -search_compound_turns(pos, thread, depth - 1, ply + 1,
                                             -beta, -alpha, aborted, leafLegalCache,
                                             leafEvalCache);
          }

          if (aborted)
          {
              pos.undo_move(move);
              return false;
          }

          if (value > best)
          {
              best = value;
              bestTurn = turn;
          }
          alpha = std::max(alpha, value);
          keepSearching = alpha < beta;
      }

      if (keepSearching && !is_pass(move)
          && usedSteps + moveCost < pos.compound_turn_steps()
          && pos.compound_turn_active())
      {
          keepSearching = search_compound_turn_candidates(
            pos, thread, depth, ply, alpha, beta, aborted, best, bestTurn, foundTurn,
            visitedMoves, turn, states, stepDepth + 1, usedSteps + moveCost,
            startBoardKey, rootSearchMoves, rootSearchMovesSpecified, rootBanMoves,
            leafLegalCache, leafEvalCache);

          alpha = std::max(alpha, best);
          keepSearching = keepSearching && alpha < beta;
      }

      pos.undo_move(move);
      if (!keepSearching)
          return false;
  }

  return true;
}

Value search_compound_turns(Position& pos,
                            Thread& thread,
                            int depth,
                            int ply,
                            Value alpha,
                            Value beta,
                            bool& aborted,
                            std::unordered_map<Key, bool>* leafLegalCache,
                            std::unordered_map<Key, Value>* leafEvalCache) {

  // Repetition legality depends on the completed-turn StateInfo history, not
  // only on the current board key. Do not reuse history-blind TT bounds here.
  Value result;
  if (pos.is_game_end(result, ply))
      return result;
  if (depth <= 0)
  {
      bool hasLegalTurn;
      // At root depth one, all candidates share the same completed-turn
      // history prefix, so the boundary key is sufficient for this probe.
      // Deeper searches can reach the same key through different histories;
      // leave those probes uncached.
      if (leafLegalCache)
      {
          const Key key = pos.key();
          auto [it, inserted] = leafLegalCache->emplace(key, false);
          if (inserted)
              it->second = has_any_compound_move(pos);
          hasLegalTurn = it->second;
      }
      else
          hasLegalTurn = has_any_compound_move(pos);

      if (!hasLegalTurn)
          return pos.stalemate_value(ply);

      if (!leafEvalCache)
          return Eval::evaluate(pos);

      const Key key = pos.key() ^ make_key(pos.rule50_count());
      auto [it, inserted] = leafEvalCache->emplace(key, VALUE_NONE);
      if (inserted)
          it->second = Eval::evaluate(pos);
      return it->second;
  }
  if (compound_should_stop(thread))
  {
      aborted = true;
      return VALUE_DRAW;
  }

  Value best = -VALUE_INFINITE;
  CompoundMove bestTurn;
  bool foundTurn = false;
  uint64_t visitedMoves = 0;
  CompoundMove turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
  search_compound_turn_candidates(pos, thread, depth, ply, alpha, beta, aborted,
                                  best, bestTurn, foundTurn, visitedMoves, turn,
                                  states, 0, 0, pos.board_layout_key(), nullptr,
                                  false, nullptr, leafLegalCache, leafEvalCache);

  if (aborted)
      return VALUE_DRAW;

  if (!foundTurn)
      return pos.stalemate_value(ply);

  return best;
}

} // namespace

void search_compound(Thread& thread) {

  Position& pos = thread.rootPos;
  thread.compoundBestTurn = CompoundMove{};
  thread.compoundBestScore = -VALUE_INFINITE;
  thread.compoundCompletedDepth = 0;

  Value rootResult;
  // Setup has no on-board pieces yet in some variants, so generic extinction is not terminal.
  if (pos.compound_turn_active() && !pos.count_in_hand(ALL_PIECES)
      && pos.is_game_end(rootResult))
      return;

  const std::vector<CompoundMove> searchMoves =
      parse_root_compound_moves(pos, Search::Limits.compoundSearchMoves);
  const bool searchMovesSpecified = Search::Limits.compoundSearchMovesSpecified;
  const std::vector<CompoundMove> banMoves =
      parse_root_compound_moves(pos, Search::Limits.compoundBanMoves);

  // Setup is not a compound turn: the placer may make many consecutive
  // drops, while ordinary negamax would incorrectly negate after each one.
  // Return a legal placement directly until both pockets are empty.
  if (!pos.compound_turn_active())
  {
      MoveList<LEGAL> moves(pos);
      for (const auto& move : moves)
      {
          CompoundMove turn;
          turn.steps[0] = move.move;
          turn.length = 1;
          if (root_compound_move_allowed(turn, searchMoves, searchMovesSpecified, banMoves))
          {
              thread.compoundBestTurn = turn;
              thread.compoundBestScore = VALUE_DRAW;
              break;
          }
      }
      return;
  }

  // A parsed root restriction is already a legal turn. Keep it as a fallback
  // in case a strict limit fires before the filtered turn reaches evaluation.
  if (searchMovesSpecified)
      for (const CompoundMove& candidate : searchMoves)
          if (root_compound_move_allowed(candidate, searchMoves, searchMovesSpecified, banMoves))
          {
              thread.compoundBestTurn = candidate;
              thread.compoundBestScore = VALUE_DRAW;
              break;
          }

  const int maxDepth = Search::Limits.depth > 0 ? Search::Limits.depth : MAX_PLY;
  for (int depth = 1; depth <= maxDepth; ++depth)
  {
      if (thread.compoundBestTurn.length)
      {
          if (compound_should_stop(thread))
              break;
      }

      Value bestScore = -VALUE_INFINITE;
      CompoundMove bestTurn;
      bool aborted = false;
      bool foundTurn = false;
      uint64_t visitedMoves = 0;
      CompoundMove turn;
      std::unordered_map<Key, bool> leafLegalCache;
      std::unordered_map<Key, Value> leafEvalCache;
      // Only depth one is a flat scan of turns from one shared root history.
      // Do not carry these history-sensitive caches into deeper iterations.
      std::unordered_map<Key, bool>* leafLegalCachePtr = depth == 1 ? &leafLegalCache : nullptr;
      std::unordered_map<Key, Value>* leafEvalCachePtr = depth == 1 ? &leafEvalCache : nullptr;
      alignas(Eval::NNUE::CacheLineSize) StateInfo states[CompoundMove::MAX_STEPS + 1];
      search_compound_turn_candidates(
        pos, thread, depth, 0, -VALUE_INFINITE, VALUE_INFINITE, aborted, bestScore,
        bestTurn, foundTurn, visitedMoves, turn, states, 0, 0,
        pos.board_layout_key(), &searchMoves, searchMovesSpecified, &banMoves,
        leafLegalCachePtr, leafEvalCachePtr);

      if (aborted)
      {
          // Keep a legal fallback if the very first iteration is interrupted,
          // but never present that partial iteration as completed.
          if (!thread.compoundBestTurn.length && bestTurn.length)
          {
              thread.compoundBestTurn = bestTurn;
              thread.compoundBestScore = bestScore == -VALUE_INFINITE ? VALUE_DRAW : bestScore;
          }
          break;
      }

      if (!foundTurn)
          break;

      if (bestTurn.length)
      {
          thread.compoundBestTurn = bestTurn;
          thread.compoundBestScore = bestScore;
          thread.compoundCompletedDepth = depth;

          if (&thread == Threads.main() && int(Options["Verbosity"]) >= 1)
              sync_cout << "info depth " << depth
                        << " score " << UCI::value(bestScore)
                        << " nodes " << thread.nodes.load(std::memory_order_relaxed)
                        << " pv " << compound_move_to_string(pos, bestTurn)
                        << sync_endl;
      }

      if (Search::Limits.mate && !Threads.stop
          && ((bestScore >= VALUE_MATE_IN_MAX_PLY
               && VALUE_MATE - bestScore <= 2 * Search::Limits.mate)
              || (bestScore <= VALUE_MATED_IN_MAX_PLY
                  && VALUE_MATE + bestScore <= 2 * Search::Limits.mate)))
          Threads.stop = true;

  }

}

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS
