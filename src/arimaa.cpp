/*
  Fairy-Stockfish-X Arimaa turn support
*/

#include "arimaa.h"

#include <algorithm>
#include <functional>
#include <unordered_map>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "uci.h"

namespace Stockfish {

namespace {

struct ArimaaTTEntry {
    int depth;
    Value value;
    Bound bound;
    ArimaaTurn best;
};

using ArimaaTT = std::unordered_map<Key, ArimaaTTEntry>;

bool arimaa_should_stop(Thread& thread) {
    if (Threads.stop.load(std::memory_order_relaxed))
        return true;

    if (&thread == Threads.main() && !(thread.nodes.load(std::memory_order_relaxed) & 1023))
        Threads.main()->check_time();

    return Threads.stop.load(std::memory_order_relaxed);
}

void order_arimaa_turns(std::vector<ArimaaTurn>& turns, const ArimaaTurn& best) {
    if (!best.length)
        return;

    auto it = std::find(turns.begin(), turns.end(), best);
    if (it != turns.end())
        std::iter_swap(turns.begin(), it);
}

int arimaa_move_cost(Move move) {
    return is_arimaa_two_step(move) ? 2 : 1;
}

bool arimaa_goal_reached(const Position& pos) {
    return pos.flag_reached(WHITE) || pos.flag_reached(BLACK);
}

std::string arimaa_step_to_string(Position& pos, Move move) {
    if (!is_arimaa_push(move))
        return UCI::move(pos, move);

    return UCI::square(pos, from_sq(move))
         + UCI::square(pos, to_sq(move))
         + "," + UCI::square(pos, arimaa_push_square(move));
}

void generate_turns(Position& pos,
                    std::vector<ArimaaTurn>& turns,
                    ArimaaTurn& turn,
                    StateInfo* states,
                    int depth,
                    int usedSteps,
                    Key startBoardKey)
{
    MoveList<LEGAL> moves(pos);

    for (const auto& move : moves)
    {
        if (is_pass(move))
            continue;

        const int moveCost = arimaa_move_cost(move);
        if (usedSteps + moveCost > ArimaaTurn::MAX_STEPS)
            continue;

        turn.steps[depth] = move;
        turn.length = uint8_t(depth + 1);

        pos.do_move(move, states[depth], false);
        if (pos.board_layout_key() != startBoardKey)
            turns.push_back(turn);

        Value result;
        if (usedSteps + moveCost < ArimaaTurn::MAX_STEPS
            && pos.compound_turn_active()
            && !arimaa_goal_reached(pos)
            && !pos.is_game_end(result))
            generate_turns(pos, turns, turn, states, depth + 1,
                           usedSteps + moveCost, startBoardKey);

        pos.undo_move(move);
    }
}

} // namespace

std::vector<ArimaaTurn> generate_arimaa_turns(Position& pos) {

  std::vector<ArimaaTurn> turns;
  if (!pos.variant()->arimaa || !pos.compound_turn_active())
      return turns;

  if (arimaa_goal_reached(pos))
      return turns;

  ArimaaTurn turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS];
  generate_turns(pos, turns, turn, states, 0, 0, pos.board_layout_key());
  return turns;
}

