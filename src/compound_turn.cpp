/*
  Fairy-Stockfish-X compound turn support
*/

#include "compound_turn.h"

#ifdef ENABLE_COMPOUND_TURNS

#include <algorithm>

#include "movegen.h"
#include "position.h"
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
    if (!is_pass(move) && pos.compound_turn_boundary_key() == startBoundaryKey)
        return false;

    bool repetitionIllegal = false;
    if (!is_pass(move) && usedSteps + moveCost < pos.compound_turn_steps())
        repetitionIllegal = pos.compound_turn_repetition_illegal(logicalRoot->previous, 1);
    else
        repetitionIllegal = pos.compound_turn_repetition_illegal(logicalRoot->previous, 0);

    return !repetitionIllegal;
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

// Recursive-descent parser for one compound turn token ("step[,step...]",
// ';' separators are also accepted). Depth is bounded by
// LogicalMove::MAX_COMPONENTS, so plain member recursion needs no
// type-erased std::function wrapper.
struct CompoundMoveParser {
    Position& pos;
    const std::string& text;
    Key startBoundaryKey;
    const StateInfo* logicalRoot;
    StateInfo* states;
    LogicalMove parsed{};

    bool parse_level(size_t offset, int usedSteps) {
        if (offset >= text.size() || parsed.length >= LogicalMove::MAX_COMPONENTS)
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
            pos.do_component(move, states[index], false, false);
            const int nextUsedSteps = usedSteps + moveCost;

            bool accepted = false;
            if (next == text.size())
                accepted = compound_turn_candidate_accepted(pos, move, usedSteps,
                                                            startBoundaryKey, logicalRoot);
            else
                accepted = !is_pass(move) && parse_level(next + 1, nextUsedSteps);

            pos.undo_component(move);
            if (accepted)
                return true;

            --parsed.length;
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
  repetitionLimit = pos.variant()->samePlayerBoardRepetitionIllegalAtN;
  if (repetitionLimit <= 0)
      return;
  for (const StateInfo* previous = logicalRoot->previous;
       previous && 2 + int(previousBoundaryLayoutKeys.size()) * 2 <= logicalRoot->pliesFromNull + 1;
       previous = previous->previous && previous->previous->previous
                ? previous->previous->previous : nullptr)
      previousBoundaryLayoutKeys.push_back(previous->layoutKey);
}

LogicalMoveSource::~LogicalMoveSource() {

  if (yielded)
      undo_yielded();
  else if (prefixApplied)
      unwind_prefix();
  if (ownsWorkspace)
      workspace->inUse = false;
}

bool LogicalMoveSource::repetition_illegal(Key layoutKey, int pliesFromNull) const {

  if (repetitionLimit <= 0 || pliesFromNull < 2)
      return false;

  const size_t count = std::min(previousBoundaryLayoutKeys.size(),
                                size_t((pliesFromNull - 2) / 2 + 1));
  int repetitions = 0;
  for (size_t i = 0; i < count; ++i)
      if (previousBoundaryLayoutKeys[i] == layoutKey
          && ++repetitions >= repetitionLimit)
          return true;
  return false;
}

void LogicalMoveSource::initialize_frame(int frameDepth) {
  Frame& frame = frames[frameDepth];
  auto& moves = workspace->moveLists[frameDepth];
  frame.usedCost = frameDepth == 0
                 ? 0
                 : frames[frameDepth - 1].usedCost
                 + compound_move_cost(pos, turn.components[frameDepth - 1]);
  moves.clear();
  for (const auto& move : MoveList<LEGAL>(pos))
      moves.push_back(move);

  // The provider does not have a Stack/MovePicker, but a small amount of
  // complete-turn ordering is still useful. Follow a remembered logical turn,
  // fall back to the TT's first-component hint, and prioritize tactical steps.
  auto tacticalBegin = moves.begin();
  Move preferred = MOVE_NONE;
  if (order.preferredTurn && !order.preferredTurn->empty()
      && frameDepth < order.preferredTurn->length
      && std::equal(turn.components.begin(), turn.components.begin() + frameDepth,
                    order.preferredTurn->components.begin()))
      preferred = order.preferredTurn->components[frameDepth];
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
      pos.do_component(turn.components[i], transaction->components[i], false, false);
}

void LogicalMoveSource::unwind_prefix() {
  for (int i = depth - 1; i >= 0; --i)
      pos.undo_component(turn.components[i]);
  prefixApplied = false;
}

bool LogicalMoveSource::next(LogicalMove& move) {

  return next_impl(move, nullptr, nullptr);
}

bool LogicalMoveSource::next(LogicalMove& move, LogicalMoveInfo& info) {

  return next_impl(move, &info, nullptr);
}

bool LogicalMoveSource::next_applied(LogicalMove& move, LogicalMoveInfo& info,
                                     StateInfo& committedState) {

  return next_impl(move, &info, &committedState);
}

void LogicalMoveSource::undo_yielded() {

  assert(yielded);
  pos.undo_move(turn, *transaction, true);
  unwind_prefix();
  yielded = false;
  yieldedString.clear();
}

bool LogicalMoveSource::next_impl(LogicalMove& move, LogicalMoveInfo* info,
                                  StateInfo* committedState) {

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
          pos.do_component(turn.components[depth - 1],
                           transaction->components[depth - 1], false, false);
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
          pos.undo_component(turn.components[depth - 1]);
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

      turn.components[depth] = component;
      turn.length = uint8_t(depth + 1);
      if (committedState)
          componentStrings[depth] = compound_step_to_string(pos, component);
      const Color mover = pos.side_to_move();
      if (depth == 0)
      {
          firstMovedPiece = pos.moved_piece(component);
          firstSeeReliable = !pos.see_pruning_unreliable(component);
          firstGivesCheck = pos.gives_check(component);
      }
      pos.do_component(component, transaction->components[depth], false, false);

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

          const StateInfo& componentState = transaction->components[depth];
          record_removed_piece(componentFrame.info, componentState.captured.piece.piece, mover);
          record_removed_piece(componentFrame.info, componentState.jumpedEnPassantCaptured.piece.piece, mover);
          record_removed_piece(componentFrame.info, componentState.dead.piece, mover);

          Bitboard removed = componentState.bycatchSquares
                           & ~componentState.blastPromotedSquares
                           & ~componentState.laserTransformedSquares;
          while (removed)
          {
              Square square = pop_lsb(removed);
              record_removed_piece(componentFrame.info, componentState.bycatchPieces[square].piece(), mover);
          }

          for (int transfer = 0; transfer < componentState.push.transferCount; ++transfer)
              record_removed_piece(componentFrame.info, componentState.push.transfers[transfer].piece, mover);

          componentFrame.info.promotionLike = componentFrame.info.promotionLike
                                          || is_promotion_move(component)
                                          || componentState.promotionPawn != NO_PIECE
                                          || componentState.consumedPromotionHandPiece != NO_PIECE;
      }

