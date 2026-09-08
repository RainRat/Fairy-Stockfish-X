/*
  Fairy-Stockfish-X compound turn support
*/

#include "compound_turn.h"

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>
#include <functional>

#include "movegen.h"
#include "position.h"
#include "uci.h"

namespace Stockfish {

namespace {

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

using LogicalMoveCallback = std::function<bool(const LogicalMove&)>;

bool generate_turns(Position& pos,
                    const LogicalMoveCallback& callback,
                    LogicalMove& turn,
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

        turn.components[depth] = move;
        turn.length = uint8_t(depth + 1);

        pos.do_component(move, states[depth], false);
        bool repetitionIllegal = false;
        if (!is_pass(move) && usedSteps + moveCost < turnSteps)
        {
            StateInfo boundaryState;
            pos.end_compound_turn(boundaryState);
            repetitionIllegal = pos.same_player_board_repetition_illegal_at_turn_boundary();
            pos.undo_compound_turn();
        }
        else
            repetitionIllegal = pos.same_player_board_repetition_illegal_at_turn_boundary();

        bool keepGenerating = true;
        if ((is_pass(move) || pos.board_layout_key() != startBoardKey)
            && !repetitionIllegal)
            keepGenerating = callback(turn);

        if (keepGenerating && !is_pass(move)
            && usedSteps + moveCost < turnSteps
            && pos.compound_turn_active())
            keepGenerating = generate_turns(pos, callback, turn, states, depth + 1,
                                            usedSteps + moveCost, startBoardKey);

        pos.undo_component(move);
        if (!keepGenerating)
            return false;
    }

    return true;
}

} // namespace

std::vector<LogicalMove> generate_compound_moves(Position& pos) {

  std::vector<LogicalMove> turns;
  if (!pos.compound_turn_active())
      return turns;

  Value result;
  if (pos.is_game_end(result))
      return turns;

  LogicalMove turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS + 1];
  generate_turns(pos,
                 [&](const LogicalMove& candidate) {
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

  LogicalMove turn;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS + 1];
  bool found = false;
  generate_turns(pos,
                 [&](const LogicalMove&) {
                     found = true;
                     return false;
                 },
                 turn, states, 0, 0, pos.board_layout_key());
  return found;
}

bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn) {

  if (!pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  LogicalMove parsed;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS + 1];
  const Key startBoardKey = pos.board_layout_key();

  std::function<bool(size_t, int)> parse = [&](size_t offset, int usedSteps) {
      if (offset >= text.size())
          return false;
      if (parsed.length >= LogicalMove::MAX_COMPONENTS)
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
          parsed.components[index] = move;
          pos.do_component(move, states[index], false);
          const int nextUsedSteps = usedSteps + moveCost;

          bool accepted = false;
          if (next == text.size())
              accepted = is_pass(move) || pos.board_layout_key() != startBoardKey;
          else
              accepted = !is_pass(move) && parse(next + 1, nextUsedSteps);

          if (accepted)
          {
              pos.undo_component(move);
              return true;
          }

          pos.undo_component(move);
          --parsed.length;
      }

      return false;
  };

  if (!parse(0, 0))
      return false;

  int turnCost = 0;
  for (int i = 0; i < parsed.length; ++i)
  {
      turnCost += compound_move_cost(pos, parsed.components[i]);
      pos.do_component(parsed.components[i], states[i], false);
  }

  const bool partialTurn = !is_pass(parsed.components[parsed.length - 1])
                         && turnCost < pos.compound_turn_steps();
  bool repetitionIllegal = false;
  if (partialTurn)
  {
      StateInfo boundaryState;
      pos.end_compound_turn(boundaryState);
      repetitionIllegal = pos.same_player_board_repetition_illegal_at_turn_boundary();
      pos.undo_compound_turn();
  }
  else
      repetitionIllegal = pos.same_player_board_repetition_illegal_at_turn_boundary();

  for (int i = parsed.length - 1; i >= 0; --i)
      pos.undo_component(parsed.components[i]);

  if (repetitionIllegal)
      return false;

  turn = parsed;
  return true;
}

void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state,
                      LogicalMoveState& transaction) {

  pos.do_move(turn, state, transaction, false);
}

void undo_compound_move(Position& pos, const LogicalMove& turn,
                        LogicalMoveState& transaction) {

  pos.undo_move(turn, transaction);
}

std::string compound_move_to_string(Position& pos, const LogicalMove& turn) {

  std::string result;
  LogicalMoveState transaction;

  for (int i = 0; i < turn.length; ++i)
  {
      if (i)
          result += ',';
      result += compound_step_to_string(pos, turn.components[i]);
      // Formatting is a read-only operation. The component executor still
      // supplies scratch state so that effects are formatted in context.
      pos.do_component(turn.components[i], transaction.components[i], false);
  }

  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_component(turn.components[i]);

  return result;
}

uint64_t compound_perft(Position& pos, int depth, bool root) {

  if (depth <= 0)
      return 1;

  uint64_t nodes = 0;
  for (const LogicalMove& turn : generate_compound_moves(pos))
  {
      uint64_t count;
      if (depth <= 1)
          count = 1;
      else
      {
          StateInfo state;
          LogicalMoveState transaction;
          do_compound_move(pos, turn, state, transaction);
          count = compound_perft(pos, depth - 1, false);
          undo_compound_move(pos, turn, transaction);
      }
      nodes += count;
      if (root)
          sync_cout << compound_move_to_string(pos, turn) << ": " << count << sync_endl;
  }
  return nodes;
}

} // namespace Stockfish

#endif // ENABLE_COMPOUND_TURNS