bool parse_arimaa_turn(Position& pos, const std::string& text, ArimaaTurn& turn) {

  if (!pos.variant()->arimaa || !pos.compound_turn_active() || text.empty())
      return false;

  ArimaaTurn parsed;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS];
  const Key startBoardKey = pos.board_layout_key();

  // Commas are retained as the turn separator for the simple coordinate
  // notation requested by the GUI contract.  They are also part of FSX's
  // single-step pull/pair/drop notation, so parse by matching legal moves
  // rather than splitting the string first.  Backtracking lets
  // "d3d2,e3,c2c3" resolve as a pull followed by a step, while
  // "a2a3,b2b3" remains two ordinary steps.  A semicolon is accepted as an
  // unambiguous separator for clients that prefer one.
  std::function<bool(size_t, int)> parse = [&](size_t offset, int usedSteps) {
      if (offset == text.size())
          return parsed.length > 0;
      if (parsed.length >= ArimaaTurn::MAX_STEPS)
          return false;

      for (const auto& move : MoveList<LEGAL>(pos))
      {
          if (is_pass(move))
              continue;

          const std::string moveText = arimaa_step_to_string(pos, move);
          if (text.compare(offset, moveText.size(), moveText) != 0)
              continue;

          const int moveCost = arimaa_move_cost(move);
          if (usedSteps + moveCost > ArimaaTurn::MAX_STEPS)
              continue;

          const size_t next = offset + moveText.size();
          if (next != text.size() && text[next] != ',' && text[next] != ';')
              continue;

          const int index = parsed.length++;
          parsed.steps[index] = move;
          pos.do_move(move, states[index], false);
          const int nextUsedSteps = usedSteps + moveCost;

          bool accepted = next == text.size()
                       && pos.board_layout_key() != startBoardKey;
          if (!accepted)
              accepted = parse(next + 1, nextUsedSteps);

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

  turn = parsed;
  return true;
}

void do_arimaa_turn(Position& pos, const ArimaaTurn& turn, StateInfo* states) {

  assert(pos.variant()->arimaa);
  assert(pos.compound_turn_active());
  assert(turn.length > 0 && turn.length <= ArimaaTurn::MAX_STEPS);

  int turnCost = 0;

  for (int i = 0; i < turn.length; ++i)
  {
      assert(pos.legal(turn.steps[i]));
      turnCost += arimaa_move_cost(turn.steps[i]);
      pos.do_move(turn.steps[i], states[i], false);
  }

  if (turnCost < pos.compound_turn_steps())
      pos.end_compound_turn(states[turn.length]);
}

void undo_arimaa_turn(Position& pos, const ArimaaTurn& turn) {

  assert(pos.variant()->arimaa);
  assert(turn.length > 0 && turn.length <= ArimaaTurn::MAX_STEPS);

  int turnCost = 0;
  for (int i = 0; i < turn.length; ++i)
      turnCost += arimaa_move_cost(turn.steps[i]);

  if (turnCost < pos.compound_turn_steps())
      pos.undo_compound_turn();

  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_move(turn.steps[i]);
}

std::string arimaa_turn_to_string(Position& pos, const ArimaaTurn& turn) {

  std::string result;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
  int turnCost = 0;

  for (int i = 0; i < turn.length; ++i)
  {
      if (i)
          result += ',';
      result += arimaa_step_to_string(pos, turn.steps[i]);
      turnCost += arimaa_move_cost(turn.steps[i]);
      pos.do_move(turn.steps[i], states[i], false);
  }

  if (turnCost < pos.compound_turn_steps())
      pos.end_compound_turn(states[turn.length]);

  if (turnCost < pos.compound_turn_steps())
      pos.undo_compound_turn();
  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_move(turn.steps[i]);

  return result;
}

uint64_t arimaa_perft(Position& pos, int depth, bool root) {

  uint64_t nodes = 0;
  for (const ArimaaTurn& turn : generate_arimaa_turns(pos))
  {
      uint64_t count;
      if (depth <= 1)
          count = 1;
      else
      {
          alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
          do_arimaa_turn(pos, turn, states);
          count = arimaa_perft(pos, depth - 1, false);
          undo_arimaa_turn(pos, turn);
      }
      nodes += count;
      if (root)
          sync_cout << arimaa_turn_to_string(pos, turn) << ": " << count << sync_endl;
  }
  return nodes;
}

namespace {

Value search_arimaa_turns(Position& pos,
                          Thread& thread,
                          ArimaaTT& tt,
                          int depth,
                          Value alpha,
                          Value beta) {

  const Value originalAlpha = alpha;

  Value result;
  if (pos.is_game_end(result))
      return result;
  if (depth <= 0)
      return Eval::evaluate(pos);

  auto ttIt = tt.find(pos.key());
  if (ttIt != tt.end() && ttIt->second.depth >= depth)
  {
      const ArimaaTTEntry& entry = ttIt->second;
      if (entry.bound == BOUND_EXACT
          || (entry.bound == BOUND_LOWER && entry.value >= beta)
          || (entry.bound == BOUND_UPPER && entry.value <= alpha))
          return entry.value;
  }

  std::vector<ArimaaTurn> turns = generate_arimaa_turns(pos);
  if (turns.empty())
      return pos.stalemate_value();

  if (ttIt != tt.end())
      order_arimaa_turns(turns, ttIt->second.best);

  Value best = -VALUE_INFINITE;
  ArimaaTurn bestTurn;
  for (const ArimaaTurn& turn : turns)
  {
      if (arimaa_should_stop(thread))
          break;

      alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
      do_arimaa_turn(pos, turn, states);
      thread.nodes.fetch_add(1, std::memory_order_relaxed);
      Value value = -search_arimaa_turns(pos, thread, tt, depth - 1, -beta, -alpha);
      undo_arimaa_turn(pos, turn);

      if (value > best)
      {
          best = value;
          bestTurn = turn;
      }
      alpha = std::max(alpha, value);
      if (alpha >= beta)
          break;
  }

  if (best == -VALUE_INFINITE)
      return VALUE_DRAW;

  if (tt.size() < (1u << 20) || tt.find(pos.key()) != tt.end())
      tt[pos.key()] = {depth, best,
                       best >= beta ? BOUND_LOWER
                       : best <= originalAlpha ? BOUND_UPPER : BOUND_EXACT,
                       bestTurn};
  return best;
}

} // namespace

void search_arimaa(Thread& thread) {

  Position& pos = thread.rootPos;
  thread.arimaaBestTurn = ArimaaTurn{};
  thread.arimaaBestScore = -VALUE_INFINITE;
  thread.arimaaCompletedDepth = 0;
  ArimaaTT tt;
  tt.reserve(1u << 16);

  const int maxDepth = Search::Limits.depth > 0 ? Search::Limits.depth : MAX_PLY;
  for (int depth = 1; depth <= maxDepth; ++depth)
  {
      if (Threads.stop && thread.arimaaBestTurn.length)
          break;

      std::vector<ArimaaTurn> turns = generate_arimaa_turns(pos);
      Value bestScore = -VALUE_INFINITE;
      ArimaaTurn bestTurn;
      for (const ArimaaTurn& turn : turns)
      {
          // Always complete at least one root turn so a GUI receives a legal
          // bestmove even if stop/quit arrives while the search is starting.
          if (Threads.stop && bestTurn.length)
              break;

          alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
          do_arimaa_turn(pos, turn, states);
          thread.nodes.fetch_add(1, std::memory_order_relaxed);
          Value score = -search_arimaa_turns(pos, thread, tt, depth - 1, -VALUE_INFINITE, VALUE_INFINITE);
          undo_arimaa_turn(pos, turn);

          if (score > bestScore)
          {
              bestScore = score;
              bestTurn = turn;
          }
      }

      if (bestTurn.length)
      {
          thread.arimaaBestTurn = bestTurn;
          thread.arimaaBestScore = bestScore;
          thread.arimaaCompletedDepth = depth;

          if (&thread == Threads.main() && int(Options["Verbosity"]) >= 1)
              sync_cout << "info depth " << depth
                        << " score cp " << int(bestScore)
                        << " nodes " << thread.nodes.load(std::memory_order_relaxed)
                        << " pv " << arimaa_turn_to_string(pos, bestTurn)
                        << sync_endl;
      }

      if (Search::Limits.depth > 0)
          break;
  }

  Threads.stop = true;
}

} // namespace Stockfish
