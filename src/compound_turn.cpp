/*
  Fairy-Stockfish-X compound turn support
*/

#include "compound_turn.h"

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>
#include <functional>

#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "uci.h"

namespace Stockfish {

namespace {

int compound_move_cost(const Position& pos, Move move) {
    return pos.compound_turn_step_cost(move);
}

bool compound_turn_candidate_accepted(Position& pos, Move move, int usedSteps,
                                      Key startBoundaryKey,
                                      const StateInfo* logicalRoot) {
    const int moveCost = compound_move_cost(pos, move);
    bool repetitionIllegal = false;
    if (!is_pass(move) && usedSteps + moveCost < pos.compound_turn_steps())
    {
        StateInfo boundaryState;
        pos.end_compound_turn(boundaryState);
        repetitionIllegal = pos.same_player_board_repetition_illegal(logicalRoot->previous);
        pos.undo_compound_turn();
    }
    else
        repetitionIllegal = pos.same_player_board_repetition_illegal(logicalRoot->previous);

    return (is_pass(move) || pos.compound_turn_boundary_key() != startBoundaryKey)
        && !repetitionIllegal;
}

void record_removed_piece(LogicalMoveInfo& info, Piece piece, Color mover) {
    if (piece == NO_PIECE)
        return;

    info.removesMaterial = true;
    if (color_of(piece) == mover)
        info.losesOwnMaterial = true;
    else
        info.capturesOpponent = true;
}

std::string compound_step_to_string(Position& pos, Move move) {
    if (!is_encoded_push(move))
        return UCI::move(pos, move);

    return UCI::square(pos, from_sq(move))
         + UCI::square(pos, to_sq(move))
         + "," + UCI::square(pos, encoded_push_square(move));
}

} // namespace

LogicalMoveSource::LogicalMoveSource(Position& pos_, Thread* thread_,
                                     LogicalMoveState& transaction_)
    : pos(pos_), thread(thread_), transaction(transaction_) {

  if (!pos.compound_turn_active())
  {
      finished = true;
      return;
  }

  Value result;
  if (pos.is_game_end(result))
  {
      finished = true;
      return;
  }

}

LogicalMoveSource::~LogicalMoveSource() = default;

void LogicalMoveSource::initialize_frame(int frameDepth) {
  ExtMove* scratch = nullptr;
  std::unique_ptr<ExtMove[]> ownedScratch;
  if (thread)
      scratch = thread->acquire_buffer();
  else
  {
      ownedScratch = std::make_unique<ExtMove[]>(MOVEGEN_OVERFLOW_CAPACITY);
      scratch = ownedScratch.get();
  }

  Frame& frame = frames[frameDepth];
  ExtMove* end = generate<LEGAL>(pos, scratch);
  assert(end - scratch <= MOVEGEN_OVERFLOW_CAPACITY);
  frame.moves.assign(scratch, end);
  frame.current = frame.moves.data();
  frame.end = frame.current + frame.moves.size();

  if (thread)
      thread->release_buffer(scratch);
}

void LogicalMoveSource::apply_path(int length) {
  for (int i = 0; i < length; ++i)
      pos.do_component(turn.components[i], transaction.components[i], false);
}

void LogicalMoveSource::undo_path(int length) {
  for (int i = length - 1; i >= 0; --i)
      pos.undo_component(turn.components[i]);
}

bool LogicalMoveSource::next(LogicalMove& move) {

  LogicalMoveInfo info;
  return next(move, info);
}

bool LogicalMoveSource::next(LogicalMove& move, LogicalMoveInfo& info) {

  if (finished)
      return false;

  if (!initialized)
  {
      startBoundaryKey = pos.compound_turn_boundary_key();
      logicalRoot = pos.state();
      initialize_frame(0);
      initialized = true;
  }

  for (;;)
  {
      if (descend)
      {
          ++depth;
          apply_path(depth);
          initialize_frame(depth);
          undo_path(depth);
          descend = false;
      }

      Frame& frame = frames[depth];
      if (frame.current == frame.end)
      {
          if (depth == 0)
          {
              finished = true;
              return false;
          }
          --depth;
          continue;
      }

      const Move component = *frame.current++;
      if (depth != 0 && is_pass(component))
          continue;

      const int usedSteps = [&] {
          int result = 0;
          for (int i = 0; i < depth; ++i)
              result += compound_move_cost(pos, turn.components[i]);
          return result;
      }();
      const int moveCost = compound_move_cost(pos, component);
      if (usedSteps + moveCost > pos.compound_turn_steps())
          continue;

      turn.components[depth] = component;
      turn.length = uint8_t(depth + 1);
      LogicalMoveInfo candidateInfo;
      candidateInfo.representative = turn.first();
      candidateInfo.movedPiece = pos.moved_piece(turn.components[0]);
      candidateInfo.historyCompatible = turn.is_single();
      candidateInfo.seeReliable = turn.is_single()
                               && !pos.see_pruning_unreliable(turn.components[0]);
      candidateInfo.givesCheck = turn.is_single() && pos.gives_check(turn.components[0]);
      const Color mover = pos.side_to_move();
      apply_path(depth + 1);

      for (int i = 0; i <= depth; ++i)
      {
          const StateInfo& componentState = transaction.components[i];
          record_removed_piece(candidateInfo, componentState.captured.piece.piece, mover);
          record_removed_piece(candidateInfo, componentState.jumpedEnPassantCaptured.piece.piece, mover);
          record_removed_piece(candidateInfo, componentState.dead.piece, mover);

          Bitboard removed = componentState.bycatchSquares
                           & ~componentState.blastPromotedSquares
                           & ~componentState.laserTransformedSquares;
          while (removed)
          {
              Square square = pop_lsb(removed);
              record_removed_piece(candidateInfo, componentState.bycatchPieces[square].piece(), mover);
          }

          for (int transfer = 0; transfer < componentState.push.transferCount; ++transfer)
              record_removed_piece(candidateInfo, componentState.push.transfers[transfer].piece, mover);

          candidateInfo.promotionLike = candidateInfo.promotionLike
                                      || is_promotion_move(turn.components[i])
                                      || componentState.promotionPawn != NO_PIECE
                                      || componentState.consumedPromotionHandPiece != NO_PIECE;
      }

      const bool accepted = compound_turn_candidate_accepted(pos, component, usedSteps,
                                                              startBoundaryKey, logicalRoot);
      const bool canDescend = !is_pass(component)
                           && usedSteps + moveCost < pos.compound_turn_steps()
                           && pos.compound_turn_active();

      undo_path(depth + 1);
      descend = canDescend;

      if (accepted)
      {
          move = turn;
          info = candidateInfo;
          return true;
      }
  }
}

std::vector<LogicalMove> generate_compound_moves(Position& pos) {

  std::vector<LogicalMove> turns;
  if (!pos.compound_turn_active())
      return turns;

  Value result;
  if (pos.is_game_end(result))
      return turns;

  LogicalMoveState transaction;
  LogicalMoveSource source(pos, pos.this_thread(), transaction);
  LogicalMove turn;
  while (source.next(turn))
      turns.push_back(turn);
  return turns;
}

bool has_any_compound_move(Position& pos) {

  if (!pos.compound_turn_active())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  LogicalMoveState transaction;
  LogicalMoveSource source(pos, pos.this_thread(), transaction);
  LogicalMove turn;
  return source.next(turn);
}

bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn) {

  if (!pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  LogicalMove parsed;
  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS + 1];
  const Key startBoundaryKey = pos.compound_turn_boundary_key();
  const StateInfo* logicalRoot = pos.state();

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
              accepted = compound_turn_candidate_accepted(pos, move, usedSteps,
                                                          startBoundaryKey, logicalRoot);
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

std::vector<std::string> compound_pv_to_strings(const Position& pos,
                                                const std::vector<LogicalMove>& pv) {

  std::vector<std::string> result;
  result.reserve(pv.size());

  Position replay;
  StateListPtr states(new std::deque<StateInfo>(1));
  replay.set(pos.variant(), pos.fen(), pos.is_chess960(), &states->back(), pos.this_thread());

  LogicalMoveState transaction;
  for (const LogicalMove& move : pv)
  {
      if (move.first() == MOVE_NONE)
          break;

      states->emplace_back();
      if (replay.compound_turn_active())
      {
          result.push_back(compound_move_to_string(replay, move));
          replay.do_move(move, states->back(), transaction, false);
      }
      else
      {
          result.push_back(UCI::move(replay, move.first()));
          replay.do_move(move.first(), states->back(), false);
      }
  }

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
