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

#include <cassert>

#include "movepick.h"

namespace Stockfish {

// Since continuation history grows quadratically with the number of piece types,
// we need to reserve a limited number of slots and map piece types to these slots
// in order to reduce memory consumption to a reasonable level.
int history_slot(Piece pc) {
    return pc == NO_PIECE ? 0 : (type_of(pc) == KING ? PIECE_SLOTS - 1 : type_of(pc) % (PIECE_SLOTS - 1)) + color_of(pc) * PIECE_SLOTS;
}

namespace {

  enum Stages {
    MAIN_TT, CAPTURE_INIT, GOOD_CAPTURE, REFUTATION, QUIET_INIT, QUIET, BAD_CAPTURE,
    EVASION_TT, EVASION_INIT, EVASION,
    PROBCUT_TT, PROBCUT_INIT, PROBCUT,
    QSEARCH_TT, QCAPTURE_INIT, QCAPTURE, QCHECK_INIT, QCHECK
  };

  // partial_insertion_sort() sorts moves in descending order up to and including
  // a given limit. The order of moves smaller than the limit is left unspecified.
  void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {

    for (ExtMove *sortedEnd = begin, *p = begin + 1; p < end; ++p)
        if (p->value >= limit)
        {
            ExtMove tmp = *p, *q;
            *p = *++sortedEnd;
            for (q = sortedEnd; q != begin && *(q - 1) < tmp; --q)
                *q = *(q - 1);
            *q = tmp;
        }
  }

} // namespace


bool MovePicker::is_useless_potion(Move m) const {

  if (!pos.potions_enabled() || !is_gating(m))
      return false;

  PieceType gatingPiece = gating_type(m);

  for (int idx = 0; idx < Variant::POTION_TYPE_NB; ++idx)
  {
      auto potion = static_cast<Variant::PotionType>(idx);
      if (pos.potion_piece(potion) != gatingPiece)
          continue;

      if (potion == Variant::POTION_FREEZE)
      {
          Bitboard zone = pos.freeze_zone_from_square(pos.gate_square(m));
          Bitboard enemies = pos.pieces(~pos.side_to_move());
          return !(zone & enemies);
      }

      if (potion == Variant::POTION_JUMP)
      {
          Square gate = pos.gate_square(m);
          if (pos.piece_on(gate) == NO_PIECE)
              return true;

          Bitboard path = between_bb(from_sq(m), to_sq(m), type_of(pos.moved_piece(m)));
          path &= ~square_bb(to_sq(m));
          return !(path & square_bb(gate));
      }

      break;
  }

  return false;
}

ExtMove* MovePicker::prune_useless_potions(ExtMove* begin, ExtMove* end) const {

  if (!pos.potions_enabled())
      return end;

  ExtMove* write = begin;
  for (ExtMove* it = begin; it != end; ++it)
      if (!is_useless_potion(it->move))
          *write++ = *it;

  return write;
}


/// Constructors of the MovePicker class. As arguments we pass information
/// to help it to return the (presumably) good moves first, to decide which
/// moves to return (in the quiescence search, for instance, we only want to
/// search captures, promotions, and some checks) and how important good move
/// ordering is at the current node.

/// MovePicker constructor for the main search
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh, const GateHistory* dh, const LowPlyHistory* lp,
                       const CapturePieceToHistory* cph, const PieceToHistory** ch, Move cm, const Move* killers, int pl)
           : pos(p), mainHistory(mh), gateHistory(dh), lowPlyHistory(lp), captureHistory(cph), continuationHistory(ch),
             ttMove(ttm), refutations{{killers[0], 0}, {killers[1], 0}, {cm, 0}}, depth(d), ply(pl) {

  assert(d > 0);
#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
  baseMoveList = std::make_unique<ExtMove[]>(MAX_MOVES);
  moveList = baseMoveList.get();
#else
  moveList = moves;
#endif
  if (pos.potions_enabled() || pos.capture_type() == PRISON)
  {
      overflowMoveList = std::make_unique<ExtMove[]>(MOVE_PICK_OVERFLOW_CAPACITY);
      moveList = overflowMoveList.get();
  }

  stage = (pos.checkers() ? EVASION_TT : MAIN_TT) +
          !(ttm && pos.pseudo_legal(ttm));
}

