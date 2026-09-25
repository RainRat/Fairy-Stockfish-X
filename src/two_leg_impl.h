/* Internal two-leg path enumeration. */
#ifndef TWO_LEG_IMPL_H_INCLUDED
#define TWO_LEG_IMPL_H_INCLUDED

#include "position.h"
#include "two_leg.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace Stockfish::detail {

struct TwoLegWalker {

  static bool step_destination(const Position& pos, Square from, Direction d, Square& to) {
      auto [dr, df] = decode_direction(d);
      return wrapped_destination_square(from, df, dr, pos.max_file(), pos.max_rank(),
                                        pos.wraps_files(), pos.wraps_ranks(), to);
  }

  static bool hook_step(const Position& pos, Square cur, Direction dir, Square& nxt) {
      if (!step_destination(pos, cur, dir, nxt))
          return false;
      return std::abs(int(file_of(nxt)) - int(file_of(cur))) <= 1
          && std::abs(int(rank_of(nxt)) - int(rank_of(cur))) <= 1;
  }

  template<typename Visit>
  static bool for_each_two_step_path(const Position& pos, Color us, PieceType pt, Square from,
                                     Bitboard occupied, Bitboard friendly, Visit&& visit,
                                     Square target = SQ_NONE, Square viaTarget = SQ_NONE) {
  uint64_t mask = pos.variant()->twoStepMoves[pt].byColor[us];
  if (!mask || !(pos.board_bb() & from))
      return false;
  std::array<std::pair<Square, Square>, 64> seen{};
  int seenCount = 0;
  Bitboard remaining = Bitboard(mask);
  while (remaining)
  {
      int pair_idx = int(pop_lsb(remaining));
      int d1 = two_leg_pair_first(pair_idx);
      int d2 = two_leg_pair_second(pair_idx);
      Square via;
      if (!step_destination(pos, from, KingDirections[d1], via))
          continue;
      if (viaTarget != SQ_NONE && via != viaTarget)
          continue;
      if (!(pos.board_bb() & via) || (friendly & via))
          continue;
      Square to;
      if (!step_destination(pos, via, KingDirections[d2], to))
          continue;
      if (viaTarget != SQ_NONE && to != target)
          continue;
      // Piece mobility constrains the completed move's endpoint; the via
      // square remains a transit/capture square.
      if (!(pos.board_bb() & to) || (to != from && (friendly & to))
          || !(pos.board_bb(us, pt) & to))
          continue;
      const auto key = std::pair{via, to};
      if (std::find(seen.begin(), seen.begin() + seenCount, key) != seen.begin() + seenCount)
          continue;
      seen[seenCount++] = key;
      TwoLegPath path;
      path.kind = TwoLegKind::TWO_STEP;
      path.via = via;
      path.to = to;
      path.captures = (square_bb(via) | square_bb(to)) & occupied & ~square_bb(from);
      path.transit = square_bb(via) & ~square_bb(to);
      if (visit(path))
          return true;
  }
  return false;
}

