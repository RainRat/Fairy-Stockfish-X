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
Bitboard PseudoAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard PseudoMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard LeaperMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
Bitboard BoardSizeBB[FILE_NB][RANK_NB];
RiderType AttackRiderTypes[PIECE_TYPE_NB];
RiderType MoveRiderTypes[2][PIECE_TYPE_NB];

Magic RookMagicsH[SQUARE_NB];
Magic RookMagicsV[SQUARE_NB];
Magic BishopMagics[SQUARE_NB];
Magic CannonMagicsH[SQUARE_NB];
Magic CannonMagicsV[SQUARE_NB];
Magic HorseMagics[SQUARE_NB];
Magic JanggiElephantMagics[SQUARE_NB];
Magic CannonDiagMagics[SQUARE_NB];
Magic NightriderMagics[SQUARE_NB];
Magic GrasshopperMagicsH[SQUARE_NB];
Magic GrasshopperMagicsV[SQUARE_NB];
Magic GrasshopperMagicsD[SQUARE_NB];

Magic* magics[] = {BishopMagics, RookMagicsH, RookMagicsV, CannonMagicsH, CannonMagicsV,
                   BishopMagics, HorseMagics, BishopMagics, JanggiElephantMagics, CannonDiagMagics, NightriderMagics,
                   GrasshopperMagicsH, GrasshopperMagicsV, GrasshopperMagicsD};

namespace {

// Some magics need to be split in order to reduce memory consumption.
// Otherwise on a 12x10 board they can be >100 MB.
#if !defined(VERY_LARGE_BOARDS)
#ifdef LARGEBOARDS
  Bitboard RookTableH[0x11800];  // To store horizontal rook attacks
  Bitboard RookTableV[0x4800];  // To store vertical rook attacks
  Bitboard BishopTable[0x33C00]; // To store bishop attacks
  Bitboard CannonTableH[0x11800];  // To store horizontal cannon attacks
  Bitboard CannonTableV[0x4800];  // To store vertical cannon attacks
  Bitboard HorseTable[0x500];  // To store horse attacks
  Bitboard JanggiElephantTable[0x1C000];  // To store janggi elephant attacks
  Bitboard CannonDiagTable[0x33C00]; // To store diagonal cannon attacks
  // Nightrider masks trim terminal leap squares; 12x10 max needs 0xD200 entries.
  Bitboard NightriderTable[0xD200]; // To store nightrider attacks
  Bitboard GrasshopperTableH[0x11800];  // To store horizontal grasshopper attacks
  Bitboard GrasshopperTableV[0x4800];  // To store vertical grasshopper attacks
  Bitboard GrasshopperTableD[0x33C00]; // To store diagonal grasshopper attacks
#else
  Bitboard RookTableH[0xA00];  // To store horizontal rook attacks
  Bitboard RookTableV[0xA00];  // To store vertical rook attacks
  Bitboard BishopTable[0x1480]; // To store bishop attacks
  Bitboard CannonTableH[0xA00];  // To store horizontal cannon attacks
  Bitboard CannonTableV[0xA00];  // To store vertical cannon attacks
  Bitboard HorseTable[0x240];  // To store horse attacks
  Bitboard JanggiElephantTable[0x5C00];  // To store janggi elephant attacks
  Bitboard CannonDiagTable[0x1480]; // To store diagonal cannon attacks
  // Nightrider masks trim terminal leap squares; 8x8 max needs 0x500 entries.
  Bitboard NightriderTable[0x500]; // To store nightrider attacks
  Bitboard GrasshopperTableH[0xA00];  // To store horizontal grasshopper attacks
  Bitboard GrasshopperTableV[0xA00];  // To store vertical grasshopper attacks
  Bitboard GrasshopperTableD[0x1480]; // To store diagonal grasshopper attacks
#endif
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
#ifdef PRECOMPUTED_MAGICS
  void init_magics(Bitboard table[], Magic magics[], const std::map<Direction, int>& directions, const Bitboard magicsInit[]);
#else
  void init_magics(Bitboard table[], Magic magics[], const std::map<Direction, int>& directions);
#endif