/// MovePicker constructor for quiescence search
MovePicker::MovePicker(const Position& p, Move ttm, Depth d, const ButterflyHistory* mh, const GateHistory* dh,
                       const CapturePieceToHistory* cph, const PieceToHistory** ch, Square rs)
           : pos(p), mainHistory(mh), gateHistory(dh), captureHistory(cph), continuationHistory(ch), ttMove(ttm), recaptureSquare(rs), depth(d) {

  assert(d <= 0);
#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
  baseMoveList = std::make_unique<ExtMove[]>(MAX_MOVES);
  moveList = baseMoveList.get();
#else
  moveList = moves;
#endif
  if (pos.potions_enabled() || pos.capture_type() == PRISON)
  {
      overflowMoveList = std::make_unique<ExtMove[]>(MOVE_PICK_OVERFLOW_CAPACITY);
      moveList = overflowMoveList.get();
  }

  stage = (pos.checkers() ? EVASION_TT : QSEARCH_TT) +
          !(   ttm
            && (pos.checkers() || depth > DEPTH_QS_RECAPTURES || to_sq(ttm) == recaptureSquare)
            && pos.pseudo_legal(ttm));
}

/// MovePicker constructor for ProbCut: we generate captures with SEE greater
/// than or equal to the given threshold.
MovePicker::MovePicker(const Position& p, Move ttm, Value th, const GateHistory* dh, const CapturePieceToHistory* cph)
           : pos(p), gateHistory(dh), captureHistory(cph), ttMove(ttm), threshold(th) {

  assert(!pos.checkers());
#ifdef USE_HEAP_INSTEAD_OF_STACK_FOR_MOVE_LIST
  baseMoveList = std::make_unique<ExtMove[]>(MAX_MOVES);
  moveList = baseMoveList.get();
#else
  moveList = moves;
#endif
  if (pos.potions_enabled() || pos.capture_type() == PRISON)
  {
      overflowMoveList = std::make_unique<ExtMove[]>(MOVE_PICK_OVERFLOW_CAPACITY);
      moveList = overflowMoveList.get();
  }

  stage = PROBCUT_TT + !(ttm && pos.capture(ttm)
                             && pos.pseudo_legal(ttm)
                             && (pos.see_pruning_unreliable() || pos.see_ge(ttm, threshold)));
}

