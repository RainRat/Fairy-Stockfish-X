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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <array>
#include <utility>
#include <string>
#include <vector>
#include <memory>

#include "types.h"

namespace Stockfish {

/// Magic holds all magic bitboards relevant data for a single square
struct Magic {
  Bitboard  mask;
  Bitboard  magic;
  Bitboard* attacks;
  unsigned  shift;

  // Compute the attack's index using the 'magic bitboards' approach
  unsigned index(Bitboard occupied) const {

    if (HasPext)
        return unsigned(pext(occupied, mask));

#ifdef LARGEBOARDS
    return unsigned(((occupied & mask) * magic) >> shift);
#else
    if (Is64Bit)
        return unsigned(((occupied & mask) * magic) >> shift);

    unsigned lo = unsigned(occupied) & unsigned(mask);
    unsigned hi = unsigned(occupied >> 32) & unsigned(mask >> 32);
    return (lo * unsigned(magic) ^ hi * unsigned(magic >> 32)) >> shift;
#endif
  }
};

struct MagicGeometry {
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

  std::vector<Bitboard> RookTableH;
  std::vector<Bitboard> RookTableV;
  std::vector<Bitboard> BishopTable;
  std::vector<Bitboard> CannonTableH;
  std::vector<Bitboard> CannonTableV;
  std::vector<Bitboard> HorseTable;
  std::vector<Bitboard> JanggiElephantTable;
  std::vector<Bitboard> CannonDiagTable;
  std::vector<Bitboard> NightriderTable;
  std::vector<Bitboard> GrasshopperTableH;
  std::vector<Bitboard> GrasshopperTableV;
  std::vector<Bitboard> GrasshopperTableD;

  Magic* magics[14];

