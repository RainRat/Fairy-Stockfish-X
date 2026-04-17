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

#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "bitboard.h"
#include "magic.h"
#include "misc.h"
#include "piece.h"

namespace Stockfish {

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard WrappedRays[SQUARE_NB][8];
Bitboard PseudoAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard PseudoMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard BoardSizeBB[FILE_NB][RANK_NB];
RiderType AttackRiderTypes[PIECE_TYPE_NB];
RiderType MoveRiderTypes[2][PIECE_TYPE_NB];

thread_local const MagicGeometry* current_magic_geometry = nullptr;

namespace {

// Some magics need to be split in order to reduce memory consumption.
// Otherwise on a 12x10 board they can be >100 MB.
#if !defined(VERY_LARGE_BOARDS)
#endif

  // Rider directions
  const std::map<Direction, int> RookDirectionsV { {NORTH, 0}, {SOUTH, 0}};
  const std::map<Direction, int> RookDirectionsH { {EAST, 0}, {WEST, 0} };
  const std::map<Direction, int> BishopDirections { {NORTH_EAST, 0}, {SOUTH_EAST, 0}, {SOUTH_WEST, 0}, {NORTH_WEST, 0} };
  const std::map<Direction, int> LameDabbabaDirections { {2 * NORTH, 0}, {2 * EAST, 0}, {2 * SOUTH, 0}, {2 * WEST, 0} };
  const std::map<Direction, int> HorseDirections { {2 * SOUTH + WEST, 0}, {2 * SOUTH + EAST, 0}, {SOUTH + 2 * WEST, 0}, {SOUTH + 2 * EAST, 0},
                                                   {NORTH + 2 * WEST, 0}, {NORTH + 2 * EAST, 0}, {2 * NORTH + WEST, 0}, {2 * NORTH + EAST, 0} };
  const std::map<Direction, int> ElephantDirections { {2 * NORTH_EAST, 0}, {2 * SOUTH_EAST, 0}, {2 * SOUTH_WEST, 0}, {2 * NORTH_WEST, 0} };
  const std::map<Direction, int> JanggiElephantDirections { {NORTH + 2 * NORTH_EAST, 0}, {EAST  + 2 * NORTH_EAST, 0},
                                                            {EAST  + 2 * SOUTH_EAST, 0}, {SOUTH + 2 * SOUTH_EAST, 0},
                                                            {SOUTH + 2 * SOUTH_WEST, 0}, {WEST  + 2 * SOUTH_WEST, 0},
                                                            {WEST  + 2 * NORTH_WEST, 0}, {NORTH + 2 * NORTH_WEST, 0} };
  const std::map<Direction, int> GrasshopperDirectionsV { {NORTH, 1}, {SOUTH, 1}};
  const std::map<Direction, int> GrasshopperDirectionsH { {EAST, 1}, {WEST, 1} };
  const std::map<Direction, int> GrasshopperDirectionsD { {NORTH_EAST, 1}, {SOUTH_EAST, 1}, {SOUTH_WEST, 1}, {NORTH_WEST, 1} };

  enum MovementType { RIDER, HOPPER, LAME_LEAPER, HOPPER_RANGE };

  template <MovementType MT>
  Bitboard sliding_attack(const std::map<Direction, int>& directions, Square sq, Bitboard occupied, Color c = WHITE) {
    assert(MT != LAME_LEAPER);

    Bitboard attack = 0;

    for (auto const& [d, limit] : directions)
    {
        int minDistance = slider_min_distance(limit);
        int maxDistance = slider_max_distance(limit);
        int count = 0;
        bool hurdle = false;
        for (Square s = sq + (c == WHITE ? d : -d);
             is_ok(s) && distance(s, s - (c == WHITE ? d : -d)) <= 2;
             s += (c == WHITE ? d : -d))
        {
            if (MT != HOPPER || hurdle)
            {
                ++count;
                if (count >= minDistance)
                    attack |= s;
                // For hoppers we consider limit == 1 as a grasshopper,
                // but limit > 1 as a limited distance hopper
                if (maxDistance > 0 && !(MT == HOPPER_RANGE && maxDistance == 1) && count >= maxDistance)
                    break;
            }

            if (occupied & s)
            {
                if (MT == HOPPER && !hurdle)
                    hurdle = true;
                else
                    break;
            }
        }
    }

    return attack;
  }

  Bitboard ski_sliding_attack(const std::map<Direction, int>& directions, Square sq, Bitboard occupied, Color c = WHITE) {
    Bitboard attack = 0;

    for (auto const& [d, _] : directions)
    {
        Square first = sq + (c == WHITE ? d : -d);
        if (!is_ok(first) || distance(first, sq) > 2)
            continue;

        for (Square s = first + (c == WHITE ? d : -d);
             is_ok(s) && distance(s, s - (c == WHITE ? d : -d)) <= 2;
             s += (c == WHITE ? d : -d))
        {
            attack |= s;
            if (occupied & s)
                break;
        }
    }

    return attack;
  }