/// MovePicker::score() assigns a numerical value to each move in a list, used
/// for sorting. Captures are ordered by Most Valuable Victim (MVV), preferring
/// captures with a good history. Quiets moves are ordered using the histories.
template<GenType Type>
void MovePicker::score() {

  static_assert(Type == CAPTURES || Type == QUIETS || Type == EVASIONS, "Wrong type");
  const Color us = pos.side_to_move();
  const PieceType myFlag = pos.flag_piece(us);
  const Bitboard myGoal = pos.flag_region(us);
  int goalDist[SQUARE_NB];
  if (myGoal && myFlag == KING)
  {
      for (int s = 0; s < SQUARE_NB; ++s)
      {
          int best = 64;
          for (Bitboard goals = myGoal; goals;)
              best = std::min(best, distance(Square(s), pop_lsb(goals)));
          goalDist[s] = best;
      }
  }
  auto flag_goal_bonus = [&](Move mv) {
      Piece mp = pos.moved_piece(mv);
      return (myGoal && mp != NO_PIECE && type_of(mp) == myFlag && (myGoal & square_bb(to_sq(mv)))) ? 30000 : 0;
  };
  auto king_goal_progress_bonus = [&](Move mv) {
      if (!myGoal || myFlag != KING)
          return 0;
      Piece mp = pos.moved_piece(mv);
      if (mp == NO_PIECE || type_of(mp) != KING)
          return 0;
      Square from = from_sq(mv);
      Square to = to_sq(mv);
      if (!is_ok(from) || !is_ok(to))
          return 0;
      int delta = goalDist[from] - goalDist[to];
      return delta > 0 ? 900 * delta : 0;
  };
  auto points_capture_bonus = [&](Move mv) {
      if (!pos.points_counting())
          return 0;
      Piece captured = pos.captured_piece(mv);
      if (captured == NO_PIECE)
          return 0;
      int pts = pos.variant()->piecePoints[type_of(captured)];
      int signedPts = 0;
      switch (pos.points_rule_captures())
      {
          case POINTS_US:        signedPts =  pts; break;
          case POINTS_THEM:      signedPts = -pts; break;
          case POINTS_OWNER:     signedPts =  color_of(captured) == pos.side_to_move() ? pts : -pts; break;
          case POINTS_NON_OWNER: signedPts =  color_of(captured) == pos.side_to_move() ? -pts : pts; break;
          case POINTS_NONE:      signedPts = 0; break;
      }
      if (pos.points_goal() > 0)
      {
          if (pos.points_goal_value() < VALUE_ZERO)
              signedPts = -signedPts;
          else if (pos.points_goal_value() == VALUE_ZERO)
              signedPts = 0;
      }
      return 20 * signedPts;
  };
  auto capture_victim_value = [&](Move mv) {
      Piece captured = pos.captured_piece(mv);
      if (captured == NO_PIECE)
          captured = pos.piece_on(to_sq(mv));
      return int(PieceValue[MG][captured]);
  };

  for (auto& m : *this)
      if constexpr (Type == CAPTURES)
      {
          m.value =  capture_victim_value(m) * 6
                   + points_capture_bonus(m)
                   + flag_goal_bonus(m)
                   + king_goal_progress_bonus(m)
                   + (*gateHistory)[pos.side_to_move()][gating_square(m)]
                   + (*captureHistory)[pos.moved_piece(m)][to_sq(m)][type_of(pos.piece_on(to_sq(m)))];
      }

      else if constexpr (Type == QUIETS)
      {
          int exchangeBonus = exchange_piece(m) != NO_PIECE_TYPE
                            ? 3 * int(PieceValue[MG][make_piece(pos.side_to_move(), exchange_piece(m))])
                            : 0;
          m.value =      (*mainHistory)[pos.side_to_move()][from_to(m)]
                   +     exchangeBonus
                   +     flag_goal_bonus(m)
                   +     king_goal_progress_bonus(m)
                   +     (*gateHistory)[pos.side_to_move()][gating_square(m)]
                   + 2 * (*continuationHistory[0])[history_slot(pos.moved_piece(m))][to_sq(m)]
                   +     (*continuationHistory[1])[history_slot(pos.moved_piece(m))][to_sq(m)]
                   +     (*continuationHistory[3])[history_slot(pos.moved_piece(m))][to_sq(m)]
                   +     (*continuationHistory[5])[history_slot(pos.moved_piece(m))][to_sq(m)]
                   + (ply < MAX_LPH ? std::min(4, depth / 3) * (*lowPlyHistory)[ply][from_to(m)] : 0);
      }

      else // Type == EVASIONS
      {
          if (pos.capture(m))
              m.value =  capture_victim_value(m)
                       + points_capture_bonus(m)
                       + flag_goal_bonus(m)
                       + king_goal_progress_bonus(m)
                       - Value(type_of(pos.moved_piece(m)));
          else
              m.value =      (*mainHistory)[pos.side_to_move()][from_to(m)]
                       +     flag_goal_bonus(m)
                       +     king_goal_progress_bonus(m)
                       + 2 * (*continuationHistory[0])[history_slot(pos.moved_piece(m))][to_sq(m)]
                       - (1 << 28);
      }
}

/// MovePicker::select() returns the next move satisfying a predicate function.
/// It never returns the TT move.
template<MovePicker::PickType T, typename Pred>
Move MovePicker::select(Pred filter) {

  const bool potions = pos.potions_enabled();

  while (cur < endMoves)
  {
      if (T == Best)
          std::swap(*cur, *std::max_element(cur, endMoves));

      Move move = *cur;

      if (move != ttMove
          && (!potions || !is_gating(move) || !is_useless_potion(move))
          && filter())
          return *cur++;

      cur++;
  }
  return MOVE_NONE;
}