  MagicGeometry() {
    magics[0] = BishopMagics;
    magics[1] = RookMagicsH;
    magics[2] = RookMagicsV;
    magics[3] = CannonMagicsH;
    magics[4] = CannonMagicsV;
    magics[5] = BishopMagics;
    magics[6] = HorseMagics;
    magics[7] = BishopMagics;
    magics[8] = JanggiElephantMagics;
    magics[9] = CannonDiagMagics;
    magics[10] = NightriderMagics;
    magics[11] = GrasshopperMagicsH;
    magics[12] = GrasshopperMagicsV;
    magics[13] = GrasshopperMagicsD;
  }
};

extern thread_local const MagicGeometry* current_magic_geometry;

namespace Bitbases {

void init();
bool probe(Square wksq, Square wpsq, Square bksq, Color us);

} // namespace Stockfish::Bitbases

namespace Bitboards {

void init_pieces();
std::shared_ptr<const MagicGeometry> init_magics(File maxFile, Rank maxRank);
void init_wrapped_rays(File maxFile, Rank maxRank, bool wrapFile, bool wrapRank);
void init();
std::string pretty(Bitboard b);

} // namespace Stockfish::Bitboards

constexpr Bitboard all_squares_bb() {
  Bitboard b = 0;
  for (int i = 0; i < SQUARE_NB; ++i)
      b = b | (Bitboard(1) << i);
  return b;
}

constexpr Bitboard dark_squares_bb() {
  Bitboard b = 0;
  for (int r = 0; r < RANK_NB; ++r)
      for (int f = 0; f < FILE_NB; ++f)
          if ((r + f) & 1)
              b = b | (Bitboard(1) << (r * FILE_NB + f));
  return b;
}

constexpr Bitboard file_a_bb() {
  Bitboard b = 0;
  for (int r = 0; r < RANK_NB; ++r)
      b = b | (Bitboard(1) << (r * FILE_NB));
  return b;
}

constexpr Bitboard AllSquares = all_squares_bb();
constexpr Bitboard DarkSquares = dark_squares_bb();
constexpr Bitboard FileABB = file_a_bb();
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;
#ifdef LARGEBOARDS
constexpr Bitboard FileIBB = FileABB << 8;
constexpr Bitboard FileJBB = FileABB << 9;
constexpr Bitboard FileKBB = FileABB << 10;
constexpr Bitboard FileLBB = FileABB << 11;
#ifdef VERY_LARGE_BOARDS
constexpr Bitboard FileMBB = FileABB << 12;
constexpr Bitboard FileNBB = FileABB << 13;
constexpr Bitboard FileOBB = FileABB << 14;
constexpr Bitboard FilePBB = FileABB << 15;
#endif
#endif

constexpr Bitboard Rank1BB = (Bitboard(1) << FILE_NB) - Bitboard(1);
constexpr Bitboard Rank2BB = Rank1BB << (FILE_NB * 1);
constexpr Bitboard Rank3BB = Rank1BB << (FILE_NB * 2);
constexpr Bitboard Rank4BB = Rank1BB << (FILE_NB * 3);
constexpr Bitboard Rank5BB = Rank1BB << (FILE_NB * 4);
constexpr Bitboard Rank6BB = Rank1BB << (FILE_NB * 5);
constexpr Bitboard Rank7BB = Rank1BB << (FILE_NB * 6);
constexpr Bitboard Rank8BB = Rank1BB << (FILE_NB * 7);
#ifdef LARGEBOARDS
constexpr Bitboard Rank9BB = Rank1BB << (FILE_NB * 8);
constexpr Bitboard Rank10BB = Rank1BB << (FILE_NB * 9);
#ifdef VERY_LARGE_BOARDS
constexpr Bitboard Rank11BB = Rank1BB << (FILE_NB * 10);
constexpr Bitboard Rank12BB = Rank1BB << (FILE_NB * 11);
constexpr Bitboard Rank13BB = Rank1BB << (FILE_NB * 12);
constexpr Bitboard Rank14BB = Rank1BB << (FILE_NB * 13);
constexpr Bitboard Rank15BB = Rank1BB << (FILE_NB * 14);
constexpr Bitboard Rank16BB = Rank1BB << (FILE_NB * 15);
#endif
#endif

constexpr Bitboard QueenSide   = FileABB | FileBBB | FileCBB | FileDBB;
constexpr Bitboard CenterFiles = FileCBB | FileDBB | FileEBB | FileFBB;
constexpr Bitboard KingSide    = FileEBB | FileFBB | FileGBB | FileHBB;
constexpr Bitboard Center      = (FileDBB | FileEBB) & (Rank4BB | Rank5BB);
constexpr Bitboard king_flank(File f);

extern uint8_t PopCnt16[1 << 16];
extern uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

extern Bitboard SquareBB[SQUARE_NB];
extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];
extern Bitboard WrappedRays[SQUARE_NB][8];
extern Bitboard PseudoAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard PseudoMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard LeaperAttacks[COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard LeaperMoves[2][COLOR_NB][PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard BoardSizeBB[FILE_NB][RANK_NB];
extern RiderType AttackRiderTypes[PIECE_TYPE_NB];
extern RiderType MoveRiderTypes[2][PIECE_TYPE_NB];
Bitboard leap_rider_attacks_bb(PieceType pt, Color c, Square s, Bitboard occupied);
Bitboard leap_rider_moves_bb(PieceType pt, bool initial, Color c, Square s, Bitboard occupied);
Bitboard tuple_rider_attacks_bb(PieceType pt, Color c, Square s, Bitboard occupied);
Bitboard tuple_rider_moves_bb(PieceType pt, bool initial, Color c, Square s, Bitboard occupied);
Bitboard tuple_rider_between_bb(PieceType pt, Square s1, Square s2);
inline Square lsb(Bitboard b);

constexpr std::array<std::pair<int, int>, 8> RoseSteps = {{
    { 2,  1}, { 1,  2}, {-1,  2}, {-2,  1},
    {-2, -1}, {-1, -2}, { 1, -2}, { 2, -1}
}};

#ifdef LARGEBOARDS
int popcount(Bitboard b); // required for 128 bit pext
#endif

constexpr Bitboard make_bitboard() { return 0; }

template<typename ...Squares>
constexpr Bitboard make_bitboard(Square s, Squares... squares) {
  return (Bitboard(1) << s) | make_bitboard(squares...);
}

inline Bitboard square_bb(Square s) {
  assert(is_ok(s));
  return SquareBB[s];
}

inline int wrap_coord(int val, int limit) {
  assert(limit > 0);
  return ((val % limit) + limit) % limit;
}

inline bool wrapped_destination_square(Square from, int df, int dr,
                                       File maxFile, Rank maxRank,
                                       bool wrapFile, bool wrapRank,
                                       Square& out) {
  const int numFiles = int(maxFile) + 1;
  const int numRanks = int(maxRank) + 1;
  int f = int(file_of(from)) + df;
  int r = int(rank_of(from)) + dr;

  if (!wrapFile && (f < 0 || f >= numFiles))
      return false;
  if (!wrapRank && (r < 0 || r >= numRanks))
      return false;

  if (wrapFile)
      f = wrap_coord(f, numFiles);
  if (wrapRank)
      r = wrap_coord(r, numRanks);

  out = make_square(File(f), Rank(r));
  return true;
}

inline Bitboard wrapping_slider_attacks(Square s, Bitboard occ, File maxFile, Rank maxRank,
                                        bool wrapFile, bool wrapRank,
                                        bool orthogonal, bool diagonal) {
  Bitboard result = 0;

  constexpr std::array<std::pair<int, int>, 8> dirs = {{
      { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
      { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
  }};

  for (size_t i = 0; i < dirs.size(); ++i)
  {
      const auto [df, dr] = dirs[i];
      const bool isDiag = df && dr;
      if ((isDiag && !diagonal) || (!isDiag && !orthogonal))
          continue;

      Square current = s;
      for (;;)
      {
          Square next = SQ_NONE;
          if (!wrapped_destination_square(current, df, dr, maxFile, maxRank, wrapFile, wrapRank, next))
              break;
          if (next == s)
              break;
          result |= next;
          if (occ & square_bb(next))
              break;
          current = next;
      }
  }

  return result;
}

inline Bitboard safe_destination_tuple_bb(Square s, int dr, int df) {
  int r = int(rank_of(s)) + dr;
  int f = int(file_of(s)) + df;
  if (r < 0 || r > int(RANK_MAX) || f < 0 || f > int(FILE_MAX))
      return Bitboard(0);
  return square_bb(make_square(File(f), Rank(r)));
}

inline Bitboard rose_attacks_bb(Square from, Bitboard occupied, const MagicGeometry* /*mg*/ = current_magic_geometry) {
  Bitboard attack = 0;

  for (int start = 0; start < 8; ++start)
      for (int turn : {-1, 1})
      {
          Square current = from;
          int index = start;
          for (int leg = 0; leg < 7; ++leg)
          {
              Bitboard dst = safe_destination_tuple_bb(current, RoseSteps[index].first, RoseSteps[index].second);
              if (!dst)
                  break;
              Square to = lsb(dst);
              attack |= dst;
              if (occupied & square_bb(to))
                  break;
              current = to;
              index = (index + turn + 8) % 8;
          }
      }

  return attack;
}

inline Bitboard rose_between_union_bb(Square from, Square to, Bitboard occupied) {
  Bitboard pathUnion = 0;

  for (int start = 0; start < 8; ++start)
      for (int turn : {-1, 1})
      {
          Square current = from;
          int index = start;
          Bitboard path = 0;
          for (int leg = 0; leg < 7; ++leg)
          {
              Bitboard dst = safe_destination_tuple_bb(current, RoseSteps[index].first, RoseSteps[index].second);
              if (!dst)
                  break;
              Square next = lsb(dst);
              path |= dst;
              if (next == to)
              {
                  pathUnion |= path;
                  break;
              }
              if (occupied & square_bb(next))
                  break;
              current = next;
              index = (index + turn + 8) % 8;
          }
      }

  return pathUnion;
}

inline Bitboard rose_between_intersection_bb(Square from, Square to, Bitboard occupied) {
  Bitboard pathIntersection = 0;
  bool found = false;

  for (int start = 0; start < 8; ++start)
      for (int turn : {-1, 1})
      {
          Square current = from;
          int index = start;
          Bitboard path = 0;
          for (int leg = 0; leg < 7; ++leg)
          {
              Bitboard dst = safe_destination_tuple_bb(current, RoseSteps[index].first, RoseSteps[index].second);
              if (!dst)
                  break;
              Square next = lsb(dst);
              path |= dst;
              if (next == to)
              {
                  pathIntersection = found ? (pathIntersection & path) : path;
                  found = true;
                  break;
              }
              if (occupied & square_bb(next))
                  break;
              current = next;
              index = (index + turn + 8) % 8;
          }
      }

  return found ? pathIntersection : square_bb(to);
}


/// Overloads of bitwise operators between a Bitboard and a Square for testing
/// whether a given bit is set in a bitboard, and for setting and clearing bits.

inline Bitboard  operator&( Bitboard  b, Square s) { return b &  square_bb(s); }
inline Bitboard  operator|( Bitboard  b, Square s) { return b |  square_bb(s); }
inline Bitboard  operator^( Bitboard  b, Square s) { return b ^  square_bb(s); }
inline Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
inline Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

inline Bitboard  operator-( Bitboard  b, Square s) { return b & ~square_bb(s); }
inline Bitboard& operator-=(Bitboard& b, Square s) { return b &= ~square_bb(s); }

inline Bitboard  operator&(Square s, Bitboard b) { return b & s; }
inline Bitboard  operator|(Square s, Bitboard b) { return b | s; }
inline Bitboard  operator^(Square s, Bitboard b) { return b ^ s; }

inline Bitboard  operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

constexpr bool more_than_one(Bitboard b) {
  return b & (b - 1);
}


inline Bitboard undo_move_board(Bitboard b, Move m) {
  return (from_sq(m) != SQ_NONE && (b & to_sq(m))) ? (b ^ to_sq(m)) | from_sq(m) : b;
}

/// board_size_bb() returns a bitboard representing all the squares
/// on a board with given size.

inline Bitboard board_size_bb(File f, Rank r) {
  return BoardSizeBB[f][r];
}

constexpr bool opposite_colors(Square s1, Square s2) {
  return (s1 + rank_of(s1) + s2 + rank_of(s2)) & 1;
}


/// rank_bb() and file_bb() return a bitboard representing all the squares on
/// the given file or rank.

constexpr Bitboard rank_bb(Rank r) {
  return Rank1BB << (FILE_NB * r);
}

constexpr Bitboard rank_bb(Square s) {
  return rank_bb(rank_of(s));
}

constexpr Bitboard file_bb(File f) {
  return FileABB << f;
}

constexpr Bitboard file_bb(Square s) {
  return file_bb(file_of(s));
}

constexpr Bitboard king_flank(File f) {
  const int fi = int(f);
  const int maxFi = FILE_NB - 1;
  const int midL = maxFi / 2;
  const int midR = (maxFi + 1) / 2;
  Bitboard queenSide = 0;
  Bitboard kingSide = 0;
  Bitboard centerFiles = 0;
  for (int i = 0; i <= midL; ++i)
      queenSide |= file_bb(File(i));
  for (int i = midR; i <= maxFi; ++i)
      kingSide |= file_bb(File(i));
  const int centerStart = midL > 0 ? midL - 1 : 0;
  const int centerEnd = midR + 1 < maxFi ? midR + 1 : maxFi;
  for (int i = centerStart; i <= centerEnd; ++i)
      centerFiles |= file_bb(File(i));

  if (fi == 0)
      return queenSide & ~file_bb(File(midL));
  if (fi < midL)
      return queenSide;
  if (fi <= midR)
      return centerFiles;
  if (fi < maxFi)
      return kingSide;
  return kingSide & ~file_bb(File(midR));
}


/// shift() moves a bitboard one or two steps as specified by the direction D

template<Direction D>
constexpr Bitboard shift(Bitboard b) {
  return  D == NORTH      ?  b                       << NORTH      : D == SOUTH      ?  b             >> NORTH
        : D == NORTH+NORTH?  b                       <<(2 * NORTH) : D == SOUTH+SOUTH?  b             >> (2 * NORTH)
        : D == EAST       ? (b & ~file_bb(FILE_MAX)) << EAST       : D == WEST       ? (b & ~FileABB) >> EAST
        : D == NORTH_EAST ? (b & ~file_bb(FILE_MAX)) << NORTH_EAST : D == NORTH_WEST ? (b & ~FileABB) << NORTH_WEST
        : D == SOUTH_EAST ? (b & ~file_bb(FILE_MAX)) >> NORTH_WEST : D == SOUTH_WEST ? (b & ~FileABB) >> NORTH_EAST
        : Bitboard(0);
}


/// shift() moves a bitboard one step along direction D (mainly for pawns)

constexpr Bitboard shift(Direction D, Bitboard b) {
  return  D == NORTH      ?  b                       << NORTH      : D == SOUTH      ?  b             >> NORTH
        : D == NORTH+NORTH?  b                       <<(2 * NORTH) : D == SOUTH+SOUTH?  b             >> (2 * NORTH)
        : D == EAST       ? (b & ~file_bb(FILE_MAX)) << EAST       : D == WEST       ? (b & ~FileABB) >> EAST
        : D == NORTH_EAST ? (b & ~file_bb(FILE_MAX)) << NORTH_EAST : D == NORTH_WEST ? (b & ~FileABB) << NORTH_WEST
        : D == SOUTH_EAST ? (b & ~file_bb(FILE_MAX)) >> NORTH_WEST : D == SOUTH_WEST ? (b & ~FileABB) >> NORTH_EAST
        : Bitboard(0);
}


/// pawn_attacks_bb() returns the squares attacked by pawns of the given color
/// from the squares in the given bitboard.

template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
  return C == WHITE ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b)
                    : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}

inline Bitboard pawn_attacks_bb(Color c, Square s) {

  assert(is_ok(s));
  return PseudoAttacks[c][PAWN][s];
}


/// pawn_double_attacks_bb() returns the squares doubly attacked by pawns of the
/// given color from the squares in the given bitboard.

template<Color C>
constexpr Bitboard pawn_double_attacks_bb(Bitboard b) {
  return C == WHITE ? shift<NORTH_WEST>(b) & shift<NORTH_EAST>(b)
                    : shift<SOUTH_WEST>(b) & shift<SOUTH_EAST>(b);
}


/// adjacent_files_bb() returns a bitboard representing all the squares on the
/// adjacent files of a given square.

constexpr Bitboard adjacent_files_bb(Square s) {
  return shift<EAST>(file_bb(s)) | shift<WEST>(file_bb(s));
}


/// line_bb() returns a bitboard representing an entire line (from board edge
/// to board edge) that intersects the two given squares. If the given squares
/// are not on a same file/rank/diagonal, the function returns 0. For instance,
/// line_bb(SQ_C4, SQ_F7) will return a bitboard with the A2-G8 diagonal.

inline Bitboard line_bb(Square s1, Square s2) {

  assert(is_ok(s1) && is_ok(s2));

  return LineBB[s1][s2];
}


/// between_bb(s1, s2) returns a bitboard representing the squares in the semi-open
/// segment between the squares s1 and s2 (excluding s1 but including s2). If the
/// given squares are not on a same file/rank/diagonal, it returns s2. For instance,
/// between_bb(SQ_C4, SQ_F7) will return a bitboard with squares D5, E6 and F7, but
/// between_bb(SQ_E6, SQ_F8) will return a bitboard with the square F8. This trick
/// allows to generate non-king evasion moves faster: the defending piece must either
/// interpose itself to cover the check or capture the checking piece.

inline Bitboard between_bb(Square s1, Square s2) {

  assert(is_ok(s1) && is_ok(s2));

  return BetweenBB[s1][s2];
}

inline Bitboard nightrider_between_bb(Square s1, Square s2) {
  int df = int(file_of(s2)) - int(file_of(s1));
  int dr = int(rank_of(s2)) - int(rank_of(s1));

  auto make_path = [&](int stepF, int stepR) {
      if ((stepF == 0) || (stepR == 0))
          return Bitboard(0);
      if (df % stepF || dr % stepR)
          return Bitboard(0);
      int nF = df / stepF;
      int nR = dr / stepR;
      if (nF != nR || nF <= 0)
          return Bitboard(0);
      int n = nF;
      Bitboard b = 0;
      int f = int(file_of(s1));
      int r = int(rank_of(s1));
      for (int i = 1; i <= n; ++i)
      {
          f += stepF;
          r += stepR;
          if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
              return Bitboard(0);
          b |= make_square(File(f), Rank(r));
      }
      return b;
  };

  // Nightrider rays are repeated knight vectors.
  static constexpr int StepFile[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
  static constexpr int StepRank[8] = { 2, 1, -1, -2, -2, -1, 1, 2 };
  for (int i = 0; i < 8; ++i)
  {
      Bitboard path = make_path(StepFile[i], StepRank[i]);
      if (path)
          return path;
  }

  return Bitboard(0);
}

inline Bitboard fixed_step_between_bb(Square s1, Square s2, int stepF, int stepR) {
  int df = int(file_of(s2)) - int(file_of(s1));
  int dr = int(rank_of(s2)) - int(rank_of(s1));

  auto make_path = [&](int sf, int sr) {
      if (sf == 0 && sr == 0)
          return Bitboard(0);
      if ((sf == 0 && df != 0) || (sr == 0 && dr != 0))
          return Bitboard(0);
      if ((sf != 0 && df % sf) || (sr != 0 && dr % sr))
          return Bitboard(0);

      int nF = sf ? df / sf : dr / sr;
      int nR = sr ? dr / sr : df / sf;
      if (nF != nR || nF <= 0)
          return Bitboard(0);

      Bitboard b = 0;
      int f = int(file_of(s1));
      int r = int(rank_of(s1));
      for (int i = 1; i <= nF; ++i)
      {
          f += sf;
          r += sr;
          if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
              return Bitboard(0);
          b |= make_square(File(f), Rank(r));
      }
      return b;
  };

  return make_path(stepF, stepR);
}

// Reconstruct the unique blocker/interposition path for bent riders by
// materializing the mandatory pivot square and then the allowed second leg.
inline Bitboard bent_slider_between_bb(Square s1, Square s2, int pivotF, int pivotR,
                                       bool allowHorizontal, bool allowVertical, bool allowDiagonal = false) {
  int f0 = int(file_of(s1));
  int r0 = int(rank_of(s1));
  int pf = f0 + pivotF;
  int pr = r0 + pivotR;
  if (pf < int(FILE_A) || pf > int(FILE_MAX) || pr < int(RANK_1) || pr > int(RANK_MAX))
      return Bitboard(0);

  int tf = int(file_of(s2));
  int tr = int(rank_of(s2));
  if (tf == pf && tr == pr)
      return Bitboard(0);

  Bitboard path = square_bb(make_square(File(pf), Rank(pr)));

  if (allowDiagonal && std::abs(tf - pf) == std::abs(tr - pr))
  {
      int stepF = tf > pf ? 1 : -1;
      int stepR = tr > pr ? 1 : -1;
      for (int f = pf + stepF, r = pr + stepR;; f += stepF, r += stepR)
      {
          if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
              return Bitboard(0);
          path |= square_bb(make_square(File(f), Rank(r)));
          if (f == tf && r == tr)
              return path;
      }
  }

  if (allowHorizontal && tr == pr)
  {
      int step = tf > pf ? 1 : -1;
      for (int f = pf + step;; f += step)
      {
          if (f < int(FILE_A) || f > int(FILE_MAX))
              return Bitboard(0);
          path |= square_bb(make_square(File(f), Rank(pr)));
          if (f == tf)
              return path;
      }
  }

  if (allowVertical && tf == pf)
  {
      int step = tr > pr ? 1 : -1;
      for (int r = pr + step;; r += step)
      {
          if (r < int(RANK_1) || r > int(RANK_MAX))
              return Bitboard(0);
          path |= square_bb(make_square(File(pf), Rank(r)));
          if (r == tr)
              return path;
      }
  }

  return Bitboard(0);
}

inline Bitboard between_bb(Square s1, Square s2, PieceType pt) {
  RiderType r = AttackRiderTypes[pt];
  Bitboard path = Bitboard(0);
  auto remap_reverse_path = [&](Bitboard reversePath) {
      return (reversePath & ~square_bb(s1)) | square_bb(s2);
  };
  if ((path = tuple_rider_between_bb(pt, s1, s2)))
      return path;

  if ((r & RIDER_HORSE) && (path = PseudoAttacks[WHITE][WAZIR][s2] & PseudoAttacks[WHITE][FERS][s1]))
      return path;

  if (r & RIDER_ELEPHANT)
  {
      for (auto [sf, sr] : { std::pair<int, int>{ 2, 2}, { 2,-2}, {-2, 2}, {-2,-2} })
      {
          if ((path = fixed_step_between_bb(s1, s2, sf, sr)))
              return path;
      }
      if ((path = PseudoAttacks[WHITE][FERS][s2] & PseudoAttacks[WHITE][FERS][s1]))
          return path;
  }

  if (r & RIDER_LAME_DABBABA)
  {
      for (auto [sf, sr] : { std::pair<int, int>{ 2, 0}, {-2, 0}, { 0, 2}, { 0,-2} })
      {
          if ((path = fixed_step_between_bb(s1, s2, sf, sr)))
              return path;
      }
      if ((path = PseudoAttacks[WHITE][WAZIR][s2] & PseudoAttacks[WHITE][WAZIR][s1]))
          return path;
  }

  if ((r & RIDER_JANGGI_ELEPHANT) && (path =  (PseudoAttacks[WHITE][WAZIR][s2] & PseudoAttacks[WHITE][ALFIL][s1])
                                             | (PseudoAttacks[WHITE][KNIGHT][s2] & PseudoAttacks[WHITE][FERS][s1])))
      return path;

  if (r & (RIDER_SKI_ROOK_H | RIDER_SKI_ROOK_V | RIDER_SKI_BISHOP))
  {
      path = between_bb(s1, s2);
      // Ski sliders ignore the first square in front of the attacker.
      path &= ~PseudoAttacks[WHITE][KING][s1];
      if (path)
          return path;
  }

  if (r & RIDER_ROSE)
  {
      if ((path = rose_between_union_bb(s1, s2, Bitboard(0))))
          return path;
  }

  if (r & RIDER_NIGHTRIDER)
  {
      if ((path = nightrider_between_bb(s1, s2)))
          return path;
  }

  if (r & RIDER_GRIFFON_NH)
  {
      if ((path = bent_slider_between_bb(s1, s2, 1, 1, true, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, 1, 1, true, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_GRIFFON_SH)
  {
      if ((path = bent_slider_between_bb(s1, s2, -1, 1, true, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, -1, 1, true, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_GRIFFON_EV)
  {
      if ((path = bent_slider_between_bb(s1, s2, 1, -1, true, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, 1, -1, true, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_GRIFFON_WV)
  {
      if ((path = bent_slider_between_bb(s1, s2, -1, -1, true, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, -1, -1, true, true)))
          return remap_reverse_path(path);
  }

  if (r & RIDER_MANTICORE_NE)
  {
      if ((path = bent_slider_between_bb(s1, s2, 0, 1, false, false, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, 0, 1, false, false, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_MANTICORE_NW)
  {
      if ((path = bent_slider_between_bb(s1, s2, -1, 0, false, false, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, -1, 0, false, false, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_MANTICORE_SE)
  {
      if ((path = bent_slider_between_bb(s1, s2, 1, 0, false, false, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, 1, 0, false, false, true)))
          return remap_reverse_path(path);
  }
  if (r & RIDER_MANTICORE_SW)
  {
      if ((path = bent_slider_between_bb(s1, s2, 0, -1, false, false, true)))
          return path;
      if ((path = bent_slider_between_bb(s2, s1, 0, -1, false, false, true)))
          return remap_reverse_path(path);
  }

  return between_bb(s1, s2);
}


/// forward_ranks_bb() returns a bitboard representing the squares on the ranks in
/// front of the given one, from the point of view of the given color. For instance,
/// forward_ranks_bb(BLACK, SQ_D3) will return the 16 squares on ranks 1 and 2.

constexpr Bitboard forward_ranks_bb(Color c, Square s) {
  return c == WHITE ? (AllSquares ^ Rank1BB) << FILE_NB * relative_rank(WHITE, s, RANK_MAX)
                    : (AllSquares ^ rank_bb(RANK_MAX)) >> FILE_NB * relative_rank(BLACK, s, RANK_MAX);
}

constexpr Bitboard forward_ranks_bb(Color c, Rank r) {
  return c == WHITE ? (AllSquares ^ Rank1BB) << FILE_NB * (r - RANK_1)
                    : (AllSquares ^ rank_bb(RANK_MAX)) >> FILE_NB * (RANK_MAX - r);
}


/// zone_bb() returns a bitboard representing the squares on all the ranks
/// in front of and on the given relative rank, from the point of view of the given color.
/// For instance, zone_bb(BLACK, RANK_7) will return the 16 squares on ranks 1 and 2.

inline Bitboard zone_bb(Color c, Rank r, Rank maxRank) {
  return forward_ranks_bb(c, relative_rank(c, r, maxRank)) | rank_bb(relative_rank(c, r, maxRank));
}


/// forward_file_bb() returns a bitboard representing all the squares along the
/// line in front of the given one, from the point of view of the given color.

constexpr Bitboard forward_file_bb(Color c, Square s) {
  return forward_ranks_bb(c, s) & file_bb(s);
}


/// pawn_attack_span() returns a bitboard representing all the squares that can
/// be attacked by a pawn of the given color when it moves along its file, starting
/// from the given square.

constexpr Bitboard pawn_attack_span(Color c, Square s) {
  return forward_ranks_bb(c, s) & adjacent_files_bb(s);
}


/// passed_pawn_span() returns a bitboard which can be used to test if a pawn of
/// the given color and on the given square is a passed pawn.

constexpr Bitboard passed_pawn_span(Color c, Square s) {
  return pawn_attack_span(c, s) | forward_file_bb(c, s);
}


/// aligned() returns true if the squares s1, s2 and s3 are aligned either on a
/// straight or on a diagonal line.

inline bool aligned(Square s1, Square s2, Square s3) {
  return line_bb(s1, s2) & s3;
}


/// distance() functions return the distance between x and y, defined as the
/// number of steps for a king in x to reach y.

template<typename T1 = Square> inline int distance(Square x, Square y);
template<> inline int distance<File>(Square x, Square y) { return std::abs(file_of(x) - file_of(y)); }
template<> inline int distance<Rank>(Square x, Square y) { return std::abs(rank_of(x) - rank_of(y)); }
template<> inline int distance<Square>(Square x, Square y) { return SquareDistance[x][y]; }

inline int edge_distance(File f, File maxFile = FILE_H) { return std::min(f, File(maxFile - f)); }
inline int edge_distance(Rank r, Rank maxRank = RANK_8) { return std::min(r, Rank(maxRank - r)); }


#ifdef VERY_LARGE_BOARDS
Bitboard rider_attacks_bb(RiderType R, Square s, Bitboard occupied);

template<RiderType R>
inline Bitboard rider_attacks_bb(Square s, Bitboard occupied) {
  static_assert(R != NO_RIDER && !(R & (R - 1))); // exactly one bit
  return rider_attacks_bb(R, s, occupied);
}

inline Square lsb(Bitboard b);
#else
inline Bitboard fixed_step_rider_attacks(Square s, Bitboard occupied, int stepF, int stepR, const MagicGeometry* mg = current_magic_geometry) {
  (void)mg;
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

inline Bitboard ski_slider_attacks(Square s, Bitboard occupied, int stepF, int stepR, const MagicGeometry* mg = current_magic_geometry) {
  (void)mg;
  int f = int(file_of(s)) + stepF;
  int r = int(rank_of(s)) + stepR;
  if (f < int(FILE_A) || f > int(FILE_MAX) || r < int(RANK_1) || r > int(RANK_MAX))
      return Bitboard(0);

  Bitboard attack = 0;
  f += stepF;
  r += stepR;
  while (f >= int(FILE_A) && f <= int(FILE_MAX) && r >= int(RANK_1) && r <= int(RANK_MAX))
  {
      Square to = make_square(File(f), Rank(r));
      attack |= to;
      if (occupied & to)
          break;
      f += stepF;
      r += stepR;
  }
  return attack;
}

template<RiderType R>
inline Bitboard rider_attacks_bb(Square s, Bitboard occupied, const MagicGeometry* mg = current_magic_geometry) {

  static_assert(R != NO_RIDER && !(R & (R - 1))); // exactly one bit
  if constexpr (R == RIDER_GRIFFON_NH || R == RIDER_GRIFFON_SH || R == RIDER_GRIFFON_EV || R == RIDER_GRIFFON_WV) {
      int r = int(rank_of(s));
      int f = int(file_of(s));
      if constexpr (R == RIDER_GRIFFON_NH) { ++r; ++f; }
      if constexpr (R == RIDER_GRIFFON_SH) { ++r; --f; }
      if constexpr (R == RIDER_GRIFFON_EV) { --r; ++f; }
      if constexpr (R == RIDER_GRIFFON_WV) { --r; --f; }
      if (r < 0 || r > int(RANK_MAX) || f < 0 || f > int(FILE_MAX))
          return Bitboard(0);
      Square src = make_square(File(f), Rank(r));
      if (occupied & src)
          return square_bb(src);
      if constexpr (R == RIDER_GRIFFON_NH)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, 1, 0)
               | fixed_step_rider_attacks(src, occupied, 0, 1);
      if constexpr (R == RIDER_GRIFFON_SH)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, -1, 0)
               | fixed_step_rider_attacks(src, occupied, 0, 1);
      if constexpr (R == RIDER_GRIFFON_EV)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, 1, 0)
               | fixed_step_rider_attacks(src, occupied, 0, -1);
      return square_bb(src)
           | fixed_step_rider_attacks(src, occupied, -1, 0)
           | fixed_step_rider_attacks(src, occupied, 0, -1);
  }
  if constexpr (R == RIDER_MANTICORE_NE || R == RIDER_MANTICORE_NW || R == RIDER_MANTICORE_SE || R == RIDER_MANTICORE_SW) {
      int r = int(rank_of(s));
      int f = int(file_of(s));
      if constexpr (R == RIDER_MANTICORE_NE) ++r;
      if constexpr (R == RIDER_MANTICORE_NW) --f;
      if constexpr (R == RIDER_MANTICORE_SE) ++f;
      if constexpr (R == RIDER_MANTICORE_SW) --r;
      if (r < 0 || r > int(RANK_MAX) || f < 0 || f > int(FILE_MAX))
          return Bitboard(0);
      Square src = make_square(File(f), Rank(r));
      if (occupied & src)
          return square_bb(src);
      if constexpr (R == RIDER_MANTICORE_NE)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, 1, 1)
               | fixed_step_rider_attacks(src, occupied, -1, 1);
      if constexpr (R == RIDER_MANTICORE_NW)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, -1, 1)
               | fixed_step_rider_attacks(src, occupied, -1, -1);
      if constexpr (R == RIDER_MANTICORE_SE)
          return square_bb(src)
               | fixed_step_rider_attacks(src, occupied, 1, 1)
               | fixed_step_rider_attacks(src, occupied, 1, -1);
      return square_bb(src)
           | fixed_step_rider_attacks(src, occupied, 1, -1)
           | fixed_step_rider_attacks(src, occupied, -1, -1);
  }
  if constexpr (R == RIDER_LAME_DABBABA)
      return  fixed_step_rider_attacks(s, occupied,  2,  0)
            | fixed_step_rider_attacks(s, occupied, -2,  0)
            | fixed_step_rider_attacks(s, occupied,  0,  2)
            | fixed_step_rider_attacks(s, occupied,  0, -2);
  if constexpr (R == RIDER_SKI_ROOK_H)
      return  ski_slider_attacks(s, occupied,  1, 0)
            | ski_slider_attacks(s, occupied, -1, 0);
  if constexpr (R == RIDER_SKI_ROOK_V)
      return  ski_slider_attacks(s, occupied, 0,  1)
            | ski_slider_attacks(s, occupied, 0, -1);
  if constexpr (R == RIDER_SKI_BISHOP)
      return  ski_slider_attacks(s, occupied,  1,  1)
            | ski_slider_attacks(s, occupied,  1, -1)
            | ski_slider_attacks(s, occupied, -1,  1)
            | ski_slider_attacks(s, occupied, -1, -1);
  if constexpr (R == RIDER_ROSE)
      return rose_attacks_bb(s, occupied);

  const Magic& m =  R == RIDER_ROOK_H ? mg->RookMagicsH[s]
                  : R == RIDER_ROOK_V ? mg->RookMagicsV[s]
                  : R == RIDER_CANNON_H ? mg->CannonMagicsH[s]
                  : R == RIDER_CANNON_V ? mg->CannonMagicsV[s]
                  : R == RIDER_LAME_DABBABA ? mg->BishopMagics[s]
                  : R == RIDER_HORSE ? mg->HorseMagics[s]
                  : R == RIDER_ELEPHANT ? mg->BishopMagics[s]
                  : R == RIDER_JANGGI_ELEPHANT ? mg->JanggiElephantMagics[s]
                  : R == RIDER_CANNON_DIAG ? mg->CannonDiagMagics[s]
                  : R == RIDER_NIGHTRIDER ? mg->NightriderMagics[s]
                  : R == RIDER_GRASSHOPPER_H ? mg->GrasshopperMagicsH[s]
                  : R == RIDER_GRASSHOPPER_V ? mg->GrasshopperMagicsV[s]
                  : R == RIDER_GRASSHOPPER_D ? mg->GrasshopperMagicsD[s]
                  : mg->BishopMagics[s];
  return m.attacks[m.index(occupied)];
}

inline Square lsb(Bitboard b);

inline Bitboard rider_attacks_bb(RiderType R, Square s, Bitboard occupied, const MagicGeometry* mg = current_magic_geometry) {

  assert(R != NO_RIDER && !(R & (R - 1))); // exactly one bit
  if (R == RIDER_LAME_DABBABA)
      return  fixed_step_rider_attacks(s, occupied,  2,  0)
            | fixed_step_rider_attacks(s, occupied, -2,  0)
            | fixed_step_rider_attacks(s, occupied,  0,  2)
            | fixed_step_rider_attacks(s, occupied,  0, -2);
  if (R == RIDER_SKI_ROOK_H)
      return  ski_slider_attacks(s, occupied,  1, 0)
            | ski_slider_attacks(s, occupied, -1, 0);
  if (R == RIDER_SKI_ROOK_V)
      return  ski_slider_attacks(s, occupied, 0,  1)
            | ski_slider_attacks(s, occupied, 0, -1);
  if (R == RIDER_SKI_BISHOP)
      return  ski_slider_attacks(s, occupied,  1,  1)
            | ski_slider_attacks(s, occupied,  1, -1)
            | ski_slider_attacks(s, occupied, -1,  1)
            | ski_slider_attacks(s, occupied, -1, -1);
  if (R == RIDER_ROSE)
      return rose_attacks_bb(s, occupied);
  if (R == RIDER_GRIFFON_NH) return rider_attacks_bb<RIDER_GRIFFON_NH>(s, occupied, mg);
  if (R == RIDER_GRIFFON_SH) return rider_attacks_bb<RIDER_GRIFFON_SH>(s, occupied, mg);
  if (R == RIDER_GRIFFON_EV) return rider_attacks_bb<RIDER_GRIFFON_EV>(s, occupied, mg);
  if (R == RIDER_GRIFFON_WV) return rider_attacks_bb<RIDER_GRIFFON_WV>(s, occupied, mg);
  if (R == RIDER_MANTICORE_NE) return rider_attacks_bb<RIDER_MANTICORE_NE>(s, occupied, mg);
  if (R == RIDER_MANTICORE_NW) return rider_attacks_bb<RIDER_MANTICORE_NW>(s, occupied, mg);
  if (R == RIDER_MANTICORE_SE) return rider_attacks_bb<RIDER_MANTICORE_SE>(s, occupied, mg);
  if (R == RIDER_MANTICORE_SW) return rider_attacks_bb<RIDER_MANTICORE_SW>(s, occupied, mg);
  const Magic& m = mg->magics[lsb(R)][s]; // re-use Bitboard lsb for riders
  return m.attacks[m.index(occupied)];
}
#endif


/// attacks_bb(Square) returns the pseudo attacks of the give piece type
/// assuming an empty board.

template<PieceType Pt>
inline Bitboard attacks_bb(Square s) {

  assert((Pt != PAWN) && (is_ok(s)));

  return PseudoAttacks[WHITE][Pt][s];
}


/// attacks_bb(Square, Bitboard) returns the attacks by the given piece
/// assuming the board is occupied according to the passed Bitboard.
/// Sliding piece attacks do not continue past an occupied square.

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied, const MagicGeometry* mg = current_magic_geometry) {

  assert((Pt != PAWN) && (is_ok(s)));

  switch (Pt)
  {
  case BISHOP: return rider_attacks_bb<RIDER_BISHOP>(s, occupied, mg);
  case ROOK  : return rider_attacks_bb<RIDER_ROOK_H>(s, occupied, mg) | rider_attacks_bb<RIDER_ROOK_V>(s, occupied, mg);
  case QUEEN : return attacks_bb<BISHOP>(s, occupied, mg) | attacks_bb<ROOK>(s, occupied, mg);
  default    : return PseudoAttacks[WHITE][Pt][s];
  }
}

/// pop_rider() finds and clears a rider in a (hybrid) rider type

inline RiderType pop_rider(RiderType& r) {
  assert(r);
  const RiderType r2 = r & ~(r - 1);
  r &= r - 1;
  return r2;
}

inline Bitboard attacks_bb(Color c, PieceType pt, Square s, Bitboard occupied, const MagicGeometry* mg = current_magic_geometry) {
  assert(pt != NO_PIECE_TYPE);
  Bitboard b = LeaperAttacks[c][pt][s];
  RiderType r = AttackRiderTypes[pt];
  while (r)
      b |= rider_attacks_bb(pop_rider(r), s, occupied, mg);
  b |= leap_rider_attacks_bb(pt, c, s, occupied);
  b |= tuple_rider_attacks_bb(pt, c, s, occupied);
  return b & PseudoAttacks[c][pt][s];
}


template <bool Initial=false>
inline Bitboard moves_bb(Color c, PieceType pt, Square s, Bitboard occupied, const MagicGeometry* mg = current_magic_geometry) {
  assert(pt != NO_PIECE_TYPE);
  Bitboard b = LeaperMoves[Initial][c][pt][s];
  RiderType r = MoveRiderTypes[Initial][pt];
  while (r)
      b |= rider_attacks_bb(pop_rider(r), s, occupied, mg);
  b |= leap_rider_moves_bb(pt, Initial, c, s, occupied);
  b |= tuple_rider_moves_bb(pt, Initial, c, s, occupied);
  return b & PseudoMoves[Initial][c][pt][s];
}


/// popcount() counts the number of non-zero bits in a bitboard

inline int popcount(Bitboard b) {

#ifndef USE_POPCNT

#ifdef VERY_LARGE_BOARDS
  return  PopCnt16[(b.b64[0] >>  0) & 0xFFFF] + PopCnt16[(b.b64[0] >> 16) & 0xFFFF]
        + PopCnt16[(b.b64[0] >> 32) & 0xFFFF] + PopCnt16[(b.b64[0] >> 48) & 0xFFFF]
        + PopCnt16[(b.b64[1] >>  0) & 0xFFFF] + PopCnt16[(b.b64[1] >> 16) & 0xFFFF]
        + PopCnt16[(b.b64[1] >> 32) & 0xFFFF] + PopCnt16[(b.b64[1] >> 48) & 0xFFFF]
        + PopCnt16[(b.b64[2] >>  0) & 0xFFFF] + PopCnt16[(b.b64[2] >> 16) & 0xFFFF]
        + PopCnt16[(b.b64[2] >> 32) & 0xFFFF] + PopCnt16[(b.b64[2] >> 48) & 0xFFFF]
        + PopCnt16[(b.b64[3] >>  0) & 0xFFFF] + PopCnt16[(b.b64[3] >> 16) & 0xFFFF]
        + PopCnt16[(b.b64[3] >> 32) & 0xFFFF] + PopCnt16[(b.b64[3] >> 48) & 0xFFFF];
#elif defined(LARGEBOARDS)
  union { Bitboard bb; uint16_t u[8]; } v = { b };
  return  PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]]
        + PopCnt16[v.u[4]] + PopCnt16[v.u[5]] + PopCnt16[v.u[6]] + PopCnt16[v.u[7]];
#else
  union { Bitboard bb; uint16_t u[4]; } v = { b };
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];
#endif

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

#ifdef VERY_LARGE_BOARDS
  return (int)_mm_popcnt_u64(b.b64[0]) + (int)_mm_popcnt_u64(b.b64[1])
       + (int)_mm_popcnt_u64(b.b64[2]) + (int)_mm_popcnt_u64(b.b64[3]);
#elif defined(LARGEBOARDS)
  return (int)_mm_popcnt_u64(uint64_t(b >> 64)) + (int)_mm_popcnt_u64(uint64_t(b));
#else
  return (int)_mm_popcnt_u64(b);
#endif

#else // Assumed gcc or compatible compiler

#ifdef VERY_LARGE_BOARDS
  return __builtin_popcountll(b.b64[0]) + __builtin_popcountll(b.b64[1])
       + __builtin_popcountll(b.b64[2]) + __builtin_popcountll(b.b64[3]);
#elif defined(LARGEBOARDS)
  return __builtin_popcountll(b >> 64) + __builtin_popcountll(b);
#else
  return __builtin_popcountll(b);
#endif

#endif
}


/// lsb() and msb() return the least/most significant bit in a non-zero bitboard

#if defined(__GNUC__)  // GCC, Clang, ICC

inline Square lsb(Bitboard b) {
  assert(b);
#ifdef VERY_LARGE_BOARDS
  if (b.b64[3]) return Square(__builtin_ctzll(b.b64[3]));
  if (b.b64[2]) return Square(__builtin_ctzll(b.b64[2]) + 64);
  if (b.b64[1]) return Square(__builtin_ctzll(b.b64[1]) + 128);
  return Square(__builtin_ctzll(b.b64[0]) + 192);
#elif defined(LARGEBOARDS)
  if (!(b << 64))
      return Square(__builtin_ctzll(b >> 64) + 64);
#endif
  return Square(__builtin_ctzll(b));
}

inline Square msb(Bitboard b) {
  assert(b);
#ifdef VERY_LARGE_BOARDS
  if (b.b64[0]) return Square(192 + (63 - __builtin_clzll(b.b64[0])));
  if (b.b64[1]) return Square(128 + (63 - __builtin_clzll(b.b64[1])));
  if (b.b64[2]) return Square(64 + (63 - __builtin_clzll(b.b64[2])));
  return Square(63 - __builtin_clzll(b.b64[3]));
#elif defined(LARGEBOARDS)
  if (b >> 64)
      return Square(int(SQUARE_BIT_MASK) ^ __builtin_clzll(b >> 64));
  return Square(int(SQUARE_BIT_MASK) ^ (__builtin_clzll(b) + 64));
#else
  return Square(int(SQUARE_BIT_MASK) ^ __builtin_clzll(b));
#endif
}

inline Square lsb_wrapped(Bitboard blockers, Square sq) {
    Bitboard hi = (sq < int(SQUARE_BIT_MASK)) ? (blockers & (AllSquares << (sq + 1))) : Bitboard(0);
    return hi ? lsb(hi) : lsb(blockers);
}

inline Square msb_wrapped(Bitboard blockers, Square sq) {
    Bitboard lo = blockers & ~(AllSquares << sq);
    return lo ? msb(lo) : msb(blockers);
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

inline Square lsb(Bitboard b) {
  assert(b);
  unsigned long idx;
#ifdef VERY_LARGE_BOARDS
  if (b.b64[3])
  {
      _BitScanForward64(&idx, b.b64[3]);
      return Square(idx);
  }
  else if (b.b64[2])
  {
      _BitScanForward64(&idx, b.b64[2]);
      return Square(idx + 64);
  }
  else if (b.b64[1])
  {
      _BitScanForward64(&idx, b.b64[1]);
      return Square(idx + 128);
  }
  else
  {
      _BitScanForward64(&idx, b.b64[0]);
      return Square(idx + 192);
  }
#elif defined(LARGEBOARDS)
  if (uint64_t(b))
  {
      _BitScanForward64(&idx, uint64_t(b));
      return Square(idx);
  }
  else
  {
      _BitScanForward64(&idx, uint64_t(b >> 64));
      return Square(idx + 64);
  }
#else
  _BitScanForward64(&idx, b);
  return (Square) idx;
#endif
}

inline Square msb(Bitboard b) {
  assert(b);
  unsigned long idx;
#ifdef VERY_LARGE_BOARDS
  if (b.b64[0])
  {
      _BitScanReverse64(&idx, b.b64[0]);
      return Square(idx + 192);
  }
  else if (b.b64[1])
  {
      _BitScanReverse64(&idx, b.b64[1]);
      return Square(idx + 128);
  }
  else if (b.b64[2])
  {
      _BitScanReverse64(&idx, b.b64[2]);
      return Square(idx + 64);
  }
  else
  {
      _BitScanReverse64(&idx, b.b64[3]);
      return Square(idx);
  }
#elif defined(LARGEBOARDS)
  if (b >> 64)
  {
      _BitScanReverse64(&idx, uint64_t(b >> 64));
      return Square(idx + 64);
  }
  else
  {
      _BitScanReverse64(&idx, uint64_t(b));
      return Square(idx);
  }
#else
  _BitScanReverse64(&idx, b);
  return (Square) idx;
#endif
}

#else  // MSVC, WIN32

inline Square lsb(Bitboard b) {
  assert(b);
  unsigned long idx;

#ifdef VERY_LARGE_BOARDS
  if (b.b64[3]) {
      if (uint32_t(b.b64[3])) {
          _BitScanForward(&idx, uint32_t(b.b64[3]));
          return Square(idx);
      }
      _BitScanForward(&idx, uint32_t(b.b64[3] >> 32));
      return Square(idx + 32);
  } else if (b.b64[2]) {
      if (uint32_t(b.b64[2])) {
          _BitScanForward(&idx, uint32_t(b.b64[2]));
          return Square(idx + 64);
      }
      _BitScanForward(&idx, uint32_t(b.b64[2] >> 32));
      return Square(idx + 96);
  } else if (b.b64[1]) {
      if (uint32_t(b.b64[1])) {
          _BitScanForward(&idx, uint32_t(b.b64[1]));
          return Square(idx + 128);
      }
      _BitScanForward(&idx, uint32_t(b.b64[1] >> 32));
      return Square(idx + 160);
  } else if (uint32_t(b.b64[0])) {
      _BitScanForward(&idx, uint32_t(b.b64[0]));
      return Square(idx + 192);
  } else {
      _BitScanForward(&idx, uint32_t(b.b64[0] >> 32));
      return Square(idx + 224);
  }
#elif defined(LARGEBOARDS)
  if (b << 96) {
      _BitScanForward(&idx, uint32_t(b));
      return Square(idx);
  } else if (b << 64) {
      _BitScanForward(&idx, uint32_t(b >> 32));
      return Square(idx + 32);
  } else if (b << 32) {
      _BitScanForward(&idx, uint32_t(b >> 64));
      return Square(idx + 64);
  } else {
      _BitScanForward(&idx, uint32_t(b >> 96));
      return Square(idx + 96);
  }
#else
  if (b & 0xffffffff) {
      _BitScanForward(&idx, uint32_t(b));
      return Square(idx);
  } else {
      _BitScanForward(&idx, uint32_t(b >> 32));
      return Square(idx + 32);
  }
#endif
}

inline Square msb(Bitboard b) {
  assert(b);
  unsigned long idx;

#ifdef VERY_LARGE_BOARDS
  if (b.b64[0] >> 32) {
      _BitScanReverse(&idx, uint32_t(b.b64[0] >> 32));
      return Square(idx + 224);
  } else if (uint32_t(b.b64[0])) {
      _BitScanReverse(&idx, uint32_t(b.b64[0]));
      return Square(idx + 192);
  } else if (b.b64[1] >> 32) {
      _BitScanReverse(&idx, uint32_t(b.b64[1] >> 32));
      return Square(idx + 160);
  } else if (uint32_t(b.b64[1])) {
      _BitScanReverse(&idx, uint32_t(b.b64[1]));
      return Square(idx + 128);
  } else if (b.b64[2] >> 32) {
      _BitScanReverse(&idx, uint32_t(b.b64[2] >> 32));
      return Square(idx + 96);
  } else if (uint32_t(b.b64[2])) {
      _BitScanReverse(&idx, uint32_t(b.b64[2]));
      return Square(idx + 64);
  } else if (b.b64[3] >> 32) {
      _BitScanReverse(&idx, uint32_t(b.b64[3] >> 32));
      return Square(idx + 32);
  } else {
      _BitScanReverse(&idx, uint32_t(b.b64[3]));
      return Square(idx);
  }
#elif defined(LARGEBOARDS)
  if (b >> 96) {
      _BitScanReverse(&idx, uint32_t(b >> 96));
      return Square(idx + 96);
  } else if (b >> 64) {
      _BitScanReverse(&idx, uint32_t(b >> 64));
      return Square(idx + 64);
  } else
#endif
  if (b >> 32) {
      _BitScanReverse(&idx, uint32_t(b >> 32));
      return Square(idx + 32);
  } else {
      _BitScanReverse(&idx, uint32_t(b));
      return Square(idx);
  }
}

#endif

#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

/// least_significant_square_bb() returns the bitboard of the least significant
/// square of a non-zero bitboard. It is equivalent to square_bb(lsb(bb)).

inline Bitboard least_significant_square_bb(Bitboard b) {
  assert(b);
  return b & -b;
}

/// pop_lsb() finds and clears the least significant bit in a non-zero bitboard

inline Square pop_lsb(Bitboard& b) {
  assert(b);
  const Square s = lsb(b);
  b &= b - 1;
  return s;
}


/// frontmost_sq() returns the most advanced square for the given color,
/// requires a non-zero bitboard.
inline Square frontmost_sq(Color c, Bitboard b) {
  assert(b);
  return c == WHITE ? msb(b) : lsb(b);
}


/// popcount() counts the number of non-zero bits in a piece set

inline int popcount(PieceSet ps) {

#ifndef USE_POPCNT

  union { uint64_t bb; uint16_t u[4]; } v = { (uint64_t)ps };
  return PopCnt16[v.u[0]] + PopCnt16[v.u[1]] + PopCnt16[v.u[2]] + PopCnt16[v.u[3]];

#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)

  return (int)_mm_popcnt_u64(ps);

#else // Assumed gcc or compatible compiler

  return __builtin_popcountll(ps);

#endif
}

/// lsb() and msb() return the least/most significant bit in a non-zero piece set

#if defined(__GNUC__)  // GCC, Clang, ICC

inline PieceType lsb(PieceSet ps) {
  assert(ps);
  return PieceType(__builtin_ctzll(ps));
}

inline PieceType msb(PieceSet ps) {
  assert(ps);
  return PieceType((PIECE_TYPE_NB - 1) ^ __builtin_clzll(ps));
}

#elif defined(_MSC_VER)  // MSVC

#ifdef _WIN64  // MSVC, WIN64

inline PieceType lsb(PieceSet ps) {
  assert(ps);
  unsigned long idx;
  _BitScanForward64(&idx, ps);
  return (PieceType) idx;
}

inline PieceType msb(PieceSet ps) {
  assert(ps);
  unsigned long idx;
  _BitScanReverse64(&idx, ps);
  return (PieceType) idx;
}

#else  // MSVC, WIN32

inline PieceType lsb(PieceSet ps) {
  assert(ps);
  unsigned long idx;

  if (ps & 0xffffffff) {
      _BitScanForward(&idx, uint32_t(ps));
      return PieceType(idx);
  } else {
      _BitScanForward(&idx, uint32_t(ps >> 32));
      return PieceType(idx + 32);
  }
}

inline PieceType msb(PieceSet ps) {
  assert(ps);
  unsigned long idx;
  if (ps >> 32) {
      _BitScanReverse(&idx, uint32_t(ps >> 32));
      return PieceType(idx + 32);
  } else {
      _BitScanReverse(&idx, uint32_t(ps));
      return PieceType(idx);
  }
}

#endif

#else  // Compiler is neither GCC nor MSVC compatible

#error "Compiler not supported."

#endif

/// pop_lsb() and pop_msb() find and clear the least/most significant bit in a non-zero piece set

inline PieceType pop_lsb(PieceSet& ps) {
  assert(ps);
  const PieceType pt = lsb(ps);
  ps &= PieceSet(ps - 1);
  return pt;
}

inline PieceType pop_msb(PieceSet& ps) {
  assert(ps);
  const PieceType pt = msb(ps);
  ps &= ~piece_set(pt);
  return pt;
}

} // namespace Stockfish

#endif // #ifndef BITBOARD_H_INCLUDED