  Bitboard contra_hopper_attack(const std::map<Direction, int>& directions, Square sq, Bitboard occupied, Color c = WHITE) {
    Bitboard attack = 0;

    for (auto const& [d, limit] : directions)
    {
      Square hurdle = sq + (c == WHITE ? d : -d);
      if (!(is_ok(hurdle) && distance(hurdle, sq) <= 2 && (occupied & hurdle)))
          continue;

      int landingDist = 0;
      for (Square s = hurdle + (c == WHITE ? d : -d);
           is_ok(s) && distance(s, s - (c == WHITE ? d : -d)) <= 2;
           s += (c == WHITE ? d : -d))
      {
        ++landingDist;
        if (!limit || landingDist <= limit)
            attack |= s;
        if (occupied & s)
            break;
      }
    }

    return attack;
  }

  Bitboard leap_rider_attacks(const std::map<Direction, int>& directions, Square s, Bitboard occupied, Color c);

  Bitboard contra_hopper_potential(const std::map<Direction, int>& directions, Square sq, Color c = WHITE) {
    Bitboard attack = 0;

    for (auto const& [d, _] : directions)
      for (Square s = sq + 2 * (c == WHITE ? d : -d);
           is_ok(s) && distance(s, s - (c == WHITE ? d : -d)) <= 2;
           s += (c == WHITE ? d : -d))
          attack |= s;

    return attack;
  }

  Bitboard special_pseudo_bb(const PieceInfo* pi, bool initial, MoveModality modality, Square s, Color c,
                             const std::map<Direction, int>& riderDirs,
                             const std::map<Direction, int>& skiDirs) {
    Bitboard pseudo = 0;

    pseudo |= sliding_attack<RIDER>(riderDirs, s, 0, c);
    pseudo |= leap_rider_attacks(pi->leapRider[initial][modality], s, 0, c);
    pseudo |= ski_sliding_attack(skiDirs, s, 0, c);
    pseudo |= sliding_attack<HOPPER_RANGE>(pi->hopper[initial][modality], s, 0, c);
    pseudo |= contra_hopper_potential(pi->contraHopper[initial][modality], s, c);

    if (pi->griffon[initial][modality])
        pseudo |= rider_attacks_bb<RIDER_GRIFFON_NH>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_GRIFFON_SH>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_GRIFFON_EV>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_GRIFFON_WV>(s, Bitboard(0));

    if (pi->manticore[initial][modality])
        pseudo |= rider_attacks_bb<RIDER_MANTICORE_NE>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_MANTICORE_NW>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_MANTICORE_SE>(s, Bitboard(0))
                | rider_attacks_bb<RIDER_MANTICORE_SW>(s, Bitboard(0));

    if (pi->rose[initial][modality])
        pseudo |= rider_attacks_bb<RIDER_ROSE>(s, Bitboard(0));

    return pseudo;
  }

  Bitboard special_leaper_bb(const PieceInfo* pi, bool initial, MoveModality modality, Square s, Color c) {
    Bitboard leaper = contra_hopper_attack(pi->contraHopper[initial][modality], s, 0, c);

    if (pi->griffon[initial][modality])
        leaper |= PseudoAttacks[WHITE][FERS][s];
    if (pi->manticore[initial][modality])
        leaper |= PseudoAttacks[WHITE][WAZIR][s];

    return leaper;
  }

  void add_step_like_rider_types(RiderType& riderTypes, Direction d) {
    const int ad = std::abs(int(d));
    if ((ad % FILE_NB) == 0)
        riderTypes |= RIDER_ROOK_V;
    if (ad < FILE_NB)
        riderTypes |= RIDER_ROOK_H;
    if ((FILE_NB > 1 && (ad % (FILE_NB - 1)) == 0) || (ad % (FILE_NB + 1)) == 0)
        riderTypes |= RIDER_BISHOP;
    if (LameDabbabaDirections.find(d) != LameDabbabaDirections.end())
        riderTypes |= RIDER_LAME_DABBABA;
    if (HorseDirections.find(d) != HorseDirections.end())
        riderTypes |= RIDER_HORSE;
    if (ElephantDirections.find(d) != ElephantDirections.end())
        riderTypes |= RIDER_ELEPHANT;
    if (JanggiElephantDirections.find(d) != JanggiElephantDirections.end())
        riderTypes |= RIDER_JANGGI_ELEPHANT;
  }

  void add_slider_rider_types(RiderType& riderTypes, Direction d, int limit) {
    if (limit == DYNAMIC_SLIDER_LIMIT)
        return;
    if (limit == SKI_SLIDER_LIMIT)
    {
        if (BishopDirections.find(d) != BishopDirections.end())
            riderTypes |= RIDER_SKI_BISHOP;
        if (RookDirectionsH.find(d) != RookDirectionsH.end())
            riderTypes |= RIDER_SKI_ROOK_H;
        if (RookDirectionsV.find(d) != RookDirectionsV.end())
            riderTypes |= RIDER_SKI_ROOK_V;
        return;
    }
    if (BishopDirections.find(d) != BishopDirections.end())
        riderTypes |= RIDER_BISHOP;
    if (RookDirectionsH.find(d) != RookDirectionsH.end())
        riderTypes |= RIDER_ROOK_H;
    if (RookDirectionsV.find(d) != RookDirectionsV.end())
        riderTypes |= RIDER_ROOK_V;
    if (LameDabbabaDirections.find(d) != LameDabbabaDirections.end())
        riderTypes |= RIDER_LAME_DABBABA;
    if (HorseDirections.find(d) != HorseDirections.end())
        riderTypes |= RIDER_NIGHTRIDER;
    if (ElephantDirections.find(d) != ElephantDirections.end())
        riderTypes |= RIDER_ELEPHANT;
    if (JanggiElephantDirections.find(d) != JanggiElephantDirections.end())
        riderTypes |= RIDER_JANGGI_ELEPHANT;
  }

