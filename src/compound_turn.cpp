/*
  Fairy-Stockfish-X compound turn support
*/

#include "compound_turn_search.h"
#include "compound_turn_internal.h"

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>

#include "movegen.h"
#include "uci.h"

namespace Stockfish {

void CompoundTurnAdapter::do_component(Position& pos, Move move, StateInfo& state,
                                       bool& previousTurnReset, bool countNode) {
    previousTurnReset = pos.compoundTurnReset;
    pos.do_component_impl<true>(move, state, countNode);
}

void CompoundTurnAdapter::undo_component(Position& pos, Move move,
                                         bool previousTurnReset) {
    pos.undo_component_impl<true>(move, previousTurnReset);
}

namespace {

int compound_move_cost(const Position& pos, Move move) {
    return pos.compound_turn_step_cost(move);
}

struct CompoundStepInfo {
    int nextUsedSteps;
    bool accepted;
    bool canDescend;
};

CompoundStepInfo classify_compound_step(Position& pos, Move move, int usedSteps,
                                        Key startBoundaryKey,
                                        const StateInfo* logicalRoot) {
    const int nextUsedSteps = usedSteps + compound_move_cost(pos, move);
    const bool boundaryChanged = is_pass(move)
                               || pos.compound_turn_boundary_key() != startBoundaryKey;
    const bool additionalBoundaryPly = !is_pass(move)
                                    && nextUsedSteps < pos.compound_turn_steps();
    const bool accepted = boundaryChanged
                       && !pos.same_player_board_repetition_illegal(
                              logicalRoot->previous, additionalBoundaryPly);
    return {nextUsedSteps, accepted,
            !is_pass(move) && additionalBoundaryPly && pos.compound_turn_active()};
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

void accumulate_component_info(LogicalMoveInfo& info, const StateInfo& state,
                               Move component, Color mover) {
    record_removed_piece(info, state.captured.piece.piece, mover);
    record_removed_piece(info, state.jumpedEnPassantCaptured.piece.piece, mover);
    record_removed_piece(info, state.dead.piece, mover);

    Bitboard removed = state.bycatchSquares
                     & ~state.blastPromotedSquares
                     & ~state.laserTransformedSquares;
    while (removed)
    {
        Square square = pop_lsb(removed);
        record_removed_piece(info, state.bycatchPieces[square].piece(), mover);
    }

    for (int transfer = 0; transfer < state.push.transferCount; ++transfer)
        record_removed_piece(info, state.push.transfers[transfer].piece, mover);

    info.promotionLike = info.promotionLike
                       || is_promotion_move(component)
                       || state.promotionPawn != NO_PIECE
                       || state.consumedPromotionHandPiece != NO_PIECE;
}

std::string compound_step_to_string(Position& pos, Move move) {
    if (!is_encoded_push(move))
        return UCI::move(pos, move);

    return UCI::square(pos, from_sq(move))
         + UCI::square(pos, to_sq(move))
         + "," + UCI::square(pos, encoded_push_square(move));
}

// Recursive-descent parser for one compound turn token ("step[,step...]",
// ';' separators are also accepted). Depth is bounded by
// LogicalMove::MAX_COMPONENTS, so plain member recursion needs no
// type-erased std::function wrapper.
struct CompoundMoveParser {
    Position& pos;
    const std::string& text;
    Key startBoundaryKey;
    const StateInfo* logicalRoot;
    LogicalMoveUndo transaction;
    LogicalMove parsed{};

    bool parse_level(size_t offset, int usedSteps) {
        if (offset >= text.size() || parsed.size() >= LogicalMove::MAX_COMPONENTS)
            return false;

        for (const auto& move : MoveList<LEGAL_COMPONENTS>(pos))
        {
            if (is_pass(move) && !parsed.empty())
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

            const int index = parsed.size();
            CompoundTurnBuilder::push_back(parsed, move);
            CompoundTurnAdapter::do_component(pos, move, transaction, index, false);

            bool accepted = false;
            const CompoundStepInfo step = classify_compound_step(
                pos, move, usedSteps, startBoundaryKey, logicalRoot);
            if (next == text.size())
                accepted = step.accepted;
            else
                accepted = step.canDescend && parse_level(next + 1, step.nextUsedSteps);

            CompoundTurnAdapter::undo_component(pos, move, transaction, index);
            if (accepted)
                return true;

            CompoundTurnBuilder::pop_back(parsed);
        }

        return false;
    }
};

} // namespace

LogicalMoveSource::LogicalMoveSource(Position& pos_, LogicalMoveWorkspace& workspace_,
                                     LogicalMoveOrder order_, bool checkGameEnd)
    : pos(pos_), order(order_) {

  if (workspace_.inUse)
  {
      nestedWorkspace = std::make_unique<LogicalMoveWorkspace>();
      workspace = nestedWorkspace.get();
  }
  else
  {
      workspace = &workspace_;
      workspace->inUse = true;
      ownsWorkspace = true;
  }
  transaction = &workspace->undo;

  if (!pos.compound_turn_active())
  {
      finished = true;
      return;
  }

  Value result;
  if (checkGameEnd && pos.is_game_end(result))
  {
      finished = true;
      return;
  }

  logicalRoot = pos.state();
  startBoundaryKey = pos.compound_turn_boundary_key();
}

LogicalMoveSource::~LogicalMoveSource() {

  if (yielded)
      undo_yielded();
  else if (prefixApplied)
      unwind_prefix();
  if (ownsWorkspace)
      workspace->inUse = false;
}

void LogicalMoveSource::initialize_frame(int frameDepth) {
  Frame& frame = frames[frameDepth];
  auto& moves = workspace->moveLists[frameDepth];
  frame.usedCost = frameDepth == 0
                 ? 0
                 : frames[frameDepth - 1].usedCost
                 + compound_move_cost(pos, turn[frameDepth - 1]);
  moves.clear();
  for (const auto& move : MoveList<LEGAL_COMPONENTS>(pos))
      moves.push_back(move);

  // The provider does not have a Stack/MovePicker, but a small amount of
  // complete-turn ordering is still useful. Follow a remembered logical turn,
  // fall back to the TT's first-component hint, and prioritize tactical steps.
  auto tacticalBegin = moves.begin();
  Move preferred = MOVE_NONE;
  if (order.preferredTurn && !order.preferredTurn->empty()
      && frameDepth < order.preferredTurn->size()
      && std::equal(turn.begin(), turn.begin() + frameDepth,
                    order.preferredTurn->begin()))
      preferred = (*order.preferredTurn)[frameDepth];
  else if (frameDepth == 0)
      preferred = order.preferredFirst;

  if (preferred != MOVE_NONE)
  {
      auto preferredIt = std::find(moves.begin(), moves.end(), preferred);
      if (preferredIt != moves.end())
      {
          std::rotate(moves.begin(), preferredIt, preferredIt + 1);
          tacticalBegin = moves.begin() + 1;
      }
  }
  std::partition(tacticalBegin, moves.end(),
                 [&](Move move) { return pos.capture_or_promotion(move); });
  frame.current = 0;
}

void LogicalMoveSource::apply_path(int length) {
  for (int i = 0; i < length; ++i)
      CompoundTurnAdapter::do_component(pos, turn[i], *transaction, i, false);
}

void LogicalMoveSource::unwind_prefix() {
  for (int i = depth - 1; i >= 0; --i)
      CompoundTurnAdapter::undo_component(pos, turn[i], *transaction, i);
  prefixApplied = false;
}

bool LogicalMoveSource::next(LogicalMove& move) {

  return next_impl(move, nullptr, nullptr);
}

bool LogicalMoveSource::next(LogicalMove& move, LogicalMoveInfo& info) {

  return next_impl(move, &info, nullptr);
}

bool LogicalMoveSource::next_applied(LogicalMove& move, LogicalMoveInfo& info,
                                     StateInfo& committedState, bool captureNotation) {

  return next_impl(move, &info, &committedState, captureNotation);
}

void LogicalMoveSource::undo_yielded() {

  assert(yielded);
  pos.undo_move(turn, *transaction, true);
  unwind_prefix();
  yielded = false;
  if (notation)
      notation->yielded.clear();
}

bool LogicalMoveSource::next_impl(LogicalMove& move, LogicalMoveInfo* info,
                                  StateInfo* committedState, bool captureNotation) {

  if (finished)
      return false;
  assert(!yielded);

  if (!initialized)
  {
      initialize_frame(0);
      initialized = true;
      prefixApplied = true;
  }
  else if (!prefixApplied)
  {
      apply_path(depth);
      prefixApplied = true;
  }

  for (;;)
  {
      if (descend)
      {
          ++depth;
          CompoundTurnAdapter::do_component(pos, turn[depth - 1],
                                             *transaction, depth - 1, false);
          initialize_frame(depth);
          descend = false;
      }

      Frame& frame = frames[depth];
      auto& moves = workspace->moveLists[depth];
      if (frame.current == moves.size())
      {
          if (depth == 0)
          {
              finished = true;
              return false;
          }
          CompoundTurnAdapter::undo_component(pos, turn[depth - 1], *transaction,
                                              depth - 1);
          --depth;
          continue;
      }

      const Move component = moves[frame.current++];
      if (depth != 0 && is_pass(component))
          continue;

      const int usedSteps = frame.usedCost;
      const int moveCost = compound_move_cost(pos, component);
      if (usedSteps + moveCost > pos.compound_turn_steps())
          continue;

      CompoundTurnBuilder::set_component(turn, depth, component);
      const Color mover = pos.side_to_move();
      if (depth == 0)
      {
          firstMovedPiece = pos.moved_piece(component);
          firstSeeReliable = !pos.see_pruning_unreliable(component);
          firstGivesCheck = pos.gives_check(component);
      }
      CompoundTurnAdapter::do_component(pos, component,
                                         *transaction, depth, false);

      if (info)
      {
          Frame& componentFrame = frames[depth];
          componentFrame.info = depth ? frames[depth - 1].info : LogicalMoveInfo();
          componentFrame.info.representative = turn.first();
          componentFrame.info.movedPiece = firstMovedPiece;
          const bool ordinaryHeuristicCompatible = turn.is_single()
                                                  && !is_two_step_move(turn.first());
          componentFrame.info.historyCompatible = ordinaryHeuristicCompatible;
          componentFrame.info.seeReliable = ordinaryHeuristicCompatible && firstSeeReliable;
          componentFrame.info.givesCheck = turn.is_single() && firstGivesCheck;

          accumulate_component_info(componentFrame.info,
                                    transaction->components[depth], component, mover);
      }

      transaction->previous = logicalRoot;
      const CompoundStepInfo step = classify_compound_step(
          pos, component, usedSteps, startBoundaryKey, logicalRoot);
      transaction->usedCost = step.nextUsedSteps;
      transaction->syntheticBoundary = false;
      descend = step.canDescend;

      if (step.accepted)
      {
          move = turn;
          if (info)
              *info = frames[depth].info;
          if (committedState)
          {
              if (captureNotation)
              {
                  if (!notation)
                      notation = std::make_unique<Notation>();
                  for (int i = depth; i >= 0; --i)
                      CompoundTurnAdapter::undo_component(pos, turn[i], *transaction, i);
                  for (int i = 0; i <= depth; ++i)
                  {
                      notation->components[i] = compound_step_to_string(pos, turn[i]);
                      CompoundTurnAdapter::do_component(pos, turn[i],
                                                         *transaction, i, false);
                  }
                  notation->yielded.clear();
                  for (int i = 0; i < turn.size(); ++i)
                  {
                      if (i)
                          notation->yielded += ',';
                      notation->yielded += notation->components[i];
                  }
              }
              pos.commit_compound_move(turn, *committedState, *transaction);
              yielded = true;
          }
          else
          {
              pos.undo_move(turn, *transaction, true);
              unwind_prefix();
          }
          return true;
      }

      pos.undo_move(turn, *transaction, true);
  }
}

std::vector<LogicalMove> generate_compound_moves(Position& pos) {

  std::vector<LogicalMove> turns;
  if (!pos.compound_turn_active())
      return turns;

  Value result;
  if (pos.is_game_end(result))
      return turns;

  LogicalMoveWorkspace workspace;
  LogicalMoveSource source(pos, workspace, {}, false);
  LogicalMove turn;
  while (source.next(turn))
      turns.push_back(turn);
  return turns;
}

namespace {

bool has_any_compound_move_impl(Position& pos, int depth, int usedSteps,
                                Key startBoundaryKey, const StateInfo* logicalRoot,
                                LogicalMoveUndo& transaction) {

    for (const auto& move : MoveList<LEGAL_COMPONENTS>(pos))
    {
        if (depth != 0 && is_pass(move))
            continue;

        const int moveCost = compound_move_cost(pos, move);
        if (usedSteps + moveCost > pos.compound_turn_steps())
            continue;

        CompoundTurnAdapter::do_component(pos, move, transaction, depth, false);
        const CompoundStepInfo step = classify_compound_step(
            pos, move, usedSteps, startBoundaryKey, logicalRoot);
        const bool found = step.accepted
                        || (step.canDescend && depth + 1 < LogicalMove::MAX_COMPONENTS
                            && has_any_compound_move_impl(pos, depth + 1, step.nextUsedSteps,
                                                          startBoundaryKey, logicalRoot, transaction));
        CompoundTurnAdapter::undo_component(pos, move, transaction, depth);
        if (found)
            return true;
    }
    return false;
}

} // namespace

bool has_any_compound_move(Position& pos, bool checkGameEnd) {

  if (!pos.compound_turn_active())
      return false;

  Value result;
  if (checkGameEnd && pos.is_game_end(result))
      return false;

  LogicalMoveUndo transaction;
  return has_any_compound_move_impl(pos, 0, 0, pos.compound_turn_boundary_key(),
                                    pos.state(), transaction);
}

bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn) {

  if (!pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  const Key startBoundaryKey = pos.compound_turn_boundary_key();
  const StateInfo* logicalRoot = pos.state();

  CompoundMoveParser parser{pos, text, startBoundaryKey, logicalRoot, {}, {}};
  if (!parser.parse_level(0, 0))
      return false;

  turn = parser.parsed;
  return true;
}

bool compound_move_info(Position& pos, const LogicalMove& turn, LogicalMoveInfo& info) {

  if (!pos.compound_turn_active() || turn.empty())
      return false;

  LogicalMoveUndo transaction;
  Piece firstMovedPiece = NO_PIECE;
  bool firstSeeReliable = false;
  bool firstGivesCheck = false;
  info = {};

  for (int i = 0; i < turn.size(); ++i)
  {
      const Move component = turn[i];
      const Color mover = pos.side_to_move();
      if (i == 0)
      {
          firstMovedPiece = pos.moved_piece(component);
          firstSeeReliable = !pos.see_pruning_unreliable(component);
          firstGivesCheck = pos.gives_check(component);
      }
      CompoundTurnAdapter::do_component(pos, component, transaction, i, false);

      accumulate_component_info(info, transaction.components[i], component, mover);
  }

  for (int i = turn.size() - 1; i >= 0; --i)
      CompoundTurnAdapter::undo_component(pos, turn[i], transaction, i);

  info.representative = turn.first();
  info.movedPiece = firstMovedPiece;
  const bool ordinaryHeuristicCompatible = turn.is_single()
                                          && !is_two_step_move(turn.first());
  info.historyCompatible = ordinaryHeuristicCompatible;
  info.seeReliable = ordinaryHeuristicCompatible && firstSeeReliable;
  info.givesCheck = turn.is_single() && firstGivesCheck;
  return true;
}

void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state) {

  LogicalMoveUndo transaction;
  do_compound_move(pos, turn, state, transaction);
}

void do_compound_move(Position& pos, const LogicalMove& turn, StateInfo& state,
                      LogicalMoveUndo& transaction) {

  pos.do_move(turn, state, transaction, false);
}

void undo_compound_move(Position& pos, const LogicalMove& turn,
                        LogicalMoveUndo& transaction) {

  pos.undo_move(turn, transaction);
}

std::string compound_move_to_string(Position& pos, const LogicalMove& turn) {

  std::string result;
  LogicalMoveUndo transaction;

  for (int i = 0; i < turn.size(); ++i)
  {
      if (i)
          result += ',';
      result += compound_step_to_string(pos, turn[i]);
      // Formatting is a read-only operation. The component executor still
      // supplies scratch state so that effects are formatted in context.
      CompoundTurnAdapter::do_component(pos, turn[i], transaction, i, false);
  }

  for (int i = turn.size() - 1; i >= 0; --i)
      CompoundTurnAdapter::undo_component(pos, turn[i], transaction, i);

  return result;
}

std::vector<std::string> compound_pv_to_strings(const Position& pos,
                                                const LogicalMove& first,
                                                const std::vector<LogicalMove>& continuation) {

  std::vector<std::string> result;
  result.reserve(continuation.size() + 1);

  Position replay;
  StateListPtr states(new std::deque<StateInfo>(1));
  replay.set(pos.variant(), pos.fen(), pos.is_chess960(), &states->back(), pos.this_thread());

  LogicalMoveUndo transaction;
  auto append = [&](const LogicalMove& move) {
      if (move.first() == MOVE_NONE)
          return false;

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
      return true;
  };

  if (!append(first))
      return result;
  for (const LogicalMove& move : continuation)
  {
      if (!append(move))
          break;
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
          LogicalMoveUndo transaction;
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
