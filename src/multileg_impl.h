/* Internal multi-leg path enumeration, included only by position.cpp and movegen.cpp. */
#ifndef MULTILEG_IMPL_H_INCLUDED
#define MULTILEG_IMPL_H_INCLUDED

namespace Stockfish::detail {

template<typename Visit>
bool for_each_two_step_path(const Position& pos, Color us, PieceType pt, Square from,
                            Bitboard friendly, Visit&& visit) {
  uint64_t mask = pos.two_step_moves_mask(us, pt);
  if (!mask || !(pos.board_bb() & from))
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
      if (!pos.step_destination(from, KingDirections[d1], via))
          continue;
      if (!(pos.board_bb() & via) || (friendly & via))
          continue;
      Square to;
      if (!pos.step_destination(via, KingDirections[d2], to))
          continue;
      if (!(pos.board_bb() & to) || (to != from && (friendly & to)))
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
bool for_each_hook_path(const Position& pos, Color us, PieceType pt, Square from,
                        Bitboard occupied, Bitboard friendly, Visit&& visit, Square target) {
  uint64_t mask = pos.hook_move_mask(us, pt);
  if (!mask || !(pos.board_bb() & from))
      return false;
  int captureLimit = pos.hook_capture_limit(pt);
  if (captureLimit < 1)
      return false;
  int range1 = pos.hook_first_range(pt);
  int range2 = pos.hook_second_range(pt);
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
          if (!pos.hook_step(bend, KingDirections[d1], step1))
              break;
          if (!(pos.board_bb() & step1) || (friendly & step1))
              break;
          bend = step1;
          firstTransit |= square_bb(bend);
          bool cap1 = bool(occupied & bend);
          if (cap1 && captureLimit == 1)
          {
              MultiLegPath path;
              path.via = bend;
              path.to = bend;
              path.captureVia = true;
              path.transit = firstTransit & ~square_bb(bend);
              if (visit(path))
                  return true;
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
                  if (!pos.hook_step(probe, KingDirections[d2], step2) || !(pos.board_bb() & step2))
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
              if (!pos.hook_step(to, KingDirections[d2], step2))
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


} // namespace Stockfish::detail

#endif // MULTILEG_IMPL_H_INCLUDED