  void add_hopper_rider_types(RiderType& riderTypes, Direction d, int limit) {
    if (RookDirectionsH.find(d) != RookDirectionsH.end())
        riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_H : RIDER_CANNON_H;
    if (RookDirectionsV.find(d) != RookDirectionsV.end())
        riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_V : RIDER_CANNON_V;
    if (BishopDirections.find(d) != BishopDirections.end())
        riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_D : RIDER_CANNON_DIAG;
  }

  Bitboard lame_leaper_path(Direction d, Square s) {
    Direction dr = d > 0 ? NORTH : SOUTH;
    Direction df = (std::abs(d % NORTH) < NORTH / 2 ? d % NORTH : -(d % NORTH)) < 0 ? WEST : EAST;
    Square to = s + d;
    Bitboard b = 0;
    if (!is_ok(to) || distance(s, to) >= 4)
        return b;
    while (s != to)
    {
        int diff = std::abs(file_of(to) - file_of(s)) - std::abs(rank_of(to) - rank_of(s));
        if (diff > 0)
            s += df;
        else if (diff < 0)
            s += dr;
        else
            s += df + dr;

        if (s != to)
            b |= s;
    }
    return b;
  }

  #if !defined(VERY_LARGE_BOARDS)
  Bitboard lame_leaper_path(std::map<Direction, int> directions, Square s) {
    Bitboard b = 0;
    for (const auto& i : directions)
        b |= lame_leaper_path(i.first, s);
    return b;
  }
  #endif

  Bitboard lame_leaper_attack(std::map<Direction, int> directions, Square s, Bitboard occupied) {
    Bitboard b = 0;
    for (const auto& i : directions)
    {
        Square to = s + i.first;
        if (is_ok(to) && distance(s, to) < 4 && !(lame_leaper_path(i.first, s) & occupied))
            b |= to;
    }
    return b;
  }

#ifdef VERY_LARGE_BOARDS
  Bitboard fixed_step_rider_attacks(Square s, Bitboard occupied, int stepF, int stepR) {
    Bitboard attack = 0;
    int f = int(file_of(s));
    int r = int(rank_of(s));

    while (true)
    {
        f += stepF;
        r += stepR;
        if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
            break;
        Square to = make_square(File(f), Rank(r));
        attack |= to;
        if (occupied & to)
            break;
    }

    return attack;
  }
#endif

  Bitboard limited_step_rider_attacks(Square s, Bitboard occupied, int stepF, int stepR, int limit) {
    Bitboard attack = 0;
    int f = int(file_of(s));
    int r = int(rank_of(s));
    int count = 0;

    while (true)
    {
        f += stepF;
        r += stepR;
        if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
            break;
        Square to = make_square(File(f), Rank(r));
        attack |= to;
        if (limit > 0 && ++count >= limit)
            break;
        if (occupied & to)
            break;
    }

    return attack;
  }

  bool decode_direction(Direction d, int& stepF, int& stepR) {
    auto [dr, df] = Stockfish::decode_direction(d);
    stepR = dr;
    stepF = df;
    return true;
  }

