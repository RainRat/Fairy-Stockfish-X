/* Internal multi-leg path enumeration, included only by position.cpp and movegen.cpp. */
#ifndef MULTILEG_IMPL_H_INCLUDED
#define MULTILEG_IMPL_H_INCLUDED

namespace Stockfish {

template<typename Visit>
bool Position::for_each_two_step_path(Color us, PieceType pt, Square from, Bitboard friendly, Visit&& visit) const {
  uint64_t mask = two_step_moves_mask(us, pt);
  if (!mask || !(board_bb() & from))
      return false;
  std::array<std::pair<Square, Square>, 64> seen{};
  int seenCount = 0;
  Bitboard remaining = Bitboard(mask);
  while (remaining)
  {
      int pair_idx = int(pop_lsb(remaining));
      int d1 = multileg_pair_first(pair_idx);
      int d2 = multileg_pair_second(pair_idx);
      Square via;
      if (!step_destination(from, KingDirections[d1], via))
          continue;
      if (!(board_bb() & via) || (friendly & via))
          continue;
      Square to;
      if (!step_destination(via, KingDirections[d2], to))
          continue;
      if (!(board_bb() & to) || (to != from && (friendly & to)))
          continue;
      const auto key = std::pair{via, to};
      if (std::find(seen.begin(), seen.begin() + seenCount, key) != seen.begin() + seenCount)
          continue;
      seen[seenCount++] = key;
      MultiLegPath path;
      path.via = via;
      path.to = to;
      path.transit = square_bb(via) & ~square_bb(to);
      if (visit(path))
          return true;
  }
  return false;
}

template<typename Visit>
bool Position::for_each_hook_path(Color us, PieceType pt, Square from,
                                  Bitboard occupied, Bitboard friendly, Visit&& visit) const {
  uint64_t mask = hook_move_mask(us, pt);
  if (!mask || !(board_bb() & from))
      return false;
  int captureLimit = hook_capture_limit(pt);
  if (captureLimit < 1)
      return false;
  int range1 = hook_first_range(pt);
  int range2 = hook_second_range(pt);
  Bitboard remaining = Bitboard(mask);
  while (remaining)
  {
      int pair_idx = int(pop_lsb(remaining));
      int d1 = multileg_pair_first(pair_idx);
      int d2 = multileg_pair_second(pair_idx);
      int cap1steps = range1 ? range1 : SQUARE_NB;
      Square bend = from;
      Bitboard firstTransit = 0;
      for (int k1 = 1; k1 <= cap1steps; ++k1)
      {
          Square step1;
          if (!hook_step(bend, KingDirections[d1], step1))
              break;
          if (!(board_bb() & step1) || (friendly & step1))
              break;
          bend = step1;
          firstTransit |= square_bb(bend);
          bool cap1 = bool(occupied & bend);
          int cap2steps = range2 ? range2 : SQUARE_NB;
          Square to = bend;
          Bitboard secondTransit = 0;
          for (int k2 = 1; k2 <= cap2steps; ++k2)
          {
              Square step2;
              if (!hook_step(to, KingDirections[d2], step2))
                  break;
              if (!(board_bb() & step2))
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
              MultiLegPath path;
              path.via = bend;
              path.to = to;
              path.captureVia = cap1;
              path.captureTo = cap2;
              path.atOrigin = atOrigin;
              path.transit = (firstTransit | secondTransit) & ~square_bb(to);
              if (visit(path))
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


} // namespace Stockfish

#endif // MULTILEG_IMPL_H_INCLUDED
