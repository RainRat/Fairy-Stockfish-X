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
#include <bitset>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "bitboard.h"
#include "magic.h"
#include "misc.h"
#include "piece.h"
#include "thread.h"

namespace Stockfish {

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard PseudoAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard PseudoMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard BoardSizeBB[FILE_NB][RANK_NB];
RiderType AttackRiderTypes[PIECE_TYPE_NB];
RiderType MoveRiderTypes[2][PIECE_TYPE_NB];

namespace {
  std::shared_ptr<const MagicGeometry> current_magic_geometry_owner;
}
const MagicGeometry* current_magic_geometry = nullptr;

namespace {

// Some magics need to be split in order to reduce memory consumption.
// Otherwise on a 12x10 board they can be >100 MB.
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

  Bitboard leap_rider_attacks(const std::map<Direction, int>& directions, Square s, Bitboard occupied, Color c);

  Bitboard universal_hopper_potential(const std::map<Direction, PieceInfo::HopperProfile>& profiles, Square sq, Color c = WHITE) {
    Bitboard attack = 0;

    for (auto const& [d, profile] : profiles) {
      Direction dir = (c == WHITE ? d : -d);
      auto [stepR, stepF] = decode_direction(dir);
      int maxRaySteps = SQUARE_NB - 1;
      if (profile.hurdlesMax == 1)
          maxRaySteps = std::min(maxRaySteps, profile.preMax + profile.postMax);
      if (profile.equiRule == PieceInfo::EQUI_HOPPER && profile.preMax < 255)
          maxRaySteps = std::min(maxRaySteps, 2 * profile.preMax + 1);

      int rayDist = 0;
      Square prev = sq;
      for (Square s = sq + dir; is_ok(s) && (rayDist < maxRaySteps); s += dir) {
        if (int(file_of(s)) - int(file_of(prev)) != stepF
            || int(rank_of(s)) - int(rank_of(prev)) != stepR)
            break;
        prev = s;
        rayDist++;
        attack |= s;
      }
    }
    return attack;
  }

  Bitboard hopper_potential(const std::map<Direction, int>& directions, Square sq, Color c = WHITE) {
    Bitboard attack = 0;

    for (auto const& [d, _] : directions) {
      Direction dir = (c == WHITE ? d : -d);
      for (Square s = sq + dir; is_ok(s) && distance(s, s - dir) <= 2; s += dir)
        attack |= s;
    }

    return attack;
  }