  Bitboard leap_rider_attacks(const std::map<Direction, int>& directions, Square s, Bitboard occupied, Color c) {
    Bitboard attack = 0;

    for (auto const& [d, limit] : directions)
    {
        int stepF = 0;
        int stepR = 0;
        if (!decode_direction(c == WHITE ? d : Direction(-d), stepF, stepR))
            continue;
        attack |= limited_step_rider_attacks(s, occupied, stepF, stepR, limit);
    }

    return attack;
  }

}

/// safe_destination() returns the bitboard of target square for the given step
/// from the given square. If the step is off the board, returns empty bitboard.

inline Bitboard safe_destination_tuple(Square s, int dr, int df) {
    int r = int(rank_of(s)) + dr;
    int f = int(file_of(s)) + df;
    if (r < 0 || r > int(RANK_MAX) || f < 0 || f > int(FILE_MAX))
        return Bitboard(0);
    return square_bb(make_square(File(f), Rank(r)));
}

inline Bitboard safe_destination(Square s, int step) {
    auto [dr, df] = decode_direction(Direction(step));
    return safe_destination_tuple(s, dr, df);
}

Bitboard tuple_rider_attacks(const std::vector<PieceInfo::TupleRay>& rays, Square s, Bitboard occupied, Color c) {
    Bitboard attack = 0;

    for (const auto& ray : rays)
    {
        const int stepR = c == WHITE ? ray.dr : -ray.dr;
        const int stepF = c == WHITE ? ray.df : -ray.df;
        Square current = s;
        int count = 0;
        for (;;)
        {
            Bitboard next = safe_destination_tuple(current, stepR, stepF);
            if (!next)
                break;

            Square to = lsb(next);
            attack |= next;
            current = to;
            if (ray.limit > 0 && ++count >= ray.limit)
                break;
            if (occupied & next)
                break;
        }
    }

    return attack;
}

Bitboard rider_terminal_squares(const std::map<Direction, int>& directions, Square sq) {
    Bitboard terminal = 0;

    for (auto const& [d, _] : directions)
    {
        Bitboard next = safe_destination(sq, d);
        while (next)
        {
            Square to = lsb(next);
            Bitboard after = safe_destination(to, d);
            if (!after)
            {
                terminal |= next;
                break;
            }
            next = after;
        }
    }

    return terminal;
}


#ifdef VERY_LARGE_BOARDS
Bitboard rider_attacks_bb(RiderType R, Square s, Bitboard occupied) {
  auto shifted_source = [&](Direction d) -> Square {
      Bitboard shifted = safe_destination(s, d);
      if (!shifted || (occupied & shifted))
          return SQ_NONE;
      return lsb(shifted);
  };

  switch (R)
  {
  case RIDER_BISHOP: return sliding_attack<RIDER>(BishopDirections, s, occupied);
  case RIDER_ROOK_H: return sliding_attack<RIDER>(RookDirectionsH, s, occupied);
  case RIDER_ROOK_V: return sliding_attack<RIDER>(RookDirectionsV, s, occupied);
  case RIDER_CANNON_H: return sliding_attack<HOPPER>(RookDirectionsH, s, occupied);
  case RIDER_CANNON_V: return sliding_attack<HOPPER>(RookDirectionsV, s, occupied);
  case RIDER_LAME_DABBABA: return  fixed_step_rider_attacks(s, occupied,  0,  2)
                                 | fixed_step_rider_attacks(s, occupied,  0, -2)
                                 | fixed_step_rider_attacks(s, occupied,  2,  0)
                                 | fixed_step_rider_attacks(s, occupied, -2,  0);
  case RIDER_HORSE: return lame_leaper_attack(HorseDirections, s, occupied);
  case RIDER_ELEPHANT: return  fixed_step_rider_attacks(s, occupied,  2,  2)
                              | fixed_step_rider_attacks(s, occupied, -2,  2)
                              | fixed_step_rider_attacks(s, occupied,  2, -2)
                              | fixed_step_rider_attacks(s, occupied, -2, -2);
  case RIDER_JANGGI_ELEPHANT: return lame_leaper_attack(JanggiElephantDirections, s, occupied);
  case RIDER_CANNON_DIAG: return sliding_attack<HOPPER>(BishopDirections, s, occupied);
  case RIDER_NIGHTRIDER: return sliding_attack<RIDER>(HorseDirections, s, occupied);
  case RIDER_GRASSHOPPER_H: return sliding_attack<HOPPER>(GrasshopperDirectionsH, s, occupied);
  case RIDER_GRASSHOPPER_V: return sliding_attack<HOPPER>(GrasshopperDirectionsV, s, occupied);
  case RIDER_GRASSHOPPER_D: return sliding_attack<HOPPER>(GrasshopperDirectionsD, s, occupied);
  case RIDER_GRIFFON_NH: {
      Square src = shifted_source(NORTH_EAST);
      return src == SQ_NONE ? Bitboard(0)
                            : fixed_step_rider_attacks(src, occupied, 1, 0) | fixed_step_rider_attacks(src, occupied, 0, 1);
  }
  case RIDER_GRIFFON_SH: {
      Square src = shifted_source(NORTH_WEST);
      return src == SQ_NONE ? Bitboard(0)
                            : fixed_step_rider_attacks(src, occupied, -1, 0) | fixed_step_rider_attacks(src, occupied, 0, 1);
  }
  case RIDER_GRIFFON_EV: {
      Square src = shifted_source(SOUTH_EAST);
      return src == SQ_NONE ? Bitboard(0)
                            : fixed_step_rider_attacks(src, occupied, 1, 0) | fixed_step_rider_attacks(src, occupied, 0, -1);
  }
  case RIDER_GRIFFON_WV: {
      Square src = shifted_source(SOUTH_WEST);
      return src == SQ_NONE ? Bitboard(0)
                            : fixed_step_rider_attacks(src, occupied, -1, 0) | fixed_step_rider_attacks(src, occupied, 0, -1);
  }
  case RIDER_MANTICORE_NE: {
      Square src = shifted_source(NORTH);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(std::map<Direction, int>{{NORTH_EAST, 0}, {NORTH_WEST, 0}}, src, occupied);
  }
  case RIDER_MANTICORE_NW: {
      Square src = shifted_source(WEST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(std::map<Direction, int>{{NORTH_WEST, 0}, {SOUTH_WEST, 0}}, src, occupied);
  }
  case RIDER_MANTICORE_SE: {
      Square src = shifted_source(EAST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(std::map<Direction, int>{{NORTH_EAST, 0}, {SOUTH_EAST, 0}}, src, occupied);
  }
  case RIDER_MANTICORE_SW: {
      Square src = shifted_source(SOUTH);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(std::map<Direction, int>{{SOUTH_EAST, 0}, {SOUTH_WEST, 0}}, src, occupied);
  }
  case RIDER_SKI_ROOK_H: return ski_sliding_attack(RookDirectionsH, s, occupied);
  case RIDER_SKI_ROOK_V: return ski_sliding_attack(RookDirectionsV, s, occupied);
  case RIDER_SKI_BISHOP: return ski_sliding_attack(BishopDirections, s, occupied);
  default: return Bitboard(0);
  }
}
#endif

Bitboard leap_rider_attacks_bb(PieceType pt, Color c, Square s, Bitboard occupied) {
  return leap_rider_attacks(pieceMap.get(pt)->leapRider[0][MODALITY_CAPTURE], s, occupied, c);
}

Bitboard leap_rider_moves_bb(PieceType pt, bool initial, Color c, Square s, Bitboard occupied) {
  return leap_rider_attacks(pieceMap.get(pt)->leapRider[initial][MODALITY_QUIET], s, occupied, c);
}

Bitboard tuple_rider_attacks_bb(PieceType pt, Color c, Square s, Bitboard occupied) {
  return tuple_rider_attacks(pieceMap.get(pt)->tupleSlider[0][MODALITY_CAPTURE], s, occupied, c);
}

Bitboard tuple_rider_moves_bb(PieceType pt, bool initial, Color c, Square s, Bitboard occupied) {
  return tuple_rider_attacks(pieceMap.get(pt)->tupleSlider[initial][MODALITY_QUIET], s, occupied, c);
}

Bitboard tuple_rider_between_bb(PieceType pt, Square s1, Square s2) {
  for (const auto& ray : pieceMap.get(pt)->tupleSlider[0][MODALITY_CAPTURE])
      if (Bitboard path = fixed_step_between_bb(s1, s2, ray.df, ray.dr))
      {
          int steps = popcount(path);
          if (steps < slider_min_distance(ray.limit))
              continue;
          int maxDistance = slider_max_distance(ray.limit);
          if (maxDistance && steps > maxDistance)
              continue;
          return path;
      }
  return Bitboard(0);
}


/// Bitboards::pretty() returns an ASCII representation of a bitboard suitable
/// to be printed to standard output. Useful for debugging.

std::string Bitboards::pretty(Bitboard b) {

  auto divider = []() {
      std::string line = "+";
      for (File f = FILE_A; f <= FILE_MAX; ++f)
          line += "---+";
      line += "\n";
      return line;
  };

  std::string s = divider();

  for (Rank r = RANK_MAX; r >= RANK_1; --r)
  {
      for (File f = FILE_A; f <= FILE_MAX; ++f)
          s += b & make_square(f, r) ? "| X " : "|   ";

      s += "| " + std::to_string(1 + r) + "\n" + divider();
  }
  s += " ";
  for (File f = FILE_A; f <= FILE_MAX; ++f)
      s += "  " + std::string(1, char('a' + f)) + " ";
  s += "\n";

  return s;
}

/// Bitboards::init_pieces() initializes piece move/attack bitboards and rider types

void Bitboards::init_pieces() {

  for (PieceType pt = PAWN; pt <= KING; ++pt)
  {
      const PieceInfo* pi = pieceMap.get(pt);

      // Detect rider types
      for (auto modality : {MODALITY_QUIET, MODALITY_CAPTURE})
      {
          for (bool initial : {false, true})
          {
              // We do not support initial captures
              if (modality == MODALITY_CAPTURE && initial)
                  continue;
              auto& riderTypes = modality == MODALITY_CAPTURE ? AttackRiderTypes[pt] : MoveRiderTypes[initial][pt];
              riderTypes = NO_RIDER;
              for (auto const& [d, limit] : pi->steps[initial][modality])
                  if (limit)
                      add_step_like_rider_types(riderTypes, d);
              for (auto const& [d, limit] : pi->slider[initial][modality])
                  add_slider_rider_types(riderTypes, d, limit);
              for (auto const& [d, limit] : pi->hopper[initial][modality])
                  add_hopper_rider_types(riderTypes, d, limit);
              if (pi->griffon[initial][modality])
                  riderTypes |= RIDER_GRIFFON_NH | RIDER_GRIFFON_SH | RIDER_GRIFFON_EV | RIDER_GRIFFON_WV;
              if (pi->manticore[initial][modality])
                  riderTypes |= RIDER_MANTICORE_NE | RIDER_MANTICORE_NW | RIDER_MANTICORE_SE | RIDER_MANTICORE_SW;
              if (pi->rose[initial][modality])
                  riderTypes |= RIDER_ROSE;
          }
      }

      // Initialize move/attack bitboards
      for (Color c : { WHITE, BLACK })
      {
          for (auto modality : {MODALITY_QUIET, MODALITY_CAPTURE})
          {
              for (bool initial : {false, true})
              {
                  // We do not support initial captures
                  if (modality == MODALITY_CAPTURE && initial)
                      continue;

                  std::map<Direction, int> riderDirs;
                  std::map<Direction, int> skiDirs;
                  for (auto const& [d, limit] : pi->slider[initial][modality])
                      if (limit == SKI_SLIDER_LIMIT)
                          skiDirs[d] = 0;
                      else if (limit == MAX_SLIDER_LIMIT)
                          riderDirs[d] = 0;
                      else if (limit >= 0 || is_slider_range(limit))
                          riderDirs[d] = limit;

                  for (Square s = SQ_A1; s <= SQ_MAX; ++s)
                  {
                      auto& pseudo = modality == MODALITY_CAPTURE ? PseudoAttacks[c][pt][s] : PseudoMoves[initial][c][pt][s];
                      auto& leaper = modality == MODALITY_CAPTURE ? LeaperAttacks[c][pt][s] : LeaperMoves[initial][c][pt][s];
                      pseudo = 0;
                      leaper = 0;
                      for (auto const& [d, limit] : pi->steps[initial][modality])
                      {
                          pseudo |= safe_destination(s, c == WHITE ? d : -d);
                          if (!limit)
                              leaper |= safe_destination(s, c == WHITE ? d : -d);
                      }
                      for (auto const& [dr, df] : pi->tupleSteps[initial][modality])
                      {
                          int tdr = c == WHITE ? dr : -dr;
                          int tdf = c == WHITE ? df : -df;
                          Bitboard dst = safe_destination_tuple(s, tdr, tdf);
                          pseudo |= dst;
                          leaper |= dst;
                      }
                      for (const auto& ray : pi->tupleSlider[initial][modality])
                      {
                          int tdr = c == WHITE ? ray.dr : -ray.dr;
                          int tdf = c == WHITE ? ray.df : -ray.df;
                          Square current = s;
                          int count = 0;
                          for (;;)
                          {
                              Bitboard dst = safe_destination_tuple(current, tdr, tdf);
                              if (!dst)
                                  break;
                              pseudo |= dst;
                              current = lsb(dst);
                              if (ray.limit > 0 && ++count >= ray.limit)
                                  break;
                          }
                      }
                      pseudo |= special_pseudo_bb(pi, initial, modality, s, c, riderDirs, skiDirs);
                      leaper |= special_leaper_bb(pi, initial, modality, s, c);
                  }
              }
          }
      }
  }
}


/// Bitboards::init() initializes various bitboard tables. It is called at
/// startup and relies on global objects to be already zero-initialized.

void Bitboards::init() {

  for (unsigned i = 0; i < (1 << 16); ++i)
      PopCnt16[i] = uint8_t(std::bitset<16>(i).count());

  for (Square s = SQ_A1; s <= SQ_MAX; ++s)
      SquareBB[s] = make_bitboard(s);

  for (File f = FILE_A; f <= FILE_MAX; ++f)
      for (Rank r = RANK_1; r <= RANK_MAX; ++r)
          BoardSizeBB[f][r] = forward_file_bb(BLACK, make_square(f, r)) | SquareBB[make_square(f, r)] | (f > FILE_A ? BoardSizeBB[f - 1][r] : Bitboard(0));

  for (Square s1 = SQ_A1; s1 <= SQ_MAX; ++s1)
      for (Square s2 = SQ_A1; s2 <= SQ_MAX; ++s2)
              SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));

#if !defined(VERY_LARGE_BOARDS)
  init_magics(FILE_MAX, RANK_MAX);
#endif

  init_pieces();

  for (Square s1 = SQ_A1; s1 <= SQ_MAX; ++s1)
  {
      for (PieceType pt : { BISHOP, ROOK })
          for (Square s2 = SQ_A1; s2 <= SQ_MAX; ++s2)
          {
              if (PseudoAttacks[WHITE][pt][s1] & s2)
              {
                  LineBB[s1][s2]    = (attacks_bb(WHITE, pt, s1, 0, current_magic_geometry) & attacks_bb(WHITE, pt, s2, 0, current_magic_geometry)) | s1 | s2;
                  BetweenBB[s1][s2] = (attacks_bb(WHITE, pt, s1, square_bb(s2), current_magic_geometry) & attacks_bb(WHITE, pt, s2, square_bb(s1), current_magic_geometry));
              }
              BetweenBB[s1][s2] |= s2;
          }
  }
}

namespace {

#if !defined(VERY_LARGE_BOARDS)