  template <MovementType MT>
  Bitboard sliding_attack(const std::map<Direction, int>& directions, Square sq, Bitboard occupied, Color c = WHITE) {
    assert(MT != LAME_LEAPER);

    Bitboard attack = 0;

    for (auto const& [d, limit] : directions)
    {
        int count = 0;
        bool hurdle = false;
        for (Square s = sq + (c == WHITE ? d : -d);
             is_ok(s) && distance(s, s - (c == WHITE ? d : -d)) <= 2;
             s += (c == WHITE ? d : -d))
        {
            if (MT != HOPPER || hurdle)
            {
                attack |= s;
                // For hoppers we consider limit == 1 as a grasshopper,
                // but limit > 1 as a limited distance hopper
                if (limit > 0 && !(MT == HOPPER_RANGE && limit == 1) && ++count >= limit)
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

}

/// safe_destination() returns the bitboard of target square for the given step
/// from the given square. If the step is off the board, returns empty bitboard.

inline Bitboard safe_destination(Square s, int step) {
    Square to = Square(int(s) + step);
    if (!is_ok(to))
        return Bitboard(0);

    // Prevent horizontal edge wrapping by decoding the linear step into a
    // canonical (dr, df) pair and matching exact board deltas.
    // For a fixed 'step', two nearby decompositions exist around trunc division:
    //   step = q * FILE_NB + r
    // and step = (q +/- 1) * FILE_NB + (r -/+ FILE_NB).
    // Pick the candidate with smaller Chebyshev distance, then prefer true
    // diagonal/orthogonal components over degenerate long-file remainders.
    const int f = FILE_NB;
    const int q1 = step / f; // trunc toward zero
    const int r1 = step - q1 * f;
    const int q2 = q1 + (step >= 0 ? 1 : -1);
    const int r2 = step - q2 * f;

    const int q1Abs = std::abs(q1), r1Abs = std::abs(r1);
    const int q2Abs = std::abs(q2), r2Abs = std::abs(r2);
    const int m1 = std::max(q1Abs, r1Abs);
    const int m2 = std::max(q2Abs, r2Abs);
    const int z1 = (q1 != 0 && r1 != 0) ? 0 : 1;
    const int z2 = (q2 != 0 && r2 != 0) ? 0 : 1;
    const int s1 = q1Abs + r1Abs;
    const int s2 = q2Abs + r2Abs;

    const bool pickSecond = (m2 < m1) || (m2 == m1 && (z2 < z1 || (z2 == z1 && s2 < s1)));
    const int decodedDr = pickSecond ? q2 : q1;
    const int decodedDf = pickSecond ? r2 : r1;
    int expectedDr = std::abs(decodedDr);
    int expectedDf = std::abs(decodedDf);
    int actualDr = std::abs(int(rank_of(to)) - int(rank_of(s)));
    int actualDf = std::abs(int(file_of(to)) - int(file_of(s)));
    return (actualDr == expectedDr && actualDf == expectedDf) ? square_bb(to) : Bitboard(0);
}

inline Bitboard safe_destination_tuple(Square s, int dr, int df) {
    int r = int(rank_of(s)) + dr;
    int f = int(file_of(s)) + df;
    if (r < 0 || r > int(RANK_MAX) || f < 0 || f > int(FILE_MAX))
        return Bitboard(0);
    return square_bb(make_square(File(f), Rank(r)));
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
  case RIDER_LAME_DABBABA: return lame_leaper_attack(LameDabbabaDirections, s, occupied);
  case RIDER_HORSE: return lame_leaper_attack(HorseDirections, s, occupied);
  case RIDER_ELEPHANT: return lame_leaper_attack(ElephantDirections, s, occupied);
  case RIDER_JANGGI_ELEPHANT: return lame_leaper_attack(JanggiElephantDirections, s, occupied);
  case RIDER_CANNON_DIAG: return sliding_attack<HOPPER>(BishopDirections, s, occupied);
  case RIDER_NIGHTRIDER: return sliding_attack<RIDER>(HorseDirections, s, occupied);
  case RIDER_GRASSHOPPER_H: return sliding_attack<HOPPER>(GrasshopperDirectionsH, s, occupied);
  case RIDER_GRASSHOPPER_V: return sliding_attack<HOPPER>(GrasshopperDirectionsV, s, occupied);
  case RIDER_GRASSHOPPER_D: return sliding_attack<HOPPER>(GrasshopperDirectionsD, s, occupied);
  case RIDER_GRIFFON_NH: {
      Square src = shifted_source(NORTH);
      return src == SQ_NONE ? Bitboard(0) : sliding_attack<RIDER>(RookDirectionsH, src, occupied);
  }
  case RIDER_GRIFFON_SH: {
      Square src = shifted_source(SOUTH);
      return src == SQ_NONE ? Bitboard(0) : sliding_attack<RIDER>(RookDirectionsH, src, occupied);
  }
  case RIDER_GRIFFON_EV: {
      Square src = shifted_source(EAST);
      return src == SQ_NONE ? Bitboard(0) : sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  case RIDER_GRIFFON_WV: {
      Square src = shifted_source(WEST);
      return src == SQ_NONE ? Bitboard(0) : sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  case RIDER_MANTICORE_NE: {
      Square src = shifted_source(NORTH_EAST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(RookDirectionsH, src, occupied) | sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  case RIDER_MANTICORE_NW: {
      Square src = shifted_source(NORTH_WEST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(RookDirectionsH, src, occupied) | sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  case RIDER_MANTICORE_SE: {
      Square src = shifted_source(SOUTH_EAST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(RookDirectionsH, src, occupied) | sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  case RIDER_MANTICORE_SW: {
      Square src = shifted_source(SOUTH_WEST);
      return src == SQ_NONE ? Bitboard(0)
                            : sliding_attack<RIDER>(RookDirectionsH, src, occupied) | sliding_attack<RIDER>(RookDirectionsV, src, occupied);
  }
  default: return Bitboard(0);
  }
}
#endif


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
      const PieceInfo* pi = pieceMap.find(pt)->second;

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
              {
                  if (limit)
                  {
                      const int ad = std::abs(int(d));
                      if ((ad % FILE_NB) == 0 || ad < FILE_NB)
                          riderTypes |= RIDER_ROOK_H | RIDER_ROOK_V;
                      if ((FILE_NB > 1 && (ad % (FILE_NB - 1)) == 0) || (ad % (FILE_NB + 1)) == 0)
                          riderTypes |= RIDER_BISHOP;
                  }
                  if (limit && LameDabbabaDirections.find(d) != LameDabbabaDirections.end())
                      riderTypes |= RIDER_LAME_DABBABA;
                  if (limit && HorseDirections.find(d) != HorseDirections.end())
                      riderTypes |= RIDER_HORSE;
                  if (limit && ElephantDirections.find(d) != ElephantDirections.end())
                      riderTypes |= RIDER_ELEPHANT;
                  if (limit && JanggiElephantDirections.find(d) != JanggiElephantDirections.end())
                      riderTypes |= RIDER_JANGGI_ELEPHANT;
              }
              for (auto const& [d, limit] : pi->slider[initial][modality])
              {
                  if (limit == DYNAMIC_SLIDER_LIMIT)
                      continue;
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
              for (auto const& [d, limit] : pi->hopper[initial][modality])
              {
                  if (RookDirectionsH.find(d) != RookDirectionsH.end())
                      riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_H : RIDER_CANNON_H;
                  if (RookDirectionsV.find(d) != RookDirectionsV.end())
                      riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_V : RIDER_CANNON_V;
                  if (BishopDirections.find(d) != BishopDirections.end())
                      riderTypes |= limit == 1 ? RIDER_GRASSHOPPER_D : RIDER_CANNON_DIAG;
              }
              if (pi->griffon[initial][modality])
                  riderTypes |= RIDER_GRIFFON_NH | RIDER_GRIFFON_SH | RIDER_GRIFFON_EV | RIDER_GRIFFON_WV;
              if (pi->manticore[initial][modality])
                  riderTypes |= RIDER_MANTICORE_NE | RIDER_MANTICORE_NW | RIDER_MANTICORE_SE | RIDER_MANTICORE_SW;
          }
      }

      // Initialize move/attack bitboards
      for (Color c : { WHITE, BLACK })
      {
          for (Square s = SQ_A1; s <= SQ_MAX; ++s)
          {
              for (auto modality : {MODALITY_QUIET, MODALITY_CAPTURE})
              {
                  for (bool initial : {false, true})
                  {
                      // We do not support initial captures
                      if (modality == MODALITY_CAPTURE && initial)
                          continue;
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
                      {
                          std::map<Direction, int> dirs;
                          for (auto const& [d, limit] : pi->slider[initial][modality])
                              if (limit >= 0)
                                  dirs[d] = limit;
                          pseudo |= sliding_attack<RIDER>(dirs, s, 0, c);
                      }
                      pseudo |= sliding_attack<HOPPER_RANGE>(pi->hopper[initial][modality], s, 0, c);
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
                  LineBB[s1][s2]    = (attacks_bb(WHITE, pt, s1, 0) & attacks_bb(WHITE, pt, s2, 0)) | s1 | s2;
                  BetweenBB[s1][s2] = (attacks_bb(WHITE, pt, s1, square_bb(s2)) & attacks_bb(WHITE, pt, s2, square_bb(s1)));
              }
              BetweenBB[s1][s2] |= s2;
          }
  }
}

namespace {

#if !defined(VERY_LARGE_BOARDS)

  File CurrentMagicMaxFile = FILE_MAX;
  Rank CurrentMagicMaxRank = RANK_MAX;
  std::atomic<bool> MagicsInitialized { false };
  std::atomic<uint16_t> CurrentMagicBoardKey { 0 };

  struct MagicNumberCache {
      std::array<Bitboard, SQUARE_NB> rookH {};
      std::array<Bitboard, SQUARE_NB> rookV {};
      std::array<Bitboard, SQUARE_NB> bishop {};
      std::array<Bitboard, SQUARE_NB> cannonH {};
      std::array<Bitboard, SQUARE_NB> cannonV {};
      std::array<Bitboard, SQUARE_NB> horse {};
      std::array<Bitboard, SQUARE_NB> janggiElephant {};
      std::array<Bitboard, SQUARE_NB> cannonDiag {};
      std::array<Bitboard, SQUARE_NB> nightrider {};
      std::array<Bitboard, SQUARE_NB> grasshopperH {};
      std::array<Bitboard, SQUARE_NB> grasshopperV {};
      std::array<Bitboard, SQUARE_NB> grasshopperD {};
  };

  std::unordered_map<uint16_t, MagicNumberCache> MagicByBoardSize;
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

  inline Bitboard active_magic_board() {
      return BoardSizeBB[CurrentMagicMaxFile][CurrentMagicMaxRank];
  }

  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
  // called "fancy" approach.

  template <MovementType MT, bool TrimRiderTerminal = false>
  void init_magic_table(Bitboard table[], Magic magics[], const std::map<Direction, int>& directions, const Bitboard* magicsInit = nullptr) {

    // Optimal PRNG seeds to pick the correct magics in the shortest time
#ifdef LARGEBOARDS
    int seeds[][RANK_NB] = { { 734, 10316, 55013, 32803, 12281, 15100,  16645, 255, 346, 89123 },
                             { 734, 10316, 55013, 32803, 12281, 15100,  16645, 255, 346, 89123 } };
#else
    int seeds[][RANK_NB] = { { 8977, 44560, 54343, 38998,  5731, 95205, 104912, 17020 },
                             {  728, 10316, 55013, 32803, 12281, 15100,  16645,   255 } };
#endif

    Bitboard* occupancy = new Bitboard[1 << (FILE_NB + RANK_NB - 4)];
    Bitboard* reference = new Bitboard[1 << (FILE_NB + RANK_NB - 4)];
    [[maybe_unused]] Bitboard edges;
    Bitboard b;
    int* epoch = new int[1 << (FILE_NB + RANK_NB - 4)]();
    int cnt = 0, size = 0;

    for (Square s = SQ_A1; s <= SQ_MAX; ++s)
    {
        // Board edges are not considered in the relevant occupancies
        edges = ((Rank1BB | rank_bb(CurrentMagicMaxRank)) & ~rank_bb(s))
              | ((FileABB | file_bb(CurrentMagicMaxFile)) & ~file_bb(s));

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
            Bitboard emptyAttack = sliding_attack<RIDER>(directions, s, 0) & active_magic_board();
            m.mask = emptyAttack & ~rider_terminal_squares(directions, s);
        }
        else
        {
            m.mask = (MT == LAME_LEAPER ? lame_leaper_path(directions, s)
                                        : sliding_attack<MT == HOPPER ? HOPPER_RANGE : MT>(directions, s, 0))
                   & active_magic_board() & ~edges;
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
                            & active_magic_board();

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

    delete[] occupancy;
    delete[] reference;
    delete[] epoch;
  }
#endif
}

void Bitboards::init_magics(File maxFile, Rank maxRank) {
#if !defined(VERY_LARGE_BOARDS)
  const uint16_t boardKey = magic_board_key(maxFile, maxRank);
  if (MagicsInitialized.load(std::memory_order_acquire)
      && boardKey == CurrentMagicBoardKey.load(std::memory_order_relaxed))
      return;

  std::lock_guard<std::mutex> lock(MagicInitMutex);
  if (MagicsInitialized.load(std::memory_order_relaxed)
      && maxFile == CurrentMagicMaxFile && maxRank == CurrentMagicMaxRank)
      return;

  CurrentMagicMaxFile = maxFile;
  CurrentMagicMaxRank = maxRank;
  const auto cacheIt = MagicByBoardSize.find(boardKey);
  const MagicNumberCache* cache = cacheIt == MagicByBoardSize.end() ? nullptr : &cacheIt->second;
  if (cache)
  {
      auto it = std::find(MagicCacheLru.begin(), MagicCacheLru.end(), boardKey);
      if (it != MagicCacheLru.end())
      {
          MagicCacheLru.erase(it);
          MagicCacheLru.push_back(boardKey);
      }
  }

#ifdef PRECOMPUTED_MAGICS
  init_magic_table<RIDER>(RookTableH, RookMagicsH, RookDirectionsH, cache ? cache->rookH.data() : RookMagicHInit);
  init_magic_table<RIDER>(RookTableV, RookMagicsV, RookDirectionsV, cache ? cache->rookV.data() : RookMagicVInit);
  init_magic_table<RIDER>(BishopTable, BishopMagics, BishopDirections, cache ? cache->bishop.data() : BishopMagicInit);
  init_magic_table<HOPPER>(CannonTableH, CannonMagicsH, RookDirectionsH, cache ? cache->cannonH.data() : CannonMagicHInit);
  init_magic_table<HOPPER>(CannonTableV, CannonMagicsV, RookDirectionsV, cache ? cache->cannonV.data() : CannonMagicVInit);
  init_magic_table<LAME_LEAPER>(HorseTable, HorseMagics, HorseDirections, cache ? cache->horse.data() : HorseMagicInit);
  init_magic_table<LAME_LEAPER>(JanggiElephantTable, JanggiElephantMagics, JanggiElephantDirections, cache ? cache->janggiElephant.data() : JanggiElephantMagicInit);
  init_magic_table<HOPPER>(CannonDiagTable, CannonDiagMagics, BishopDirections, cache ? cache->cannonDiag.data() : CannonDiagMagicInit);
  init_magic_table<RIDER, true>(NightriderTable, NightriderMagics, HorseDirections, cache ? cache->nightrider.data() : NightriderMagicInit);
  init_magic_table<HOPPER>(GrasshopperTableH, GrasshopperMagicsH, GrasshopperDirectionsH, cache ? cache->grasshopperH.data() : GrasshopperMagicHInit);
  init_magic_table<HOPPER>(GrasshopperTableV, GrasshopperMagicsV, GrasshopperDirectionsV, cache ? cache->grasshopperV.data() : GrasshopperMagicVInit);
  init_magic_table<HOPPER>(GrasshopperTableD, GrasshopperMagicsD, GrasshopperDirectionsD, cache ? cache->grasshopperD.data() : GrasshopperMagicDInit);
#else
  init_magic_table<RIDER>(RookTableH, RookMagicsH, RookDirectionsH, cache ? cache->rookH.data() : nullptr);
  init_magic_table<RIDER>(RookTableV, RookMagicsV, RookDirectionsV, cache ? cache->rookV.data() : nullptr);
  init_magic_table<RIDER>(BishopTable, BishopMagics, BishopDirections, cache ? cache->bishop.data() : nullptr);
  init_magic_table<HOPPER>(CannonTableH, CannonMagicsH, RookDirectionsH, cache ? cache->cannonH.data() : nullptr);
  init_magic_table<HOPPER>(CannonTableV, CannonMagicsV, RookDirectionsV, cache ? cache->cannonV.data() : nullptr);
  init_magic_table<LAME_LEAPER>(HorseTable, HorseMagics, HorseDirections, cache ? cache->horse.data() : nullptr);
  init_magic_table<LAME_LEAPER>(JanggiElephantTable, JanggiElephantMagics, JanggiElephantDirections, cache ? cache->janggiElephant.data() : nullptr);
  init_magic_table<HOPPER>(CannonDiagTable, CannonDiagMagics, BishopDirections, cache ? cache->cannonDiag.data() : nullptr);
  init_magic_table<RIDER, true>(NightriderTable, NightriderMagics, HorseDirections, cache ? cache->nightrider.data() : nullptr);
  init_magic_table<HOPPER>(GrasshopperTableH, GrasshopperMagicsH, GrasshopperDirectionsH, cache ? cache->grasshopperH.data() : nullptr);
  init_magic_table<HOPPER>(GrasshopperTableV, GrasshopperMagicsV, GrasshopperDirectionsV, cache ? cache->grasshopperV.data() : nullptr);
  init_magic_table<HOPPER>(GrasshopperTableD, GrasshopperMagicsD, GrasshopperDirectionsD, cache ? cache->grasshopperD.data() : nullptr);
#endif

  if (!cache) {
      if (MagicByBoardSize.size() >= MAX_MAGIC_CACHE_ENTRIES && !MagicCacheLru.empty())
      {
          MagicByBoardSize.erase(MagicCacheLru.front());
          MagicCacheLru.erase(MagicCacheLru.begin());
      }
      MagicNumberCache fresh {};
      snapshot_magic_numbers(fresh.rookH, RookMagicsH);
      snapshot_magic_numbers(fresh.rookV, RookMagicsV);
      snapshot_magic_numbers(fresh.bishop, BishopMagics);
      snapshot_magic_numbers(fresh.cannonH, CannonMagicsH);
      snapshot_magic_numbers(fresh.cannonV, CannonMagicsV);
      snapshot_magic_numbers(fresh.horse, HorseMagics);
      snapshot_magic_numbers(fresh.janggiElephant, JanggiElephantMagics);
      snapshot_magic_numbers(fresh.cannonDiag, CannonDiagMagics);
      snapshot_magic_numbers(fresh.nightrider, NightriderMagics);
      snapshot_magic_numbers(fresh.grasshopperH, GrasshopperMagicsH);
      snapshot_magic_numbers(fresh.grasshopperV, GrasshopperMagicsV);
      snapshot_magic_numbers(fresh.grasshopperD, GrasshopperMagicsD);
      MagicByBoardSize.emplace(boardKey, std::move(fresh));
      MagicCacheLru.push_back(boardKey);
  }

  CurrentMagicBoardKey.store(boardKey, std::memory_order_relaxed);
  MagicsInitialized.store(true, std::memory_order_release);
#else
  (void) maxFile;
  (void) maxRank;
#endif
}

} // namespace Stockfish