  Bitboard special_pseudo_bb(const PieceInfo* pi, bool initial, MoveModality modality, Square s, Color c,
                             const std::map<Direction, int>& riderDirs,
                             const std::map<Direction, int>& skiDirs) {
    Bitboard pseudo = 0;

    pseudo |= sliding_attack<RIDER>(riderDirs, s, 0, c);
    pseudo |= leap_rider_attacks(pi->leapRider[initial][modality], s, 0, c);
    pseudo |= ski_sliding_attack(skiDirs, s, 0, c);
    pseudo |= hopper_potential(pi->hopper[initial][modality], s, c);
    pseudo |= universal_hopper_potential(pi->universalHopper[initial][modality], s, c);

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

  Bitboard special_leaper_bb(const PieceInfo* pi, bool initial, MoveModality modality, Square s) {
    Bitboard leaper = 0;

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
    if (limit == MAX_SLIDER_LIMIT)
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
    int maxDist = slider_max_distance(limit);
    if (RookDirectionsH.find(d) != RookDirectionsH.end())
        riderTypes |= maxDist == 1 ? RIDER_GRASSHOPPER_H : maxDist == 0 ? RIDER_CANNON_H : NO_RIDER;
    if (RookDirectionsV.find(d) != RookDirectionsV.end())
        riderTypes |= maxDist == 1 ? RIDER_GRASSHOPPER_V : maxDist == 0 ? RIDER_CANNON_V : NO_RIDER;
    if (BishopDirections.find(d) != BishopDirections.end())
        riderTypes |= maxDist == 1 ? RIDER_GRASSHOPPER_D : maxDist == 0 ? RIDER_CANNON_DIAG : NO_RIDER;
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
  Bitboard lame_leaper_path(const std::map<Direction, int>& directions, Square s) {
    Bitboard b = 0;
    for (const auto& i : directions)
        b |= lame_leaper_path(i.first, s);
    return b;
  }
  #endif

  Bitboard lame_leaper_attack(const std::map<Direction, int>& directions, Square s, Bitboard occupied) {
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
  Bitboard fixed_step_lame_rider_attacks(Square s, Bitboard occupied, int stepF, int stepR) {
    return fixed_step_lame_rider_attacks_core(s, occupied, stepF, stepR);
  }
#endif

  Bitboard limited_step_rider_attacks(Square s, Bitboard occupied, Direction d, int limit) {
    auto [stepR, stepF] = decode_direction(d);
    int count = 0;

    return walk_ray(s, stepF, stepR, false, [&](Square to, Bitboard& attack) {
        attack |= to;
        if (limit > 0 && ++count >= limit)
            return false;
        return !(occupied & to);
    });
  }

  Bitboard leap_rider_attacks(const std::map<Direction, int>& directions, Square s, Bitboard occupied, Color c) {
    Bitboard attack = 0;

    for (auto const& [d, limit] : directions)
    {
        attack |= limited_step_rider_attacks(s, occupied, c == WHITE ? d : Direction(-d), limit);
    }

    return attack;
  }

  inline Bitboard safe_destination(Square s, int step) {
      auto [dr, df] = decode_direction(Direction(step));
      return safe_destination_tuple(s, dr, df);
  }

}

template <typename Fn>
void walk_tuple_ray(const PieceInfo::TupleRay& ray, Square s, Color c, Bitboard occupied, Fn&& on_step) {
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
        if (!on_step(next, to, count))
            break;
        current = to;
        if (ray.limit > 0 && ++count >= ray.limit)
            break;
        if (occupied & next)
            break;
    }
}

Bitboard tuple_rider_attacks(const std::vector<PieceInfo::TupleRay>& rays, Square s, Bitboard occupied, Color c) {
  Bitboard attack = 0;

  for (const auto& ray : rays)
      walk_tuple_ray(ray, s, c, occupied, [&](Bitboard next, Square, int) {
          attack |= next;
          return true;
      });

    return attack;
}

Bitboard rider_terminal_squares(const std::map<Direction, int>& directions, Square sq, Bitboard activeBoard) {
    Bitboard terminal = 0;

    for (auto const& [d, _] : directions)
    {
        Bitboard next = safe_destination(sq, d);
        while (next)
        {
            Square to = lsb(next);
            Bitboard after = safe_destination(to, d);
            if (!after || !(activeBoard & after))
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
  Bitboard rider_attacks_bb(
    RiderType R, Square s, Bitboard occupied, const MagicGeometry* mg) {
  (void)mg;

  switch (R)
  {
  case RIDER_BISHOP: return sliding_attack<RIDER>(BishopDirections, s, occupied);
  case RIDER_ROOK_H: return sliding_attack<RIDER>(RookDirectionsH, s, occupied);
  case RIDER_ROOK_V: return sliding_attack<RIDER>(RookDirectionsV, s, occupied);
  case RIDER_CANNON_H: return sliding_attack<HOPPER>(RookDirectionsH, s, occupied);
  case RIDER_CANNON_V: return sliding_attack<HOPPER>(RookDirectionsV, s, occupied);
  case RIDER_LAME_DABBABA:
      return fixed_step_lame_rider_attacks(s, occupied, 2, 0)
           | fixed_step_lame_rider_attacks(s, occupied, -2, 0)
           | fixed_step_lame_rider_attacks(s, occupied, 0, 2)
           | fixed_step_lame_rider_attacks(s, occupied, 0, -2);
  case RIDER_HORSE: return lame_leaper_attack(HorseDirections, s, occupied);
  case RIDER_ELEPHANT:
      return fixed_step_lame_rider_attacks(s, occupied, 2, 2)
           | fixed_step_lame_rider_attacks(s, occupied, 2, -2)
           | fixed_step_lame_rider_attacks(s, occupied, -2, 2)
           | fixed_step_lame_rider_attacks(s, occupied, -2, -2);
  case RIDER_JANGGI_ELEPHANT: return lame_leaper_attack(JanggiElephantDirections, s, occupied);
  case RIDER_CANNON_DIAG: return sliding_attack<HOPPER>(BishopDirections, s, occupied);
  case RIDER_NIGHTRIDER: return sliding_attack<RIDER>(HorseDirections, s, occupied);
  case RIDER_GRASSHOPPER_H: return sliding_attack<HOPPER>(GrasshopperDirectionsH, s, occupied);
  case RIDER_GRASSHOPPER_V: return sliding_attack<HOPPER>(GrasshopperDirectionsV, s, occupied);
  case RIDER_GRASSHOPPER_D: return sliding_attack<HOPPER>(GrasshopperDirectionsD, s, occupied);
  case RIDER_GRIFFON_NH:
  case RIDER_GRIFFON_SH:
  case RIDER_GRIFFON_EV:
  case RIDER_GRIFFON_WV:
  case RIDER_MANTICORE_NE:
  case RIDER_MANTICORE_NW:
  case RIDER_MANTICORE_SE:
  case RIDER_MANTICORE_SW:
      return bent_rider_attack(R, s, occupied);
  case RIDER_SKI_ROOK_H: return ski_sliding_attack(RookDirectionsH, s, occupied);
  case RIDER_SKI_ROOK_V: return ski_sliding_attack(RookDirectionsV, s, occupied);
  case RIDER_SKI_BISHOP: return ski_sliding_attack(BishopDirections, s, occupied);
  case RIDER_ROSE: return rose_attacks_bb(s, occupied);
  default: return Bitboard(0);
  }
}
#endif

Bitboard custom_rider_attacks(PieceType pt, bool initial, bool isCapture, Color c, Square s, Bitboard occupied) {
  MoveModality m = isCapture ? MODALITY_CAPTURE : MODALITY_QUIET;
  return leap_rider_attacks(pieceMap.get(pt)->leapRider[initial][m], s, occupied, c)
       | tuple_rider_attacks(pieceMap.get(pt)->tupleSlider[initial][m], s, occupied, c);
}

Bitboard stepped_rider_attacks(Square s, Bitboard occupied, int stepF, int stepR) {
  return limited_step_rider_attacks(s, occupied, Direction(stepR * FILE_NB + stepF), 0);
}

Bitboard bent_rider_attack(RiderType R, Square s, Bitboard occupied) {
  int r = int(rank_of(s));
  int f = int(file_of(s));

  auto make_src = [&](int pivotF, int pivotR) {
      int srcF = f + pivotF;
      int srcR = r + pivotR;
      if (srcF < int(FILE_A) || srcF > int(FILE_MAX) || srcR < int(RANK_1) || srcR > int(RANK_MAX))
          return SQ_NONE;
      return make_square(File(srcF), Rank(srcR));
  };

  auto attack_from = [&](Square src, int stepF1, int stepR1, int stepF2, int stepR2) {
      if (occupied & src)
          return square_bb(src);
      return square_bb(src)
           | stepped_rider_attacks(src, occupied, stepF1, stepR1)
           | stepped_rider_attacks(src, occupied, stepF2, stepR2);
  };

  switch (R)
  {
    case RIDER_GRIFFON_NH:
    {
        Square src = make_src(1, 1);
        return is_ok(src) ? attack_from(src, 1, 0, 0, 1) : Bitboard(0);
    }
    case RIDER_GRIFFON_SH:
    {
        Square src = make_src(-1, 1);
        return is_ok(src) ? attack_from(src, -1, 0, 0, 1) : Bitboard(0);
    }
    case RIDER_GRIFFON_EV:
    {
        Square src = make_src(1, -1);
        return is_ok(src) ? attack_from(src, 1, 0, 0, -1) : Bitboard(0);
    }
    case RIDER_GRIFFON_WV:
    {
        Square src = make_src(-1, -1);
        return is_ok(src) ? attack_from(src, -1, 0, 0, -1) : Bitboard(0);
    }
    case RIDER_MANTICORE_NE:
    {
        Square src = make_src(0, 1);
        return is_ok(src) ? attack_from(src, 1, 1, -1, 1) : Bitboard(0);
    }
    case RIDER_MANTICORE_NW:
    {
        Square src = make_src(-1, 0);
        return is_ok(src) ? attack_from(src, -1, 1, -1, -1) : Bitboard(0);
    }
    case RIDER_MANTICORE_SE:
    {
        Square src = make_src(1, 0);
        return is_ok(src) ? attack_from(src, 1, 1, 1, -1) : Bitboard(0);
    }
    case RIDER_MANTICORE_SW:
    {
        Square src = make_src(0, -1);
        return is_ok(src) ? attack_from(src, 1, -1, -1, -1) : Bitboard(0);
    }
    default:
        return Bitboard(0);
  }
}

Bitboard tuple_rider_between_bb(PieceType pt, MoveModality modality, bool initial, Square s1, Square s2) {
  for (const auto& ray : pieceMap.get(pt)->tupleSlider[initial][modality])
  {
      Bitboard path = 0;
      bool hit = false;
      walk_tuple_ray(ray, s1, WHITE, Bitboard(0), [&](Bitboard next, Square to, int count) {
          path |= next;
          if (to != s2)
              return true;

          int steps = count + 1;
          if (steps < slider_min_distance(ray.limit))
              return false;
          int maxDistance = slider_max_distance(ray.limit);
          if (maxDistance && steps > maxDistance)
              return false;
          hit = true;
          return false;
      });
      if (hit)
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
      for (auto modality : {MODALITY_QUIET, MODALITY_CAPTURE})
      {
          for (bool initial : {false, true})
          {
              // We do not support initial captures
              if (modality == MODALITY_CAPTURE && initial)
                  continue;

              std::map<Direction, int> riderDirs;
              std::map<Direction, int> skiDirs;
              for (const auto& slider : pi->slider[initial][modality])
              {
                  const Direction d = slider.first;
                  const int limit = slider.second;
                  if (limit == SKI_SLIDER_LIMIT)
                      skiDirs[d] = 0;
                  else if (limit == MAX_SLIDER_LIMIT)
                      riderDirs[d] = 0;
                  else if (limit >= 0 || is_slider_range(limit))
                      riderDirs[d] = limit;
              }

              for (Color c : { WHITE, BLACK })
              {
                  for (Square s = SQ_A1; s <= SQ_MAX; ++s)
                  {
                      auto& pseudo = modality == MODALITY_CAPTURE ? PseudoAttacks[c][pt][s] : PseudoMoves[initial][c][pt][s];
                      auto& leaper = modality == MODALITY_CAPTURE ? LeaperAttacks[c][pt][s] : LeaperMoves[initial][c][pt][s];
                       pseudo = 0;
                       leaper = 0;
                       for (auto const& [d, limit] : pi->steps[initial][modality])
                       {
                           Bitboard dst = safe_destination(s, c == WHITE ? d : -d);
                           pseudo |= dst;
                           if (!limit)
                               leaper |= dst;
                       }
                       for (auto const& [d, profile] : pi->stepsLame[initial][modality])
                       {
                           Bitboard dst = safe_destination(s, c == WHITE ? d : -d);
                           pseudo |= dst;
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
                          walk_tuple_ray(ray, s, c, Bitboard(0), [&](Bitboard next, Square, int) {
                              pseudo |= next;
                              return true;
                          });
                      }
                      pseudo |= special_pseudo_bb(pi, initial, modality, s, c, riderDirs, skiDirs);
                      leaper |= special_leaper_bb(pi, initial, modality, s);
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

  [[noreturn]] void fatal_magic_initialization_error(const char* fmt, ...) {
      va_list args;
      va_start(args, fmt);
      std::vfprintf(stderr, fmt, args);
      va_end(args);
      std::abort();
  }

#if !defined(VERY_LARGE_BOARDS)

  std::unordered_map<uint16_t, std::shared_ptr<const MagicGeometry>> MagicByBoardSize;
  std::vector<uint16_t> MagicCacheLru;
  std::mutex MagicInitMutex;
  constexpr size_t MAX_MAGIC_CACHE_ENTRIES = 16;

  inline uint16_t magic_board_key(File f, Rank r) {
      return (uint16_t(f) << 8) | uint16_t(r);
  }

  inline Bitboard active_magic_board(File maxFile, Rank maxRank) {
      const int f = int(maxFile);
      const int r = int(maxRank);
      if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
          fatal_magic_initialization_error("invalid magic board size: file=%d rank=%d\n", f, r);
      if (BoardSizeBB[maxFile][maxRank] == Bitboard(0))
          fatal_magic_initialization_error("uninitialized magic board size: file=%d rank=%d\n", f, r);
      return BoardSizeBB[maxFile][maxRank];
  }

  template <MovementType MT, bool TrimRiderTerminal = false>
  Bitboard magic_mask_for_square(const std::map<Direction, int>& directions, Square s, File maxFile, Rank maxRank) {
      if constexpr (MT == RIDER && TrimRiderTerminal)
      {
          // For leap-riders (e.g. nightrider), occupancy on the final square
          // of each ray cannot affect attacks, so it is not a relevant bit.
          const Bitboard board = active_magic_board(maxFile, maxRank);
          Bitboard emptyAttack = sliding_attack<RIDER>(directions, s, 0) & board;
          return emptyAttack & ~rider_terminal_squares(directions, s, board);
      }
      else
      {
          // Board edges are not considered in the relevant occupancies.
          Bitboard edges = ((Rank1BB | rank_bb(maxRank)) & ~rank_bb(s))
                         | ((FileABB | file_bb(maxFile)) & ~file_bb(s));
          return (MT == LAME_LEAPER ? lame_leaper_path(directions, s)
                                    : sliding_attack<MT == HOPPER ? HOPPER_RANGE : MT>(directions, s, 0))
               & active_magic_board(maxFile, maxRank) & ~edges;
      }
  }

  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
  // called "fancy" approach.

  template <MovementType MT, bool TrimRiderTerminal = false>
  void init_magic_table(std::vector<Bitboard>& table, Magic magics[], const std::map<Direction, int>& directions, File maxFile, Rank maxRank, const Bitboard* magicsInit = nullptr) {

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
    std::array<Bitboard, SQUARE_NB> masks{};
    Bitboard b;
    std::vector<int> epoch(TempTableSize);
    int cnt = 0, size = 0;

    size_t requiredSize = 0;
    for (Square s = SQ_A1; s <= SQ_MAX; ++s)
    {
        masks[s] = magic_mask_for_square<MT, TrimRiderTerminal>(directions, s, maxFile, maxRank);
        requiredSize += size_t(1) << popcount(masks[s]);
    }
    if (table.size() < requiredSize)
        table.resize(requiredSize);

    for (Square s = SQ_A1; s <= SQ_MAX; ++s)
    {
        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask. Hence we deduce the size of the shift to
        // apply to the 64 or 32 bits word to get the index.
        Magic& m = magics[s];
        // The mask for hoppers is unlimited distance, even if the hopper is limited distance (e.g., grasshopper).
        m.mask = masks[s];
#ifdef LARGEBOARDS
        m.shift = 128 - popcount(m.mask);
#else
        m.shift = (Is64Bit ? 64 : 32) - popcount(m.mask);
#endif

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == SQ_A1 ? table.data() : magics[s - 1].attacks + size;
        assert(size_t((m.attacks - table.data()) + (size_t(1) << popcount(m.mask))) <= table.size());

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

// init_magics() computes all magic bitboard tables for a given board size.
// Assumes single-threaded initialization: must be called only from the main
// thread before search threads start, as current_magic_geometry is accessed
// lock-free during search.
std::shared_ptr<const MagicGeometry> Bitboards::init_magics(File maxFile, Rank maxRank) {
  if (!Threads.empty() && Threads.is_searching())
      fatal_magic_initialization_error("Bitboards::init_magics called while search threads are active\n");
#if !defined(VERY_LARGE_BOARDS)
  active_magic_board(maxFile, maxRank);
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
      current_magic_geometry_owner = cacheIt->second;
      current_magic_geometry = current_magic_geometry_owner.get();
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
  #define SELECT_MAGIC(init) init
#else
  #define SELECT_MAGIC(init) nullptr
#endif

  init_magic_table<RIDER>(mg->RookTableH, mg->RookMagicsH, RookDirectionsH, maxFile, maxRank, SELECT_MAGIC(RookMagicHInit));
  init_magic_table<RIDER>(mg->RookTableV, mg->RookMagicsV, RookDirectionsV, maxFile, maxRank, SELECT_MAGIC(RookMagicVInit));
  init_magic_table<RIDER>(mg->BishopTable, mg->BishopMagics, BishopDirections, maxFile, maxRank, SELECT_MAGIC(BishopMagicInit));
  init_magic_table<HOPPER>(mg->CannonTableH, mg->CannonMagicsH, RookDirectionsH, maxFile, maxRank, SELECT_MAGIC(CannonMagicHInit));
  init_magic_table<HOPPER>(mg->CannonTableV, mg->CannonMagicsV, RookDirectionsV, maxFile, maxRank, SELECT_MAGIC(CannonMagicVInit));
  init_magic_table<LAME_LEAPER>(mg->HorseTable, mg->HorseMagics, HorseDirections, maxFile, maxRank, SELECT_MAGIC(HorseMagicInit));
  init_magic_table<LAME_LEAPER>(mg->JanggiElephantTable, mg->JanggiElephantMagics, JanggiElephantDirections, maxFile, maxRank, SELECT_MAGIC(JanggiElephantMagicInit));
  init_magic_table<HOPPER>(mg->CannonDiagTable, mg->CannonDiagMagics, BishopDirections, maxFile, maxRank, SELECT_MAGIC(CannonDiagMagicInit));
  init_magic_table<RIDER, true>(mg->NightriderTable, mg->NightriderMagics, HorseDirections, maxFile, maxRank, SELECT_MAGIC(NightriderMagicInit));
  init_magic_table<HOPPER>(mg->GrasshopperTableH, mg->GrasshopperMagicsH, GrasshopperDirectionsH, maxFile, maxRank, SELECT_MAGIC(GrasshopperMagicHInit));
  init_magic_table<HOPPER>(mg->GrasshopperTableV, mg->GrasshopperMagicsV, GrasshopperDirectionsV, maxFile, maxRank, SELECT_MAGIC(GrasshopperMagicVInit));
  init_magic_table<HOPPER>(mg->GrasshopperTableD, mg->GrasshopperMagicsD, GrasshopperDirectionsD, maxFile, maxRank, SELECT_MAGIC(GrasshopperMagicDInit));

#undef SELECT_MAGIC

  if (MagicByBoardSize.size() >= MAX_MAGIC_CACHE_ENTRIES && !MagicCacheLru.empty())
  {
      MagicByBoardSize.erase(MagicCacheLru.front());
      MagicCacheLru.erase(MagicCacheLru.begin());
  }
  MagicByBoardSize.emplace(boardKey, mg);
  MagicCacheLru.push_back(boardKey);

  current_magic_geometry_owner = mg;
  current_magic_geometry = current_magic_geometry_owner.get();
  return mg;
#else
  (void) maxFile;
  (void) maxRank;
  return nullptr;
#endif
}


} // namespace Stockfish