  std::unordered_map<uint16_t, std::shared_ptr<const MagicGeometry>> MagicByBoardSize;
  std::vector<uint16_t> MagicCacheLru;
  std::mutex MagicInitMutex;
  constexpr size_t MAX_MAGIC_CACHE_ENTRIES = 16;

  inline uint16_t magic_board_key(File f, Rank r) {
      return (uint16_t(f) << 8) | uint16_t(r);
  }

  inline void snapshot_magic_numbers(std::array<Bitboard, SQUARE_NB>& out, const Magic in[]) {
      for (Square s = SQ_A1; s <= SQ_MAX; ++s)
          out[s] = in[s].magic;
  }

  inline Bitboard active_magic_board(File maxFile, Rank maxRank) {
      return BoardSizeBB[maxFile][maxRank];
  }

  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
  // called "fancy" approach.

  template <MovementType MT, bool TrimRiderTerminal = false>
  void init_magic_table(Bitboard table[], Magic magics[], const std::map<Direction, int>& directions, File maxFile, Rank maxRank, const Bitboard* magicsInit = nullptr) {

    // Optimal PRNG seeds to pick the correct magics in the shortest time
#ifdef LARGEBOARDS
    int seeds[][RANK_NB] = { { 734, 10316, 55013, 32803, 12281, 15100,  16645, 255, 346, 89123 },
                             { 734, 10316, 55013, 32803, 12281, 15100,  16645, 255, 346, 89123 } };
#else
    int seeds[][RANK_NB] = { { 8977, 44560, 54343, 38998,  5731, 95205, 104912, 17020 },
                             {  728, 10316, 55013, 32803, 12281, 15100,  16645,   255 } };
#endif

    constexpr size_t TempTableSize = size_t(1) << (FILE_NB + RANK_NB - 4);
    std::vector<Bitboard> occupancy(TempTableSize);
    std::vector<Bitboard> reference(TempTableSize);
    [[maybe_unused]] Bitboard edges;
    Bitboard b;
    std::vector<int> epoch(TempTableSize);
    int cnt = 0, size = 0;

    for (Square s = SQ_A1; s <= SQ_MAX; ++s)
    {
        // Board edges are not considered in the relevant occupancies
        edges = ((Rank1BB | rank_bb(maxRank)) & ~rank_bb(s))
              | ((FileABB | file_bb(maxFile)) & ~file_bb(s));

        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask. Hence we deduce the size of the shift to
        // apply to the 64 or 32 bits word to get the index.
        Magic& m = magics[s];
        // The mask for hoppers is unlimited distance, even if the hopper is limited distance (e.g., grasshopper).
        if constexpr (MT == RIDER && TrimRiderTerminal)
        {
            // For leap-riders (e.g. nightrider), occupancy on the final square
            // of each ray cannot affect attacks, so it is not a relevant bit.
            Bitboard emptyAttack = sliding_attack<RIDER>(directions, s, 0) & active_magic_board(maxFile, maxRank);
            m.mask = emptyAttack & ~rider_terminal_squares(directions, s);
        }
        else
        {
            m.mask = (MT == LAME_LEAPER ? lame_leaper_path(directions, s)
                                        : sliding_attack<MT == HOPPER ? HOPPER_RANGE : MT>(directions, s, 0))
                   & active_magic_board(maxFile, maxRank) & ~edges;
        }
#ifdef LARGEBOARDS
        m.shift = 128 - popcount(m.mask);
#else
        m.shift = (Is64Bit ? 64 : 32) - popcount(m.mask);
#endif

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == SQ_A1 ? table : magics[s - 1].attacks + size;

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding sliding attack bitboard in reference[].
        b = size = 0;
        do {
            occupancy[size] = b;
            reference[size] = (MT == LAME_LEAPER ? lame_leaper_attack(directions, s, b) : sliding_attack<MT>(directions, s, b))
                            & active_magic_board(maxFile, maxRank);

            if (HasPext)
                m.attacks[pext(b, m.mask)] = reference[size];

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        if (HasPext)
            continue;

        PRNG rng(seeds[Is64Bit][rank_of(s)]);

        // Find a magic for square 's' picking up an (almost) random number
        // until we find the one that passes the verification test.
        // If a precomputed candidate is available, try it first and fall back
        // to randomized search if it collides for this board size.
        bool triedProvidedMagic = false;
        for (int i = 0; i < size; )
        {
            bool usedProvidedMagic = false;
            if (magicsInit && !triedProvidedMagic)
            {
                m.magic = magicsInit[s];
                triedProvidedMagic = true;
                usedProvidedMagic = true;
            }

            if (!usedProvidedMagic)
#ifdef LARGEBOARDS
                m.magic = (rng.sparse_rand<Bitboard>() << 64) ^ rng.sparse_rand<Bitboard>();
#else
                m.magic = rng.sparse_rand<Bitboard>();
#endif

            // A good magic must map every possible occupancy to an index that
            // looks up the correct sliding attack in the attacks[s] database.
            // Note that we build up the database for square 's' as a side
            // effect of verifying the magic. Keep track of the attempt count
            // and save it in epoch[], little speed-up trick to avoid resetting
            // m.attacks[] after every failed attempt.
            for (++cnt, i = 0; i < size; ++i)
            {
                unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt)
                {
                    epoch[idx] = cnt;
                    m.attacks[idx] = reference[i];
                }
                else if (m.attacks[idx] != reference[i])
                    break;
            }

            // Precomputed candidate failed for this board size, continue with
            // randomized search to guarantee progress.
            if (i < size && usedProvidedMagic)
                continue;
        }
    }

  }
#endif
}

std::shared_ptr<const MagicGeometry> Bitboards::init_magics(File maxFile, Rank maxRank) {
#if !defined(VERY_LARGE_BOARDS)
  const uint16_t boardKey = magic_board_key(maxFile, maxRank);

  std::lock_guard<std::mutex> lock(MagicInitMutex);
  const auto cacheIt = MagicByBoardSize.find(boardKey);
  if (cacheIt != MagicByBoardSize.end())
  {
      auto it = std::find(MagicCacheLru.begin(), MagicCacheLru.end(), boardKey);
      if (it != MagicCacheLru.end())
      {
          MagicCacheLru.erase(it);
          MagicCacheLru.push_back(boardKey);
      }
      current_magic_geometry = cacheIt->second.get();
      return cacheIt->second;
  }

  std::shared_ptr<MagicGeometry> mg = std::make_shared<MagicGeometry>();

#ifdef LARGEBOARDS
  mg->RookTableH.resize(0x11800);
  mg->RookTableV.resize(0x4800);
  mg->BishopTable.resize(0x33C00);
  mg->CannonTableH.resize(0x11800);
  mg->CannonTableV.resize(0x4800);
  mg->HorseTable.resize(0x500);
  mg->JanggiElephantTable.resize(0x1C000);
  mg->CannonDiagTable.resize(0x33C00);
  mg->NightriderTable.resize(0xD200);
  mg->GrasshopperTableH.resize(0x11800);
  mg->GrasshopperTableV.resize(0x4800);
  mg->GrasshopperTableD.resize(0x33C00);
#else
  mg->RookTableH.resize(0xA00);
  mg->RookTableV.resize(0xA00);
  mg->BishopTable.resize(0x1480);
  mg->CannonTableH.resize(0xA00);
  mg->CannonTableV.resize(0xA00);
  mg->HorseTable.resize(0x240);
  mg->JanggiElephantTable.resize(0x5C00);
  mg->CannonDiagTable.resize(0x1480);
  mg->NightriderTable.resize(0x500);
  mg->GrasshopperTableH.resize(0xA00);
  mg->GrasshopperTableV.resize(0xA00);
  mg->GrasshopperTableD.resize(0x1480);
#endif

#ifdef PRECOMPUTED_MAGICS
  init_magic_table<RIDER>(mg->RookTableH.data(), mg->RookMagicsH, RookDirectionsH, maxFile, maxRank, RookMagicHInit);
  init_magic_table<RIDER>(mg->RookTableV.data(), mg->RookMagicsV, RookDirectionsV, maxFile, maxRank, RookMagicVInit);
  init_magic_table<RIDER>(mg->BishopTable.data(), mg->BishopMagics, BishopDirections, maxFile, maxRank, BishopMagicInit);
  init_magic_table<HOPPER>(mg->CannonTableH.data(), mg->CannonMagicsH, RookDirectionsH, maxFile, maxRank, CannonMagicHInit);
  init_magic_table<HOPPER>(mg->CannonTableV.data(), mg->CannonMagicsV, RookDirectionsV, maxFile, maxRank, CannonMagicVInit);
  init_magic_table<LAME_LEAPER>(mg->HorseTable.data(), mg->HorseMagics, HorseDirections, maxFile, maxRank, HorseMagicInit);
  init_magic_table<LAME_LEAPER>(mg->JanggiElephantTable.data(), mg->JanggiElephantMagics, JanggiElephantDirections, maxFile, maxRank, JanggiElephantMagicInit);
  init_magic_table<HOPPER>(mg->CannonDiagTable.data(), mg->CannonDiagMagics, BishopDirections, maxFile, maxRank, CannonDiagMagicInit);
  init_magic_table<RIDER, true>(mg->NightriderTable.data(), mg->NightriderMagics, HorseDirections, maxFile, maxRank, NightriderMagicInit);
  init_magic_table<HOPPER>(mg->GrasshopperTableH.data(), mg->GrasshopperMagicsH, GrasshopperDirectionsH, maxFile, maxRank, GrasshopperMagicHInit);
  init_magic_table<HOPPER>(mg->GrasshopperTableV.data(), mg->GrasshopperMagicsV, GrasshopperDirectionsV, maxFile, maxRank, GrasshopperMagicVInit);
  init_magic_table<HOPPER>(mg->GrasshopperTableD.data(), mg->GrasshopperMagicsD, GrasshopperDirectionsD, maxFile, maxRank, GrasshopperMagicDInit);
#else
  init_magic_table<RIDER>(mg->RookTableH.data(), mg->RookMagicsH, RookDirectionsH, maxFile, maxRank, nullptr);
  init_magic_table<RIDER>(mg->RookTableV.data(), mg->RookMagicsV, RookDirectionsV, maxFile, maxRank, nullptr);
  init_magic_table<RIDER>(mg->BishopTable.data(), mg->BishopMagics, BishopDirections, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->CannonTableH.data(), mg->CannonMagicsH, RookDirectionsH, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->CannonTableV.data(), mg->CannonMagicsV, RookDirectionsV, maxFile, maxRank, nullptr);
  init_magic_table<LAME_LEAPER>(mg->HorseTable.data(), mg->HorseMagics, HorseDirections, maxFile, maxRank, nullptr);
  init_magic_table<LAME_LEAPER>(mg->JanggiElephantTable.data(), mg->JanggiElephantMagics, JanggiElephantDirections, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->CannonDiagTable.data(), mg->CannonDiagMagics, BishopDirections, maxFile, maxRank, nullptr);
  init_magic_table<RIDER, true>(mg->NightriderTable.data(), mg->NightriderMagics, HorseDirections, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->GrasshopperTableH.data(), mg->GrasshopperMagicsH, GrasshopperDirectionsH, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->GrasshopperTableV.data(), mg->GrasshopperMagicsV, GrasshopperDirectionsV, maxFile, maxRank, nullptr);
  init_magic_table<HOPPER>(mg->GrasshopperTableD.data(), mg->GrasshopperMagicsD, GrasshopperDirectionsD, maxFile, maxRank, nullptr);
#endif

  if (MagicByBoardSize.size() >= MAX_MAGIC_CACHE_ENTRIES && !MagicCacheLru.empty())
  {
      MagicByBoardSize.erase(MagicCacheLru.front());
      MagicCacheLru.erase(MagicCacheLru.begin());
  }
  MagicByBoardSize.emplace(boardKey, mg);
  MagicCacheLru.push_back(boardKey);

  current_magic_geometry = mg.get();
  return mg;
#else
  (void) maxFile;
  (void) maxRank;
  return nullptr;
#endif
}


void Bitboards::init_wrapped_rays(File maxFile, Rank maxRank, bool wrapFile, bool wrapRank) {
    if (!wrapFile && !wrapRank)
        return;

    static const int df[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dr[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };

    for (Rank r = RANK_1; r <= maxRank; ++r)
    {
        for (File f = FILE_A; f <= maxFile; ++f)
        {
            Square s = make_square(f, r);
            for (int d = 0; d < 8; ++d)
            {
                Bitboard b = 0;
                Square current = s;
                for (;;)
                {
                    Square next = SQ_NONE;
                    if (!wrapped_destination_square(current, df[d], dr[d], maxFile, maxRank, wrapFile, wrapRank, next))
                        break;
                    if (next == s)
                        break;
                    b |= next;
                    current = next;
                }
                WrappedRays[s][d] = b;
            }
        }
    }
}
} // namespace Stockfish