/// MovePicker::next_move() is the most important method of the MovePicker class. It
/// returns a new pseudo-legal move every time it is called until there are no more
/// moves left, picking the move with the highest score from a list of generated moves.
Move MovePicker::next_move(bool skipQuiets) {

  auto assert_move_list_bounds = [&]() {
      assert(endMoves >= moveList);
      assert(endMoves - moveList <= MOVE_PICK_OVERFLOW_CAPACITY);
      assert(cur >= moveList && cur <= endMoves);
  };
  const bool potions = pos.potions_enabled();

top:
  switch (stage) {

  case MAIN_TT:
  case EVASION_TT:
  case QSEARCH_TT:
  case PROBCUT_TT:
      ++stage;
      if (ttMove && !is_useless_potion(ttMove))
      {
          assert(pos.legal(ttMove) == MoveList<LEGAL>(pos).contains(ttMove) || pos.virtual_drop(ttMove) || exchange_piece(ttMove));
          return ttMove;
      }
      ttMove = MOVE_NONE;
      goto top;

  case CAPTURE_INIT:
  case PROBCUT_INIT:
  case QCAPTURE_INIT:
      cur = endBadCaptures = moveList;
      endMoves = generate<CAPTURES>(pos, cur);
      if (potions)
          endMoves = prune_useless_potions(cur, endMoves);
      assert_move_list_bounds();

      score<CAPTURES>();
      ++stage;
      goto top;

  case GOOD_CAPTURE:
      if (select<Best>([&](){
                       return pos.see_ge(*cur, Value(-69 * cur->value / 1024 - 500 * (pos.captures_to_hand() && pos.gives_check(*cur))))?
                              // Move losing capture to endBadCaptures to be tried later
                              true : (*endBadCaptures++ = *cur, false); }))
          return *(cur - 1);

      // Prepare the pointers to loop over the refutations array
      cur = std::begin(refutations);
      endMoves = std::end(refutations);

      // If the countermove is the same as a killer, skip it
      if (   refutations[0].move == refutations[2].move
          || refutations[1].move == refutations[2].move)
          --endMoves;

      ++stage;
      [[fallthrough]];

  case REFUTATION:
      if (select<Next>([&](){ return    *cur != MOVE_NONE
                                    && !pos.capture(*cur)
                                    &&  pos.pseudo_legal(*cur); }))
          return *(cur - 1);
      ++stage;
      [[fallthrough]];

  case QUIET_INIT:
      if (!skipQuiets && !(pos.must_capture() && pos.has_capture()))
      {
          cur = endBadCaptures;
          endMoves = generate<QUIETS>(pos, cur);
          if (potions)
              endMoves = prune_useless_potions(cur, endMoves);
          assert_move_list_bounds();

          score<QUIETS>();
          partial_insertion_sort(cur, endMoves, -3000 * depth);
      }

      ++stage;
      [[fallthrough]];

  case QUIET:
      if (   !skipQuiets
          && select<Next>([&](){return   *cur != refutations[0].move
                                      && *cur != refutations[1].move
                                      && *cur != refutations[2].move;}))
          return *(cur - 1);

      // Prepare the pointers to loop over the bad captures
      cur = moveList;
      endMoves = endBadCaptures;

      ++stage;
      [[fallthrough]];

  case BAD_CAPTURE:
      return select<Next>([](){ return true; });

  case EVASION_INIT:
      cur = moveList;
      endMoves = generate<EVASIONS>(pos, cur);
      if (potions)
          endMoves = prune_useless_potions(cur, endMoves);
      assert_move_list_bounds();

      score<EVASIONS>();
      ++stage;
      [[fallthrough]];

  case EVASION:
      return select<Best>([](){ return true; });

  case PROBCUT:
      return select<Best>([&](){ return pos.see_pruning_unreliable() || pos.see_ge(*cur, threshold); });

  case QCAPTURE:
      if (select<Best>([&](){ return   depth > DEPTH_QS_RECAPTURES
                                    || to_sq(*cur) == recaptureSquare; }))
          return *(cur - 1);

      // If we did not find any move and we do not try checks, we have finished
      if (depth != DEPTH_QS_CHECKS)
          return MOVE_NONE;

      ++stage;
      [[fallthrough]];

  case QCHECK_INIT:
      cur = moveList;
      endMoves = generate<QUIET_CHECKS>(pos, cur);
      if (potions)
          endMoves = prune_useless_potions(cur, endMoves);
      assert_move_list_bounds();

      ++stage;
      [[fallthrough]];

  case QCHECK:
      return select<Next>([](){ return true; });
  }

  assert(false);
  return MOVE_NONE; // Silence warning
}

} // namespace Stockfish