      transaction->previous = logicalRoot;
      transaction->usedCost = usedSteps + moveCost;
      transaction->syntheticBoundary = false;
      const bool boundaryChanged = is_pass(component)
                                 || pos.compound_turn_boundary_key() != startBoundaryKey;
      const int boundaryPlies = pos.state()->pliesFromNull
                              + (!is_pass(component)
                                 && usedSteps + moveCost < pos.compound_turn_steps());
      const bool accepted = boundaryChanged
                         && !repetition_illegal(pos.state()->layoutKey, boundaryPlies);
      const bool canDescend = !is_pass(component)
                           && usedSteps + moveCost < pos.compound_turn_steps()
                           && pos.compound_turn_active();

      descend = canDescend;

      if (accepted)
      {
          move = turn;
          if (info)
              *info = frames[depth].info;
          if (committedState)
          {
              yieldedString.clear();
              for (int i = 0; i < turn.length; ++i)
              {
                  if (i)
                      yieldedString += ',';
                  yieldedString += componentStrings[i];
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
                                StateInfo* states) {

    for (const auto& move : MoveList<LEGAL>(pos))
    {
        if (depth != 0 && is_pass(move))
            continue;

        const int moveCost = compound_move_cost(pos, move);
        if (usedSteps + moveCost > pos.compound_turn_steps())
            continue;

        pos.do_component(move, states[depth], false, false);
        const int nextUsedSteps = usedSteps + moveCost;
        const bool accepted = compound_turn_candidate_accepted(pos, move, usedSteps,
                                                                startBoundaryKey, logicalRoot);
        const bool canDescend = !is_pass(move)
                             && nextUsedSteps < pos.compound_turn_steps()
                             && pos.compound_turn_active();
        const bool found = accepted
                        || (canDescend && depth + 1 < LogicalMove::MAX_COMPONENTS
                            && has_any_compound_move_impl(pos, depth + 1, nextUsedSteps,
                                                          startBoundaryKey, logicalRoot, states));
        pos.undo_component(move);
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

  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS];
  return has_any_compound_move_impl(pos, 0, 0, pos.compound_turn_boundary_key(),
                                    pos.state(), states);
}

bool parse_compound_move(Position& pos, const std::string& text, LogicalMove& turn) {

  if (!pos.compound_turn_active() || text.empty())
      return false;

  Value result;
  if (pos.is_game_end(result))
      return false;

  alignas(Eval::NNUE::CacheLineSize) StateInfo states[LogicalMove::MAX_COMPONENTS + 1];
  const Key startBoundaryKey = pos.compound_turn_boundary_key();
  const StateInfo* logicalRoot = pos.state();

  CompoundMoveParser parser{pos, text, startBoundaryKey, logicalRoot, states};
  if (!parser.parse_level(0, 0))
      return false;

  turn = parser.parsed;
  return true;
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

  for (int i = 0; i < turn.length; ++i)
  {
      if (i)
          result += ',';
      result += compound_step_to_string(pos, turn.components[i]);
      // Formatting is a read-only operation. The component executor still
      // supplies scratch state so that effects are formatted in context.
      pos.do_component(turn.components[i], transaction.components[i], false, false);
  }

  for (int i = turn.length - 1; i >= 0; --i)
      pos.undo_component(turn.components[i]);

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