  template<typename Visit>
  static bool for_each_hook_path(const Position& pos, Color us, PieceType pt, Square from,
                                 Bitboard occupied, Bitboard friendly, Visit&& visit,
                                 Square target = SQ_NONE, Square viaTarget = SQ_NONE) {
  uint64_t mask = pos.variant()->hookMoves[pt].directions.byColor[us];
  if (!mask || !(pos.board_bb() & from))
      return false;
  int captureLimit = pos.variant()->hookMoves[pt].captureLimit;
  if (captureLimit < 1)
      return false;
  int range1 = pos.variant()->hookMoves[pt].firstRange;
  int range2 = pos.variant()->hookMoves[pt].secondRange;
  Bitboard remaining = Bitboard(mask);
  while (remaining)
  {
      int pair_idx = int(pop_lsb(remaining));
      int d1 = two_leg_pair_first(pair_idx);
      int d2 = two_leg_pair_second(pair_idx);
      int cap1steps = range1 ? range1 : SQUARE_NB;
      Square bend = from;
      Bitboard firstTransit = 0;
      for (int k1 = 1; k1 <= cap1steps; ++k1)
      {
          Square step1;
          if (!hook_step(pos, bend, KingDirections[d1], step1))
              break;
          if (!(pos.board_bb() & step1) || (friendly & step1))
              break;
          bend = step1;
          firstTransit |= square_bb(bend);
          bool cap1 = bool(occupied & bend);
          if (viaTarget != SQ_NONE && bend != viaTarget)
          {
              if (cap1)
                  break;
              continue;
          }
          if (cap1)
          {
              TwoLegPath path;
              path.kind = TwoLegKind::HOOK;
              path.via = bend;
              path.to = bend;
              path.captures = square_bb(bend);
              path.transit = firstTransit & ~square_bb(bend);
              if ((pos.board_bb(us, pt) & bend) && visit(path))
                  return true;
              if (captureLimit == 1)
                  break;
          }
          int cap2steps = range2 ? range2 : SQUARE_NB;
          if (target != SQ_NONE && bend != target)
          {
              Square probe = bend;
              bool reachesTarget = false;
              for (int k2 = 1; k2 <= cap2steps; ++k2)
              {
                  Square step2;
                  if (!hook_step(pos, probe, KingDirections[d2], step2) || !(pos.board_bb() & step2))
                      break;
                  bool atOrigin = step2 == from;
                  if (!atOrigin && (friendly & step2))
                      break;
                  probe = step2;
                  bool cap2 = !atOrigin && bool(occupied & probe);
                  if (cap1 && cap2 && captureLimit < 2)
                      break;
                  if (probe == target)
                  {
                      reachesTarget = true;
                      break;
                  }
                  if (cap2 || atOrigin)
                      break;
              }
              if (!reachesTarget)
              {
                  if (cap1)
                      break;
                  continue;
              }
          }
          Square to = bend;
          Bitboard secondTransit = 0;
          for (int k2 = 1; k2 <= cap2steps; ++k2)
          {
              Square step2;
              if (!hook_step(pos, to, KingDirections[d2], step2))
                  break;
              if (!(pos.board_bb() & step2))
                  break;
              // The origin is a landing square (igui) but never
              // transit: the walk must not continue past it.
              bool atOrigin = (step2 == from);
              if (!atOrigin && (friendly & step2))
                  break;
              to = step2;
              secondTransit |= square_bb(to);
              bool cap2 = !atOrigin && bool(occupied & to);
              // A :1 hook may not capture on both legs; the limit lives here
              // so generation, validation, and attack detection share it.
              // (cap2 ends the ray either way, so skipping the emit keeps
              // the walk identical.)
              if (cap1 && cap2 && captureLimit < 2)
                  break;
              TwoLegPath path;
              path.kind = TwoLegKind::HOOK;
              path.via = bend;
              path.to = to;
              path.captures = (cap1 ? square_bb(bend) : Bitboard(0))
                            | (cap2 ? square_bb(to) : Bitboard(0));
              path.transit = (firstTransit | secondTransit) & ~square_bb(to);
              if ((pos.board_bb(us, pt) & to) && visit(path))
                  return true;
              if (cap2 || atOrigin)
                  break;
          }
          if (cap1)
              break;
      }
  }
  return false;
}

  template<typename Visit>
  static bool for_each_two_leg_path(const Position& pos, Color us, PieceType pt, Square from,
                                     Bitboard occupied, Bitboard friendly, Visit&& visit,
                                     Square target = SQ_NONE, Square viaTarget = SQ_NONE) {
      const Variant* rules = pos.variant();
      if ((rules->twoStepPieceTypes & piece_set(pt))
          && for_each_two_step_path(pos, us, pt, from, occupied, friendly, visit, target, viaTarget))
          return true;
      return (rules->hookPieceTypes & piece_set(pt))
          && for_each_hook_path(pos, us, pt, from, occupied, friendly, visit, target, viaTarget);
  }

};

} // namespace Stockfish::detail

#endif // TWO_LEG_IMPL_H_INCLUDED
