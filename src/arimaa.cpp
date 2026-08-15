/*
  Fairy-Stockfish-X Arimaa turn support
*/

#include "arimaa.h"

#include <algorithm>
#include <functional>

#include "evaluate.h"
#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "uci.h"

namespace Stockfish {

namespace {

bool arimaa_should_stop(Thread& thread) {
    if (Threads.stop.load(std::memory_order_relaxed))
        return true;

    if (&thread == Threads.main() && !(thread.nodes.load(std::memory_order_relaxed) & 1023))
        Threads.main()->check_time();

    return Threads.stop.load(std::memory_order_relaxed);
}

int arimaa_move_cost(Move move) {
    return is_arimaa_two_step(move) ? 2 : 1;
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
        bool repetitionIllegal = false;
        if (usedSteps + moveCost < ArimaaTurn::MAX_STEPS)
        {
            StateInfo boundaryState;
            pos.end_compound_turn(boundaryState);
            repetitionIllegal = pos.arimaa_repetition_illegal();
            pos.undo_compound_turn();
        }
        else
            repetitionIllegal = pos.arimaa_repetition_illegal();

        if (pos.board_layout_key() != startBoardKey && !repetitionIllegal)
            turns.push_back(turn);

        if (usedSteps + moveCost < ArimaaTurn::MAX_STEPS
            && pos.compound_turn_active())
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

  Value result;
  if (pos.is_game_end(result))
      return turns;

  ArimaaTurn turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS];
  generate_turns(pos, turns, turn, states, 0, 0, pos.board_layout_key());
  return turns;
}

std::vector<ArimaaTurn> parse_root_turns(Position& pos, const std::vector<std::string>& texts) {

  std::vector<ArimaaTurn> turns;
  for (const std::string& text : texts)
  {
      ArimaaTurn turn;
      if (pos.compound_turn_active())
      {
          if (parse_arimaa_turn(pos, text, turn))
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

bool root_turn_allowed(const ArimaaTurn& turn,
                       const std::vector<ArimaaTurn>& searchMoves,
                       const std::vector<ArimaaTurn>& banMoves) {

  return (searchMoves.empty()
          || std::find(searchMoves.begin(), searchMoves.end(), turn) != searchMoves.end())
      && std::find(banMoves.begin(), banMoves.end(), turn) == banMoves.end();
}

bool parse_arimaa_turn(Position& pos, const std::string& text, ArimaaTurn& turn) {

  if (!pos.variant()->arimaa || !pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
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
      if (offset >= text.size())
          return false;
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

          bool accepted = false;
          if (next == text.size())
              accepted = pos.board_layout_key() != startBoardKey;
          else
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

  int turnCost = 0;
  for (int i = 0; i < parsed.length; ++i)
  {
      turnCost += arimaa_move_cost(parsed.steps[i]);
      pos.do_move(parsed.steps[i], states[i], false);
  }

  const bool partialTurn = turnCost < pos.compound_turn_steps();
  bool repetitionIllegal = false;
  if (partialTurn)
  {
      StateInfo boundaryState;
      pos.end_compound_turn(boundaryState);
      repetitionIllegal = pos.arimaa_repetition_illegal();
      pos.undo_compound_turn();
  }
  else
      repetitionIllegal = pos.arimaa_repetition_illegal();

  for (int i = parsed.length - 1; i >= 0; --i)
      pos.undo_move(parsed.steps[i]);

  if (repetitionIllegal)
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
                          int depth,
                          Value alpha,
                          Value beta,
                          bool& aborted) {

  // Repetition legality depends on the completed-turn StateInfo history, not
  // only on the current board key. Do not reuse history-blind TT bounds here.
  Value result;
  if (pos.is_game_end(result))
      return result;
  if (depth <= 0)
      return Eval::evaluate(pos);
  if (arimaa_should_stop(thread))
  {
      aborted = true;
      return VALUE_DRAW;
  }

  std::vector<ArimaaTurn> turns = generate_arimaa_turns(pos);
  if (turns.empty())
      return pos.stalemate_value();

  Value best = -VALUE_INFINITE;
  ArimaaTurn bestTurn;
  for (const ArimaaTurn& turn : turns)
  {
      if (arimaa_should_stop(thread))
      {
          aborted = true;
          break;
      }

      alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
      do_arimaa_turn(pos, turn, states);
      thread.nodes.fetch_add(1, std::memory_order_relaxed);
      Value value = -search_arimaa_turns(pos, thread, depth - 1, -beta, -alpha, aborted);
      undo_arimaa_turn(pos, turn);

      if (aborted)
          return VALUE_DRAW;

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

  return best;
}

} // namespace

void search_arimaa(Thread& thread) {

  Position& pos = thread.rootPos;
  thread.arimaaBestTurn = ArimaaTurn{};
  thread.arimaaBestScore = -VALUE_INFINITE;
  thread.arimaaCompletedDepth = 0;

  const std::vector<ArimaaTurn> searchMoves =
      parse_root_turns(pos, Search::Limits.arimaaSearchMoves);
  const std::vector<ArimaaTurn> banMoves =
      parse_root_turns(pos, Search::Limits.arimaaBanMoves);

  // Setup is not a compound turn: the placer may make many consecutive
  // drops, while ordinary negamax would incorrectly negate after each one.
  // Return a legal placement directly until both pockets are empty.
  if (!pos.compound_turn_active())
  {
      MoveList<LEGAL> moves(pos);
      for (const auto& move : moves)
      {
          ArimaaTurn turn;
          turn.steps[0] = move.move;
          turn.length = 1;
          if (root_turn_allowed(turn, searchMoves, banMoves))
          {
              thread.arimaaBestTurn = turn;
              thread.arimaaBestScore = VALUE_DRAW;
              break;
          }
      }
      return;
  }

  const int maxDepth = Search::Limits.depth > 0 ? Search::Limits.depth : MAX_PLY;
  for (int depth = 1; depth <= maxDepth; ++depth)
  {
      if (thread.arimaaBestTurn.length)
      {
          if (&thread == Threads.main())
              Threads.main()->check_time();
          if (arimaa_should_stop(thread))
              break;
      }

      std::vector<ArimaaTurn> turns = generate_arimaa_turns(pos);
      turns.erase(std::remove_if(turns.begin(), turns.end(), [&](const ArimaaTurn& turn) {
          return !root_turn_allowed(turn, searchMoves, banMoves);
      }), turns.end());
      Value bestScore = -VALUE_INFINITE;
      ArimaaTurn bestTurn;
      bool aborted = false;
      for (const ArimaaTurn& turn : turns)
      {
          // Always complete at least one root turn so a GUI receives a legal
          // bestmove even if stop/quit arrives while the search is starting.
          if (bestTurn.length)
          {
              if (&thread == Threads.main())
                  Threads.main()->check_time();
              if (arimaa_should_stop(thread))
              {
                  aborted = true;
                  break;
              }
          }

          alignas(Eval::NNUE::CacheLineSize) StateInfo states[ArimaaTurn::MAX_STEPS + 1];
          do_arimaa_turn(pos, turn, states);
          thread.nodes.fetch_add(1, std::memory_order_relaxed);
          Value score = -search_arimaa_turns(pos, thread, depth - 1,
                                              -VALUE_INFINITE, -bestScore, aborted);
          undo_arimaa_turn(pos, turn);

          if (aborted)
              break;

          if (score > bestScore)
          {
              bestScore = score;
              bestTurn = turn;
          }
      }

      if (aborted)
      {
          // Keep a legal fallback if the very first iteration is interrupted,
          // but never present that partial iteration as completed.
          if (!thread.arimaaBestTurn.length && bestTurn.length)
          {
              thread.arimaaBestTurn = bestTurn;
              thread.arimaaBestScore = bestScore;
          }
          break;
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

  }

  if (&thread == Threads.main())
      Threads.stop = true;
}

} // namespace Stockfish
