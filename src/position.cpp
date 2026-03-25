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
#include <cassert>
#include <cctype>
#include <cstddef> // For offsetof()
#include <cstring> // For std::memset, std::memcmp
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>
#include <cstdlib>

#include "bitboard.h"
#include "misc.h"
#include "movegen.h"
#include "position.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"

using std::string;

namespace Stockfish {

namespace {

  inline Variant::PotionType potion_type_from_piece(const Variant* var, PieceType pt) {
    if (!var || !var->potions)
        return static_cast<Variant::PotionType>(Variant::POTION_TYPE_NB);
    if (pt == var->potionPiece[Variant::POTION_FREEZE])
        return Variant::POTION_FREEZE;
    if (pt == var->potionPiece[Variant::POTION_JUMP])
        return Variant::POTION_JUMP;
    return static_cast<Variant::PotionType>(Variant::POTION_TYPE_NB);
  }

  struct SpellContextScope {
    Position& pos;
    bool active;
    bool hadContext;
    Bitboard prevFreeze;
    Bitboard prevJump;

    SpellContextScope(const Position& position, Bitboard freezeExtra, Bitboard jumpRemoved)
        : pos(const_cast<Position&>(position)),
          active((freezeExtra | jumpRemoved) != Bitboard(0)),
          hadContext(pos.spell_context_active()),
          prevFreeze(pos.spell_freeze_extra()),
          prevJump(pos.spell_jump_removed()) {
        if (active)
            pos.set_spell_context(freezeExtra, jumpRemoved);
    }

    ~SpellContextScope() {
        if (!active)
            return;
        if (hadContext)
            pos.set_spell_context(prevFreeze, prevJump);
        else
            pos.clear_spell_context();
    }
  };

  inline Bitboard retro_asymmetric_check_squares(Color attacker, PieceType pt, Square kingSq, Bitboard occupied) {
    // Hopper families need hurdle-aware retro logic. Keep pseudo candidates for
    // those, and use path-based retro filtering for other asymmetrical riders.
    if (AttackRiderTypes[pt] & HOPPING_RIDERS)
        return PseudoAttacks[~attacker][pt][kingSq];

    Bitboard checks = 0;
    Bitboard candidates = PseudoAttacks[~attacker][pt][kingSq];

    while (candidates)
    {
        Square from = pop_lsb(candidates);
        Bitboard blockers = between_bb(kingSq, from, pt) & ~square_bb(from);
        if (!(blockers & occupied))
            checks |= from;
    }

    return checks;
  }

  bool is_pure_hopper_like(const PieceInfo* pi) {
    bool hasHopper = false;

    for (MoveModality modality : {MODALITY_QUIET, MODALITY_CAPTURE})
    {
        if (!pi->steps[0][modality].empty()
            || !pi->tupleSteps[0][modality].empty()
            || !pi->slider[0][modality].empty()
            || pi->griffon[0][modality]
            || pi->manticore[0][modality]
            || pi->rose[0][modality])
            return false;

        hasHopper = hasHopper
                 || !pi->hopper[0][modality].empty()
                 || !pi->contraHopper[0][modality].empty();
    }

    return hasHopper;
  }

  bool has_hopper_potential_from_square(const PieceInfo* pi, Color c, Square sq) {
    auto has_two_step_lane = [&](const std::map<Direction, int>& directions) {
        for (auto const& [d, _] : directions)
        {
            Direction step = c == WHITE ? d : -d;
            Square hurdle = sq + step;
            if (!is_ok(hurdle) || distance(hurdle, sq) > 2)
                continue;

            Square landing = hurdle + step;
            if (is_ok(landing) && distance(landing, hurdle) <= 2)
                return true;
        }
        return false;
    };

    for (MoveModality modality : {MODALITY_QUIET, MODALITY_CAPTURE})
        if (has_two_step_lane(pi->hopper[0][modality]) || has_two_step_lane(pi->contraHopper[0][modality]))
            return true;

    return false;
  }

  Bitboard rose_revealed_blockers(Square target, Square attackerSq, Bitboard occupied) {
    Bitboard blockers = 0;
    Bitboard candidates = rose_between_union_bb(target, attackerSq, Bitboard(0)) & occupied & ~square_bb(attackerSq);

    while (candidates)
    {
        Square blocker = pop_lsb(candidates);
        if (rose_attacks_bb(attackerSq, occupied ^ square_bb(blocker)) & target)
            blockers |= blocker;
    }

    return blockers;
  }

} // namespace

namespace Zobrist {

  constexpr int MAX_ZOBRIST_POINTS = 512;
  Key psq[PIECE_NB][SQUARE_NB];
  Key enpassant[SQUARE_NB];
  Key castling[CASTLING_RIGHT_NB];
  Key side, noPawns;
  Key inHand[PIECE_NB][SQUARE_NB];
  Key checks[COLOR_NB][CHECKS_NB];
  Key committed[COLOR_NB][FILE_NB][PIECE_TYPE_NB];
  Key potionZone[COLOR_NB][Variant::POTION_TYPE_NB][SQUARE_NB];
  Key potionCooldown[COLOR_NB][Variant::POTION_TYPE_NB][POTION_COOLDOWN_BITS];
  Key wall[SQUARE_NB];
  Key dead[SQUARE_NB];
  Key endgame[EG_EVAL_NB];
  Key points[COLOR_NB][MAX_ZOBRIST_POINTS];
}

Square JumpMidpoint[SQUARE_NB][SQUARE_NB];


namespace {

  inline void xor_potion_zone(Key& key, Color c, Variant::PotionType potion, Bitboard zone) {
      while (zone)
          key ^= Zobrist::potionZone[c][potion][pop_lsb(zone)];
  }

  inline void xor_potion_cooldown(Key& key, Color c, Variant::PotionType potion, int cooldown) {
      assert(cooldown >= 0);
      unsigned value = static_cast<unsigned>(cooldown);
      assert(value < (1u << POTION_COOLDOWN_BITS));
      if (!value)
          return;
      for (int bit = 0; bit < POTION_COOLDOWN_BITS; ++bit)
          if (value & (1u << bit))
              key ^= Zobrist::potionCooldown[c][potion][bit];
  }

  inline void xor_committed_gate(Key& key, Color c, File f, PieceType pt) {
      assert(pt >= NO_PIECE_TYPE && pt < PIECE_TYPE_NB);
      if (pt != NO_PIECE_TYPE)
          key ^= Zobrist::committed[c][f][pt];
  }

  inline Square parse_fen_square(const Position& pos, const std::string& spec) {
      if (spec.size() < 2 || spec[0] < 'a' || spec[0] > 'a' + pos.max_file())
          return SQ_NONE;

      int rankNumber = 0;
      for (size_t i = 1; i < spec.size(); ++i)
      {
          if (!std::isdigit(static_cast<unsigned char>(spec[i])))
              return SQ_NONE;
          int digit = spec[i] - '0';
          if (rankNumber > (std::numeric_limits<int>::max() - digit) / 10)
              return SQ_NONE;
          rankNumber = rankNumber * 10 + digit;
      }

      if (rankNumber < 1 || rankNumber > pos.max_rank() + 1)
          return SQ_NONE;

      return make_square(File(spec[0] - 'a'), Rank(rankNumber - 1));
  }

  inline std::array<int, 4> parse_potion_cooldowns(std::string content) {
      // Order: white-freeze, white-jump, black-freeze, black-jump.
      std::array<int, 4> parsed = {0, 0, 0, 0};
      for (char& ch : content)
          if (!(std::isdigit(static_cast<unsigned char>(ch)) || ch == '-'))
              ch = ' ';

      std::istringstream ss(content);
      std::vector<int> vals;
      int v = 0;
      while (ss >> v)
          vals.push_back(std::max(0, v));

      if (vals.size() >= 4)
      {
          parsed[0] = vals[0];
          parsed[1] = vals[1];
          parsed[2] = vals[2];
          parsed[3] = vals[3];
      }
      else if (vals.size() == 2)
      {
          // Compact form: "<w b>" means both potion cooldowns for each side.
          parsed[0] = parsed[1] = vals[0];
          parsed[2] = parsed[3] = vals[1];
      }
      return parsed;
  }

} // namespace


/// operator<<(Position) returns an ASCII representation of the position

std::ostream& operator<<(std::ostream& os, const Position& pos) {

  os << "\n ";
  for (File f = FILE_A; f <= pos.max_file(); ++f)
      os << "+---";
  os << "+\n";

  for (Rank r = pos.max_rank(); r >= RANK_1; --r)
  {
      for (File f = FILE_A; f <= pos.max_file(); ++f)
      {
          Square sq = make_square(f, r);
          if (pos.state()->deadSquares & sq)
              os << " | ^";
          else if (pos.state()->wallSquares & sq)
              os << " | *";
          else if (pos.variant()->shogiStylePromotions && pos.unpromoted_piece_on(sq))
              os << " |+" << pos.piece_to_char()[pos.unpromoted_piece_on(sq)];
          else if (((pos.captures_to_hand() && !pos.drop_loop()) || pos.two_boards()) && pos.is_promoted(sq))
              os << " |~" << pos.piece_to_char()[pos.piece_on(sq)];
          else
              os << " | " << pos.piece_to_char()[pos.piece_on(sq)];
      }

#ifdef LARGEBOARDS
      os << " |" << (pos.max_rank() == RANK_10 && CurrentProtocol != UCI_GENERAL ? r : 1 + r);
#else
      os << " |" << (1 + r);
#endif
      if (r == pos.max_rank() || r == RANK_1)
      {
          Color c = r == RANK_1 ? WHITE : BLACK;
          if (c == pos.side_to_move())
              os << " *";
          else
              os << "  ";
          if (!pos.free_drops() && (pos.piece_drops() || pos.seirawan_gating()))
          {
              os << " [";
              for (PieceType pt = KING; pt >= PAWN; --pt)
                  os << std::string(pos.count_in_hand(c, pt), pos.piece_to_char()[make_piece(c, pt)]);
              os << "]";
          }
      }
      os << "\n ";
      for (File f = FILE_A; f <= pos.max_file(); ++f)
          os << "+---";
      os << "+\n";
  }

  for (File f = FILE_A; f <= pos.max_file(); ++f)
      os << "   " << char('a' + f);
  os << "\n";
  os << "\nFen: " << pos.fen() << "\nSfen: " << pos.fen(true) << "\nKey: " << std::hex << std::uppercase
     << std::setfill('0') << std::setw(16) << pos.key()
     << std::setfill(' ') << std::dec << "\nCheckers: ";

  for (Bitboard b = pos.checkers(); b; )
      os << UCI::square(pos, pop_lsb(b)) << " ";

  os << "\nChased: ";
  for (Bitboard b = pos.state()->chased; b; )
      os << UCI::square(pos, pop_lsb(b)) << " ";

  if (    int(Tablebases::MaxCardinality) >= popcount(pos.pieces())
      && Options["UCI_Variant"] == "chess"
      && !pos.can_castle(ANY_CASTLING))
  {
      StateInfo st;
      ASSERT_ALIGNED(&st, Eval::NNUE::CacheLineSize);

      Position p;
      p.set(pos.variant(), pos.fen(), pos.is_chess960(), &st, pos.this_thread());
      Tablebases::ProbeState s1, s2;
      Tablebases::WDLScore wdl = Tablebases::probe_wdl(p, &s1);
      int dtz = Tablebases::probe_dtz(p, &s2);
      os << "\nTablebases WDL: " << std::setw(4) << wdl << " (" << s1 << ")"
         << "\nTablebases DTZ: " << std::setw(4) << dtz << " (" << s2 << ")";
  }

  return os;
}


// Marcel van Kervinck's cuckoo algorithm for fast detection of "upcoming repetition"
// situations. Description of the algorithm in the following paper:
// https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf

// First and second hash functions for indexing the cuckoo tables
#if defined(VERY_LARGE_BOARDS)
inline int H1(Key h) { return h & 0xffff; }
inline int H2(Key h) { return (h >> 16) & 0xffff; }
#elif defined(LARGEBOARDS)
inline int H1(Key h) { return h & 0x7fff; }
inline int H2(Key h) { return (h >> 16) & 0x7fff; }
#else
inline int H1(Key h) { return h & 0x1fff; }
inline int H2(Key h) { return (h >> 16) & 0x1fff; }
#endif

// Cuckoo tables with Zobrist hashes of valid reversible moves, and the moves themselves
#ifdef LARGEBOARDS
Key cuckoo[65536];
Move cuckooMove[65536];
#else
Key cuckoo[8192];
Move cuckooMove[8192];
#endif

inline int in_hand_zobrist_index(int count) {
  return std::clamp(count, 0, SQUARE_NB - 1);
}

inline void xor_in_hand_count(Key& k, Piece pc, int oldCount, int newCount) {
  k ^= Zobrist::inHand[pc][in_hand_zobrist_index(oldCount)]
    ^ Zobrist::inHand[pc][in_hand_zobrist_index(newCount)];
}

inline void xor_points_bucket(Key& k, Color c, int points) {
  if (points < 0)
      return;
  int idx = std::min(points, Stockfish::Zobrist::MAX_ZOBRIST_POINTS - 1);
  k ^= Zobrist::points[c][idx];
}

inline int non_negative_points(int points) {
  return std::max(points, 0);
}

inline int append_dirty(StateInfo* st, Piece pc, Square from, Square to, Piece handPiece = NO_PIECE, int handCount = 0) {
  auto& dp = st->dirtyPiece;
  if (dp.dirty_num >= DIRTY_PIECE_MAX) {
      assert(false && "DirtyPiece overflow");
      st->nnueRefreshNeeded = true;
      return -1;
  }

  int idx = dp.dirty_num++;
  dp.piece[idx] = pc;
  dp.from[idx] = from;
  dp.to[idx] = to;
  dp.handPiece[idx] = handPiece;
  dp.handCount[idx] = handCount;
  return idx;
}

Key Position::reserve_key() const {
  if (!(piece_drops() || seirawan_gating() || potions_enabled()
        || capture_type() == PRISON || two_boards() || prison_pawn_promotion()))
      return 0;

  Key k = 0;
  for (Color c : {WHITE, BLACK})
      for (PieceType pt = PAWN; pt <= KING; ++pt)
      {
          Piece pc = make_piece(c, pt);
          if (piece_drops() || seirawan_gating() || potions_enabled() || two_boards())
              k ^= Zobrist::inHand[pc][in_hand_zobrist_index(pieceCountInHand[c][pt])];
          if (capture_type() == PRISON || prison_pawn_promotion())
              k ^= Zobrist::inHand[pc][in_hand_zobrist_index(pieceCountInPrison[~c][pt])];
      }

  return k;
}

Key Position::layout_key() const {
  Key k = 0;

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(b);
      k ^= Zobrist::psq[piece_on(s)][s];
  }

  for (Bitboard b = st->wallSquares; b; )
      k ^= Zobrist::wall[pop_lsb(b)];

  for (Bitboard b = st->deadSquares; b; )
      k ^= Zobrist::dead[pop_lsb(b)];

  return k;
}

bool Position::violates_same_player_board_repetition(Move m) const {

  if (!var->samePlayerBoardRepetitionIllegal)
      return false;

  Position* pos = const_cast<Position*>(this);
  StateInfo nextState;
  pos->do_move(m, nextState, false);

  bool repeated = false;
  int end = pos->captures_to_hand() ? pos->st->pliesFromNull : std::min(pos->st->rule50, pos->st->pliesFromNull);
  if (end >= 4)
  {
      StateInfo* stp = pos->st->previous->previous;
      for (int i = 4; i <= end; i += 2)
      {
          stp = stp->previous->previous;
          if (stp->move != MOVE_NONE && stp->layoutKey == pos->st->layoutKey)
          {
              repeated = true;
              break;
          }
      }
  }

  pos->undo_move(m);
  return repeated;
}


/// Position::init() initializes at startup the various arrays used to compute hash keys

void Position::init() {

  PRNG rng(1070372);

  for (Color c : {WHITE, BLACK})
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          for (Square s = SQ_A1; s <= SQ_MAX; ++s)
              Zobrist::psq[make_piece(c, pt)][s] = rng.rand<Key>();

  for (Square s = SQ_A1; s <= SQ_MAX; ++s)
      Zobrist::enpassant[s] = rng.rand<Key>();

  for (int cr = NO_CASTLING; cr <= ANY_CASTLING; ++cr)
      Zobrist::castling[cr] = rng.rand<Key>();

  Zobrist::side = rng.rand<Key>();
  Zobrist::noPawns = rng.rand<Key>();

  for (Color c : {WHITE, BLACK})
      for (int n = 0; n < CHECKS_NB; ++n)
          Zobrist::checks[c][n] = rng.rand<Key>();

  for (Color c : {WHITE, BLACK})
      for (PieceType pt = PAWN; pt <= KING; ++pt)
          for (int n = 0; n < SQUARE_NB; ++n)
              Zobrist::inHand[make_piece(c, pt)][n] = rng.rand<Key>();

  for (Color c : {WHITE, BLACK})
      for (File f = FILE_A; f <= FILE_MAX; ++f)
          for (int pt = 0; pt < PIECE_TYPE_NB; ++pt)
              Zobrist::committed[c][f][pt] = rng.rand<Key>();

  for (Color c : {WHITE, BLACK})
      for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
      {
          for (Square s = SQ_A1; s <= SQ_MAX; ++s)
              Zobrist::potionZone[c][pt][s] = rng.rand<Key>();

          for (int bit = 0; bit < POTION_COOLDOWN_BITS; ++bit)
              Zobrist::potionCooldown[c][pt][bit] = rng.rand<Key>();
      }

  for (Square s = SQ_A1; s <= SQ_MAX; ++s)
  {
      Zobrist::wall[s] = rng.rand<Key>();
      Zobrist::dead[s] = rng.rand<Key>();
  }

  for (int i = NO_EG_EVAL; i < EG_EVAL_NB; ++i)
      Zobrist::endgame[i] = rng.rand<Key>();

  for (Color c : {WHITE, BLACK})
      for (int i = 0; i < Stockfish::Zobrist::MAX_ZOBRIST_POINTS; ++i)
          Zobrist::points[c][i] = rng.rand<Key>();

  for (Square from = SQ_A1; from <= SQ_MAX; ++from)
      for (Square to = SQ_A1; to <= SQ_MAX; ++to)
      {
          int df = std::abs(int(file_of(to)) - int(file_of(from)));
          int dr = std::abs(int(rank_of(to)) - int(rank_of(from)));
          JumpMidpoint[from][to] = std::max(df, dr) == 2 && (df == 0 || dr == 0 || df == dr)
                                 ? make_square(File((int(file_of(from)) + int(file_of(to))) / 2),
                                               Rank((int(rank_of(from)) + int(rank_of(to))) / 2))
                                 : SQ_NONE;
      }

  // Prepare the cuckoo tables
  std::memset(cuckoo, 0, sizeof(cuckoo));
  std::memset(cuckooMove, 0, sizeof(cuckooMove));
  [[maybe_unused]] int count = 0;
  for (Color c : {WHITE, BLACK})
      for (PieceSet ps = CHESS_PIECES & ~piece_set(PAWN); ps;)
      {
      Piece pc = make_piece(c, pop_lsb(ps));
      for (Square s1 = SQ_A1; s1 <= SQ_MAX; ++s1)
          for (Square s2 = Square(s1 + 1); s2 <= SQ_MAX; ++s2)
              if ((type_of(pc) != PAWN) && (attacks_bb(c, type_of(pc), s1, 0) & s2))
              {
                  Move move = make_move(s1, s2);
                  Key key = Zobrist::psq[pc][s1] ^ Zobrist::psq[pc][s2] ^ Zobrist::side;
                  int i = H1(key);
                  while (true)
                  {
                      std::swap(cuckoo[i], key);
                      std::swap(cuckooMove[i], move);
                      if (move == MOVE_NONE) // Arrived at empty slot?
                          break;
                      i = (i == H1(key)) ? H2(key) : H1(key); // Push victim to alternative slot
                  }
                  count++;
             }
      }
#if defined(VERY_LARGE_BOARDS)
  // Very-large boards have a higher cuckoo insertion count than LARGEBOARDS.
  // Keep this as a sanity bound to avoid overfitting to one exact board shape.
  assert(count >= 9344);
#elif defined(LARGEBOARDS)
  assert(count == 9344);
#else
  assert(count == 3668);
#endif
}

Key Position::material_key(EndgameEval e) const {
  return st->materialKey ^ reserve_key() ^ Zobrist::endgame[e];
}


/// Position::set() initializes the position object with the given FEN string.
/// This function is not very robust - make sure that input FENs are correct,
/// this is assumed to be the responsibility of the GUI.

Position& Position::set(const Variant* v, const string& fenStr, bool isChess960, StateInfo* si, Thread* th, bool sfen) {
/*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 8 and ending with rank 1. Within each rank, the contents of each
      square are described from file A through file H. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("PNBRQK") whilst Black uses lowercase ("pnbrqk"). Blank squares are
      noted using digits 1 through 8 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Castling availability. If neither side can castle, this is "-". Otherwise,
      this has one or more letters: "K" (White can castle kingside), "Q" (White
      can castle queenside), "k" (Black can castle kingside), and/or "q" (Black
      can castle queenside).

   4) En passant target square (in algebraic notation). If there's no en passant
      target square, this is "-". If a pawn has just made a 2-square move, this
      is the position "behind" the pawn. Following X-FEN standard, this is recorded only
      if there is a pawn in position to make an en passant capture, and if there really
      is a pawn that might have advanced two squares.

   5) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   6) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

  unsigned char col, token;
  size_t idx;
  std::istringstream ss(fenStr);

  std::memset(static_cast<void*>(this), 0, sizeof(Position));
  std::memset(static_cast<void*>(si), 0, sizeof(StateInfo));
  st = si;

  var = v;

  ss >> std::noskipws;

  Rank r = max_rank();
  Square sq = SQ_A1 + r * NORTH;

  int commitFile = 0;
  int rank = 0;

  // 1. Piece placement
  while ((ss >> token) && !isspace(token))
  {
      if (std::isdigit(static_cast<unsigned char>(token)))
      {
          int steps = token - '0';
          while (true)
          {
              int next = ss.peek();
              if (next == std::char_traits<char>::eof()
                  || !std::isdigit(static_cast<unsigned char>(next)))
                  break;

              ss >> token;
              steps = 10 * steps + (token - '0');
          }
          if (commit_gates() && (rank == 0 || rank == max_rank() + 2))
              commitFile += steps;
          else
              sq += steps * EAST; // Advance the given number of files
      }

      else if (token == '/')
      {
          if(commit_gates())
          {
              if(rank != 0 && rank <= max_rank()){
                  sq += 2 * SOUTH + (FILE_MAX - max_file()) * EAST;
              }
              else if (rank == max_rank() + 1) {
                  sq = SQ_A1; // dummy to proceed with white musketeer pieces setup
              }
              ++rank;
              commitFile = 0;
          }
          else {
              sq = SQ_A1 + --r * NORTH;
          }
          if (!is_ok(sq))
              break;
      }

      // Stop before pieces in hand
      else if (token == '[')
          break;

      // Ignore pieces outside the board and wait for next / or [ to return to a valid state
      else if (!is_ok(sq) || file_of(sq) > max_file() || rank_of(sq) > r)
          continue;

      else if (token == '*')
      {
          if(commit_gates())
          {
              // musketeer
              ++commitFile;
          }
          else {
              // Wall square
              st->wallSquares |= sq;
              byTypeBB[ALL_PIECES] |= sq;
              ++sq;
          }
      }
      else if (token == '^')
      {
          // Dead square (neutral capturable blocker)
          st->deadSquares |= sq;
          byTypeBB[ALL_PIECES] |= sq;
          ++sq;
      }

      else if ((idx = piece_to_char().find(token)) != string::npos || (idx = piece_to_char_synonyms().find(token)) != string::npos)
      {
          if (ss.peek() == '~')
              ss >> token;

          if(v->commitGates && (rank == 0 || rank == max_rank() + 2))
          {
              commit_piece(Piece(idx), File(commitFile));
              ++commitFile;
          }
          else{
              put_piece(Piece(idx), sq, token == '~', NO_PIECE, true);
              ++sq;
          }
      }

      // Promoted shogi pieces
      else if (token == '+' && var->shogiStylePromotions && (idx = piece_to_char().find(ss.peek())) != string::npos && promoted_piece_type(type_of(Piece(idx))))
      {
          ss >> token;
          if(v->commitGates && (rank == 0 || rank == max_rank() + 2)){
            commit_piece(Piece(idx), File(commitFile));
            ++commitFile;
          }
          else {
            put_piece(make_piece(color_of(Piece(idx)), promoted_piece_type(type_of(Piece(idx)))), sq, true, Piece(idx), true);
            ++sq;
          }
      }
  }
  // Pieces in hand
  if (!isspace(token)) {
      bool prison = false;
      while ((ss >> token) && !isspace(token)) {
          if (token == ']') {
              continue;
          } else if (token == '#') {
              prison = true;
              continue;
          } else if ((idx = piece_to_char().find(token)) != string::npos) {
              if (prison) {
                  add_to_prison(Piece(idx));
              } else {
                  add_to_hand(Piece(idx));
              }
          }
      }
  }

  // Switch back to token-based parsing for the remaining FEN fields.
  ss >> std::skipws;

  // 2. Active color
  ss >> token;
  sideToMove = (token != (sfen ? 'w' : 'b') ? WHITE : BLACK);  // Invert colors for SFEN

  // 3-4. Skip parsing castling and en passant flags if not present
  st->epSquares = 0;
  st->castlingKingSquare[WHITE] = st->castlingKingSquare[BLACK] = SQ_NONE;
  ss >> std::ws;
  if (!std::isdigit(ss.peek()) && !sfen)
  {
      std::string castlingSpec;
      ss >> castlingSpec;

      Bitboard explicitGates[COLOR_NB] = {Bitboard(0), Bitboard(0)};
      bool hasExplicitGates = false;
      std::string legacyCastlingSpec = castlingSpec;

      // Optional extended syntax:
      //   <legacy-castling-and-gating>|<white-gating-mask>/<black-gating-mask>
      // where each mask has `files()` characters of 0/1.
      std::size_t sep = castlingSpec.find('|');
      if (sep != std::string::npos)
      {
          legacyCastlingSpec = castlingSpec.substr(0, sep);
          std::string maskSpec = castlingSpec.substr(sep + 1);
          std::size_t slash = maskSpec.find('/');
          if (slash != std::string::npos)
          {
              std::string whiteMask = maskSpec.substr(0, slash);
              std::string blackMask = maskSpec.substr(slash + 1);
              if (whiteMask.size() == std::size_t(files()) && blackMask.size() == std::size_t(files()))
              {
                  for (int i = 0; i < files(); ++i)
                  {
                      if (whiteMask[i] == '1')
                          explicitGates[WHITE] |= file_bb(File(i));
                      if (blackMask[i] == '1')
                          explicitGates[BLACK] |= file_bb(File(i));
                  }
                  hasExplicitGates = true;
              }
          }
      }

      // 3. Castling availability. Compatible with 3 standards: Normal FEN standard,
      // Shredder-FEN that uses the letters of the columns on which the rooks began
      // the game instead of KQkq and also X-FEN standard that, in case of Chess960,
      // if an inner rook is associated with the castling right, the castling tag is
      // replaced by the file letter of the involved rook, as for the Shredder-FEN.
      for (char castlingToken : legacyCastlingSpec)
      {
          token = castlingToken;
          Square rsq;
          Color c = std::islower(static_cast<unsigned char>(token)) ? BLACK : WHITE;

          token = char(std::toupper(static_cast<unsigned char>(token)));

          if (castling_enabled() && token == 'K')
              for (rsq = make_square(var->castlingRookKingsideFile, castling_rank(c)); (!(castling_rook_pieces(c) & type_of(piece_on(rsq))) || color_of(piece_on(rsq)) != c) && file_of(rsq) > FILE_A; --rsq) {}

          else if (castling_enabled() && token == 'Q')
              for (rsq = make_square(var->castlingRookQueensideFile, castling_rank(c)); (!(castling_rook_pieces(c) & type_of(piece_on(rsq))) || color_of(piece_on(rsq)) != c) && file_of(rsq) < max_file(); ++rsq) {}

          else if (token >= 'A' && token <= 'A' + max_file())
              rsq = make_square(File(token - 'A'), castling_rank(c));

          else
              continue;

          // Determine castling "king" position
          if (castling_enabled() && st->castlingKingSquare[c] == SQ_NONE)
          {
              Bitboard castlingKings = pieces(c, castling_king_piece(c)) & rank_bb(castling_rank(c));
              // Ambiguity resolution for 960 variants with more than one "king"
              // e.g., EAH means that an e-file king can castle with a- and h-file rooks
              st->castlingKingSquare[c] =  isChess960 && piece_on(rsq) == make_piece(c, castling_king_piece(c)) ? rsq
                                         : castlingKings && (!more_than_one(castlingKings) || isChess960) ? lsb(castlingKings)
                                         : make_square(castling_king_file(), castling_rank(c));
              // Skip invalid castling rights
              if (!(castlingKings & st->castlingKingSquare[c]))
                  st->castlingKingSquare[c] = SQ_NONE;
          }

          // Set gates (and skip castling rights)
          if (gating() && !commit_gates())
          {
              // Only add gates for occupied squares
              if (pieces(c) & rsq)
                  st->gatesBB[c] |= rsq;
              if ((token == 'K' || token == 'Q') && st->castlingKingSquare[c] != SQ_NONE)
                  st->gatesBB[c] |= st->castlingKingSquare[c];
              // Do not set castling rights for gates unless there are no pieces in hand,
              // which means that the file is referring to a chess960 castling right.
              else if (!seirawan_gating() || count_in_hand(c, ALL_PIECES) > 0 || captures_to_hand())
                  continue;
          }

          // Only add castling right if both king and rook are on expected squares
          if (   castling_enabled()
              && st->castlingKingSquare[c] != SQ_NONE
              && (castling_rook_pieces(c) & type_of(piece_on(rsq))) && color_of(piece_on(rsq)) == c)
              set_castling_right(c, rsq);
      }

      // Set castling rights for 960 gating variants
      if (gating() && castling_enabled())
          for (Color c : {WHITE, BLACK})
              if ((gates(c) & pieces(castling_king_piece(c))) && !castling_rights(c) && (!seirawan_gating() || count_in_hand(c, ALL_PIECES) > 0 || captures_to_hand()))
              {
                  Bitboard castling_rooks = gates(c) & pieces(c);
                  while (castling_rooks)
                  {
                      Square s = pop_lsb(castling_rooks);
                      if (castling_rook_pieces(c) & type_of(piece_on(s)))
                          set_castling_right(c, s);
                  }
              }

      // Extended FEN gating masks override implicit gate inference when provided.
      if (hasExplicitGates && gating() && !commit_gates())
          for (Color c : {WHITE, BLACK})
              st->gatesBB[c] = explicitGates[c] & pieces(c);

      // counting limit
      ss >> std::ws;
      if (counting_rule() && std::isdigit(ss.peek()))
          ss >> st->countingLimit;

      // 4. En passant square.
      // Ignore if square is invalid or not on side to move relative rank 6.
      else
      {
          std::string epSpec;
          ss >> epSpec;
          if (epSpec == "-")
              epSpec.clear();

          for (std::size_t i = 0; i < epSpec.size();)
          {
              col = epSpec[i++];
              if (col < 'a' || col > 'a' + max_file())
                  break;

              std::string rankDigits;
              while (i < epSpec.size() && std::isdigit(static_cast<unsigned char>(epSpec[i])))
              {
                  rankDigits.push_back(epSpec[i++]);
              }
              if (rankDigits.empty())
                  break;

              int rankNumber = 0;
              bool rankOverflow = false;
              for (char d : rankDigits)
              {
                  int digit = d - '0';
                  if (rankNumber > (std::numeric_limits<int>::max() - digit) / 10)
                  {
                      rankOverflow = true;
                      break;
                  }
                  rankNumber = rankNumber * 10 + digit;
              }
              if (rankOverflow)
                  continue;
              if (rankNumber < 1 || rankNumber > max_rank() + 1)
                  continue;

              Square epSquare = make_square(File(col - 'a'), Rank(rankNumber - 1));
#ifdef LARGEBOARDS
              // Consider different rank numbering in CECP
              if (max_rank() == RANK_10 && CurrentProtocol == XBOARD)
                  epSquare += NORTH;
#endif

              // En passant square will be considered only if
              // epSquare is within enPassantRegion and
              // 1) variant has non-standard rules
              // or
              // 2)
              // a) side to move have a pawn threatening epSquare
              // b) there is an enemy pawn one or two (for triple steps) squares in front of epSquare
              // c) there is no (non-wall) piece on epSquare or behind epSquare
              if (   (var->enPassantRegion[sideToMove] & epSquare)
                  && (   !var->fastAttacks
                      || (var->enPassantTypes[sideToMove] & ~piece_set(PAWN))
                      || (   pawn_attacks_bb(~sideToMove, epSquare) & pieces(sideToMove, PAWN)
                          && (   (pieces(~sideToMove, PAWN) & (epSquare + pawn_push(~sideToMove)))
                              || (pieces(~sideToMove, PAWN) & (epSquare + 2 * pawn_push(~sideToMove))))
                          && !((pieces(WHITE) | pieces(BLACK)) & (epSquare | (epSquare + pawn_push(sideToMove)))))))
                  st->epSquares |= epSquare;
          }
      }
  }

  // Check counter for nCheck
  ss >> std::skipws >> token >> std::noskipws;

  if (check_counting())
  {
      if (ss.peek() == '+')
      {
          st->checksRemaining[WHITE] = CheckCount(std::max(token - '0', 0));
          ss >> token >> token;
          st->checksRemaining[BLACK] = CheckCount(std::max(token - '0', 0));
      }
      else
      {
          // If check count is not provided, assume that the next check wins
          st->checksRemaining[WHITE] = CheckCount(1);
          st->checksRemaining[BLACK] = CheckCount(1);
          ss.putback(token);
      }
  }
  else
      ss.putback(token);

  // 5-6. Halfmove clock and fullmove number
  if (sfen)
  {
      // Pieces in hand for SFEN
      int handCount = 1;
      while ((ss >> token) && !isspace(token))
      {
          if (token == '-')
              continue;
          else if (std::isdigit(static_cast<unsigned char>(token)))
          {
              handCount = token - '0';
              while (std::isdigit(ss.peek()) && ss >> token)
                  handCount = 10 * handCount + (token - '0');
          }
          else if ((idx = piece_to_char().find(token)) != string::npos)
          {
              for (int i = 0; i < handCount; i++)
                  add_to_hand(Piece(idx));
              handCount = 1;
          }
      }
      // Move count is in ply for SFEN
      ss >> std::skipws >> gamePly;
      gamePly = std::max(gamePly - 1, 0);
  }
  else
  {
      ss >> std::skipws >> st->rule50 >> gamePly;

      // Convert from fullmove starting from 1 to gamePly starting from 0,
      // handle also common incorrect FEN with fullmove = 0.
      gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);
  }

  // counting rules
  if (st->countingLimit && st->rule50)
  {
      st->countingPly = st->rule50;
      st->rule50 = 0;
  }

  // Lichess-style counter for 3check
  if (check_counting())
  {
      if (ss >> token && token == '+')
      {
          ss >> token;
          st->checksRemaining[WHITE] = CheckCount(std::max(3 - (token - '0'), 0));
          ss >> token >> token;
          st->checksRemaining[BLACK] = CheckCount(std::max(3 - (token - '0'), 0));
      }
  }

  st->pointsCount[WHITE] = 0;
  st->pointsCount[BLACK] = 0;
  if (var->pointsCounting)
  {
      ss >> std::ws;
      if (ss.peek() == '{')
      {
          char openBrace = 0, closeBrace = 0;
          int whitePoints = 0, blackPoints = 0;
          if (ss >> openBrace >> whitePoints >> blackPoints >> closeBrace
              && openBrace == '{' && closeBrace == '}')
          {
              st->pointsCount[WHITE] = non_negative_points(whitePoints);
              st->pointsCount[BLACK] = non_negative_points(blackPoints);
          }
      }
  }

  if (potions_enabled())
  {
      ss >> std::ws;
      std::string potionSpec;
      if (ss.peek() == 'f' || ss.peek() == 'j' || ss.peek() == '-')
          ss >> potionSpec;

      if (!potionSpec.empty() && potionSpec != "-")
      {
          Color zoneColor = ~sideToMove;
          if (potionSpec.size() > 2 && potionSpec[1] == ':')
          {
              Square zoneCenter = parse_fen_square(*this, potionSpec.substr(2));
              if (is_ok(zoneCenter))
              {
                  if (potionSpec[0] == 'f')
                      st->potionZones[zoneColor][Variant::POTION_FREEZE] = freeze_zone_from_square(zoneCenter);
                  else if (potionSpec[0] == 'j')
                      st->potionZones[zoneColor][Variant::POTION_JUMP] = square_bb(zoneCenter);
              }
          }
      }

      ss >> std::ws;
      if (ss.peek() == '<')
      {
          char open = 0;
          ss >> open;
          std::string cooldownSpec;
          std::getline(ss, cooldownSpec, '>');
          auto vals = parse_potion_cooldowns(cooldownSpec);
          int maxCooldown = (1 << POTION_COOLDOWN_BITS) - 1;
          st->potionCooldown[WHITE][Variant::POTION_FREEZE] = std::min(vals[0], maxCooldown);
          st->potionCooldown[WHITE][Variant::POTION_JUMP]   = std::min(vals[1], maxCooldown);
          st->potionCooldown[BLACK][Variant::POTION_FREEZE] = std::min(vals[2], maxCooldown);
          st->potionCooldown[BLACK][Variant::POTION_JUMP]   = std::min(vals[3], maxCooldown);
      }
  }

  chess960 = isChess960 || v->chess960;
  tsumeMode = Options["TsumeMode"];
  thisThread = th;
  updatePawnCheckZone();
  set_state(st);

  assert(pos_is_ok());

  return *this;
}


/// Position::set_castling_right() is a helper function used to set castling
/// rights given the corresponding color and the rook starting square.

void Position::set_castling_right(Color c, Square rfrom) {

  assert(st->castlingKingSquare[c] != SQ_NONE);
  Square kfrom = st->castlingKingSquare[c];
  CastlingRights cr = c & (kfrom < rfrom ? KING_SIDE: QUEEN_SIDE);

  st->castlingRights |= cr;
  castlingRightsMask[kfrom] |= cr;
  castlingRightsMask[rfrom] |= cr;
  castlingRookSquare[cr] = rfrom;

  Square kto = make_square(cr & KING_SIDE ? castling_kingside_file() : castling_queenside_file(), castling_rank(c));
  Square rto = kto + (cr & KING_SIDE ? WEST : EAST);

  castlingPath[cr] =   (between_bb(rfrom, rto) | between_bb(kfrom, kto))
                    & ~(kfrom | rfrom);
}


/// Position::set_check_info() sets king attacks to detect if a move gives check

void Position::set_check_info(StateInfo* si) const {

  if (topology_wraps())
  {
      si->blockersForKing[WHITE] = si->blockersForKing[BLACK] = 0;
      si->pinners[WHITE] = si->pinners[BLACK] = 0;

      Square ksq = count<KING>(~sideToMove) ? square<KING>(~sideToMove) : SQ_NONE;
      si->nonSlidingRiders = 0;

      for (PieceSet ps = piece_types(); ps;)
      {
          PieceType pt = pop_lsb(ps);
          PieceType movePt = pt == KING ? king_type() : pt;
          si->checkSquares[pt] = 0;

          if (ksq != SQ_NONE)
          {
              Bitboard candidates = board_bb();
              while (candidates)
              {
                  Square s = pop_lsb(candidates);
                  if (attacks_from(sideToMove, pt, s) & ksq)
                      si->checkSquares[pt] |= s;
              }
          }

          if (AttackRiderTypes[movePt] & NON_SLIDING_RIDERS)
              si->nonSlidingRiders |= pieces(pt);
      }

      si->shak = si->checkersBB & (byTypeBB[KNIGHT] | byTypeBB[ROOK] | byTypeBB[BERS]);
      si->bikjang = false;
      si->chased = Bitboard(0);
      si->legalCapture = NO_VALUE;
      si->legalEnPassant = NO_VALUE;
      if (pseudo_royal_types())
      {
          si->pseudoRoyalCandidates = 0;
          si->pseudoRoyals = 0;
          for (PieceSet ps = pseudo_royal_types(); ps;)
          {
              PieceType pt = pop_lsb(ps);
              si->pseudoRoyalCandidates |= pieces(pt);
              if (count(sideToMove, pt) <= pseudo_royal_count())
                  si->pseudoRoyals |= pieces(sideToMove, pt);
              if (count(~sideToMove, pt) <= pseudo_royal_count())
                  si->pseudoRoyals |= pieces(~sideToMove, pt);
          }
      }
      return;
  }

  si->blockersForKing[WHITE] = slider_blockers(pieces(BLACK), count<KING>(WHITE) ? square<KING>(WHITE) : SQ_NONE, si->pinners[BLACK], BLACK);
  si->blockersForKing[BLACK] = slider_blockers(pieces(WHITE), count<KING>(BLACK) ? square<KING>(BLACK) : SQ_NONE, si->pinners[WHITE], WHITE);

  Square ksq = count<KING>(~sideToMove) ? square<KING>(~sideToMove) : SQ_NONE;
  Bitboard occupied = pieces();

  // For unused piece types, the check squares are left uninitialized
  if (fast_attacks())
  {
      const bool hasKing = ksq != SQ_NONE;
      const Bitboard pawnAttacks = hasKing ? pawn_attacks_bb(~sideToMove, ksq) : Bitboard(0);
      const Bitboard knightAttacks = hasKing ? attacks_bb<KNIGHT>(ksq) : Bitboard(0);
      const Bitboard bishopAttacks = hasKing ? attacks_bb<BISHOP>(ksq, occupied) : Bitboard(0);
      const Bitboard rookAttacks = hasKing ? attacks_bb<ROOK>(ksq, occupied) : Bitboard(0);
      const Bitboard kingAttacks = hasKing ? attacks_bb<KING>(ksq) : Bitboard(0);
      const Bitboard queenAttacks = bishopAttacks | rookAttacks;

      si->checkSquares[PAWN] = pawnAttacks;
      si->checkSquares[KNIGHT] = knightAttacks;
      si->checkSquares[BISHOP] = bishopAttacks;
      si->checkSquares[ROOK] = rookAttacks;
      si->checkSquares[QUEEN] = queenAttacks;
      si->checkSquares[KING] = kingAttacks;
      si->checkSquares[COMMONER] = kingAttacks;
      si->checkSquares[ARCHBISHOP] = bishopAttacks | knightAttacks;
      si->checkSquares[CHANCELLOR] = rookAttacks | knightAttacks;
      si->checkSquares[IMMOBILE_PIECE] = Bitboard(0);

      si->nonSlidingRiders = 0;
  }
  else
  {
      si->nonSlidingRiders = 0;
      if (ksq == SQ_NONE)
          for (PieceSet ps = piece_types(); ps;)
              si->checkSquares[pop_lsb(ps)] = Bitboard(0);
      else
      {
          for (PieceSet ps = piece_types(); ps;)
          {
              PieceType pt = pop_lsb(ps);
              PieceType movePt = pt == KING ? king_type() : pt;
              if (AttackRiderTypes[movePt] & ASYMMETRICAL_RIDERS)
                  // For asymmetrical riders, use true retro paths from the king square.
                  si->checkSquares[pt] = retro_asymmetric_check_squares(sideToMove, movePt, ksq, occupied);
              else
                  si->checkSquares[pt] = attacks_bb(~sideToMove, movePt, ksq, occupied);
              // Collect special piece types that require slower check and evasion detection
              if (AttackRiderTypes[movePt] & NON_SLIDING_RIDERS)
                  si->nonSlidingRiders |= pieces(pt);
          }
      }
  }
  si->shak = si->checkersBB & (byTypeBB[KNIGHT] | byTypeBB[ROOK] | byTypeBB[BERS]);
  si->bikjang = var->bikjangRule && ksq != SQ_NONE ? bool(attacks_bb(sideToMove, ROOK, ksq, pieces()) & pieces(sideToMove, KING)) : false;
  si->chased = var->chasingRule ? chased() : Bitboard(0);
  si->legalCapture = NO_VALUE;
  si->legalEnPassant = NO_VALUE;
  if (pseudo_royal_types())
  {
      si->pseudoRoyalCandidates = 0;
      si->pseudoRoyals = 0;
      for (PieceSet ps = pseudo_royal_types(); ps;)
      {
          PieceType pt = pop_lsb(ps);
          si->pseudoRoyalCandidates |= pieces(pt);
          if (count(sideToMove, pt) <= pseudo_royal_count())
              si->pseudoRoyals |= pieces(sideToMove, pt);
          if (count(~sideToMove, pt) <= pseudo_royal_count())
              si->pseudoRoyals |= pieces(~sideToMove, pt);
      }
  }

}


/// Position::set_state() computes the hash keys of the position, and other
/// data that once computed is updated incrementally as moves are made.
/// The function is only used when a new position is set up, and to verify
/// the correctness of the StateInfo data when running in debug mode.

void Position::set_state(StateInfo* si) const {

  si->key = si->materialKey = 0;
  si->pawnKey = Zobrist::noPawns;
  si->nonPawnMaterial[WHITE] = si->nonPawnMaterial[BLACK] = VALUE_ZERO;
  si->checkersBB = !allow_checks() && count<KING>(sideToMove)
                 ? attackers_to_king(square<KING>(sideToMove), ~sideToMove)
                 : Bitboard(0);

  if (!allow_checks())
  {
      if (pseudo_royal_types())
          si->checkersBB |= checked_pseudo_royals(sideToMove);
      if (anti_royal_types())
          si->checkersBB |= checked_anti_royals(sideToMove);
  }
  si->move = MOVE_NONE;
  si->removedGatingType = NO_PIECE_TYPE;
  si->removedCastlingGatingType = NO_PIECE_TYPE;
  si->capturedGatingType = NO_PIECE_TYPE;
  si->deadPiece = NO_PIECE;
  si->deadUnpromotedPiece = NO_PIECE;
  si->deadPiecePromoted = false;
  si->didColorChange = false;
  si->colorChangedFrom = NO_PIECE;
  si->colorChangeSquare = SQ_NONE;
  si->colorChangedPromoted = false;
  si->colorChangedUnpromoted = NO_PIECE;
  si->forcedJumpSquare = SQ_NONE;
  si->forcedJumpHasFollowup = false;
  si->forcedJumpStep = 0;

  set_check_info(si);

  for (Bitboard b = pieces(); b; )
  {
      Square s = pop_lsb(b);
      Piece pc = piece_on(s);
      si->key ^= Zobrist::psq[pc][s];

      if (!pc)
          si->key ^= (st->deadSquares & s) ? Zobrist::dead[s] : Zobrist::wall[s];

      else if (type_of(pc) == PAWN)
          si->pawnKey ^= Zobrist::psq[pc][s];

      else if (type_of(pc) != KING)
          si->nonPawnMaterial[color_of(pc)] += PieceValue[MG][pc];
  }

  for (Bitboard b = si->epSquares; b; )
      si->key ^= Zobrist::enpassant[pop_lsb(b)];

  if (sideToMove == BLACK)
      si->key ^= Zobrist::side;

  si->key ^= Zobrist::castling[si->castlingRights];

  for (Color c : {WHITE, BLACK})
      for (PieceType pt = PAWN; pt <= KING; ++pt)
      {
          Piece pc = make_piece(c, pt);

          for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
              si->materialKey ^= Zobrist::psq[pc][cnt];

          if (piece_drops() || seirawan_gating() || potions_enabled() || two_boards())
          {
              int n = std::clamp(pieceCountInHand[c][pt], 0, SQUARE_NB - 1);
              si->key ^= Zobrist::inHand[pc][n];
          }

          if (capture_type() == PRISON || prison_pawn_promotion())
          {
              int n = std::clamp(pieceCountInPrison[~c][pt], 0, SQUARE_NB - 1);
              si->key ^= Zobrist::inHand[pc][n];
          }
      }

  if (potions_enabled())
      for (Color c : {WHITE, BLACK})
          for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
          {
              Variant::PotionType potion = static_cast<Variant::PotionType>(pt);
              if (potion_piece(potion) == NO_PIECE_TYPE)
                  continue;

              xor_potion_zone(si->key, c, potion, si->potionZones[c][pt]);
              xor_potion_cooldown(si->key, c, potion, si->potionCooldown[c][pt]);
          }

  if (commit_gates())
      for (Color c : {WHITE, BLACK})
          for (File f = FILE_A; f <= max_file(); ++f)
              xor_committed_gate(si->key, c, f, committed_piece_type(c, f));

  if (check_counting())
      for (Color c : {WHITE, BLACK})
          si->key ^= Zobrist::checks[c][si->checksRemaining[c]];

  if (var->pointsCounting) {
      for (Color c : {WHITE, BLACK}) {
          xor_points_bucket(si->key, c, si->pointsCount[c]);
      }
  }

  si->boardKey = si->key ^ reserve_key();
  si->layoutKey = layout_key();
  si->repetition = 0;
  si->boardRepetition = 0;

}


/// Position::set() is an overload to initialize the position object with
/// the given endgame code string like "KBPKN". It is mainly a helper to
/// get the material key out of an endgame code.

Position& Position::set(const string& code, Color c, StateInfo* si) {

  string sides[] = { code.substr(code.find('v') != string::npos ? code.find('v') + 1 : code.find('K', 1)),      // Weak
                     code.substr(0, std::min(code.find('v'), code.find('K', 1))) }; // Strong

  assert(sides[0].length() > 0 && sides[0].length() < 8);
  assert(sides[1].length() > 0 && sides[1].length() < 8);

  std::transform(sides[c].begin(), sides[c].end(), sides[c].begin(), tolower);

  string n = std::to_string(8);
  string fenStr =  sides[0] + "///////" + sides[1] + " w - - 0 10";

  return set(variants.get("fairy"), fenStr, false, si, nullptr);
}


/// Position::fen() returns a FEN representation of the position. In case of
/// Chess960 the Shredder-FEN notation is used. This is mainly a debugging function.

string Position::fen(bool sfen, bool showPromoted, int countStarted, std::string holdings, Bitboard fogArea) const {

  int emptyCnt;
  std::ostringstream ss;
  if(commit_gates()){
      for(File f = FILE_A; f <= max_file(); ++f){
          if(has_committed_piece(BLACK, f)) ss << piece_to_char()[make_piece(BLACK, committedGates[BLACK][f])];
          else ss << "*";
      }
      ss << "/";
  }

  for (Rank r = max_rank(); r >= RANK_1; --r)
  {
      for (File f = FILE_A; f <= max_file(); ++f)
      {
          for (emptyCnt = 0; f <= max_file() && !(pieces() & make_square(f, r)) && !(fogArea & make_square(f, r)); ++f)
              ++emptyCnt;

          if (emptyCnt)
              ss << emptyCnt;

          if (f <= max_file())
          {
              if (empty(make_square(f, r)) || fogArea & make_square(f, r))
              {
                  Square s = make_square(f, r);
                  ss << ((st->deadSquares & s) ? "^" : "*");
              }
              else if (var->shogiStylePromotions && unpromoted_piece_on(make_square(f, r)))
                  // Promoted shogi pieces, e.g., +r for dragon
                  ss << "+" << piece_to_char()[unpromoted_piece_on(make_square(f, r))];
              else
              {
                  ss << piece_to_char()[piece_on(make_square(f, r))];

                  // Set promoted pieces
                  if (((captures_to_hand() && !drop_loop()) || two_boards() ||  showPromoted) && is_promoted(make_square(f, r)))
                      ss << "~";
              }
          }
      }

      if (r > RANK_1)
          ss << '/';
  }
  if(commit_gates()){
      ss << "/";
      for(File f = FILE_A; f <= max_file(); ++f){
          if(has_committed_piece(WHITE, f)) ss << piece_to_char()[make_piece(WHITE, committedGates[WHITE][f])];
          else ss << "*";
      }
  }
  // SFEN
  if (sfen)
  {
      ss << (sideToMove == WHITE ? " b " : " w ");
      for (Color c : {WHITE, BLACK})
          for (PieceType pt = KING; pt >= PAWN; --pt)
              if (pieceCountInHand[c][pt] > 0)
              {
                  if (pieceCountInHand[c][pt] > 1)
                      ss << pieceCountInHand[c][pt];
                  ss << piece_to_char()[make_piece(c, pt)];
              }
      if (count_in_hand(ALL_PIECES) == 0)
          ss << '-';
      ss << " " << gamePly + 1;
      return ss.str();
  }

  // pieces in hand
  // Keep reserve state in FEN whenever it affects legality/search state, even
  // if literal drop moves are disabled (e.g. reserve-backed promotions).
  if (   !free_drops()
      && (   piece_drops()
          || seirawan_gating()
          || potions_enabled()
          || var->promotionRequireInHand
          || var->promotionConsumeInHand)
      && !commit_gates())
  {
      ss << '[';
      if (holdings != "-") {
          ss << holdings;
      } else {
          for (Color c: {WHITE, BLACK})
              for (PieceType pt = KING; pt >= PAWN; --pt) {
                  assert(pieceCountInHand[c][pt] >= 0);
                  ss << std::string(pieceCountInHand[c][pt], piece_to_char()[make_piece(c, pt)]);
              }
          if (capture_type() == PRISON &&
              (count_in_prison(WHITE, ALL_PIECES) > 0 || count_in_prison(BLACK, ALL_PIECES) > 0)) {
              ss << '#';
              for (Color c: {BLACK, WHITE})
                  for (PieceType pt = KING; pt >= PAWN; --pt) {
                      assert(pieceCountInPrison[c][pt] >= 0);
                      int n = pieceCountInPrison[c][pt];
                      if (n > 0) {
                          ss << std::string(n, piece_to_char()[make_piece(~c, pt)]);
                      }
                  }
          }
      }
      ss << ']';
  }

  ss << (sideToMove == WHITE ? " w " : " b ");

  // Disambiguation for chess960 "king" square
  if (chess960 && can_castle(WHITE_CASTLING) && popcount(pieces(WHITE, castling_king_piece(WHITE)) & rank_bb(castling_rank(WHITE))) > 1)
      ss << char('A' + castling_king_square(WHITE));

  if (can_castle(WHITE_OO))
      ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO ))) : 'K');

  if (can_castle(WHITE_OOO))
      ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');

  if (gating() && !commit_gates() && gates(WHITE) && (!seirawan_gating() || count_in_hand(WHITE, ALL_PIECES) > 0 || captures_to_hand()))
      for (File f = FILE_A; f <= max_file(); ++f)
          if (   (gates(WHITE) & file_bb(f))
              // skip gating flags redundant with castling flags
              && !(!chess960 && can_castle(WHITE_CASTLING) && f == file_of(castling_king_square(WHITE)))
              && !(can_castle(WHITE_OO ) && f == file_of(castling_rook_square(WHITE_OO )))
              && !(can_castle(WHITE_OOO) && f == file_of(castling_rook_square(WHITE_OOO))))
              ss << char('A' + f);

  // Disambiguation for chess960 "king" square
  if (chess960 && can_castle(BLACK_CASTLING) && popcount(pieces(BLACK, castling_king_piece(BLACK)) & rank_bb(castling_rank(BLACK))) > 1)
      ss << char('a' + castling_king_square(BLACK));

  if (can_castle(BLACK_OO))
      ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO ))) : 'k');

  if (can_castle(BLACK_OOO))
      ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');

  if (gating() && gates(BLACK) && (!seirawan_gating() || count_in_hand(BLACK, ALL_PIECES) > 0 || captures_to_hand()))
      for (File f = FILE_A; f <= max_file(); ++f)
          if (   (gates(BLACK) & file_bb(f))
              // skip gating flags redundant with castling flags
              && !(!chess960 && can_castle(BLACK_CASTLING) && f == file_of(castling_king_square(BLACK)))
              && !(can_castle(BLACK_OO ) && f == file_of(castling_rook_square(BLACK_OO )))
              && !(can_castle(BLACK_OOO) && f == file_of(castling_rook_square(BLACK_OOO))))
              ss << char('a' + f);

  if (!can_castle(ANY_CASTLING) && !(gating() && !commit_gates() && (gates(WHITE) | gates(BLACK))))
      ss << '-';

  // Preserve exact gating state in extended FEN for reliable roundtrips.
  // Keep 8x8 output backward-compatible for existing GUIs and tooling.
  if (gating() && !commit_gates() && files() > 8)
  {
      ss << "|";
      for (Color c : {WHITE, BLACK})
      {
          for (File f = FILE_A; f <= max_file(); ++f)
              ss << ((gates(c) & file_bb(f)) ? '1' : '0');
          if (c == WHITE)
              ss << "/";
      }
  }

  // Counting limit or ep-square
  if (st->countingLimit)
      ss << " " << counting_limit(countStarted) << " ";
  else if (!ep_squares())
      ss << " - ";
  else
  {
      ss << " ";
      for (Bitboard b = ep_squares(); b; )
          ss << UCI::square(*this, pop_lsb(b));
      ss << " ";
  }

  // Check count
  if (check_counting())
      ss << st->checksRemaining[WHITE] << "+" << st->checksRemaining[BLACK] << " ";

  // Counting ply or 50-move rule counter
  if (st->countingLimit)
      ss << counting_ply(countStarted);
  else
      ss << st->rule50;

  ss << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

  if (variant()->pointsCounting)
  {
      ss << " {" << st->pointsCount[WHITE] << " " << st->pointsCount[BLACK] << "}";
  }

  if (potions_enabled())
  {
      int wf = st->potionCooldown[WHITE][Variant::POTION_FREEZE];
      int wj = st->potionCooldown[WHITE][Variant::POTION_JUMP];
      int bf = st->potionCooldown[BLACK][Variant::POTION_FREEZE];
      int bj = st->potionCooldown[BLACK][Variant::POTION_JUMP];
      bool hasCooldownState = wf || wj || bf || bj;

      Color zoneColor = ~sideToMove;
      Variant::PotionType zoneType = Variant::POTION_TYPE_NB;
      Bitboard zone = st->potionZones[zoneColor][Variant::POTION_FREEZE];
      if (zone)
          zoneType = Variant::POTION_FREEZE;
      else
      {
          zone = st->potionZones[zoneColor][Variant::POTION_JUMP];
          if (zone)
              zoneType = Variant::POTION_JUMP;
      }

      Square zoneCenter = SQ_NONE;
      if (zoneType == Variant::POTION_JUMP)
          zoneCenter = lsb(zone);
      else if (zoneType == Variant::POTION_FREEZE)
          for (Square s = SQ_A1; s <= SQ_MAX; ++s)
              if ((board_bb() & s) && freeze_zone_from_square(s) == zone)
              {
                  zoneCenter = s;
                  break;
              }

      bool hasZone = is_ok(zoneCenter);
      if (hasZone || hasCooldownState)
      {
          ss << " ";
          if (hasZone)
              ss << (zoneType == Variant::POTION_FREEZE ? "f:" : "j:") << UCI::square(*this, zoneCenter);
          else
              ss << "-";
          ss << " <" << wf << " " << wj << " " << bf << " " << bj << ">";
      }
  }

  return ss.str();
}


/// Position::slider_blockers() returns a bitboard of all the pieces (both colors)
/// that are blocking attacks on the square 's' from 'sliders'. A piece blocks a
/// slider if removing that piece from the board would result in a position where
/// square 's' is attacked. For example, a king-attack blocking piece can be either
/// a pinned or a discovered check piece, according if its color is the opposite
/// or the same of the color of the slider.

Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& pinners, Color c) const {

  Bitboard blockers = 0;
  pinners = 0;

  if (s == SQ_NONE || !sliders)
      return blockers;

  // Snipers are sliders that attack 's' when a piece and other snipers are removed
  Bitboard snipers = 0;
  Bitboard slidingSnipers = 0;

  if (var->fastAttacks)
  {
      snipers = (  (attacks_bb<  ROOK>(s) & pieces(c, QUEEN, ROOK, CHANCELLOR))
                 | (attacks_bb<BISHOP>(s) & pieces(c, QUEEN, BISHOP, ARCHBISHOP))) & sliders;
      slidingSnipers = snipers;
  }
  else
  {
      for (PieceSet ps = piece_types(); ps;)
      {
          PieceType pt = pop_lsb(ps);
          RiderType riderTypes = AttackRiderTypes[pt];
          Bitboard ptPieces = pieces(c, pt);
          Bitboard b = sliders & (PseudoAttacks[~c][pt][s] ^ LeaperAttacks[~c][pt][s]) & ptPieces;
          if (b)
          {
              // Consider asymmetrical moves (e.g., horse)
              if (riderTypes & ASYMMETRICAL_RIDERS)
              {
                  Bitboard asymmetricals = PseudoAttacks[~c][pt][s] & ptPieces;
                  while (asymmetricals)
                  {
                      Square s2 = pop_lsb(asymmetricals);
                      if (!(attacks_from(c, pt, s2) & s))
                          snipers |= s2;
                  }
              }
              else
                  snipers |= b & ~attacks_bb(~c, pt, s, pieces());
              if (riderTypes & ~HOPPING_RIDERS)
                  slidingSnipers |= snipers & ptPieces;
          }
      }
      // Diagonal rook pins in Janggi palace
      if (diagonal_lines() & s)
      {
          Bitboard diags = diagonal_lines() & PseudoAttacks[~c][BISHOP][s] & sliders & pieces(c, ROOK);
          while (diags)
          {
              Square s2 = pop_lsb(diags);
              if (!(attacks_from(c, ROOK, s2) & s))
              {
                  snipers |= s2;
                  slidingSnipers |= s2;
              }
          }
      }
  }
  Bitboard allPieces = pieces();
  Bitboard occupancy = allPieces ^ slidingSnipers;
  Bitboard pinnedColorPieces = pieces(color_of(piece_on(s)));
  Bitboard janggiCannons = pieces(JANGGI_CANNON);

  while (snipers)
  {
    Square sniperSq = pop_lsb(snipers);
    Piece sniper = piece_on(sniperSq);
    PieceType sniperType = type_of(sniper);
    if (AttackRiderTypes[sniperType] & RIDER_ROSE)
    {
        Bitboard b = rose_revealed_blockers(s, sniperSq, occupancy);
        if (b && !more_than_one(b))
            pinners |= pieces(c) & sniperSq;
        blockers |= b;
        continue;
    }
    bool isHopper = AttackRiderTypes[sniperType] & HOPPING_RIDERS;
    Bitboard b = between_bb(s, sniperSq, sniperType) & (isHopper ? (allPieces ^ sniperSq) : occupancy);

    if (b && (!more_than_one(b) || (isHopper && popcount(b) == 2)))
    {
        // Janggi cannons block each other
        if ((janggiCannons & sniperSq) && (janggiCannons & b))
            b &= janggiCannons;
        blockers |= b;
        if (b & pinnedColorPieces)
            pinners |= sniperSq;
    }
  }
  return blockers;
}


/// Position::attackers_to() computes a bitboard of all pieces which attack a
/// given square. Slider attacks use the occupied bitboard to indicate occupancy.

Bitboard Position::attackers_to(Square s, Bitboard occupied, Color c, Bitboard janggiCannons) const {

  if (topology_wraps())
  {
      Bitboard b = 0;
      for (PieceSet ps = piece_types(); ps;)
      {
          PieceType pt = pop_lsb(ps);
          Bitboard ptPieces = pieces(c, pt);
          if (!ptPieces)
              continue;

          PieceType move_pt = pt == KING ? king_type() : pt;
          if (pt == JANGGI_CANNON)
              b |= attacks_from(~c, move_pt, s, occupied)
                 & attacks_from(~c, move_pt, s, occupied & ~janggiCannons)
                 & ptPieces;
          else
              b |= attacks_from(~c, move_pt, s, occupied) & ptPieces;
      }
      return b;
  }

  // Use a faster version for variants with moderate rule variations
  if (fast_attacks())
  {
      return  (pawn_attacks_bb(~c, s)          & pieces(c, PAWN))
            | (attacks_bb<KNIGHT>(s)           & pieces(c, KNIGHT, ARCHBISHOP, CHANCELLOR))
            | (attacks_bb<  ROOK>(s, occupied) & pieces(c, ROOK, QUEEN, CHANCELLOR))
            | (attacks_bb<BISHOP>(s, occupied) & pieces(c, BISHOP, QUEEN, ARCHBISHOP))
            | (attacks_bb<KING>(s)             & pieces(c, KING, COMMONER));
  }

  // Use a faster version for selected fairy pieces
  if (fast_attacks2())
  {
      return  (pawn_attacks_bb(~c, s)             & pieces(c, PAWN, BREAKTHROUGH_PIECE, GOLD))
            | (attacks_bb<KNIGHT>(s)              & pieces(c, KNIGHT))
            | (attacks_bb<  ROOK>(s, occupied)    & (  pieces(c, ROOK, QUEEN, DRAGON)
                                                     | (pieces(c, LANCE) & PseudoAttacks[~c][LANCE][s])))
            | (attacks_bb<BISHOP>(s, occupied)    & pieces(c, BISHOP, QUEEN, DRAGON_HORSE))
            | (attacks_bb<KING>(s)                & pieces(c, KING, COMMONER))
            | (attacks_bb<FERS>(s)                & pieces(c, FERS, DRAGON, SILVER))
            | (attacks_bb<WAZIR>(s)               & pieces(c, WAZIR, DRAGON_HORSE, GOLD))
            | (LeaperAttacks[~c][SHOGI_KNIGHT][s] & pieces(c, SHOGI_KNIGHT))
            | (LeaperAttacks[~c][SHOGI_PAWN][s]   & pieces(c, SHOGI_PAWN, SILVER));
  }

  Bitboard b = 0;
  for (PieceSet ps = piece_types(); ps;)
  {
      PieceType pt = pop_lsb(ps);
      if (board_bb(c, pt) & s)
      {
          PieceType move_pt = pt == KING ? king_type() : pt;
          // Consider asymmetrical moves (e.g., horse)
          if (AttackRiderTypes[move_pt] & ASYMMETRICAL_RIDERS)
          {
              Bitboard asymmetricals = PseudoAttacks[~c][move_pt][s] & pieces(c, pt);
              while (asymmetricals)
              {
                  Square s2 = pop_lsb(asymmetricals);
                  if (attacks_bb(c, move_pt, s2, occupied) & s)
                      b |= s2;
              }
          }
          else if (pt == JANGGI_CANNON)
              b |= attacks_bb(~c, move_pt, s, occupied) & attacks_bb(~c, move_pt, s, occupied & ~janggiCannons) & pieces(c, JANGGI_CANNON);
          else
              b |= attacks_bb(~c, move_pt, s, occupied) & pieces(c, pt);
      }
  }

  // Janggi palace moves
  if (diagonal_lines() & s)
  {
      Bitboard diags = 0;
      if (king_type() == WAZIR)
          diags |= attacks_bb(~c, FERS, s, occupied) & pieces(c, KING);
      diags |= attacks_bb(~c, FERS, s, occupied) & pieces(c, WAZIR);
      diags |= attacks_bb(~c, PAWN, s, occupied) & pieces(c, SOLDIER);
      diags |= rider_attacks_bb<RIDER_BISHOP>(s, occupied) & pieces(c, ROOK);
      diags |=  rider_attacks_bb<RIDER_CANNON_DIAG>(s, occupied)
              & rider_attacks_bb<RIDER_CANNON_DIAG>(s, occupied & ~janggiCannons)
              & pieces(c, JANGGI_CANNON);
      b |= diags & diagonal_lines();
  }

  // Unpromoted soldiers
  if (b & pieces(SOLDIER) && relative_rank(c, s, max_rank()) < var->soldierPromotionRank)
      b ^= b & pieces(SOLDIER) & ~PseudoAttacks[~c][SHOGI_PAWN][s];

  return b;
}

Bitboard Position::attackers_to_king(Square s, Bitboard occupied, Color c, Bitboard janggiCannons) const {

  Bitboard attackers = attackers_to(s, occupied, c, janggiCannons);
  // Frozen pieces cannot give check (relevant for spell-chess freeze effects).
  attackers &= ~(freeze_squares(c) | (pieces(c, PAWN) & pawnCannotCheckZone[c]));
  PieceSet forbiddenToKing = var->captureForbiddenToKing;
  if (!attackers || !forbiddenToKing)
      return attackers;

  for (PieceSet ps = forbiddenToKing; ps; )
      attackers &= ~pieces(c, pop_lsb(ps));

  return attackers;
}


Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
  return attackers_to(s, occupied, WHITE) | attackers_to(s, occupied, BLACK);
}

bool Position::has_forced_jump_followup() const {

  if (!forced_jump_continuation() || st->forcedJumpSquare == SQ_NONE)
      return false;

  return st->forcedJumpHasFollowup;
}

bool Position::compute_forced_jump_followup(Square s, int step) const {

  Piece mover = piece_on(s);
  if (mover == NO_PIECE)
      return false;
  if (freeze_squares() & s)
      return false;

  PieceSet jumpTypes = jump_capture_types();
  if (!(jumpTypes & ALL_PIECES) && !(jumpTypes & type_of(mover)))
      return false;

  Color c = color_of(mover);
  PieceType pt = type_of(mover);
  Bitboard candidates = (attacks_from(c, pt, s) | moves_from(c, pt, s)) & ~pieces();
  while (candidates)
  {
      Square to = pop_lsb(candidates);
      if (step && int(to) - int(s) != step)
          continue;
      if (jump_capture_square(s, to) != SQ_NONE)
          return true;
  }
  return false;
}

/// Position::checked_pseudo_royals computes a bitboard of
/// all pseudo-royal pieces of a particular color that are in check
Bitboard Position::checked_pseudo_royals(Color c) const {
  assert(pseudo_royal_types());
  const bool blastOnCapture = blast_on_capture();
  Bitboard checked = 0;
  Bitboard occupied = pieces();
  Bitboard pseudoRoyals = st->pseudoRoyals & pieces(c);
  Bitboard pseudoRoyalCandidates = var->dupleCheck ? st->pseudoRoyalCandidates & pieces(c) : Bitboard(0);
  if (!pseudoRoyals && !pseudoRoyalCandidates)
      return checked;
  Bitboard pseudoRoyalsTheirs = (blastOnCapture && !blast_promotion())
                              ? st->pseudoRoyals & pieces(~c)
                              : Bitboard(0);
  const bool blastRelevant = bool(pseudoRoyalsTheirs);

  // If royal pieces are immune to blasts, then their checks remain threats even
  // when the attacker is inside the blast radius. Build a bitboard of such
  // blast-immune pieces.
  Bitboard blastImmune = blastRelevant ? blast_immune_bb() : Bitboard(0);
  Bitboard vulnerablePseudoRoyalsTheirs = pseudoRoyalsTheirs & ~blastImmune;

  if (blastRelevant)
      while (pseudoRoyals)
      {
          Square sr = pop_lsb(pseudoRoyals);
          // Skip if capturing this piece would blast any non-immune enemy
          // pseudo-royal pieces
          if (!(vulnerablePseudoRoyalsTheirs & blast_pattern(sr))
              && attackers_to(sr, occupied, ~c))
              checked |= sr;
      }
  else
      while (pseudoRoyals)
      {
          Square sr = pop_lsb(pseudoRoyals);
          if (attackers_to(sr, occupied, ~c))
              checked |= sr;
      }
  // Look for duple check
  if (var->dupleCheck)
  {
      Bitboard allAttacked = 0;
      if (blastRelevant)
          while (pseudoRoyalCandidates)
          {
              Square sr = pop_lsb(pseudoRoyalCandidates);
              if (!(vulnerablePseudoRoyalsTheirs & blast_pattern(sr))
                  && attackers_to(sr, occupied, ~c))
                  allAttacked |= sr;
              else
                  // If at least one isn't attacked, it is not a duple check
                  return checked;
          }
      else
          while (pseudoRoyalCandidates)
          {
              Square sr = pop_lsb(pseudoRoyalCandidates);
              if (attackers_to(sr, occupied, ~c))
                  allAttacked |= sr;
              else
                  // If at least one isn't attacked, it is not a duple check
                  return checked;
          }
      checked |= allAttacked;
  }
  return checked;
}

/// Position::checked_anti_royals computes a bitboard of
/// all anti-royal pieces of a particular color that are NOT attacked
Bitboard Position::checked_anti_royals(Color c) const {
  assert(anti_royal_types());
  const bool blastOnCapture = blast_on_capture();

  Bitboard occupied = pieces();
  Bitboard vulnerableEnemyRoyals = 0;
  if (blastOnCapture)
      vulnerableEnemyRoyals = ((st->pseudoRoyals | pieces(king_type())) & pieces(~c) & occupied) & ~blast_immune_bb();

  Bitboard checked = 0;
  for (PieceSet ps = anti_royal_types(); ps; )
  {
      PieceType pt = pop_lsb(ps);
      if (count(c, pt) <= anti_royal_count())
      {
          if (count(c, pt) > 0)
          {
              Bitboard antiRoyals = pieces(c, pt);
              while (antiRoyals)
              {
                  Square sr = pop_lsb(antiRoyals);
                  if (!(attackers_to(sr, occupied, ~c))
                      || (blastOnCapture && (vulnerableEnemyRoyals & blast_pattern(sr))))
                      checked |= sr;
              }
          }
          else
              checked = board_bb(); // Check can't be resolved if piece is gone
      }
  }
  return checked;
}


/// Position::legal() tests whether a pseudo-legal move is legal

bool Position::legal(Move m) const {

  assert(is_ok(m));
  assert(type_of(m) != DROP || piece_drops());

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);

  if (in_opening_self_removal_phase())
      return is_opening_self_removal_move(m);

  if (is_pass(m) && !(pass(us) || wall_or_move()))
      return false;

  if (pass_until_setup() && must_drop()
      && !has_setup_drop(us)
      && has_setup_drop(them)
      && !is_pass(m))
      return false;

  Bitboard freezeExtra = 0;
  Bitboard jumpRemoved = 0;
  Variant::PotionType gatingPotion = Variant::POTION_TYPE_NB;
  if (is_gating(m))
  {
      gatingPotion = potion_type_from_piece(var, gating_type(m));
      if (gatingPotion != Variant::POTION_TYPE_NB)
      {
          if (!can_cast_potion(us, gatingPotion))
              return false;
          if (gatingPotion == Variant::POTION_FREEZE)
              freezeExtra = freeze_zone_from_square(gating_square(m));
          else if (gatingPotion == Variant::POTION_JUMP)
          {
              jumpRemoved = square_bb(gating_square(m));
              if (!piece_on(gating_square(m)))
                  return false;
          }
      }
  }

  SpellContextScope spellScope(*this, freezeExtra, jumpRemoved);

  if (type_of(m) != DROP && (freeze_squares() & from))
      return false;
  // Castling is also blocked if the participating rook is frozen.
  if (type_of(m) == CASTLING)
  {
      Square rookFrom = to;
      if (piece_on(rookFrom) == NO_PIECE
          || color_of(piece_on(rookFrom)) != us
          || !(castling_rook_pieces(us) & type_of(piece_on(rookFrom))))
          rookFrom = castling_rook_square(us & (to > from ? KING_SIDE : QUEEN_SIDE));
      if (freeze_squares() & rookFrom)
          return false;
  }
  if (type_of(m) == CASTLING && gamePly < var->castlingForbiddenPlies)
      return false;
  if (jumpRemoved && (square_bb(to) & jumpRemoved))
      return false;

  if (from == to && !(is_pass(m) || (type_of(m) == PROMOTION && sittuyin_promotion())))
      return false;

  bool rifleShot = rifle_capture(m) && capture(m) && type_of(m) != CASTLING;
  Square shotSq = capture(m) ? capture_square(m) : to;
  Bitboard removedAttackers = rifleShot ? square_bb(shotSq) : Bitboard(0);
  Square effectiveTo = rifleShot ? from : to;

  if (type_of(m) == PROMOTION && !promotion_allowed(us, promotion_type(m)))
      return false;
  if (type_of(m) == PIECE_PROMOTION && (is_promoted(from) || !promotion_allowed(us, promoted_piece_type(type_of(moved_piece(m))))))
      return false;
  if (rifleShot && (type_of(m) == PROMOTION || type_of(m) == PIECE_PROMOTION))
      return false;
  if (type_of(m) != DROP && type_of(m) != PROMOTION && type_of(m) != PIECE_PROMOTION)
  {
      Piece mover = moved_piece(m);
      Bitboard mandatoryZone = mover == NO_PIECE ? Bitboard(0) : mandatory_promotion_zone(mover);
      if ((mandatoryZone & effectiveTo) && !(mandatoryZone & from))
          return false;
  }

  assert(color_of(moved_piece(m)) == us);
  assert(!count<KING>(us) || piece_on(square<KING>(us)) == make_piece(us, KING));
  assert(board_bb() & to);

  if (forced_jump_continuation() && st->forcedJumpSquare != SQ_NONE)
  {
      Piece forcedPiece = piece_on(st->forcedJumpSquare);
      if (forcedPiece != NO_PIECE && st->forcedJumpHasFollowup)
      {
          if (color_of(forcedPiece) != us)
          {
              Piece passPiece = moved_piece(m);
              return is_pass(m) && passPiece != NO_PIECE && color_of(passPiece) == us;
          }
          if (is_pass(m) || from != st->forcedJumpSquare || !is_jump_capture(m))
              return false;
          if (forced_jump_same_direction() && st->forcedJumpStep && int(to) - int(from) != st->forcedJumpStep)
              return false;
      }
  }
  PieceSet jumpTypes = jump_capture_types();
  bool jumpTypeMover = (jumpTypes & ALL_PIECES) || (jumpTypes & type_of(moved_piece(m)));
  if ((type_of(m) == NORMAL || type_of(m) == PROMOTION) && jumpTypeMover && !empty(to))
      return false;
  if ((type_of(m) == NORMAL || type_of(m) == PROMOTION) && jumpTypeMover)
  {
      int df = std::abs(int(file_of(to)) - int(file_of(from)));
      int dr = std::abs(int(rank_of(to)) - int(rank_of(from)));
      bool isHop = std::max(df, dr) == 2 && (df == 0 || dr == 0 || df == dr);
      if (isHop && jump_capture_square(from, to) == SQ_NONE)
          return false;
  }
  if (topology_wraps() && !allow_checks() && (pieces(them) & to) && type_of(piece_on(to)) == KING)
      return false;
  PieceType movePt = type_of(moved_piece(m));
  if (type_of(m) != DROP && (var->mutuallyHopIllegalTypes & movePt) && (AttackRiderTypes[movePt] & HOPPING_RIDERS))
  {
      Bitboard between = between_bb(from, to);
      Bitboard hopIllegalPieces = 0;
      for (PieceSet ps = var->mutuallyHopIllegalTypes; ps;)
          hopIllegalPieces |= pieces(pop_lsb(ps));
      if ((between & pieces()) && ((between | to) & hopIllegalPieces))
          return false;
  }

  // Illegal checks
  if (((!checking_permitted() && !allow_checks()) || (sittuyin_promotion() && type_of(m) == PROMOTION) || (!drop_checks() && type_of(m) == DROP)) && gives_check(m))
      return false;

  // Optional rule: disallow checkmate by drops.
  // Shogi pawn-drop mate rule is a stricter piece-specific version.
  if (   type_of(m) == DROP
      && gives_check(m)
      && (   !drop_mates()
          || (shogi_pawn_drop_mate_illegal() && type_of(moved_piece(m)) == SHOGI_PAWN)))
  {
      StateInfo setupState, nextState;
      Position p;
      p.set(variant(), fen(), is_chess960(), &setupState, this_thread());
      p.do_move(m, nextState, true);
      if (p.checkers() && MoveList<LEGAL>(p).size() == 0)
          return false;
  }

  // Illegal quiet moves
  if (must_capture() && !capture(m) && has_capture())
      return false;
  if (must_capture_en_passant() && type_of(m) != EN_PASSANT && has_en_passant_capture())
      return false;

  // Illegal captures
  if (topology_wraps() && !allow_checks() && (pieces(them) & to) && type_of(piece_on(to)) == KING)
      return false;

  // Illegal non-drop moves
  PieceType requiredDropType = must_drop_type();
  if (must_drop() && count_in_hand(us, requiredDropType) > 0)
  {
      if (type_of(m) == DROP)
      {
          if (requiredDropType != ALL_PIECES && requiredDropType != in_hand_piece_type(m))
              return false;
      }
      else if (checkers())
      {
          for (const auto& mevasion : MoveList<EVASIONS>(*this))
              if (type_of(mevasion) == DROP && legal(mevasion))
                  return false;
      }
      else
      {
          for (const auto& mquiet : MoveList<QUIETS>(*this))
              if (type_of(mquiet) == DROP && legal(mquiet))
                  return false;
      }
  }

  // Illegal drop move
  if (drop_opposite_colored_bishop() && type_of(m) == DROP)
  {
      if (type_of(moved_piece(m)) != BISHOP)
      {
          Bitboard remaining = drop_region(us, BISHOP) & ~pieces() & ~square_bb(to);
          // Are enough squares available to drop bishops on opposite colors?
          if (   popcount( DarkSquares & (pieces(us, BISHOP) | remaining)) < count_with_hand(us, BISHOP) / 2
              || popcount(~DarkSquares & (pieces(us, BISHOP) | remaining)) < count_with_hand(us, BISHOP) / 2)
              return false;
      }
      else
          // Drop resulting in same-colored bishops
          if (popcount((DarkSquares & to ? DarkSquares : ~DarkSquares) & pieces(us, BISHOP)) + 1 > (count_with_hand(us, BISHOP) + 1) / 2)
              return false;
  }
  if (type_of(m) == DROP && (!var->isPriorityDrop[type_of(moved_piece(m))]) && priorityDropCountInHand[us] > 0)
      return false;

  if (type_of(m) == DROP && pay_points_to_drop()
      && st->pointsCount[us] < var->piecePoints[type_of(moved_piece(m))])
      return false;

  // No legal moves from target square
  if (immobility_illegal() && (type_of(m) == DROP || type_of(m) == NORMAL))
  {
      PieceType pt = type_of(moved_piece(m));
      const PieceInfo* pi = pieceMap.get(pt);
      bool hasPotentialMove = PseudoMoves[0][us][pt][to] & board_bb();
      if (is_pure_hopper_like(pi))
          hasPotentialMove = has_hopper_potential_from_square(pi, us, to);
      if (   !hasPotentialMove
          && !(jump_capture_types() & ALL_PIECES) && !(jump_capture_types() & pt))
          return false;
  }

  // Illegal king passing move
  if (pass_on_stalemate(us) && is_pass(m) && !checkers())
  {
      for (const auto& move : MoveList<NON_EVASIONS>(*this))
          if (!is_pass(move) && legal(move))
              return false;
  }

  // Multimoves
  if (var->multimoveOffset || var->progressiveMultimove)
  {
      if (is_pass(m) != multimove_pass(gamePly))
          return false;
      if (multimove_pass(gamePly + 1) && ((!var->multimoveCapture && capture(m)) || (!var->multimoveCheck && gives_check(m))))
          return false;
  }

  // Check for attacks to pseudo-royal pieces
  if (pseudo_royal_types())
  {
      const bool blastOnCapture = blast_on_capture();
      Square kto = rifleShot ? from : to;
      Square blastCenter = (type_of(m) == EN_PASSANT || rifleShot) ? shotSq : kto;
      Bitboard occupied = rifleShot ? pieces() : (type_of(m) != DROP ? pieces() ^ from : pieces());
      Bitboard blastImmune = blastOnCapture ? blast_immune_bb() : Bitboard(0);
      if (walling_rule() == DUCK)
          occupied ^= st->wallSquares;
      if (walling(us) || is_gating(m))
          occupied |= gating_square(m);
      if (type_of(m) == CASTLING)
      {
          // After castling, the rook and king final positions are the same in
          // Chess960 as they would be in standard chess.
          kto = make_square(to > from ? castling_kingside_file() : castling_queenside_file(), castling_rank(us));
          Direction step = kto > from ? EAST : WEST;
          Square rto = kto - (to > from ? EAST : WEST);
          // Pseudo-royal king
          if (st->pseudoRoyals & from)
              // Loop over squares between the king and its final position
              // Ensure to include the initial square if from == kto
              for (Square s = from; from != kto ? s != kto : s == from; s += step)
                  if (  !(blastOnCapture && (blast_pattern(s) & st->pseudoRoyals & pieces(~sideToMove) & ~blastImmune))
                      && (attackers_to(s, occupied, ~us) & ~removedAttackers))
                      return false;
          // Move the rook
          occupied ^= to | rto;
      }
      if (!rifleShot)
          occupied |= kto;
      if (type_of(m) == EN_PASSANT)
          occupied &= ~square_bb(capture_square(to));
      else if (rifleShot)
          occupied &= ~square_bb(shotSq);
      if (capture(m) && blastOnCapture)
          occupied &= ~blast_squares(blastCenter);
      // Petrifying a pseudo-royal piece is illegal
      if (capture(m) && (var->petrifyOnCaptureTypes & type_of(moved_piece(m))) && (st->pseudoRoyals & from))
          return false;
      Bitboard pseudoRoyals = st->pseudoRoyals & pieces(sideToMove);
      // Add dropped pseudo-royal
      if (type_of(m) == DROP && (pseudo_royal_types() & piece_set(type_of(moved_piece(m)))))
          pseudoRoyals |= square_bb(to);
      Bitboard pseudoRoyalsTheirs = st->pseudoRoyals & pieces(~sideToMove);
      if (!rifleShot && is_ok(from) && (pseudoRoyals & from))
          pseudoRoyals ^= square_bb(from) ^ kto;
      if (type_of(m) == PROMOTION && (pseudo_royal_types() & promotion_type(m)))
      {
          if (count(sideToMove, promotion_type(m)) >= pseudo_royal_count())
              // increase in count leads to loss of pseudo-royalty
              pseudoRoyals &= ~pieces(sideToMove, promotion_type(m));
          else
              // promoted piece is pseudo-royal
              pseudoRoyals |= kto;
      }
      // Self-explosions are illegal
      if (pseudoRoyals & ~occupied)
          return false;
      // Petrifiable pseudo-royals can't capture
      Bitboard attackerCandidatesTheirs = occupied & ~square_bb(kto);
      for (PieceSet ps = var->petrifyOnCaptureTypes & pseudo_royal_types(); ps;)
          attackerCandidatesTheirs &= ~pieces(~us, pop_lsb(ps));
      // Check for legality unless we capture a pseudo-royal piece
      if (!(pseudoRoyalsTheirs & ~occupied))
          while (pseudoRoyals)
          {
              Square sr = pop_lsb(pseudoRoyals);
              // Touching pseudo-royal pieces are immune
              if (  !(blastOnCapture && (pseudoRoyalsTheirs & blast_pattern(sr) & ~blastImmune))
                  && (attackers_to(sr, occupied, ~us) & ~removedAttackers & attackerCandidatesTheirs))
                  return false;
          }
      // Look for duple check
      if (var->dupleCheck)
      {
          Bitboard pseudoRoyalCandidates = st->pseudoRoyalCandidates & pieces(sideToMove);
          if (!rifleShot && is_ok(from) && (pseudoRoyalCandidates & from))
              pseudoRoyalCandidates ^= square_bb(from) ^ kto;
          if (type_of(m) == PROMOTION && (pseudo_royal_types() & promotion_type(m)))
              pseudoRoyalCandidates |= kto;
          bool allCheck = bool(pseudoRoyalCandidates);
          while (allCheck && pseudoRoyalCandidates)
          {
              Square sr = pop_lsb(pseudoRoyalCandidates);
              // Touching pseudo-royal pieces are immune
              if (!(  !(blastOnCapture && (pseudoRoyalsTheirs & blast_pattern(sr) & ~blastImmune))
                    && (attackers_to(sr, occupied, ~us) & ~removedAttackers & attackerCandidatesTheirs)))
                  allCheck = false;
          }
          if (allCheck)
              return false;
      }
  }

  // Anti-royal pieces must remain under attack.
  if (anti_royal_types())
  {
      const bool blastOnCapture = blast_on_capture();
      Square kto = rifleShot ? from : to;
      Square blastCenter = (type_of(m) == EN_PASSANT || rifleShot) ? shotSq : kto;
      Square rfrom = SQ_NONE, rto = SQ_NONE;
      Bitboard occupied = rifleShot ? pieces() : (type_of(m) != DROP ? pieces() ^ from : pieces());
      Bitboard blastImmune = blastOnCapture ? blast_immune_bb() : Bitboard(0);
      if (walling_rule() == DUCK)
          occupied ^= st->wallSquares;
      if (walling(us) || is_gating(m))
          occupied |= gating_square(m);
      if (type_of(m) == CASTLING)
      {
          kto = make_square(to > from ? castling_kingside_file() : castling_queenside_file(), castling_rank(us));
          rfrom = to;
          rto = kto - (to > from ? EAST : WEST);
          occupied ^= to | rto;
      }
      if (!rifleShot)
          occupied |= kto;
      if (type_of(m) == EN_PASSANT)
          occupied &= ~square_bb(capture_square(to));
      if (capture(m) && blastOnCapture)
          occupied &= ~blast_squares(blastCenter);

      Bitboard antiRoyals = 0;
      for (PieceSet ps = anti_royal_types(); ps; )
      {
          PieceType pt = pop_lsb(ps);
          if (count(sideToMove, pt) <= anti_royal_count())
          {
              if (count(sideToMove, pt) > 0)
                  antiRoyals |= pieces(sideToMove, pt);
              else if (type_of(m) != DROP || type_of(moved_piece(m)) != pt)
                  return false; // Anti-royal piece is missing and not replaced
          }
      }
      if (!rifleShot && is_ok(from) && (antiRoyals & from))
          antiRoyals ^= square_bb(from) ^ kto;
      if (is_ok(rfrom) && (antiRoyals & rfrom))
          antiRoyals ^= square_bb(rfrom) ^ rto;
      if (type_of(m) == DROP && (anti_royal_types() & type_of(moved_piece(m))))
          antiRoyals |= square_bb(to);
      if (type_of(m) == PROMOTION)
      {
          if (anti_royal_types() & type_of(moved_piece(m)))
          {
              if (count(sideToMove, type_of(moved_piece(m))) > anti_royal_count())
                  antiRoyals &= ~pieces(sideToMove, type_of(moved_piece(m)));
          }
          if (anti_royal_types() & promotion_type(m))
          {
              if (count(sideToMove, promotion_type(m)) <= anti_royal_count())
                  antiRoyals |= kto;
          }
      }

      Bitboard vulnerableEnemyRoyals = blastOnCapture
                                     ? ((st->pseudoRoyals | pieces(king_type())) & pieces(~us) & occupied) & ~blastImmune
                                     : Bitboard(0);
      while (antiRoyals)
      {
          Square sr = pop_lsb(antiRoyals);
          if (!(occupied & sr)
              || (blastOnCapture && (vulnerableEnemyRoyals & blast_pattern(sr)))
              || !(attackers_to(sr, occupied, ~us) & ~removedAttackers))
              return false;
      }
  }

  if (capture(m) && type_of(m) != CASTLING)
  {
      PieceType attacker = type_of(moved_piece(m));
      PieceType target = type_of(captured_piece(m));
      if (attacker < PIECE_TYPE_NB && target < PIECE_TYPE_NB && (var->captureForbidden[attacker] & target))
          return false;
  }

  // En passant captures are a tricky special case. Because they are rather
  // uncommon, we do it simply by testing whether the king is attacked after
  // the move is made.
  if (!allow_checks() && type_of(m) == EN_PASSANT && count<KING>(us))
  {
      Square ksq = square<KING>(us);
      Square capsq = capture_square(to);
      Bitboard occupied = rifleShot ? (pieces() ^ capsq) : ((pieces() ^ from ^ capsq) | to);

      assert(ep_squares() & to);
      assert(piece_on(to) == NO_PIECE);

      return !((attackers_to_king(ksq, occupied, ~us) & ~removedAttackers) & occupied)
          && !violates_same_player_board_repetition(m);
  }

  // Castling moves generation does not check if the castling path is clear of
  // enemy attacks, it is delayed at a later time: now!
  if (type_of(m) == CASTLING)
  {
      Square rookFrom = to;
      if (piece_on(rookFrom) == NO_PIECE
          || color_of(piece_on(rookFrom)) != us
          || !(castling_rook_pieces(us) & type_of(piece_on(rookFrom))))
          rookFrom = castling_rook_square(us & (to > from ? KING_SIDE : QUEEN_SIDE));
      // After castling, the rook and king final positions are the same in
      // Chess960 as they would be in standard chess.
      to = make_square(to > from ? castling_kingside_file() : castling_queenside_file(), castling_rank(us));
      Direction step = to > from ? WEST : EAST;

      // Will the gate be blocked by king or rook?
      Square rto = to + (to_sq(m) > from_sq(m) ? WEST : EAST);
      if (is_gating(m) && (gating_square(m) == to || gating_square(m) == rto))
          return false;

      // Non-royal castlers are not impeded by attacked transit squares.
      // Spell Chess uses a COMMONER as castling king while still requiring
      // king-like castling safety checks.
      bool royalLikeCastler = type_of(piece_on(from)) == KING
                           || (potions_enabled() && type_of(piece_on(from)) == castling_king_piece(us));
      if (!royalLikeCastler)
          return !violates_same_player_board_repetition(m);

      // Spell-chess uses COMMONER as castling king and still requires
      // "cannot castle through attack", with frozen attackers ignored.
      bool spellLikeCastler = type_of(piece_on(from)) != KING && potions_enabled();
      auto attackers_for_castling = [&](Square s, Bitboard occ) {
          Bitboard att = spellLikeCastler ? attackers_to(s, occ, ~us)
                                          : attackers_to_king(s, occ, ~us);
          att &= ~removedAttackers;
          if (spellLikeCastler)
              att &= ~freeze_squares(~us);
          return att;
      };

      if (((!allow_checks()) || spellLikeCastler) && attackers_for_castling(from, pieces()))
          return false;

      for (Square s = to; s != from; s += step)
          if (   (((!allow_checks()) || spellLikeCastler) && attackers_for_castling(s, pieces()))
              || (var->flyingGeneral && (attacks_bb(~us, ROOK, s, pieces() ^ from) & pieces(~us, KING)))
              || (var->diagonalGeneral && (attacks_bb(~us, BISHOP, s, pieces() ^ from) & pieces(~us, KING))))
              return false;

      // In case of Chess960, verify if the Rook blocks some checks
      // For instance an enemy queen in SQ_A1 when castling rook is in SQ_B1.
      return ((allow_checks() && !spellLikeCastler)
          || !attackers_for_castling(to, pieces() ^ rookFrom))
          && !violates_same_player_board_repetition(m);
  }

  Bitboard occupied = rifleShot ? (pieces() ^ square_bb(shotSq))
                                : (((type_of(m) != DROP ? pieces() ^ from : pieces()) ^ square_bb(shotSq)) | to);

  Bitboard removedByEffects = 0;
  if (!is_pass(m))
  {
      if (((capture(m) || rifleShot) && blast_on_capture()) || (blast_on_move() && !capture(m)))
      {
          Square blastCenter = (capture(m) || rifleShot) ? shotSq : effectiveTo;
          Bitboard blastRelevant = occupied & ~blast_immune_bb();
          removedByEffects |= blast_pattern(blastCenter) & blastRelevant;
          if (blast_center())
              removedByEffects |= square_bb(blastCenter) & blastRelevant;
      }

      if ((capture(m) || rifleShot) && (var->petrifyOnCaptureTypes & movePt))
          removedByEffects |= square_bb(effectiveTo);
  }

  // Gated kings and pseudo-royals must not be introduced onto attacked squares.
  // If the move captures an attacker on its way, ignore that disappearing attack.
  if (is_gating(m) && gating_type(m) != NO_PIECE_TYPE)
  {
      PieceType gateType = gating_type(m);
      if (gateType == KING || (pseudo_royal_types() & piece_set(gateType)))
      {
          Square gate = gating_square(m);
          Bitboard occ = occupied | gate;

          Bitboard attackers = gateType == KING ? attackers_to_king(gate, occ, ~us)
                                                : attackers_to(gate, occ, ~us);
          attackers &= ~removedAttackers;

          if (attackers)
              return false;
      }
  }

  // Flying general rule and bikjang
  // In case of bikjang passing is always allowed, even when in check
  if (st->bikjang && is_pass(m))
      return !violates_same_player_board_repetition(m);
  if ((var->flyingGeneral && count<KING>(us)) || st->bikjang)
  {
      Square s = type_of(moved_piece(m)) == KING ? (rifleShot ? from : to) : square<KING>(us);
      if (attacks_bb(~us, ROOK, s, occupied) & pieces(~us, KING) & ~square_bb(to))
          return false;
  }
  if (var->diagonalGeneral && count<KING>(us))
  {
      Square s = type_of(moved_piece(m)) == KING ? (rifleShot ? from : to) : square<KING>(us);
      if (attacks_bb(~us, BISHOP, s, occupied) & pieces(~us, KING) & ~square_bb(to))
          return false;
  }

  // Makpong rule
  if (var->makpongRule && checkers() && type_of(moved_piece(m)) == KING && (checkers() ^ to))
      return false;

  if (var->royalPieceNoThroughCheck && type_of(moved_piece(m)) == king_type())
  {
      Bitboard traversed = between_bb(from, to) & ~square_bb(to);
      while (traversed)
      {
          Square s = pop_lsb(traversed);
          Bitboard pathOccupied = (pieces() ^ from) | s;
          if (attackers_to_king(s, pathOccupied, ~us) & ~removedAttackers)
              return false;
      }
  }

  if (count<KING>(us))
  {
      Square kingSquareAfterMove = type_of(moved_piece(m)) == KING ? (rifleShot ? from : to)
                                                                   : square<KING>(us);
      if (removedByEffects & square_bb(kingSquareAfterMove))
          return false;
  }

  // If the moving piece is a king, check whether the destination square is
  // attacked by the opponent.
  if (!allow_checks() && type_of(moved_piece(m)) == KING)
      return !(attackers_to_king(rifleShot ? from : to, occupied, ~us) & ~removedAttackers)
          && !violates_same_player_board_repetition(m);

  // Return early when without king
  if (!count<KING>(us))
      return !violates_same_player_board_repetition(m);

  Bitboard janggiCannons = pieces(JANGGI_CANNON);
  if (type_of(moved_piece(m)) == JANGGI_CANNON)
      janggiCannons = rifleShot ? (janggiCannons & ~square_bb(shotSq))
                                : ((type_of(m) == DROP ? janggiCannons : janggiCannons ^ from) | to);
  else if (janggiCannons & to)
      janggiCannons ^= to;

  // A non-king move is legal if the king is not under attack after the move.
  return (allow_checks() || !(attackers_to_king(square<KING>(us), occupied, ~us, janggiCannons)
                              & ~removedAttackers
                              & ~(rifleShot ? Bitboard(0) : SquareBB[to])))
      && !violates_same_player_board_repetition(m);
}


/// Position::pseudo_legal() takes a random move and tests whether the move is
/// pseudo legal. It is used to validate moves from TT that can be corrupted
/// due to SMP concurrent access or hash position key aliasing.

bool Position::pseudo_legal(const Move m) const {

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);
  bool rifleShot = rifle_capture(m) && capture(m) && type_of(m) != CASTLING;
  Square effectiveTo = rifleShot ? from : to;
  Bitboard removedAttackers = capture(m) ? square_bb(capture_square(m)) : Bitboard(0);

  if (in_opening_self_removal_phase())
      return is_opening_self_removal_move(m);

  if (is_pass(m) && !(pass(us) || wall_or_move()))
      return false;

  if (pass_until_setup() && must_drop()
      && !has_setup_drop(us)
      && has_setup_drop(them)
      && !is_pass(m))
      return false;

  if (from == to && !(is_pass(m) || (type_of(m) == PROMOTION && sittuyin_promotion())))
      return false;

  if (type_of(m) == PROMOTION && !promotion_allowed(us, promotion_type(m)))
      return false;
  if (type_of(m) == PIECE_PROMOTION && (is_promoted(from) || !promotion_allowed(us, promoted_piece_type(type_of(pc)))))
      return false;
  if (type_of(m) != DROP && type_of(m) != PROMOTION && type_of(m) != PIECE_PROMOTION)
  {
      Bitboard mandatoryZone = pc == NO_PIECE ? Bitboard(0) : mandatory_promotion_zone(pc);
      if ((mandatoryZone & effectiveTo) && !(mandatoryZone & from))
          return false;
  }

  if (forced_jump_continuation() && st->forcedJumpSquare != SQ_NONE)
  {
      Piece forcedPiece = piece_on(st->forcedJumpSquare);
      if (forcedPiece != NO_PIECE && st->forcedJumpHasFollowup)
      {
          if (color_of(forcedPiece) != us)
          {
              Piece passPiece = moved_piece(m);
              return is_pass(m) && passPiece != NO_PIECE && color_of(passPiece) == us;
          }
          if (is_pass(m))
              return false;
          if (from != st->forcedJumpSquare || !is_jump_capture(m))
              return false;
          if (forced_jump_same_direction() && st->forcedJumpStep && int(to) - int(from) != st->forcedJumpStep)
              return false;
      }
  }
  PieceSet jumpTypes = jump_capture_types();
  bool jumpTypeMover = (jumpTypes & ALL_PIECES) || (jumpTypes & type_of(pc));
  if ((type_of(m) == NORMAL || type_of(m) == PROMOTION) && jumpTypeMover && !empty(to))
      return false;
  if ((type_of(m) == NORMAL || type_of(m) == PROMOTION) && jumpTypeMover)
  {
      int df = std::abs(int(file_of(to)) - int(file_of(from)));
      int dr = std::abs(int(rank_of(to)) - int(rank_of(from)));
      bool isHop = std::max(df, dr) == 2 && (df == 0 || dr == 0 || df == dr);
      if (isHop && jump_capture_square(from, to) == SQ_NONE)
          return false;
  }

  // Illegal moves to squares outside of board or to wall squares
  if (!(board_bb() & to))
      return false;

  // Use a fast check for piece drops
  if (type_of(m) == DROP)
  {
      Bitboard legalDropTargets = ~pieces();
      if (capture_drop_types() & in_hand_piece_type(m))
      {
          legalDropTargets |= pieces(them);
          if (self_capture())
              legalDropTargets |= pieces(us) & ~pieces(us, KING);
      }

      return   piece_drops()
            && pc != NO_PIECE
            && color_of(pc) == us
            && (!pay_points_to_drop() || st->pointsCount[us] >= var->piecePoints[type_of(pc)])
            && (can_drop(us, in_hand_piece_type(m))
                || (two_boards() && allow_virtual_drop(us, type_of(pc)))
                || (capture_type() == PRISON && exchange_piece(m) != NO_PIECE_TYPE
                        && count_in_prison(us, exchange_piece(m)) > 0
                        && count_in_prison(~us, in_hand_piece_type(m)) > 0))
            && (drop_region(us, type_of(pc)) & legalDropTargets & to)
            && (   type_of(pc) == in_hand_piece_type(m)
                || (drop_promoted() && type_of(pc) == promoted_piece_type(in_hand_piece_type(m))));
  }

  // Use a slower but simpler function for uncommon cases
  // yet we skip the legality check of MoveList<LEGAL>().
  Bitboard freezeExtra = 0;
  Bitboard jumpRemoved = 0;
  if (is_gating(m))
  {
      Variant::PotionType potion = potion_type_from_piece(var, gating_type(m));
      if (potion != Variant::POTION_TYPE_NB)
      {
          if (!can_cast_potion(us, potion))
              return false;
          if (potion == Variant::POTION_FREEZE)
              freezeExtra = freeze_zone_from_square(gating_square(m));
          else if (potion == Variant::POTION_JUMP)
          {
              jumpRemoved = square_bb(gating_square(m));
              if (!piece_on(gating_square(m)))
                  return false;
          }
      }
  }

  SpellContextScope spellScope(*this, freezeExtra, jumpRemoved);

  if (type_of(m) != DROP && (freeze_squares() & from))
      return false;
  if (type_of(m) == CASTLING && gamePly < var->castlingForbiddenPlies)
      return false;
  if (jumpRemoved && (square_bb(to) & jumpRemoved))
      return false;

  if (type_of(m) != NORMAL || is_gating(m))
  {
      const bool useWrappedFallback = topology_wraps() && checkers();
      return ((checkers() && !useWrappedFallback) ? MoveList<    EVASIONS>(*this).contains(m)
                                                  : MoveList<NON_EVASIONS>(*this).contains(m))
          && !violates_same_player_board_repetition(m);
  }

  //if walling, and walling is not optional, or they didn't move, do the checks.
  if (walling(us) && (!wall_or_move() || (from == to)))
  {
      Bitboard wallsquares = st->wallSquares;
      Square capSq = capture(m) ? capture_square(m) : to;

      // Illegal wall square placement
      if (!((board_bb() & ~(((pieces() ^ from) ^ capSq) | to)) & gating_square(m)))
          return false;
      if (!(walling_region(us) & gating_square(m)) || //putting a wall on disallowed square
          wallsquares & gating_square(m)) //or square already with a wall
          return false;
      if (walling_rule() == ARROW && !(moves_bb(us, type_of(pc), to, pieces() ^ from) & gating_square(m)))
          return false;
      if (walling_rule() == PAST && (from != gating_square(m)))
          return false;
      if (walling_rule() == EDGE)
      {
          Bitboard validsquares = board_bb() &
                  ((FileABB | file_bb(max_file()) | Rank1BB | rank_bb(max_rank())) |
                  ( shift<NORTH     >(wallsquares) | shift<SOUTH     >(wallsquares)
                  | shift<EAST      >(wallsquares) | shift<WEST      >(wallsquares)));
          if (!(validsquares & gating_square(m))) return false;
      };
  }

  // Handle the case where a mandatory piece promotion/demotion is not taken
  if (    mandatory_piece_promotion()
      && (is_promoted(from) ? piece_demotion() : promoted_piece_type(type_of(pc)) != NO_PIECE_TYPE)
      && (promotion_zone(pc) & (SquareBB[from] | effectiveTo))
      && (!piece_promotion_on_capture() || capture(m)))
      return false;

  // Is not a promotion, so promotion piece must be empty
  if (promotion_type(m) != NO_PIECE_TYPE)
      return false;

  // If the 'from' square is not occupied by a piece belonging to the side to
  // move, the move is obviously not legal.
  if (pc == NO_PIECE || color_of(pc) != us)
      return false;

  // The destination square cannot be occupied by a friendly piece unless
  // self-capture is enabled. Friendly kings remain uncapturable.
  if (pieces(us) & to)
  {
      if (!(self_capture() && capture(m)))
          return false;
      if (type_of(piece_on(to)) == KING)
          return false;
  }

  if (topology_wraps() && !allow_checks() && (pieces(them) & to) && type_of(piece_on(to)) == KING)
      return false;

  // Handle the special case of a pawn move
  if (type_of(pc) == PAWN && !topology_wraps())
  {
      // We have already handled promotion moves, so destination
      // cannot be on the 8th/1st rank.
      if (mandatory_pawn_promotion() && (promotion_zone(pc) & effectiveTo) && !sittuyin_promotion())
          return false;

      if (   !(pawn_attacks_bb(us, from) & (self_capture() ? pieces() : pieces(~us)) & to) // Not a capture
          && !((from + pawn_push(us) == to) && !(pieces() & to)) // Not a single push
          && !(   (from + 2 * pawn_push(us) == to)               // Not a double push
               && (double_step_region(pc) & from)
               && !(pieces() & (to | (to - pawn_push(us)))))
          && !(   (from + 3 * pawn_push(us) == to)               // Not a triple push
               && (triple_step_region(pc) & from)
               && !(pieces() & (to | (to - pawn_push(us)) | (to - 2 * pawn_push(us))))))
          return false;
  }
  else if (!((capture(m) ? attacks_from(us, type_of(pc), from) : moves_from(us, type_of(pc), from)) & to))
      return false;

  // Hopper-type pieces can optionally be configured to avoid hopping over
  // or capturing selected piece types (e.g. Janggi cannons vs cannons).
  PieceType movePt = type_of(pc);
  if ((var->mutuallyHopIllegalTypes & movePt) && (AttackRiderTypes[movePt] & HOPPING_RIDERS))
  {
      Bitboard between = between_bb(from, to);
      Bitboard hopIllegalPieces = 0;
      for (PieceSet ps = var->mutuallyHopIllegalTypes; ps;)
          hopIllegalPieces |= pieces(pop_lsb(ps));
      if ((between & pieces()) && ((between | to) & hopIllegalPieces))
          return false;
  }

  // Evasions generator already takes care to avoid some kind of illegal moves
  // and legal() relies on this. We therefore have to take care that the same
  // kind of moves are filtered out here.
  if (!allow_checks() && checkers() && !(checkers() & non_sliding_riders()))
  {
      if (type_of(pc) != KING)
      {
          if (topology_wraps())
          {
              Bitboard occupied = (pieces() ^ from) | to;
              if (attackers_to_king(square<KING>(us), occupied, ~us) & ~removedAttackers)
                  return false;
          }
          else
          {
          auto checker_targets = [&](Square checksq) {
              PieceType checkerPt = type_of(piece_on(checksq));
              Bitboard checkerMask = square_bb(checksq);
              Bitboard t = (AttackRiderTypes[checkerPt] & RIDER_ROSE)
                         ? rose_between_intersection_bb(square<KING>(us), checksq, pieces())
                         : between_bb(square<KING>(us), checksq, checkerPt);

              bool blockableNightrider = AttackRiderTypes[checkerPt] & RIDER_NIGHTRIDER;
              if ((checkerMask & non_sliding_riders()) && !blockableNightrider)
                  t = ~pieces(us);
              if (LeaperAttacks[~us][checkerPt][checksq] & square<KING>(us))
                  t = checkerMask;
              return t;
          };

          Bitboard evasionTargets = AllSquares;
          Bitboard remaining = checkers();
          while (remaining)
              evasionTargets &= checker_targets(pop_lsb(remaining));

          if (!(evasionTargets & to))
              return false;
          }
      }
      // In case of king moves under check we have to remove king so as to catch
      // invalid moves like b1a1 when opposite queen is on c1.
      else if (attackers_to_king(to, pieces() ^ from, ~us))
          return false;
  }

  return !violates_same_player_board_repetition(m);
}


/// Position::gives_check() tests whether a pseudo-legal move gives a check

bool Position::gives_check(Move m) const {

  assert(is_ok(m));
  assert(color_of(moved_piece(m)) == sideToMove);

  Square from = from_sq(m);
  Square to = to_sq(m);

  Bitboard freezeExtra = 0;
  Bitboard jumpRemoved = 0;
  Variant::PotionType gatingPotion = Variant::POTION_TYPE_NB;
  if (is_gating(m))
  {
      gatingPotion = potion_type_from_piece(var, gating_type(m));
      if (gatingPotion != Variant::POTION_TYPE_NB)
      {
          if (!can_cast_potion(sideToMove, gatingPotion))
              return false;
          if (gatingPotion == Variant::POTION_FREEZE)
              freezeExtra = freeze_zone_from_square(gating_square(m));
          else if (gatingPotion == Variant::POTION_JUMP)
          {
              jumpRemoved = square_bb(gating_square(m));
              if (!piece_on(gating_square(m)))
                  return false;
          }
      }
  }

  SpellContextScope spellScope(*this, freezeExtra, jumpRemoved);

  if (type_of(m) != DROP && (freeze_squares() & from))
      return false;
  if (jumpRemoved && (square_bb(to) & jumpRemoved))
      return false;

  bool rifleShot = rifle_capture(m) && capture(m) && type_of(m) != CASTLING;
  Square shotSq = capture(m) ? capture_square(m) : to;
  Square attackFrom = rifleShot ? from : to;

  // No check possible without king
  if (!count<KING>(~sideToMove))
      return false;

  Bitboard occupied = rifleShot ? (pieces() ^ square_bb(shotSq))
                                : (((type_of(m) != DROP ? pieces() ^ from : pieces()) ^ square_bb(shotSq)) | to);
  Bitboard janggiCannons = pieces(JANGGI_CANNON);
  if (type_of(moved_piece(m)) == JANGGI_CANNON)
      janggiCannons = rifleShot ? (janggiCannons & ~square_bb(shotSq))
                                : ((type_of(m) == DROP ? janggiCannons : janggiCannons ^ from) | to);
  else if (janggiCannons & to)
      janggiCannons ^= to;

  if (topology_wraps())
  {
      Position* pos = const_cast<Position*>(this);
      StateInfo nextState;
      pos->do_move(m, nextState, false);
      bool givesCheck = pos->count<KING>(pos->side_to_move())
                     && bool(pos->attackers_to_king(pos->square<KING>(pos->side_to_move()), ~pos->side_to_move()));
      pos->undo_move(m);
      return givesCheck;
  }

  // Is there a direct check?
  if (type_of(m) != PROMOTION && type_of(m) != PIECE_PROMOTION && type_of(m) != PIECE_DEMOTION && type_of(m) != CASTLING
      && !((var->petrifyOnCaptureTypes & type_of(moved_piece(m))) && capture(m)))
  {
      PieceType pt = type_of(moved_piece(m));
      if (!(var->captureForbidden[pt] & KING))
      {
          if (pt == JANGGI_CANNON)
          {
              if (attacks_bb(sideToMove, pt, attackFrom, occupied) & attacks_bb(sideToMove, pt, attackFrom, occupied & ~janggiCannons) & square<KING>(~sideToMove))
                  return true;
          }
          else if (AttackRiderTypes[pt] & HOPPING_RIDERS)
          {
              if (attacks_bb(sideToMove, pt, attackFrom, occupied) & square<KING>(~sideToMove))
                  return true;
          }
          else if (AttackRiderTypes[pt] & ASYMMETRICAL_RIDERS)
          {
              if ((check_squares(pt) & attackFrom) && (attacks_bb(sideToMove, pt, attackFrom, occupied) & square<KING>(~sideToMove)))
                  return true;
          }
          else if (check_squares(pt) & attackFrom)
              return true;
      }
  }

  // Is there a discovered check?
  if (  ((type_of(m) != DROP && (blockers_for_king(~sideToMove) & (rifleShot ? square_bb(to) : square_bb(from)))) || (non_sliding_riders() & pieces(sideToMove)))
      && attackers_to_king(square<KING>(~sideToMove), occupied, sideToMove, janggiCannons) & occupied)
      return true;

  // Is there a check by gated pieces?
  if (    is_gating(m)
      && gatingPotion == Variant::POTION_TYPE_NB
      && attacks_bb(sideToMove, gating_type(m), gating_square(m), (pieces() ^ from) | to) & square<KING>(~sideToMove))
      return true;

  // Petrified piece can't give check
  if ((var->petrifyOnCaptureTypes & type_of(moved_piece(m))) && capture(m))
      return false;

  // Is there a check by special diagonal moves?
  if (more_than_one(diagonal_lines() & (to | square<KING>(~sideToMove))))
  {
      PieceType pt = type_of(moved_piece(m));
      PieceType diagType = pt == WAZIR ? FERS : pt == SOLDIER ? PAWN : pt == ROOK ? BISHOP : NO_PIECE_TYPE;
      if (diagType && (attacks_bb(sideToMove, diagType, attackFrom, occupied) & square<KING>(~sideToMove)))
          return true;
      else if (pt == JANGGI_CANNON && (  rider_attacks_bb<RIDER_CANNON_DIAG>(attackFrom, occupied)
                                       & rider_attacks_bb<RIDER_CANNON_DIAG>(attackFrom, occupied & ~janggiCannons)
                                       & square<KING>(~sideToMove)))
          return true;
  }

  switch (type_of(m))
  {
  case NORMAL:
  case DROP:
  case SPECIAL:
      return false;

  case PROMOTION:
      return attacks_bb(sideToMove, promotion_type(m), to, pieces() ^ from) & square<KING>(~sideToMove);

  case PIECE_PROMOTION:
      return attacks_bb(sideToMove, promoted_piece_type(type_of(moved_piece(m))), to, pieces() ^ from) & square<KING>(~sideToMove);

  case PIECE_DEMOTION:
      return attacks_bb(sideToMove, type_of(unpromoted_piece_on(from)), to, pieces() ^ from) & square<KING>(~sideToMove);

  // En passant capture with check? We have already handled the case
  // of direct checks and ordinary discovered check, so the only case we
  // need to handle is the unusual case of a discovered check through
  // the captured pawn.
  case EN_PASSANT:
  {
      Square capsq = capture_square(to);
      Bitboard b = rifleShot ? (pieces() ^ capsq) : ((pieces() ^ from ^ capsq) | to);

      return attackers_to_king(square<KING>(~sideToMove), b, sideToMove) & pieces(sideToMove) & b;
  }
  default: //CASTLING
  {
      // Castling is encoded as 'king captures the rook'
      Square kfrom = from;
      Square rfrom = to;
      Square kto = make_square(rfrom > kfrom ? castling_kingside_file() : castling_queenside_file(), castling_rank(sideToMove));
      Square rto = kto + (rfrom > kfrom ? WEST : EAST);

      // Is there a discovered check?
      if (   castling_rank(WHITE) > RANK_1
          && ((blockers_for_king(~sideToMove) & rfrom) || (non_sliding_riders() & pieces(sideToMove)))
          && attackers_to_king(square<KING>(~sideToMove), (pieces() ^ kfrom ^ rfrom) | rto | kto, sideToMove))
          return true;

      return   (PseudoAttacks[sideToMove][type_of(piece_on(rfrom))][rto] & square<KING>(~sideToMove))
            && (attacks_bb(sideToMove, type_of(piece_on(rfrom)), rto, (pieces() ^ kfrom ^ rfrom) | rto | kto) & square<KING>(~sideToMove));
  }
  }
}

/// Position::do_move() makes a move, and saves all information necessary
/// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
/// moves should be filtered out before this function is called.

void Position::do_move(Move m, StateInfo& newSt, bool givesCheck) {

  assert(is_ok(m));
  assert(&newSt != st);

#ifndef NO_THREADS
  thisThread->nodes.fetch_add(1, std::memory_order_relaxed);
#endif
  Key k = st->key ^ Zobrist::side;

  // Copy some fields of the old state to our new StateInfo object except the
  // ones which are going to be recalculated from scratch anyway and then switch
  // our state pointer to point to the new (ready to be updated) state.
  std::memcpy(static_cast<void*>(&newSt), static_cast<void*>(st), offsetof(StateInfo, key));
  newSt.previous = st;
  st = &newSt;
  st->move = m;
  st->legalCapture = NO_VALUE;
  st->legalEnPassant = NO_VALUE;
  st->blastPromotedSquares = 0;
  st->bycatchSquares = 0;
  st->consumedPromotionHandPiece = NO_PIECE;
  st->deadPiece = NO_PIECE;
  st->deadUnpromotedPiece = NO_PIECE;
  st->deadPiecePromoted = false;

  if (commit_gates()) {
      st->removedGatingType = NO_PIECE_TYPE;
      st->removedCastlingGatingType = NO_PIECE_TYPE;
      st->capturedGatingType = NO_PIECE_TYPE;
  }
  st->didMorph = false;
  st->morphedFrom = NO_PIECE;
  st->morphSquare = SQ_NONE;
  st->didColorChange = false;
  st->colorChangedFrom = NO_PIECE;
  st->colorChangeSquare = SQ_NONE;
  st->colorChangedPromoted = false;
  st->colorChangedUnpromoted = NO_PIECE;
  // Mandatory multimove pass plies should not advance the halfmove clock.
  const bool currentMultimovePass = is_pass(m) && multimove_pass(gamePly);

  // Increment ply counters. In particular, rule50 will be reset to zero later on
  // in case of a capture or a pawn move.
  ++gamePly;
  if (!currentMultimovePass)
      ++st->rule50;
  ++st->pliesFromNull;
  if (st->countingLimit)
      ++st->countingPly;

  // Used by NNUE
  st->accumulator.computed[WHITE] = false;
  st->accumulator.computed[BLACK] = false;
  st->nnueRefreshNeeded = false;
  auto& dp = st->dirtyPiece;
  dp.dirty_num = 1;

  Color us = sideToMove;
  Color them = ~us;
  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);
  PieceType movedType = type_of(pc);
  Piece captured = captured_piece(m);
  bool rifleShot = rifle_capture(m) && captured != NO_PIECE && type_of(m) != CASTLING;
  bool capturedDeadSquare = type_of(m) != DROP && from != to && bool(st->deadSquares & to);
  PieceType exchanged = exchange_piece(m);
  Square jumpCapsq = is_jump_capture(m) ? jump_capture_square(from, to) : SQ_NONE;
  Square moverSq = rifleShot ? from : to;
  bool openingSelfRemoval = in_opening_self_removal_phase() && is_opening_self_removal_move(m);
  if (to == from)
  {
      assert((type_of(m) == PROMOTION && sittuyin_promotion()) || is_pass(m) || openingSelfRemoval);
      captured = NO_PIECE;
  }
  Square capturedSq = captured ? capture_square(m) : SQ_NONE;
  st->capturedpromoted = captured ? is_promoted(capturedSq) : false;
  st->unpromotedCapturedPiece = captured ? unpromoted_piece_on(capturedSq) : NO_PIECE;
  st->captureSquare = capturedSq;
  st->pass = is_pass(m) && !openingSelfRemoval;
  st->suppressedCaptureTransfer = false;

  Variant::PotionType gatingPotion = Variant::POTION_TYPE_NB;
  Bitboard freezeExtra = 0;
  Bitboard jumpRemoved = 0;
  if (is_gating(m))
  {
      gatingPotion = potion_type_from_piece(var, gating_type(m));
      if (gatingPotion == Variant::POTION_FREEZE)
          freezeExtra = freeze_zone_from_square(gating_square(m));
      else if (gatingPotion == Variant::POTION_JUMP)
          jumpRemoved = square_bb(gating_square(m));
  }

  SpellContextScope spellScope(*this, freezeExtra, jumpRemoved);

  assert(color_of(pc) == us);
  assert(captured == NO_PIECE
         || (type_of(m) == CASTLING ? color_of(captured) == us
                                    : (color_of(captured) == them
                                       || (self_capture() && color_of(captured) == us))));
  assert(type_of(captured) != KING || allow_checks());

  auto trigger_matches = [](ColorChangeTrigger trigger, bool isCapture) {
      switch (trigger)
      {
      case ColorChangeTrigger::NEVER:
          return false;
      case ColorChangeTrigger::ON_CAPTURE:
          return isCapture;
      case ColorChangeTrigger::ON_NON_CAPTURE:
          return !isCapture;
      case ColorChangeTrigger::ALWAYS:
          return true;
      }
      return false;
  };

  if (type_of(m) == CASTLING)
  {
      assert(type_of(pc) != NO_PIECE_TYPE);
      assert(castling_rook_pieces(us) & type_of(captured));

      Square rfrom, rto;
      do_castling<true>(us, from, to, rfrom, rto);

      k ^= Zobrist::psq[captured][rfrom] ^ Zobrist::psq[captured][rto];
      captured = NO_PIECE;

      if (commit_gates() && st->removedCastlingGatingType > NO_PIECE_TYPE)
      {
          xor_committed_gate(k, us, file_of(rfrom), st->removedCastlingGatingType);
          Piece dropped = make_piece(us, st->removedCastlingGatingType);
          Square gateSq = make_square(file_of(rfrom), us == WHITE ? RANK_1 : max_rank());
          k ^= Zobrist::psq[dropped][gateSq];
          st->materialKey ^= Zobrist::psq[dropped][pieceCount[dropped] - 1];
          if (type_of(dropped) == PAWN)
              st->pawnKey ^= Zobrist::psq[dropped][gateSq];
          else
              st->nonPawnMaterial[us] += PieceValue[MG][dropped];

          if (Eval::useNNUE)
          {
              append_dirty(st, dropped, SQ_NONE, gateSq);
          }
      }
  }

  if (captured)
  {
      st->suppressedCaptureTransfer = var->petrifyOnCaptureSuppressTransfer
                                   && bool(var->petrifyOnCaptureTypes & type_of(pc));

      Square capsq = to;
      if (jumpCapsq != SQ_NONE)
          capsq = jumpCapsq;

      if (type_of(m) == EN_PASSANT)
      {
          capsq = capture_square(to);
          st->captureSquare = capsq;

          assert(st->epSquares & to);
          assert(var->enPassantRegion[us] & to);
          assert(piece_on(to) == NO_PIECE);
      }

      // If the captured piece is a pawn, update pawn hash key, otherwise
      // update non-pawn material.
      if (type_of(captured) == PAWN)
          st->pawnKey ^= Zobrist::psq[captured][capsq];
      else
          st->nonPawnMaterial[color_of(captured)] -= PieceValue[MG][captured];

      if (Eval::useNNUE)
      {
          if (rifleShot)
          {
              dp.dirty_num = 1;
              dp.piece[0] = captured;
              dp.from[0] = capsq;
              dp.to[0] = SQ_NONE;
          }
          else
          {
              dp.dirty_num = 2;  // 1 piece moved, 1 piece captured
              dp.piece[1] = captured;
              dp.from[1] = capsq;
              dp.to[1] = SQ_NONE;
          }
      }

      // Update board and piece lists
      bool capturedPromoted = is_promoted(capsq);
      Piece unpromotedCaptured = unpromoted_piece_on(capsq);
      remove_piece(capsq);

      if (type_of(m) == EN_PASSANT)
          board[capsq] = NO_PIECE;
      if (capture_type() == HAND && !st->suppressedCaptureTransfer)
      {
          Piece pieceToHand = !capturedPromoted || drop_loop()
                             ? make_piece(us, type_of(captured))
                             : unpromotedCaptured ? make_piece(us, type_of(unpromotedCaptured))
                                                  : make_piece(us, main_promotion_pawn_type(color_of(captured)));
          add_to_hand(pieceToHand);
          int newN = pieceCountInHand[color_of(pieceToHand)][type_of(pieceToHand)];
          xor_in_hand_count(k, pieceToHand, newN - 1, newN);

          if (Eval::useNNUE)
          {
              dp.handPiece[1] = pieceToHand;
              dp.handCount[1] = pieceCountInHand[color_of(pieceToHand)][type_of(pieceToHand)];
          }
      }
      else if (capture_type() == PRISON && !st->suppressedCaptureTransfer)
      {
          Piece pieceToPrison = !capturedPromoted || drop_loop()
                  ? captured
                  : unpromotedCaptured
                      ? unpromotedCaptured
                      : make_piece(color_of(captured), main_promotion_pawn_type(color_of(captured)));
          int n = add_to_prison(pieceToPrison);
          xor_in_hand_count(k, pieceToPrison, n - 1, n);
      }
      else if (Eval::useNNUE)
          dp.handPiece[1] = NO_PIECE;

      // Points assignment logic
      if (points_counting()) {
          PointsRule pointsOwner = points_rule_captures();
          int points = var->piecePoints[type_of(captured)];

          switch (pointsOwner) {
              case POINTS_US:
                  st->pointsCount[us] += points;
                  break;
              case POINTS_THEM:
                  st->pointsCount[them] += points;
                  break;
              case POINTS_OWNER:
                  st->pointsCount[color_of(captured)] += points;
                  break;
              case POINTS_NON_OWNER:
                  st->pointsCount[~color_of(captured)] += points;
                  break;
              case POINTS_NONE:
                  break;
          }
      }

      // Update material hash key and prefetch access to materialTable
      k ^= Zobrist::psq[captured][capsq];
      // (captured piece count was decremented before this line; old count is pieceCount[captured] + 1)
      st->materialKey ^= Zobrist::psq[captured][pieceCount[captured]];
#ifndef NO_THREADS
      prefetch(thisThread->materialTable[material_key(endgame_eval())]);
#endif
      // Reset rule 50 counter
      st->rule50 = 0;
  }

  if (capturedDeadSquare)
  {
      st->deadSquares ^= to;
      byTypeBB[ALL_PIECES] ^= to;
      k ^= Zobrist::dead[to];
      st->rule50 = 0;
  }

  // Update hash key
  if (type_of(m) == DROP)
  {
      Piece pc_hand = make_piece(us, in_hand_piece_type(m));
      k ^= Zobrist::psq[pc][to];
      if (exchanged == NO_PIECE_TYPE)
      {
          int n = pieceCountInHand[color_of(pc_hand)][type_of(pc_hand)];
          xor_in_hand_count(k, pc_hand, n - 1, n);
      }
      else
      {
          Piece exchangedPiece = make_piece(them, exchanged);

          // Exchange drop mutates one hand bucket and two prison buckets.
          int handOld = pieceCountInHand[them][exchanged];
          int handNew = handOld + 1;
          xor_in_hand_count(k, exchangedPiece, handOld, handNew);

          int prisonOldEx = pieceCountInPrison[us][exchanged];
          int prisonNewEx = prisonOldEx - 1;
          xor_in_hand_count(k, exchangedPiece, prisonOldEx, prisonNewEx);

          int prisonOldDrop = pieceCountInPrison[them][type_of(pc)];
          int prisonNewDrop = prisonOldDrop - 1;
          xor_in_hand_count(k, pc, prisonOldDrop, prisonNewDrop);
      }

      // Reset rule 50 counter for irreversible drops
      st->rule50 = 0;
  }
  else
  {
      k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

      // Reset rule 50 draw counter for irreversible moves
      // - irreversible pawn/piece promotions
      // - irreversible pawn moves
      if (    type_of(m) == PROMOTION
          || (type_of(m) == PIECE_PROMOTION && !piece_demotion())
          || (    (var->nMoveRuleTypes[us] & type_of(pc))
              && !(PseudoMoves[0][us][type_of(pc)][to] & from)))
          st->rule50 = 0;
  }

  // Reset en passant squares
  while (st->epSquares)
      k ^= Zobrist::enpassant[pop_lsb(st->epSquares)];

  // Update castling rights if needed
  if (type_of(m) != DROP && !is_pass(m) && st->castlingRights && (castlingRightsMask[from] | castlingRightsMask[to]))
  {
      k ^= Zobrist::castling[st->castlingRights];
      st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);

      // Remove castling rights from opponent on the same side if oppositeCastling
      if ((var->oppositeCastling) && (type_of(m) == CASTLING))
      {
        bool kingSide = to > from;
        st->castlingRights &= ~(~us & (kingSide ? KING_SIDE : QUEEN_SIDE));
      }
      k ^= Zobrist::castling[st->castlingRights];
  }

  // Flip enclosed pieces
  st->flippedPieces = 0;
  if (flip_enclosed_pieces() && !is_pass(m))
  {
      // Find end of rows to be flipped
      if (flip_enclosed_pieces() == REVERSI)
      {
          Bitboard b = attacks_bb(us, QUEEN, to, ~pieces(~us)) & ~PseudoAttacks[us][KING][to] & pieces(us);
          while(b)
              st->flippedPieces |= between_bb(pop_lsb(b), to) ^ to;
      }
      else
      {
          assert((flip_enclosed_pieces() == ATAXX) || (flip_enclosed_pieces() == QUADWRANGLE));
          if ((flip_enclosed_pieces() == ATAXX) || (flip_enclosed_pieces() == QUADWRANGLE && (PseudoAttacks[us][KING][to] & pieces(us) || type_of(m) == NORMAL)))
          {
              st->flippedPieces = PseudoAttacks[us][KING][to] & pieces(~us);
          }
      }

      // Flip pieces
      Bitboard to_flip = st->flippedPieces;
      while(to_flip)
      {
          Square s = pop_lsb(to_flip);
          Piece flipped = piece_on(s);
          Piece resulting = ~flipped;

          // remove opponent's piece
          remove_piece(s);
          k ^= Zobrist::psq[flipped][s];
          st->materialKey ^= Zobrist::psq[flipped][pieceCount[flipped]];
          st->nonPawnMaterial[them] -= PieceValue[MG][flipped];

          // add our piece
          put_piece(resulting, s);
          k ^= Zobrist::psq[resulting][s];
          st->materialKey ^= Zobrist::psq[resulting][pieceCount[resulting]-1];
          st->nonPawnMaterial[us] += PieceValue[MG][resulting];
      }
  }

  // Move the piece. The tricky Chess960 castling is handled earlier
  if (type_of(m) == DROP)
  {
      if (pay_points_to_drop())
      {
          st->pointsCount[us] -= var->piecePoints[type_of(pc)];
          st->pointsCount[us] = non_negative_points(st->pointsCount[us]);
      }

      if (Eval::useNNUE)
      {
          // Add drop piece
          dp.piece[0] = pc;
          dp.handPiece[0] = make_piece(us, in_hand_piece_type(m));
          dp.handCount[0] = pieceCountInHand[us][in_hand_piece_type(m)];
          dp.from[0] = SQ_NONE;
          dp.to[0] = to;

          // Exchange drops also add a piece to the opponent's hand without any board
          // move to pair with it. DirtyPiece cannot represent that standalone hand
          // delta, so force a full NNUE refresh for correctness.
          if (exchanged != NO_PIECE_TYPE)
              st->nnueRefreshNeeded = true;
      }

      drop_piece(make_piece(us, in_hand_piece_type(m)), pc, to, exchanged);
      st->materialKey ^= Zobrist::psq[pc][pieceCount[pc]-1];
      if (type_of(pc) != PAWN)
          st->nonPawnMaterial[us] += PieceValue[MG][pc];
      // Set castling rights for dropped king or rook
      if (castling_dropped_piece() && rank_of(to) == castling_rank(us))
      {
          if (type_of(pc) == castling_king_piece(us) && file_of(to) == castling_king_file())
          {
              st->castlingKingSquare[us] = to;
              Bitboard castling_rooks =  pieces(us)
                                       & rank_bb(castling_rank(us))
                                       & (file_bb(FILE_A) | file_bb(max_file()));
              while (castling_rooks)
              {
                  Square s = pop_lsb(castling_rooks);
                  if (castling_rook_pieces(us) & type_of(piece_on(s)))
                      set_castling_right(us, s);
              }
          }
          else if (castling_rook_pieces(us) & type_of(pc))
          {
              if (   (file_of(to) == FILE_A || file_of(to) == max_file())
                  && piece_on(make_square(castling_king_file(), castling_rank(us))) == make_piece(us, castling_king_piece(us)))
              {
                  st->castlingKingSquare[us] = make_square(castling_king_file(), castling_rank(us));
                  set_castling_right(us, to);
              }
          }
      }
  }
  else if (openingSelfRemoval)
  {
      st->deadPiece = pc;
      st->deadPiecePromoted = is_promoted(from);
      st->deadUnpromotedPiece = st->deadPiecePromoted ? unpromoted_piece_on(from) : NO_PIECE;

      if (Eval::useNNUE)
      {
          dp.piece[0] = pc;
          dp.from[0] = from;
          dp.to[0] = SQ_NONE;
      }

      remove_piece(from);
      board[from] = NO_PIECE;
      k ^= Zobrist::psq[pc][from];
      st->materialKey ^= Zobrist::psq[pc][pieceCount[pc]];
      if (type_of(pc) == PAWN)
          st->pawnKey ^= Zobrist::psq[pc][from];
      else
          st->nonPawnMaterial[us] -= PieceValue[MG][pc];
      st->rule50 = 0;
  }
  else if (type_of(m) != CASTLING)
  {
      if (Eval::useNNUE)
      {
          if (rifleShot)
              dp.dirty_num = std::min(dp.dirty_num, 1);
          else
          {
              dp.piece[0] = pc;
              dp.from[0] = from;
              dp.to[0] = to;
          }
      }

      if (!rifleShot)
          move_piece(from, to);
  }

  // If the moving piece is a pawn do some special extra work
  if (type_of(pc) == PAWN)
  {
      if (type_of(m) == PROMOTION || type_of(m) == PIECE_PROMOTION)
      {
          Piece promotion = make_piece(us, type_of(m) == PROMOTION ? promotion_type(m) : promoted_piece_type(PAWN));
          Piece promotedHandPiece = make_piece(us, type_of(promotion));

          assert((promotion_zone(pc) & to) || sittuyin_promotion());
          assert(type_of(promotion) >= KNIGHT && type_of(promotion) < KING);

          st->promotionPawn = piece_on(to);
          remove_piece(to);
          // Preserve exact source piece for variants with multiple promotion pawn types.
          put_piece(promotion, to, true, pc);
          if (prison_pawn_promotion() && type_of(m) == PROMOTION) {
              int addedN = add_to_prison(st->promotionPawn);
              int removedN = remove_from_prison(promotion);
              // Keep prison inventory hash in sync with promotion swap.
              xor_in_hand_count(k, st->promotionPawn, addedN - 1, addedN);
              xor_in_hand_count(k, promotion, removedN + 1, removedN);
          }

          int promoDirtyIdx = -1;
          if (Eval::useNNUE)
          {
              // Promoting pawn to SQ_NONE, promoted piece from SQ_NONE
              dp.to[0] = SQ_NONE;
              dp.handPiece[0] = NO_PIECE;
              promoDirtyIdx = append_dirty(st, promotion, SQ_NONE, to);
          }
          if (var->promotionConsumeInHand)
          {
              remove_from_hand(promotedHandPiece);
              int newN = pieceCountInHand[us][type_of(promotedHandPiece)];
              xor_in_hand_count(k, promotedHandPiece, newN + 1, newN);
              st->consumedPromotionHandPiece = promotedHandPiece;
              if (Eval::useNNUE && promoDirtyIdx >= 0)
              {
                  dp.handPiece[promoDirtyIdx] = promotedHandPiece;
                  dp.handCount[promoDirtyIdx] = newN;
              }
          }

          // Update hash keys
          k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[promotion][to];
          st->pawnKey ^= Zobrist::psq[pc][to];
          st->materialKey ^=  Zobrist::psq[promotion][pieceCount[promotion]-1]
                            ^ Zobrist::psq[pc][pieceCount[pc]];

          // Update material
          st->nonPawnMaterial[us] += PieceValue[MG][promotion];
      }

      // Set en passant square(s) if the moved pawn can be captured
      else if (   !topology_wraps()
          && type_of(m) != DROP
          && (   std::abs(int(to) - int(from)) == 2 * NORTH
              || std::abs(int(to) - int(from)) == 3 * NORTH))
      {
          if (   (var->enPassantRegion[them] & (to - pawn_push(us)))
              && ((pawn_attacks_bb(us, to - pawn_push(us)) & pieces(them, PAWN)) || (var->enPassantTypes[them] & ~piece_set(PAWN)))
              && !(walling(us) && gating_square(m) == to - pawn_push(us)))
          {
              st->epSquares |= to - pawn_push(us);
              k ^= Zobrist::enpassant[to - pawn_push(us)];
          }
          if (   std::abs(int(to) - int(from)) == 3 * NORTH
              && (var->enPassantRegion[them] & (to - 2 * pawn_push(us)))
              && ((pawn_attacks_bb(us, to - 2 * pawn_push(us)) & pieces(them, PAWN)) || (var->enPassantTypes[them] & ~piece_set(PAWN)))
              && !(walling(us) && gating_square(m) == to - 2 * pawn_push(us)))
          {
              st->epSquares |= to - 2 * pawn_push(us);
              k ^= Zobrist::enpassant[to - 2 * pawn_push(us)];
          }
      }
      else if (type_of(m) != DROP && topology_wraps())
      {
          auto stepped = [&](Square start, int count) {
              Square s = start;
              int forwardDr = us == WHITE ? 1 : -1;
              for (int i = 0; i < count; ++i)
              {
                  Square next;
                  if (!wrapped_destination_square(s, 0, forwardDr, max_file(), max_rank(), wraps_files(), wraps_ranks(), next))
                      break;
                  s = next;
              }
              return s;
          };

          auto maybe_add_ep = [&](Square epSq) {
              if (   epSq != SQ_NONE
                  && (var->enPassantRegion[them] & epSq)
                  && ((attacks_from(us, PAWN, epSq) & pieces(them, PAWN)) || (var->enPassantTypes[them] & ~piece_set(PAWN)))
                  && !(walling(us) && gating_square(m) == epSq))
              {
                  st->epSquares |= epSq;
                  k ^= Zobrist::enpassant[epSq];
              }
          };

          Square step1 = stepped(from, 1);
          Square step2 = stepped(from, 2);
          Square step3 = stepped(from, 3);

          if (to == step2)
              maybe_add_ep(step1);
          else if (to == step3)
          {
              maybe_add_ep(step1);
              maybe_add_ep(step2);
          }
      }

      // Update pawn hash key
      st->pawnKey ^= (type_of(m) != DROP ? Zobrist::psq[pc][from] : 0) ^ Zobrist::psq[pc][to];
  }
  else if (type_of(m) == PROMOTION || type_of(m) == PIECE_PROMOTION)
  {
      Piece promotion = make_piece(us, type_of(m) == PROMOTION ? promotion_type(m) : promoted_piece_type(type_of(pc)));
      Piece promotedHandPiece = make_piece(us, type_of(promotion));

      st->promotionPawn = piece_on(to);
      remove_piece(to);
      // Preserve exact source piece for variants with multiple promotion sources.
      put_piece(promotion, to, true, pc);

      int promoDirtyIdx = -1;
      if (Eval::useNNUE)
      {
          // Promoting piece to SQ_NONE, promoted piece from SQ_NONE
          dp.to[0] = SQ_NONE;
          dp.handPiece[0] = NO_PIECE;
          promoDirtyIdx = append_dirty(st, promotion, SQ_NONE, to);
      }
      if (var->promotionConsumeInHand)
      {
          remove_from_hand(promotedHandPiece);
          int newN = pieceCountInHand[us][type_of(promotedHandPiece)];
          xor_in_hand_count(k, promotedHandPiece, newN + 1, newN);
          st->consumedPromotionHandPiece = promotedHandPiece;
          if (Eval::useNNUE && promoDirtyIdx >= 0)
          {
              dp.handPiece[promoDirtyIdx] = promotedHandPiece;
              dp.handCount[promoDirtyIdx] = newN;
          }
      }

      // Update hash keys
      k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[promotion][to];
      st->materialKey ^=  Zobrist::psq[promotion][pieceCount[promotion]-1]
                        ^ Zobrist::psq[pc][pieceCount[pc]];

      // Update material
      st->nonPawnMaterial[us] += PieceValue[MG][promotion] - PieceValue[MG][pc];
  }
  else if (type_of(m) == PIECE_DEMOTION)
  {
      Piece demotion = unpromoted_piece_on(to);

      remove_piece(to);
      put_piece(demotion, to);

      if (Eval::useNNUE)
      {
          // Demoting piece to SQ_NONE, demoted piece from SQ_NONE
          dp.to[0] = SQ_NONE;
          dp.handPiece[0] = NO_PIECE;
          append_dirty(st, demotion, SQ_NONE, to);
      }

      // Update hash keys
      k ^= Zobrist::psq[pc][to] ^ Zobrist::psq[demotion][to];
      st->materialKey ^=  Zobrist::psq[demotion][pieceCount[demotion]-1]
                        ^ Zobrist::psq[pc][pieceCount[pc]];

      // Update material
      st->nonPawnMaterial[us] += PieceValue[MG][demotion] - PieceValue[MG][pc];
  }
  // Set en passant square(s) if the moved piece can be captured
  else if (   type_of(m) != DROP
           && ((PseudoMoves[1][us][type_of(pc)][from] & ~PseudoMoves[0][us][type_of(pc)][from]) & to))
  {
      assert(type_of(pc) != PAWN);
      st->epSquares = between_bb(from, to) & var->enPassantRegion[them];
      if (st->epSquares)
      {
          switch (var->enPassantPassedSquares)
          {
          case EnPassantPassedSquares::ALL:
              break;
          case EnPassantPassedSquares::FIRST:
              st->epSquares = square_bb(us == WHITE ? lsb(st->epSquares) : msb(st->epSquares));
              break;
          case EnPassantPassedSquares::LAST:
              st->epSquares = square_bb(us == WHITE ? msb(st->epSquares) : lsb(st->epSquares));
              break;
          }
      }
      for (Bitboard b = st->epSquares; b; )
          k ^= Zobrist::enpassant[pop_lsb(b)];
  }

  // Benedict Morph: capturer changes type to the captured piece type.
  // Keep promotion moves unchanged to preserve current promotion undo semantics.
  if (   capture_morph()
      && captured != NO_PIECE
      && type_of(m) != DROP
      && type_of(m) != CASTLING
      && type_of(m) != PROMOTION
      && type_of(m) != PIECE_PROMOTION
      && !is_pass(m))
  {
      Piece cur = piece_on(moverSq);
      if (cur != NO_PIECE && !(rex_exclusive_morph() && type_of(cur) == KING))
      {
          Piece morphed = make_piece(color_of(cur), type_of(captured));
          if (morphed != cur)
          {
              st->didMorph = true;
              st->morphedFrom = cur;
              st->morphSquare = moverSq;

              remove_piece(moverSq);
              put_piece(morphed, moverSq);

              k ^= Zobrist::psq[cur][moverSq] ^ Zobrist::psq[morphed][moverSq];
              st->materialKey ^= Zobrist::psq[cur][pieceCount[cur]]
                               ^ Zobrist::psq[morphed][pieceCount[morphed] - 1];
              if (type_of(cur) == PAWN)
                  st->pawnKey ^= Zobrist::psq[cur][moverSq];
              if (type_of(morphed) == PAWN)
                  st->pawnKey ^= Zobrist::psq[morphed][moverSq];
              if (type_of(cur) != PAWN)
                  st->nonPawnMaterial[us] -= PieceValue[MG][cur];
              if (type_of(morphed) != PAWN)
                  st->nonPawnMaterial[us] += PieceValue[MG][morphed];

              if (Eval::useNNUE)
              {
                  append_dirty(st, cur, moverSq, SQ_NONE);
                  append_dirty(st, morphed, SQ_NONE, moverSq);
              }
          }
      }
  }

  // Set capture piece
  st->capturedPiece = captured;

  // Add gating piece
  if (is_gating(m) && !rifleShot)
  {
      Square gate = gating_square(m);
      Piece gating_piece = make_piece(us, gating_type(m));

      if (gatingPotion != Variant::POTION_TYPE_NB)
      {
          int oldCount = pieceCountInHand[us][gating_type(m)];
          remove_from_hand(gating_piece);
          int newCount = pieceCountInHand[us][gating_type(m)];
          xor_in_hand_count(k, gating_piece, oldCount, newCount);

          if (Eval::useNNUE)
              append_dirty(st, gating_piece, SQ_NONE, SQ_NONE, gating_piece, pieceCountInHand[us][gating_type(m)]);
      }
      else
      {
          if (Eval::useNNUE)
              // Add gating piece
              append_dirty(st, gating_piece, SQ_NONE, gate, gating_piece, pieceCountInHand[us][gating_type(m)]);

          put_piece(gating_piece, gate);
          int oldCount = pieceCountInHand[us][gating_type(m)];
          remove_from_hand(gating_piece);
          int newCount = pieceCountInHand[us][gating_type(m)];
          xor_in_hand_count(k, gating_piece, oldCount, newCount);

          st->gatesBB[us] ^= gate;
          k ^= Zobrist::psq[gating_piece][gate];
          st->materialKey ^= Zobrist::psq[gating_piece][pieceCount[gating_piece]];
          st->nonPawnMaterial[us] += PieceValue[MG][gating_piece];
      }
  }

  // Musketeer gating
  if(commit_gates() && !rifleShot){
      {
          Rank r = rank_of(from);
          if (us == WHITE && r == RANK_1 && has_committed_piece(WHITE, file_of(from))){
              st->removedGatingType = drop_committed_piece(WHITE, file_of(from));
              xor_committed_gate(k, WHITE, file_of(from), st->removedGatingType);
          } else if (us == BLACK && r == max_rank() && has_committed_piece(BLACK, file_of(from))){
              st->removedGatingType = drop_committed_piece(BLACK, file_of(from));
              xor_committed_gate(k, BLACK, file_of(from), st->removedGatingType);
          }
      }
      if (captured) {
          // remove uncommitted musketeer piece if piece at the front row is captured
          Rank r = rank_of(to);
          if (r == RANK_1 && color_of(captured) == WHITE){
              st->capturedGatingType = uncommit_piece(WHITE, file_of(to));
              xor_committed_gate(k, WHITE, file_of(to), st->capturedGatingType);
          } else if (r == max_rank() && color_of(captured) == BLACK) {
              st->capturedGatingType = uncommit_piece(BLACK, file_of(to));
              xor_committed_gate(k, BLACK, file_of(to), st->capturedGatingType);
          }
      }

      if (st->removedGatingType > NO_PIECE_TYPE)
      {
          Piece dropped = make_piece(us, st->removedGatingType);
          Square gateSq = make_square(file_of(from), us == WHITE ? RANK_1 : max_rank());
          k ^= Zobrist::psq[dropped][gateSq];
          st->materialKey ^= Zobrist::psq[dropped][pieceCount[dropped] - 1];
          if (type_of(dropped) == PAWN)
              st->pawnKey ^= Zobrist::psq[dropped][gateSq];
          else
              st->nonPawnMaterial[us] += PieceValue[MG][dropped];

          if (Eval::useNNUE)
          {
              append_dirty(st, dropped, SQ_NONE, gateSq);
          }
      }
  }
  // Remove gates
  if (gating())
  {
      if (is_ok(from) && (gates(us) & from))
          st->gatesBB[us] ^= from;
      if (type_of(m) == CASTLING && (gates(us) & to_sq(m)))
          st->gatesBB[us] ^= to_sq(m);
      if (gates(them) & to)
          st->gatesBB[them] ^= to;
      if (seirawan_gating() && count_in_hand(us, ALL_PIECES) == 0 && !captures_to_hand())
          st->gatesBB[us] = 0;
  }

  // Remove king leaping right when aimed by a rook
  if (cambodian_moves() && type_of(pc) == ROOK && (square<KING>(them) & gates(them) & attacks_bb<ROOK>(to)))
      st->gatesBB[them] ^= square<KING>(them);

  //resolve blast and custodial capture. custodial capture is essentially blast with extra restrictions
  if (
       (
         ( surround_capture_opposite() || surround_capture_intervene() || surround_capture_edge() ) ||
         ( captured && (blast_on_capture() || var->petrifyOnCaptureTypes) ) ||
         ( blast_on_move() && !captured ) ||
         ( remove_connect_n() > 0 )
       )
       && !is_pass(m)
     )

  {
      Bitboard removal_mask = 0;
      Bitboard blast_mask = 0;
      Bitboard connect_mask = 0;
      std::memset(st->unpromotedBycatch, 0, sizeof(st->unpromotedBycatch));
      st->bycatchSquares = 0;
      st->promotedBycatch = st->demotedBycatch = Bitboard(0);
      st->blastPromotedSquares = 0;

      if ( ( captured && (blast_on_capture() || var->petrifyOnCaptureTypes) ) ||
           ( blast_on_move() && !captured) ) {

          blast_mask = (blast_on_capture() || blast_on_move()) ? blast_squares(captured ? st->captureSquare : to)
              : (var->petrifyOnCaptureTypes & type_of(pc) ? square_bb(moverSq) : Bitboard(0));
          removal_mask |= blast_mask;
      };

      //Use the same removal_mask variable; surround_capture only ORs.
      //A piece could be immune to blast but not immune to custodial.

      if ( surround_capture_opposite() || surround_capture_intervene() || surround_capture_edge() ) {
          for (int sign : {-1, 1}) {

              for (const Direction& d : var->connectDirections)
              //using getConnectDirections to determine whether two pieces are connected
              //in that they can capture a piece between them. if there was a
              //variant with connection as a victory condition, and different
              //directions for surround-capture, yes, we'd have to separate them
              {
                  Direction mod_d = d * sign;
                  Square s = moverSq + mod_d;
                  if (!is_ok(s)) continue;
                  if (!(s&pieces(~us))) continue;
                  Square oppSquare = s + mod_d;

                  if (s & surround_capture_max_region()) {
                      bool surrounded = true;
                      Bitboard b = attacks_bb(us, WAZIR, s, pieces(~us));
                      while(b) {
                          Square s2 = pop_lsb(b);
                          if (!((s2 & surround_capture_hostile_region()) || (s2 & pieces(us)))) {
                              surrounded = false;
                              break;
                          };
                      };
                      if (surrounded) { removal_mask |= s; } else continue;
                  };

                  if (!is_ok(oppSquare)) {
                      if (surround_capture_edge()) removal_mask |= s;
                  }
                  else {
                      if (surround_capture_opposite() && ((pieces(us) & oppSquare) || (surround_capture_hostile_region() & oppSquare))) removal_mask |= s;
                  };
              };
          };
      };

      if (surround_capture_intervene()) {
          for (const Direction& d : var->connectDirections)
          {
              Square s1 = moverSq + d;
              Square s2 = moverSq - d;
              if (!is_ok(s1) || !is_ok(s2))
                  continue;
              if ((s1 & pieces(~us)) && (s2 & pieces(~us)))
                  removal_mask |= s1 | s2;
          }
      }

      if (remove_connect_n() > 0) {
          auto mark_line = [&](Bitboard line) {
              for (Direction d : var->connectDirections) {
                  Bitboard temp = line;
                  for (int i = 1; i < remove_connect_n(); ++i)
                      temp &= shift(d, temp);

                  Bitboard lineStarts = temp;
                  while (lineStarts) {
                      Square start = pop_lsb(lineStarts);
                      for (int i = 0; i < remove_connect_n(); ++i) {
                          Square sq = start - i * d;
                          if (is_ok(sq)) {
                              removal_mask |= sq;
                              connect_mask |= sq;
                          }
                      }
                  }
              }
          };

          if (remove_connect_n_by_type()) {
              for (PieceSet ps = variant()->pieceTypes; ps; ) {
                  PieceType pt = pop_lsb(ps);
                  Bitboard line = pieces(pt);

                  // removeConnectN is evaluated after move effects. With blastPromotion,
                  // adjust per-type occupancy to the post-promotion type map.
                  if (blast_promotion() && blast_mask) {
                      Bitboard b = blast_mask;
                      while (b) {
                          Square sq = pop_lsb(b);
                          Piece p = piece_on(sq);
                          if (p == NO_PIECE)
                              continue;

                          PieceType fromPt = type_of(p);
                          PieceType toPt = promoted_piece_type(fromPt);

                          if (fromPt == pt)
                              line &= ~square_bb(sq);
                          if (toPt == pt)
                              line |= square_bb(sq);
                      }
                  }

                  mark_line(line);
              }
          } else {
              Bitboard whiteLine = pieces(WHITE);
              Bitboard blackLine = pieces(BLACK);

              // Color ownership is stable under blastPromotion, except when a piece
              // has no promoted target and is therefore removed.
              if (blast_promotion() && blast_mask) {
                  Bitboard b = blast_mask;
                  while (b) {
                      Square sq = pop_lsb(b);
                      Piece p = piece_on(sq);
                      if (p == NO_PIECE)
                          continue;
                      if (promoted_piece_type(type_of(p)) == NO_PIECE_TYPE) {
                          if (color_of(p) == WHITE)
                              whiteLine &= ~square_bb(sq);
                          else
                              blackLine &= ~square_bb(sq);
                      }
                  }
              }

              mark_line(whiteLine);
              mark_line(blackLine);
          }
      }

      while (removal_mask)
      {
          Square bsq = pop_lsb(removal_mask);
          Piece bpc = piece_on(bsq);
          if (bpc == NO_PIECE) continue;
          Color bc = color_of(bpc);

          if (blast_promotion() && (blast_mask & bsq) && !(connect_mask & bsq)) {
              PieceType promoted = promoted_piece_type(type_of(bpc));
              if (promoted != NO_PIECE_TYPE) {
                  Piece promotedPiece = make_piece(bc, promoted);
                  st->unpromotedBycatch[bsq] = bpc;
                  st->bycatchSquares |= bsq;
                  st->blastPromotedSquares |= bsq;

                  remove_piece(bsq);
                  put_piece(promotedPiece, bsq);

                  if (Eval::useNNUE) {
                      append_dirty(st, promotedPiece, SQ_NONE, bsq);
                  }

                  k ^= Zobrist::psq[bpc][bsq] ^ Zobrist::psq[promotedPiece][bsq];
                  st->materialKey ^= Zobrist::psq[promotedPiece][pieceCount[promotedPiece] - 1]
                                  ^ Zobrist::psq[bpc][pieceCount[bpc]];
                  st->nonPawnMaterial[bc] += PieceValue[MG][promotedPiece]
                                          - (type_of(bpc) != PAWN ? PieceValue[MG][bpc] : 0);
                  continue;
              }
          }

          if (type_of(bpc) != PAWN)
              st->nonPawnMaterial[bc] -= PieceValue[MG][bpc];

          int bycatchDirtyIdx = -1;
          if (Eval::useNNUE)
              bycatchDirtyIdx = append_dirty(st, bpc, bsq, SQ_NONE);

          // Update board and piece lists
          // In order to not have to store the values of both board and unpromotedBoard,
          // demote promoted pieces, but keep promoted pawns as promoted,
          // and store demotion/promotion bitboards to disambiguate the piece state
          bool capturedPromoted = is_promoted(bsq);
          Piece unpromotedCaptured = unpromoted_piece_on(bsq);
          st->unpromotedBycatch[bsq] = unpromotedCaptured ? unpromotedCaptured : bpc;
          st->bycatchSquares |= bsq;
          if (unpromotedCaptured)
              st->demotedBycatch |= bsq;
          else if (capturedPromoted)
              st->promotedBycatch |= bsq;
          remove_piece(bsq);
          board[bsq] = NO_PIECE;

          // Points assignment logic
          if (points_counting()) {
              int pts = var->piecePoints[type_of(bpc)];
              switch (points_rule_captures()) {
                  case POINTS_US:    st->pointsCount[us]  += pts; break;
                  case POINTS_THEM:  st->pointsCount[~us] += pts; break;
                  case POINTS_OWNER: st->pointsCount[bc]  += pts; break;
                  case POINTS_NON_OWNER: st->pointsCount[~bc] += pts; break;
                 default: break;
              }
          }

          bool petrifiedCenter = bsq == moverSq && (var->petrifyOnCaptureTypes & type_of(bpc));
          if (captures_to_hand() && !petrifiedCenter && !st->suppressedCaptureTransfer)
          {
              Piece pieceToHand = !capturedPromoted || drop_loop()
                                 ? make_piece(us, type_of(bpc))
                                 : unpromotedCaptured ? make_piece(us, type_of(unpromotedCaptured))
                                                      : make_piece(us, main_promotion_pawn_type(color_of(bpc)));
              int n;
              if (capture_type() == PRISON) {
                  pieceToHand = ~pieceToHand;
                  n = add_to_prison(pieceToHand);
              } else {
                  add_to_hand(pieceToHand);
                  n = pieceCountInHand[color_of(pieceToHand)][type_of(pieceToHand)];
              }
              xor_in_hand_count(k, pieceToHand, n - 1, n);

              if (Eval::useNNUE && bycatchDirtyIdx >= 0)
              {
                  dp.handPiece[bycatchDirtyIdx] = pieceToHand;
                  dp.handCount[bycatchDirtyIdx] = pieceCountInHand[color_of(pieceToHand)][type_of(pieceToHand)];
              }
          }

          // Update material hash key
          k ^= Zobrist::psq[bpc][bsq];
          st->materialKey ^= Zobrist::psq[bpc][pieceCount[bpc]];
          if (type_of(bpc) == PAWN)
              st->pawnKey ^= Zobrist::psq[bpc][bsq];

          // Update castling rights if needed
          if (st->castlingRights && castlingRightsMask[bsq])
          {
             k ^= Zobrist::castling[st->castlingRights];
             st->castlingRights &= ~castlingRightsMask[bsq];
             k ^= Zobrist::castling[st->castlingRights];
          }

          // Make a wall square where the piece was
          if (bsq == to ? bool(var->petrifyOnCaptureTypes & type_of(bpc)) : var->petrifyBlastPieces)
          {
              st->wallSquares |= bsq;
              byTypeBB[ALL_PIECES] |= bsq;
              k ^= Zobrist::wall[bsq];
          }
      };
  };

  // Add gated wall square
  // if wallOrMove, only actually place the wall if they gave up their move
  if (walling(us) && (!wall_or_move() || (from == to)))
  {
      // Reset wall squares for duck walling
      if (walling_rule() == DUCK)
      {
          Bitboard b = st->previous->wallSquares;
          byTypeBB[ALL_PIECES] ^= b;
          while (b)
              k ^= Zobrist::wall[pop_lsb(b)];
          st->wallSquares = 0;
      }
      st->wallSquares |= gating_square(m);
      byTypeBB[ALL_PIECES] |= gating_square(m);
      k ^= Zobrist::wall[gating_square(m)];
  }

  // Shared helper for "piece removed from its destination square" effects that
  // are not normal captures (e.g. capturer-dies or death-on-capture).
  auto remove_destination_piece_no_capture_effects = [&](Square sq, bool makeDeadSquare) {
      Piece deadPiece = piece_on(sq);
      Color dc = color_of(deadPiece);

      st->deadPiece = deadPiece;
      st->deadPiecePromoted = is_promoted(sq);
      st->deadUnpromotedPiece = st->deadPiecePromoted ? unpromoted_piece_on(sq) : NO_PIECE;

      if (Eval::useNNUE)
          append_dirty(st, deadPiece, sq, SQ_NONE);

      remove_piece(sq);
      board[sq] = NO_PIECE;

      k ^= Zobrist::psq[deadPiece][sq];
      st->materialKey ^= Zobrist::psq[deadPiece][pieceCount[deadPiece]];
      if (type_of(deadPiece) == PAWN)
          st->pawnKey ^= Zobrist::psq[deadPiece][sq];
      else
          st->nonPawnMaterial[dc] -= PieceValue[MG][deadPiece];

      if (makeDeadSquare)
      {
          st->deadSquares |= sq;
          byTypeBB[ALL_PIECES] |= sq;
          k ^= Zobrist::dead[sq];
      }
  };

  bool diesOnSameTypeCapture = var->capturerDiesOnSameTypeCapture
                            && captured != NO_PIECE
                            && type_of(captured) == movedType;
  bool exemptFromCapturerDeath = (var->capturerDiesExemptTypes & piece_set(movedType))
                              || (var->capturerDiesExemptPawns && movedType == PAWN);
  bool diesOnCapture = (death_on_capture_types() & piece_set(movedType))
                    || (var->capturerDiesOnCapture && !exemptFromCapturerDeath)
                    || diesOnSameTypeCapture;
  if (!capturedDeadSquare && captured != NO_PIECE && type_of(m) != DROP && diesOnCapture
      && piece_on(moverSq) != NO_PIECE)
  {
      bool makeDeadSquare = bool(death_on_capture_types() & piece_set(movedType));
      remove_destination_piece_no_capture_effects(moverSq, makeDeadSquare);

      if (!allow_checks() && givesCheck && count<KING>(them))
          givesCheck = bool(attackers_to_king(square<KING>(them), us) & pieces(us));
  }

  if (trigger_matches(var->changingColorTrigger, captured != NO_PIECE || st->bycatchSquares != 0)
      && !is_pass(m)
      && type_of(m) != DROP
      && piece_on(moverSq) != NO_PIECE
      && color_of(piece_on(moverSq)) == us
      && (var->changingColorPieceTypes & type_of(piece_on(moverSq))))
  {
      Piece cur = piece_on(moverSq);
      Piece changed = make_piece(them, type_of(cur));
      st->didColorChange = true;
      st->colorChangedFrom = cur;
      st->colorChangeSquare = moverSq;
      st->colorChangedPromoted = is_promoted(moverSq);
      st->colorChangedUnpromoted = st->colorChangedPromoted ? unpromoted_piece_on(moverSq) : NO_PIECE;

      remove_piece(moverSq);
      put_piece(changed, moverSq, st->colorChangedPromoted, st->colorChangedUnpromoted);

      k ^= Zobrist::psq[cur][moverSq] ^ Zobrist::psq[changed][moverSq];
      st->materialKey ^= Zobrist::psq[cur][pieceCount[cur]]
                       ^ Zobrist::psq[changed][pieceCount[changed] - 1];
      if (type_of(cur) == PAWN)
          st->pawnKey ^= Zobrist::psq[cur][moverSq] ^ Zobrist::psq[changed][moverSq];
      else
      {
          st->nonPawnMaterial[us] -= PieceValue[MG][cur];
          st->nonPawnMaterial[them] += PieceValue[MG][changed];
      }

      if (Eval::useNNUE)
      {
          append_dirty(st, cur, moverSq, SQ_NONE);
          append_dirty(st, changed, SQ_NONE, moverSq);
      }
  }

  if (forced_jump_continuation())
  {
      if (is_pass(m))
      {
          // Keep pending continuation across the forced opponent pass.
          st->forcedJumpSquare = st->previous->forcedJumpSquare;
          st->forcedJumpHasFollowup = st->forcedJumpSquare != SQ_NONE
                                    ? compute_forced_jump_followup(st->forcedJumpSquare, st->previous->forcedJumpStep)
                                    : false;
          st->forcedJumpStep = st->previous->forcedJumpStep;
      }
      else if (jumpCapsq != SQ_NONE && type_of(m) != PROMOTION && type_of(m) != PIECE_PROMOTION
            && piece_on(moverSq) != NO_PIECE
            && color_of(piece_on(moverSq)) == us)
      {
          st->forcedJumpSquare = moverSq;
          st->forcedJumpStep = forced_jump_same_direction() ? int(to) - int(from) : 0;
          st->forcedJumpHasFollowup = compute_forced_jump_followup(moverSq, st->forcedJumpStep);
      }
      else
      {
          st->forcedJumpSquare = SQ_NONE;
          st->forcedJumpHasFollowup = false;
          st->forcedJumpStep = 0;
      }
  }
  else
  {
      st->forcedJumpSquare = SQ_NONE;
      st->forcedJumpHasFollowup = false;
      st->forcedJumpStep = 0;
  }

  if (potions_enabled())
  {
      auto togglePotionHashes = [&](Key& key) {
          for (Color c : {WHITE, BLACK})
              for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
              {
                  Variant::PotionType potion = static_cast<Variant::PotionType>(pt);
                  if (potion_piece(potion) == NO_PIECE_TYPE)
                      continue;

                  xor_potion_zone(key, c, potion, st->potionZones[c][pt]);
                  xor_potion_cooldown(key, c, potion, st->potionCooldown[c][pt]);
              }
      };

      togglePotionHashes(k);

      for (int pt = 0; pt < Variant::POTION_TYPE_NB; ++pt)
      {
          Variant::PotionType potion = static_cast<Variant::PotionType>(pt);
          if (potion_piece(potion) == NO_PIECE_TYPE)
              continue;

          if (gatingPotion == potion)
          {
              int cooldown = var->potionCooldown[pt];
              st->potionCooldown[us][pt] = std::max(cooldown - 1, 0);
          }
          else if (st->potionCooldown[us][pt] > 0)
              --st->potionCooldown[us][pt];
      }

      st->potionZones[us][Variant::POTION_FREEZE] = gatingPotion == Variant::POTION_FREEZE ? freezeExtra : Bitboard(0);
      st->potionZones[us][Variant::POTION_JUMP] = Bitboard(0);

      st->potionZones[them][Variant::POTION_FREEZE] = Bitboard(0);
      st->potionZones[them][Variant::POTION_JUMP] = Bitboard(0);

      togglePotionHashes(k);
  }

  updatePawnCheckZone();
  if (var->pointsCounting) {
      for (Color c : {WHITE, BLACK}) {
          if (st->pointsCount[c] == st->previous->pointsCount[c])
              continue;
          xor_points_bucket(k, c, st->previous->pointsCount[c]);
          xor_points_bucket(k, c, st->pointsCount[c]);
      }
  }

  // Update the key with the final value
  st->key = k;
  st->boardKey = st->key ^ reserve_key();
  if (var->samePlayerBoardRepetitionIllegal)
      st->layoutKey = layout_key();
  if (var->changingColorTrigger != ColorChangeTrigger::NEVER && !allow_checks() && count<KING>(them))
      givesCheck = bool(attackers_to_king(square<KING>(them), us) & pieces(us));
  // Calculate checkers bitboard (if move gives check)
  st->checkersBB = !allow_checks() && givesCheck
                 ? attackers_to_king(square<KING>(them), us) & pieces(us)
                 : Bitboard(0);

  if (!allow_checks())
  {
      if (pseudo_royal_types())
          st->checkersBB |= checked_pseudo_royals(them);
      if (anti_royal_types())
          st->checkersBB |= checked_anti_royals(them);
  }
  assert(allow_checks() || givesCheck == bool(st->checkersBB) || (givesCheck && var->prisonPawnPromotion) || pseudo_royal_types() || anti_royal_types());

  sideToMove = ~sideToMove;

  if (counting_rule())
  {
      if (counting_rule() != ASEAN_COUNTING && type_of(captured) == PAWN && count<ALL_PIECES>(~sideToMove) == 1 && !count<PAWN>() && count_limit(~sideToMove))
      {
          st->countingLimit = 2 * count_limit(~sideToMove);
          st->countingPly = 2 * count<ALL_PIECES>() - 1;
      }

      if ((!st->countingLimit || ((captured || type_of(m) == PROMOTION) && count<ALL_PIECES>(sideToMove) == 1)) && count_limit(sideToMove))
      {
          st->countingLimit = 2 * count_limit(sideToMove);
          st->countingPly = counting_rule() == ASEAN_COUNTING || count<ALL_PIECES>(sideToMove) > 1 ? 0 : 2 * count<ALL_PIECES>();
      }
  }

  // Update king attacks used for fast check detection
  set_check_info(st);

  // n-check accounting must include pseudo-royal checks (legacy extinctionPseudoRoyal path).
  if (check_counting() && st->checksRemaining[us] > 0)
  {
      bool givesCountingCheck = bool(st->checkersBB);
      if (!givesCountingCheck && pseudo_royal_types())
          givesCountingCheck = bool(checked_pseudo_royals(sideToMove));

      if (givesCountingCheck)
      {
          Key diff = Zobrist::checks[us][st->checksRemaining[us]]
                  ^  Zobrist::checks[us][st->checksRemaining[us] - 1];
          st->key ^= diff;
          st->boardKey ^= diff;
          --st->checksRemaining[us];
      }
  }

  // Calculate the repetition info. It is the ply distance from the previous
  // occurrence of the same position, negative in the 3-fold case, or zero
  // if the position was not repeated.
  st->repetition = 0;
  st->boardRepetition = 0;
  int end = captures_to_hand() ? st->pliesFromNull : std::min(st->rule50, st->pliesFromNull);
  if (end >= 4)
  {
      StateInfo* stp = st->previous->previous;
      for (int i = 4; i <= end; i += 2)
      {
          stp = stp->previous->previous;
          if (stp->key == st->key)
              st->repetition = stp->repetition ? -i : i;
          if (stp->boardKey == st->boardKey)
              st->boardRepetition = stp->boardRepetition ? -i : i;
          if (st->repetition && st->boardRepetition)
              break;
      }
  }

  assert(pos_is_ok());
}


/// Position::undo_move() unmakes a move. When it returns, the position should
/// be restored to exactly the same state as before the move was made.

void Position::undo_move(Move m) {

  assert(is_ok(m));

  sideToMove = ~sideToMove;

  Color us = sideToMove;
  Square from = from_sq(m);
  Square to = to_sq(m);
  bool rifleShot = rifle_capture(m) && st->capturedPiece != NO_PIECE && type_of(m) != CASTLING;
  Square moverSq = rifleShot ? from : to;
  Piece pc = piece_on(moverSq);
  PieceType exchange = exchange_piece(m);
  bool wasOpeningSelfRemoval = opening_self_removal()
                            && gamePly <= 2
                            && type_of(m) == SPECIAL
                            && from == to
                            && !st->pass;

  assert(type_of(m) == DROP || empty(from) || type_of(m) == CASTLING || is_gating(m)
         || (type_of(m) == PROMOTION && sittuyin_promotion())
         || is_pass(m)
         || wasOpeningSelfRemoval
         || (commit_gates() && st->removedGatingType > NO_PIECE_TYPE)
  );
  assert(type_of(st->capturedPiece) != KING || allow_checks());

  // Reset wall squares
  byTypeBB[ALL_PIECES] ^= st->wallSquares ^ st->previous->wallSquares;
  byTypeBB[ALL_PIECES] ^= st->deadSquares ^ st->previous->deadSquares;

  // Add the blast pieces
  if (
       ( surround_capture_opposite() || surround_capture_intervene() || surround_capture_edge() ) ||
       ( st->capturedPiece && (blast_on_capture() || var->petrifyOnCaptureTypes) ) ||
       ( blast_on_move() && !st->capturedPiece ) ||
       ( remove_connect_n() > 0 )
     )
  {
      //It's ok to just loop through all, not taking into account immunities/pawnness
      //because we'll just not find the piece in unpromotedBycatch.
      //Same if surround_capture_opposite is true, king is superset of all directions.
      Bitboard restoreMask = st->bycatchSquares;
      while (restoreMask)
      {
          Square bsq = pop_lsb(restoreMask);
          Piece unpromotedBpc = st->unpromotedBycatch[bsq];
          Piece bpc = st->demotedBycatch & bsq ? make_piece(color_of(unpromotedBpc), promoted_piece_type(type_of(unpromotedBpc)))
                                               : unpromotedBpc;
          bool isPromoted = (st->promotedBycatch | st->demotedBycatch) & bsq;
          bool wasBlastPromoted = bool(st->blastPromotedSquares & bsq);

          // Update board and piece lists
          if (bpc || wasBlastPromoted)
          {
              if (wasBlastPromoted && piece_on(bsq) != NO_PIECE) {
                  remove_piece(bsq);
                  board[bsq] = NO_PIECE;
              }
              put_piece(bpc, bsq, isPromoted, st->demotedBycatch & bsq ? unpromotedBpc : NO_PIECE);
              bool petrifiedCenter = bsq == moverSq && (var->petrifyOnCaptureTypes & type_of(bpc));
              if (!wasBlastPromoted && !petrifiedCenter && !st->suppressedCaptureTransfer && capture_type() == HAND) {
                  remove_from_hand(!drop_loop() && (st->promotedBycatch & bsq)
                                    ? make_piece(us, main_promotion_pawn_type(color_of(unpromotedBpc)))
                                    : make_piece(us, type_of(unpromotedBpc)));
              } else if (!wasBlastPromoted && !petrifiedCenter && !st->suppressedCaptureTransfer && capture_type() == PRISON) {
                  remove_from_prison(!drop_loop() && (st->promotedBycatch & bsq)
                                    ? make_piece(color_of(unpromotedBpc), main_promotion_pawn_type(color_of(unpromotedBpc)))
                                    : unpromotedBpc);
              }
          }
      }
      // Reset piece since it exploded itself
      pc = piece_on(moverSq);
  }

  if (st->didMorph && st->morphSquare == moverSq)
  {
      remove_piece(moverSq);
      put_piece(st->morphedFrom, moverSq);
      pc = st->morphedFrom;
  }

  if (st->didColorChange && st->colorChangeSquare == moverSq)
  {
      remove_piece(moverSq);
      put_piece(st->colorChangedFrom, moverSq, st->colorChangedPromoted, st->colorChangedUnpromoted);
      pc = st->colorChangedFrom;
  }

  // Remove gated piece or restore potion
  if (is_gating(m))
  {
      Piece gating_piece = make_piece(us, gating_type(m));
      Variant::PotionType potion = potion_type_from_piece(var, gating_type(m));

      if (potion != Variant::POTION_TYPE_NB)
      {
          add_to_hand(gating_piece);
      }
      else
      {
          remove_piece(gating_square(m));
          board[gating_square(m)] = NO_PIECE;
          add_to_hand(gating_piece);
          st->gatesBB[us] |= gating_square(m);
      }
  }

  if(commit_gates() && st->removedGatingType > NO_PIECE_TYPE){
      Square gateSq = make_square(file_of(from), us == WHITE ? RANK_1 : max_rank());
      if (piece_on(gateSq) != NO_PIECE)
      {
          remove_piece(gateSq);
          board[gateSq] = NO_PIECE;
      }
      // Restore the removed committed slot.
      commit_piece(make_piece(us, st->removedGatingType), file_of(from));
  }
  if (commit_gates() && st->capturedPiece && st->capturedGatingType > NO_PIECE_TYPE){
      // return musketeer piece fronted by the captured piece
      commit_piece(make_piece(color_of(st->capturedPiece), st->capturedGatingType), file_of(to));
  }

  if (type_of(m) == PROMOTION)
  {
      assert((promotion_zone(st->promotionPawn) & to) || sittuyin_promotion());
      Piece promotedPiece = piece_on(moverSq);
      if (promotedPiece == NO_PIECE)
          promotedPiece = make_piece(us, promotion_type(m));
      assert(type_of(promotedPiece) == promotion_type(m));
      assert(type_of(promotedPiece) >= KNIGHT && type_of(promotedPiece) < KING);
      assert(type_of(st->promotionPawn) == main_promotion_pawn_type(us) || !captures_to_hand());

      if (prison_pawn_promotion() && type_of(st->promotionPawn) == PAWN) {
          remove_from_prison(st->promotionPawn);
          add_to_prison(promotedPiece);
      }
      if (piece_on(moverSq) != NO_PIECE)
          remove_piece(moverSq);
      pc = st->promotionPawn;
      put_piece(pc, moverSq);
      if (st->consumedPromotionHandPiece != NO_PIECE)
          add_to_hand(st->consumedPromotionHandPiece);
  }
  else if (type_of(m) == PIECE_PROMOTION)
  {
      // The promoted piece on 'moverSq' may have been removed by blast/connect-N side effects.
      // For undo, rely on the original mover cached at do_move() time.
      Piece unpromotedPiece = st->promotionPawn;
      assert(unpromotedPiece != NO_PIECE);
      if (piece_on(moverSq) != NO_PIECE)
          remove_piece(moverSq);
      pc = unpromotedPiece;
      put_piece(pc, moverSq);
      if (st->consumedPromotionHandPiece != NO_PIECE)
          add_to_hand(st->consumedPromotionHandPiece);
  }
  else if (type_of(m) == PIECE_DEMOTION)
  {
      remove_piece(moverSq);
      Piece unpromotedPc = pc;
      pc = make_piece(us, promoted_piece_type(type_of(pc)));
      put_piece(pc, moverSq, true, unpromotedPc);
  }

  if (type_of(m) == CASTLING)
  {
      Square rfrom, rto;
      do_castling<false>(us, from, to, rfrom, rto);
  }
  else
  {
      if (type_of(m) == DROP)
          undrop_piece(make_piece(us, in_hand_piece_type(m)), to, exchange); // Remove the dropped piece
      else if (wasOpeningSelfRemoval)
          put_piece(st->deadPiece, from, st->deadPiecePromoted, st->deadUnpromotedPiece);
      else
      {
          if (st->deadPiece && type_of(m) != PROMOTION && type_of(m) != PIECE_PROMOTION)
          {
              if (st->deadSquares & moverSq)
                  st->deadSquares ^= moverSq;
              put_piece(st->deadPiece, moverSq, st->deadPiecePromoted, st->deadUnpromotedPiece);
              pc = piece_on(moverSq);
          }
          if (!rifleShot)
              move_piece(to, from); // Put the piece back at the source square
      }

      if (st->capturedPiece)
      {
          Square capsq = to;

          if (type_of(m) == EN_PASSANT)
          {
              capsq = st->captureSquare;

              assert(st->previous->epSquares & to);
              assert(var->enPassantRegion[sideToMove] & to);
              assert(piece_on(capsq) == NO_PIECE);
          }
          else if (st->captureSquare != SQ_NONE)
              capsq = st->captureSquare;

          put_piece(st->capturedPiece, capsq, st->capturedpromoted, st->unpromotedCapturedPiece); // Restore the captured piece
          if (!st->suppressedCaptureTransfer && capture_type() == HAND) {
              remove_from_hand(!drop_loop() && st->capturedpromoted
                               ? (st->unpromotedCapturedPiece
                                  ? make_piece(us, type_of(st->unpromotedCapturedPiece))
                                  : make_piece(us, main_promotion_pawn_type(color_of(st->capturedPiece))))
                               : make_piece(us, type_of(st->capturedPiece)));
          } else if (!st->suppressedCaptureTransfer && capture_type() == PRISON) {
              remove_from_prison(!drop_loop() && st->capturedpromoted
                               ? (st->unpromotedCapturedPiece
                                  ? st->unpromotedCapturedPiece
                                  : make_piece(color_of(st->capturedPiece), main_promotion_pawn_type(color_of(st->capturedPiece))))
                               : st->capturedPiece);
          }
      }
  }

  if (flip_enclosed_pieces())
  {
      // Flip pieces
      Bitboard to_flip = st->flippedPieces;
      while(to_flip)
      {
          Square s = pop_lsb(to_flip);
          Piece resulting = ~piece_on(s);
          remove_piece(s);
          put_piece(resulting, s);
      }
  }

  // Finally point our state pointer back to the previous state
  st = st->previous;
  --gamePly;
  updatePawnCheckZone();

  assert(pos_is_ok());
}


/// Position::do_castling() is a helper used to do/undo a castling move. This
/// is a bit tricky in Chess960 where from/to squares can overlap.
template<bool Do>
void Position::do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto) {

  bool kingSide = to > from;
  rfrom = to; // Castling is encoded as "king captures friendly rook"
  to = make_square(kingSide ? castling_kingside_file() : castling_queenside_file(), castling_rank(us));
  rto = to + (kingSide ? WEST : EAST);

  Piece castlingKingPiece = piece_on(Do ? from : to);
  Piece castlingRookPiece = piece_on(Do ? rfrom : rto);

  if (Do && Eval::useNNUE)
  {
      auto& dp = st->dirtyPiece;
      dp.piece[0] = castlingKingPiece;
      dp.from[0] = from;
      dp.to[0] = to;
      dp.piece[1] = castlingRookPiece;
      dp.from[1] = rfrom;
      dp.to[1] = rto;
      dp.dirty_num = 2;
  }

  // Remove both pieces first since squares could overlap in Chess960
  remove_piece(Do ? from : to);
  remove_piece(Do ? rfrom : rto);
  board[Do ? from : to] = board[Do ? rfrom : rto] = NO_PIECE; // Since remove_piece doesn't do it for us
  put_piece(castlingKingPiece, Do ? to : from);
  if (!Do && commit_gates() && st->removedCastlingGatingType > NO_PIECE_TYPE && piece_on(rfrom) != NO_PIECE)
  {
      remove_piece(rfrom);
      board[rfrom] = NO_PIECE;
  }
  put_piece(castlingRookPiece, Do ? rto : rfrom);

  if (!Do && commit_gates() && st->removedCastlingGatingType > NO_PIECE_TYPE) {
      // Restore the removed committed slot for the rook file.
      commit_piece(make_piece(us, st->removedCastlingGatingType), file_of(rfrom));
  }

  if (Do && commit_gates() && has_committed_piece(us, file_of(rfrom))) {
      st->removedCastlingGatingType = drop_committed_piece(us, file_of(rfrom));
  }

}


/// Position::do_null_move() is used to do a "null move": it flips
/// the side to move without executing any move on the board.

void Position::do_null_move(StateInfo& newSt) {

  assert(!checkers());
  assert(&newSt != st);

  std::memcpy(static_cast<void*>(&newSt), static_cast<const void*>(st), offsetof(StateInfo, accumulator));

  newSt.previous = st;
  st = &newSt;

  st->dirtyPiece.dirty_num = 0;
  st->dirtyPiece.piece[0] = NO_PIECE; // Avoid checks in UpdateAccumulator()
  st->accumulator.computed[WHITE] = false;
  st->accumulator.computed[BLACK] = false;
  st->nnueRefreshNeeded = false;

  while (st->epSquares)
      st->key ^= Zobrist::enpassant[pop_lsb(st->epSquares)];

  st->key ^= Zobrist::side;
  st->boardKey = st->key ^ reserve_key();
  prefetch(TT.first_entry(key()));

  ++st->rule50;
  st->pliesFromNull = 0;

  sideToMove = ~sideToMove;

  set_check_info(st);

  st->repetition = 0;
  st->boardRepetition = 0;

  assert(pos_is_ok());
}


/// Position::undo_null_move() must be used to undo a "null move"

void Position::undo_null_move() {

  assert(!checkers());

  st = st->previous;
  sideToMove = ~sideToMove;
}


/// Position::key_after() computes the new hash key after the given move. Needed
/// for speculative prefetch. It doesn't recognize special moves like castling,
/// en passant and promotions.

Key Position::key_after(Move m) const {

  Square from = from_sq(m);
  Square to = to_sq(m);
  Piece pc = moved_piece(m);
  Piece captured = piece_on(to);
  Key k = st->key ^ Zobrist::side;

  if (captured)
  {
      k ^= Zobrist::psq[captured][to];
      if (captures_to_hand()) {
          Piece removedPiece = !drop_loop() && is_promoted(to)
                               ? make_piece(sideToMove, main_promotion_pawn_type(color_of(captured)))
                               : make_piece(sideToMove, type_of(captured));
          int n;
          if (capture_type() == HAND) {
              n = pieceCountInHand[color_of(removedPiece)][type_of(removedPiece)];
          } else {
              n = pieceCountInPrison[color_of(removedPiece)][type_of(removedPiece)];
              removedPiece = ~removedPiece;
          }
          {
              xor_in_hand_count(k, removedPiece, n + 1, n);
          }
      }
  }
  if (type_of(m) == DROP)
  {
      Piece pc_hand = make_piece(sideToMove, in_hand_piece_type(m));
      PieceType exchanged = exchange_piece(m);
      k ^= Zobrist::psq[pc][to];

      if (exchanged == NO_PIECE_TYPE)
      {
          int n = pieceCountInHand[color_of(pc_hand)][type_of(pc_hand)];
          xor_in_hand_count(k, pc_hand, n - 1, n);
          return k;
      }

      Color us = sideToMove;
      Color them = ~us;
      Piece exchangedPiece = make_piece(them, exchanged);

      int handOld = pieceCountInHand[them][exchanged];
      int handNew = handOld + 1;
      xor_in_hand_count(k, exchangedPiece, handOld, handNew);

      int prisonOldEx = pieceCountInPrison[us][exchanged];
      int prisonNewEx = prisonOldEx - 1;
      xor_in_hand_count(k, exchangedPiece, prisonOldEx, prisonNewEx);

      int prisonOldDrop = pieceCountInPrison[them][type_of(pc)];
      int prisonNewDrop = prisonOldDrop - 1;
      xor_in_hand_count(k, pc, prisonOldDrop, prisonNewDrop);
      return k;
  }

  return k ^ Zobrist::psq[pc][to] ^ Zobrist::psq[pc][from];
}


Value Position::blast_see(Move m) const {
  assert(is_ok(m));

  Square from = from_sq(m);
  Square to = to_sq(m);
  Color us = color_of(moved_piece(m));
  Bitboard fromto = type_of(m) == DROP ? square_bb(to) : from | to;
  Bitboard blast = blast_squares(capture(m) ? capture_square(m) : to);

  // If the explosion would capture an opponent royal or pseudo-royal piece,
  // treat the move as delivering immediate mate. This prevents the static
  // evaluation from underestimating winning blast captures.
  Bitboard enemyRoyal = st->pseudoRoyals & pieces(~us);
  enemyRoyal |= pieces(~us, king_type());
  if (blast & enemyRoyal)
      return -checkmate_value();

  Value result = VALUE_ZERO;

  // Add the least valuable attacker for quiet moves
  if (!capture(m) && !blast_on_move())
  {
      Bitboard attackers = attackers_to(to, pieces() ^ fromto, ~us);
      Value minAttacker = VALUE_INFINITE;

      while (attackers)
      {
          Square s = pop_lsb(attackers);
          if (!(extinction_piece_types() & type_of(piece_on(s))))
              minAttacker = std::min(minAttacker, blast & s ? VALUE_ZERO : CapturePieceValue[MG][piece_on(s)]);
      }

      if (minAttacker == VALUE_INFINITE)
          return VALUE_ZERO;

      result += minAttacker;
      if (type_of(m) == DROP)
          result -= CapturePieceValue[MG][dropped_piece_type(m)];
  }

  // Sum up blast piece values
  bool extinctsUs = false;
  bool extinctsThem = false;
  while (blast)
  {
      Piece bpc = piece_on(pop_lsb(blast));
      if (!var->extinctionAllPieceTypes && (extinction_piece_types() & type_of(bpc)))
      {
          if (color_of(bpc) == us)
              extinctsUs = true;
          else
              extinctsThem = true;
      }
      result += color_of(bpc) == us ? -CapturePieceValue[MG][bpc] : CapturePieceValue[MG][bpc];
  }

  // Evaluate extinctions
  if (!capture(m) && !blast_on_move())
  {
      // For quiet moves, the opponent can decide whether to capture or not
      // so they can pick the better of the two
      if (extinctsThem && extinctsUs)
          return VALUE_ZERO;
      if (extinctsThem)
          return std::min(-extinction_value(), VALUE_ZERO);
      if (extinctsUs)
          return std::min(extinction_value(), VALUE_ZERO);
  }
  else
  {
      if (extinctsUs)
          return extinction_value();
      if (extinctsThem)
          return -extinction_value();
  }

  return capture(m) || must_capture() || blast_on_move() ? result - 1 : std::min(result, VALUE_ZERO);
}


/// Position::see_ge (Static Exchange Evaluation Greater or Equal) tests if the
/// SEE value of move is greater or equal to the given threshold. We'll use an
/// algorithm similar to alpha-beta pruning with a null window.

bool Position::see_ge(Move m, Value threshold) const {

  assert(is_ok(m));

  // Only deal with normal moves, assume others pass a simple SEE
  if (type_of(m) != NORMAL && type_of(m) != DROP && type_of(m) != PIECE_PROMOTION)
      return VALUE_ZERO >= threshold;

  Square from = from_sq(m), to = to_sq(m);

  // nCheck
  if (check_counting() && color_of(moved_piece(m)) == sideToMove && gives_check(m))
      return true;

  // Atomic explosion SEE
  if (blast_on_capture() || blast_on_move())
      return blast_see(m) >= threshold;

  Piece victim = captured_piece(m);

  // Extinction
  if (   extinction_value() != VALUE_NONE
      && victim != NO_PIECE
      && !var->extinctionAllPieceTypes
      && (   (   (extinction_piece_types() & type_of(victim))
              && pieceCount[victim] == extinction_piece_count() + 1)
          || (   (extinction_piece_types() & ALL_PIECES)
              && count<ALL_PIECES>(~sideToMove) == extinction_piece_count() + 1)))
      return extinction_value() < VALUE_ZERO;

  // Do not evaluate SEE if value would be unreliable
  if (must_capture() || !checking_permitted() || is_gating(m) || count<CLOBBER_PIECE>() == count<ALL_PIECES>())
      return VALUE_ZERO >= threshold;

  // Wrapped topology invalidates the ordinary ray/pin exchange model.
  // Use a neutral fallback until SEE is made topology-aware.
  if (topology_wraps())
      return VALUE_ZERO >= threshold;

  int victimValue = PieceValue[MG][victim];
  if (victim != NO_PIECE && color_of(victim) == color_of(moved_piece(m)) && self_capture())
      victimValue = -victimValue;
  if (points_counting() && victim != NO_PIECE)
  {
      int signedPts = 0;
      int pts = variant()->piecePoints[type_of(victim)];
      switch (points_rule_captures())
      {
          case POINTS_US:        signedPts =  pts; break;
          case POINTS_THEM:      signedPts = -pts; break;
          case POINTS_OWNER:     signedPts = color_of(victim) == sideToMove ? pts : -pts; break;
          case POINTS_NON_OWNER: signedPts = color_of(victim) == sideToMove ? -pts : pts; break;
          case POINTS_NONE:      signedPts = 0; break;
      }
      if (points_goal() > 0)
      {
          if (points_goal_value() < VALUE_ZERO)
              signedPts = -signedPts;
          else if (points_goal_value() == VALUE_ZERO)
              signedPts = 0;
      }
      victimValue += 20 * signedPts;
  }
  int swap = victimValue - threshold;
  if (swap < 0)
      return false;

  // In morph-capture variants, the capturing piece on 'to' may immediately
  // change type, so the first recapture loses the morphed piece value.
  Piece seeMover = moved_piece(m);
  if (capture_morph() && capture(m) && victim != NO_PIECE)
      if (!(rex_exclusive_morph() && type_of(seeMover) == KING))
          seeMover = make_piece(color_of(seeMover), type_of(victim));

  swap = PieceValue[MG][seeMover] - swap;
  if (swap <= 0)
      return true;

  // Petrification ends SEE
  if (var->petrifyOnCaptureTypes & type_of(moved_piece(m)) && capture(m))
      return false;

  Bitboard occupied = (type_of(m) != DROP ? pieces() ^ from : pieces()) ^ to;
  Color stm = color_of(moved_piece(m));
  auto update_attackers = [&](Bitboard occ) {
      Bitboard a = attackers_to(to, occ);
      if (var->flyingGeneral)
      {
          if (a & pieces(stm, KING))
              a |= attacks_bb(stm, ROOK, to, occ & ~pieces(ROOK)) & pieces(~stm, KING);
          if (a & pieces(~stm, KING))
              a |= attacks_bb(~stm, ROOK, to, occ & ~pieces(ROOK)) & pieces(stm, KING);
      }
      if (var->diagonalGeneral)
      {
          if (a & pieces(stm, KING))
              a |= attacks_bb(stm, BISHOP, to, occ & ~pieces(BISHOP)) & pieces(~stm, KING);
          if (a & pieces(~stm, KING))
              a |= attacks_bb(~stm, BISHOP, to, occ & ~pieces(BISHOP)) & pieces(stm, KING);
      }
      return a;
  };

  Bitboard attackers = update_attackers(occupied);
  Bitboard stmAttackers, bb;
  int res = 1;

  // Janggi cannons can not capture each other
  if (type_of(moved_piece(m)) == JANGGI_CANNON && !(attackers & pieces(~stm) & ~pieces(JANGGI_CANNON)))
      attackers &= ~pieces(~stm, JANGGI_CANNON);

  while (true)
  {
      stm = ~stm;
      attackers &= occupied;

      // If stm has no more attackers then give up: stm loses
      if (!(stmAttackers = attackers & pieces(stm)))
          break;

      // Don't allow pinned pieces to attack as long as there are
      // pinners on their original square.
      if (pinners(~stm) & occupied)
          stmAttackers &= ~blockers_for_king(stm);

      // Ignore distant sliders
      if (walling_rule() == DUCK)
          stmAttackers &= attacks_bb<KING>(to) | ~(pieces(BISHOP, ROOK) | pieces(QUEEN));

      if (!stmAttackers)
          break;

      res ^= 1;

      bb = stmAttackers & ~pieces(stm, KING);
      if (bb)
      {
          // Pick the least valuable available attacker among all piece types.
          Square leastSq = lsb(bb);
          int leastValue = CapturePieceValue[MG][piece_on(leastSq)];
          for (Bitboard b = bb & ~square_bb(leastSq); b; )
          {
              Square s = pop_lsb(b);
              int v = CapturePieceValue[MG][piece_on(s)];
              if (v < leastValue)
              {
                  leastValue = v;
                  leastSq = s;
              }
          }

          if ((swap = leastValue - swap) < res)
              break;

          occupied ^= square_bb(leastSq);
          attackers = update_attackers(occupied);
      }

      else // KING
           // If we "capture" with the king but opponent still has attackers,
           // reverse the result.
          return (attackers & ~pieces(stm)) ? res ^ 1 : res;
  }

  return bool(res);
}

/// Position::is_optional_game_end() tests whether the position may end the game by
/// 50-move rule, by repetition, or a variant rule that allows a player to claim a game result.

bool Position::n_fold_game_end(Value& result, int ply, int target) const {

  if (target <= 0)
      return false;

  int end = captures_to_hand() ? st->pliesFromNull : std::min(st->rule50, st->pliesFromNull);

  if (end < 4)
      return false;

  StateInfo* stp = st->previous->previous;
  int cnt = 0;
  bool perpetualThem = var->perpetualCheckIllegal && st->checkersBB && stp->checkersBB;
  bool perpetualUs = var->perpetualCheckIllegal && st->previous->checkersBB && stp->previous->checkersBB;
  Bitboard chaseThem = undo_move_board(st->chased, st->previous->move) & stp->chased;
  Bitboard chaseUs = undo_move_board(st->previous->chased, stp->move) & stp->previous->chased;
  int moveRepetition = var->moveRepetitionIllegal
                        && type_of(st->move) == NORMAL
                        && !st->previous->checkersBB && !stp->previous->checkersBB
                        && (board_bb(~side_to_move(), type_of(piece_on(to_sq(st->move)))) & board_bb(side_to_move(), KING))
                        ? (stp->move == reverse_move(st->move) ? 2 : is_pass(stp->move) ? 1 : 0) : 0;

  for (int i = 4; i <= end; i += 2)
  {
      // Janggi repetition rule
      if (moveRepetition > 0)
      {
          if (i + 1 <= end && stp->previous->previous->previous->checkersBB)
              moveRepetition = 0;
          else if (moveRepetition < 4)
          {
              if (stp->previous->previous->move == reverse_move((moveRepetition == 1 ? st : stp)->move))
                  moveRepetition++;
              else
                  moveRepetition = 0;
          }
          else
          {
              assert(moveRepetition == 4);
              if (!stp->previous->previous->capturedPiece && from_sq(stp->move) == to_sq(stp->previous->previous->move))
              {
                  result = VALUE_MATE;
                  return true;
              }
              else
                  moveRepetition = 0;
          }
      }
      // Chased pieces are empty when there is no previous move
      if (i != st->pliesFromNull)
          chaseThem = undo_move_board(chaseThem, stp->previous->move) & stp->previous->previous->chased;
      stp = stp->previous->previous;
      perpetualThem &= bool(stp->checkersBB);

      // Return a draw score if a position repeats once earlier but strictly
      // after the root, or repeats twice before or at the root.
      if (   stp->key == st->key
          && ++cnt + 1 >= (ply > i && !moveRepetition && !chaseUs && !chaseThem && !perpetualUs && !perpetualThem ? 2 : target))
      {
          result = convert_mate_value(  (perpetualThem || perpetualUs) ? (!perpetualUs ? VALUE_MATE : !perpetualThem ? -VALUE_MATE : VALUE_DRAW)
                                      : (chaseThem || chaseUs) ? (!chaseUs ? VALUE_MATE : !chaseThem ? -VALUE_MATE : VALUE_DRAW)
                                      : var->nFoldValueAbsolute && sideToMove == BLACK ? -var->nFoldValue
                                      : var->nFoldValue, ply);
          if (result == VALUE_DRAW && var->materialCounting)
              result = convert_mate_value(material_counting_result(), ply);
          return true;
      }

      if (i + 1 <= end)
      {
          perpetualUs &= bool(stp->previous->checkersBB);
          chaseUs = undo_move_board(chaseUs, stp->move) & stp->previous->chased;
      }
  }

  return false;
}

bool Position::is_optional_game_end(Value& result, int ply, int countStarted) const {

  // n-move rule
  if (n_move_rule() && st->rule50 > (2 * n_move_rule() - 1) && (!checkers() || MoveList<LEGAL>(*this).size()))
  {
      int offset = 0;
      if (var->chasingRule == AXF_CHASING && st->pliesFromNull >= 20)
      {
          int end = std::min(st->rule50, st->pliesFromNull);
          StateInfo* stp = st;
          int checkThem = bool(stp->checkersBB);
          int checkUs = bool(stp->previous->checkersBB);
          for (int i = 2; i < end; i += 2)
          {
              stp = stp->previous->previous;
              checkThem += bool(stp->checkersBB);
              checkUs += bool(stp->previous->checkersBB);
          }
          offset = 2 * std::max(std::max(checkThem, checkUs) - 10, 0) + 20 * (CurrentProtocol == UCCI || CurrentProtocol == UCI_CYCLONE);
      }
      if (st->rule50 - offset > (2 * n_move_rule() - 1))
      {
          result = var->materialCounting ? convert_mate_value(material_counting_result(), ply) : VALUE_DRAW;
          return true;
      }
  }

  if (n_fold_game_end(result, ply, n_fold_rule()))
      return true;

  // counting rules
  if (   counting_rule()
      && st->countingLimit
      && counting_ply(countStarted) > counting_limit(countStarted)
      && (!checkers() || MoveList<LEGAL>(*this).size()))
  {
      result = VALUE_DRAW;
      return true;
  }

  // sittuyin stalemate due to optional promotion (3.9 c.7)
  if (   sittuyin_promotion()
      && count<ALL_PIECES>(sideToMove) == 2
      && count<PAWN>(sideToMove) == 1
      && !checkers())
  {
      bool promotionsOnly = true;
      for (const auto& m : MoveList<LEGAL>(*this))
          if (type_of(m) != PROMOTION)
          {
              promotionsOnly = false;
              break;
          }
      if (promotionsOnly)
      {
          result = VALUE_DRAW;
          return true;
      }
  }

  return false;
}

/// Position::is_immediate_game_end() tests whether the position ends the game
/// immediately by a variant rule, i.e., there are no more legal moves.
/// It does not detect stalemates.

bool Position::is_immediate_game_end(Value& result, int ply) const {

  // Pseudo-royal loss
  // Some variants transfer royal status across a family of piece types.
  // If a special capture removes the current pseudo-royal, the game ends
  // immediately even though the family may include other off-board types.
  if (pseudo_royal_value() != VALUE_NONE)
      for (Color c : { ~sideToMove, sideToMove })
          if (!(st->pseudoRoyals & pieces(c)))
          {
              result = c == sideToMove ? pseudo_royal_value(ply) : -pseudo_royal_value(ply);
              return true;
          }

  // Extinction
  // Extinction does not apply for pseudo-royal pieces in normal capture rules,
  // because they cannot be captured directly.
  if (extinction_value() != VALUE_NONE)
  {
      PieceSet extinctTargets = extinction_piece_types();
      if (!blast_on_capture())
          extinctTargets &= ~pseudo_royal_types();

      for (Color c : { ~sideToMove, sideToMove })
      {
          bool allTypesExtinct = true;
          bool anyTypeExtinct = false;
          for (PieceSet ps = extinctTargets; ps;)
          {
              PieceType pt = pop_lsb(ps);
              bool extinct = count_with_hand(c, pt) <= var->extinctionPieceCount
                          && count_with_hand(~c, pt) >= var->extinctionOpponentPieceCount + (extinction_claim() && c == sideToMove);
              anyTypeExtinct |= extinct;
              allTypesExtinct &= extinct;
          }

          if ((var->extinctionAllPieceTypes ? allTypesExtinct : anyTypeExtinct) && extinctTargets)
          {
              result = c == sideToMove ? extinction_value(ply) : -extinction_value(ply);
              return true;
          }
      }
  }
  // capture the flag
  // A flag win by the side to move is only possible if flagMove is enabled
  // and they already reached the flag region the move before.
  // In the case both colors reached it, it is a draw if white was first.
  if (flag_move() && flag_reached(sideToMove))
  {
      result = sideToMove == WHITE && flag_reached(BLACK) ? VALUE_DRAW : mate_in(ply);
      return true;
  }
  // A direct flag win is possible if the opponent does not get an extra flag move
  // or we can detect early for kings that they won't be able to reach the flag region
  // Note: This condition has to be after the above, since both might be true e.g. in racing kings.
  if (   (!flag_move() || (flag_piece(sideToMove) == KING && !allow_checks())) // king-only shortcut is invalid when kings are capturable
       && flag_reached(~sideToMove))
  {
      bool gameEnd = true;
      // Check whether king can move to CTF zone (racing kings) to draw
      if (   flag_move() && sideToMove == BLACK && !checkers() && count<KING>(sideToMove)
          && (flag_region(sideToMove) & attacks_from(sideToMove, KING, square<KING>(sideToMove))))
      {
          assert(flag_piece(sideToMove) == KING);
          for (const auto& m : MoveList<NON_EVASIONS>(*this))
              if (type_of(moved_piece(m)) == KING && (flag_region(sideToMove) & to_sq(m)) && legal(m))
              {
                  gameEnd = false;
                  break;
              }
      }
      if (gameEnd)
      {
          result = mated_in(ply);
          return true;
      }
  }

  // Castle chess
  if (var->castlingWins)
  {
      if (st->pliesFromNull > 0 && type_of(st->move) == CASTLING)
      {
          // check for victory first, because castling also removes castling rights.
          CastlingRights justCastled = ~sideToMove & ((from_sq(st->move) < to_sq(st->move)) ? KING_SIDE : QUEEN_SIDE);
          if (var->castlingWins & justCastled)
          {
              result = mated_in(ply);
              return true;
          }
      }
      // We check the opponent side first, because a rook capturing a rook could remove both sides castling rights,
      // which should likely be seen as losing, analogous to extinction rules.
      for (Color c : { ~sideToMove, sideToMove })
          if ((c & var->castlingWins) && !(c & var->castlingWins & st->castlingRights))
          {
              // player permanently losing castling rights. either through moving a castling piece,
              // or having their rook captured.
              result = c == sideToMove ? mated_in(ply) : mate_in(ply);
              return true;
          }
  }

  // nCheck
  if (check_counting() && checks_remaining(~sideToMove) == 0)
  {
      result = mated_in(ply);
      return true;
  }

  // Immediate n-move adjudication (e.g. FIDE 75-move rule)
  if (   n_move_rule_immediate()
      && st->rule50 > (2 * n_move_rule_immediate() - 1)
      && (!checkers() || MoveList<LEGAL>(*this).size()))
  {
      result = var->materialCounting ? convert_mate_value(material_counting_result(), ply) : VALUE_DRAW;
      return true;
  }

  // Immediate n-fold adjudication (e.g. FIDE fivefold repetition)
  if (n_fold_game_end(result, ply, n_fold_rule_immediate()))
      return true;

  if (points_counting() && points_goal() > 0)
  {
      //Handle the case where both players met the goal.
      if (st->pointsCount[~sideToMove]>=points_goal() && st->pointsCount[sideToMove]>=points_goal())
      {
          //If both players are drawn on points, or the rules say it's a draw, then declare draw.
          if ((st->pointsCount[~sideToMove] == st->pointsCount[sideToMove]) || (var->pointsGoalSimulValue == VALUE_DRAW))
          {
              result = convert_mate_value(VALUE_DRAW, ply);
              return true;
          };
          //Otherwise pointsGoalSimulValue rules on ending, from perspective of player with most points.
          result = convert_mate_value(
            st->pointsCount[~sideToMove] > st->pointsCount[sideToMove] ?
            var->pointsGoalSimulValue : -var->pointsGoalSimulValue, ply);
          return true;
      };
      //Finally, rule on the simple cases.
      if (st->pointsCount[~sideToMove]>=points_goal())
      {
          result = convert_mate_value(var->pointsGoalValue, ply);
          return true;
      };
      if (st->pointsCount[sideToMove]>=points_goal())
      {
          result = convert_mate_value(-var->pointsGoalValue, ply);
          return true;
      };
  };

  if (connect_goal_by_type())
  {
      auto has_connect_goal = [&](Color c) {
          const auto& goal = connect_piece_goal_types(c);
          if (goal.empty())
              return false;

          if (!var->connectLines.empty() && goal.size() == var->connectLines.front().size())
          {
              for (const auto& line : var->connectLines)
              {
                  bool forward = true;
                  bool reverse = true;
                  for (size_t i = 0; i < goal.size(); ++i)
                  {
                      forward &= type_of(piece_on(line[i])) == goal[i] && color_of(piece_on(line[i])) == c;
                      reverse &= type_of(piece_on(line[goal.size() - 1 - i])) == goal[i] && color_of(piece_on(line[goal.size() - 1 - i])) == c;
                  }
                  if (forward || reverse)
                      return true;
              }
              return false;
          }

          for (Direction d : var->connectDirections)
          {
              Bitboard starts = pieces(goal.front());
              while (starts)
              {
                  Square s = pop_lsb(starts);
                  Bitboard cur = square_bb(s);
                  bool matched = true;
                  for (size_t i = 1; i < goal.size(); ++i)
                  {
                      cur = shift(d, cur);
                      if (!cur || type_of(piece_on(lsb(cur))) != goal[i])
                      {
                          matched = false;
                          break;
                      }
                  }
                  if (matched)
                      return true;
              }
          }
          return false;
      };

      bool prevMoverGoal = has_connect_goal(~sideToMove);
      bool stmGoal = has_connect_goal(sideToMove);
      if (prevMoverGoal && stmGoal)
      {
          result = convert_mate_value(VALUE_DRAW, ply);
          return true;
      }
      if (prevMoverGoal)
      {
          result = convert_mate_value(-connect_value(), ply);
          return true;
      }
      if (stmGoal)
      {
          result = convert_mate_value(connect_value(), ply);
          return true;
      }
  }

  //Calculate eligible pieces for connection once.
  Bitboard connectPieces = 0;
  for (PieceSet ps = connect_piece_types(); ps;){
      PieceType pt = pop_lsb(ps);
      connectPieces |= pieces(pt);
  };
  connectPieces &= pieces(~sideToMove);

  // Connect-n
  if ((connect_n() > 0) && (popcount(connectPieces) >= connect_n()))
  {
      if (!var->connectLines.empty() && connect_n() == int(var->connectLines.front().size()))
      {
          for (const auto& line : var->connectLines)
          {
              bool complete = true;
              for (Square s : line)
                  complete &= bool(connectPieces & square_bb(s));
              if (complete)
              {
                  result = convert_mate_value(-connect_value(), ply);
                  return true;
              }
          }
      }
      else
      {
          Bitboard b;

          for (Direction d : var->connectDirections)
          {
              b = connectPieces;
              for (int i = 1; i < connect_n() && b; i++)
                  b &= shift(d, b);
              if (b)
              {
                  result = convert_mate_value(-connect_value(), ply);
                  return true;
              }
          }
      }
  }

  if ((var->connectRegion1[~sideToMove] & connectPieces) && (var->connectRegion2[~sideToMove] & connectPieces))
  {
      Bitboard target = var->connectRegion2[~sideToMove];
      Bitboard current = var->connectRegion1[~sideToMove] & connectPieces;

      while (true) {
          Bitboard newBitboard = 0;
          for (Direction d : var->connectDirections) {
              newBitboard |= shift(d, current | newBitboard) & connectPieces; // the "| newBitboard" here probably saves a few loops
          }

          if (newBitboard & target) {
              // A connection has been made
              result = convert_mate_value(-connect_value(), ply);
              return true;
          }

          if (!(newBitboard & ~current)) {
              // The expansion got stuck; no further squares to explore
              break;
          }

          current |= newBitboard;
      }
  }

  if ((connect_nxn()) && (popcount(connectPieces) >= connect_nxn() * connect_nxn()))
  {
      Bitboard connectors = connectPieces;
      for (int i = 1; i < connect_nxn() && connectors; i++)
          connectors &= shift<SOUTH>(connectors) & shift<EAST>(connectors) & shift<SOUTH_EAST>(connectors);
      if (connectors)
      {
          result = convert_mate_value(-connect_value(), ply);
          return true;
      }
  }

  // Collinear-n
  if ((collinear_n() > 0) && (popcount(connectPieces) >= collinear_n())) {
      // Directional line scan from border starts using shift() to avoid wraps.
      auto next_square = [&](Square sq, Direction d) {
          Bitboard n = shift(d, square_bb(sq));
          return n ? Square(lsb(n)) : SQ_NONE;
      };

      for (Direction d : var->connectDirections) {
          for (Square s = SQ_A1; s <= SQ_MAX; ++s) {
              // Start only once per line: predecessor in -d must be off-board.
              if (next_square(s, -d) != SQ_NONE)
                  continue;

              int cnt = 0;
              for (Square cur = s; cur != SQ_NONE; cur = next_square(cur, d))
                  cnt += int(bool(connectPieces & cur));

              if (cnt >= collinear_n()) {
                  result = convert_mate_value(-connect_value(), ply);
                  return true;
              }
          }
      }
  }

  // Connect-Group (ends game if a player has N pieces in any connected group)
  if (connect_group() != 0 && (popcount(pieces(~sideToMove)) >= std::abs(connect_group()) || connect_group() == -1)) {
      Bitboard playerPieces = pieces(~sideToMove); // Pieces of the player who just moved
      Bitboard visited = 0;
      int targetGroupSize = connect_group();
      int totalPlayerPieces = popcount(playerPieces);

      if (targetGroupSize == -1) {
          targetGroupSize = totalPlayerPieces;
      }

      if (targetGroupSize > 0 && totalPlayerPieces >= targetGroupSize) { // Optimization: no need to check if not enough pieces
          while (playerPieces & ~visited) {
              Square start_sq = lsb(playerPieces & ~visited);
              Bitboard current_group = 0;
              std::deque<Square> q;

              q.push_back(start_sq);
              current_group |= start_sq;
              visited |= start_sq;
              int group_size = 0;

              while (!q.empty()) {
                  Square s = q.front();
                  q.pop_front();
                  group_size++;

                  for (Direction d : getConnectDirections()) {
                      Square next_sq = s + d;
                      // Guard against horizontal/diagonal edge wrap when adding raw directions.
                      if (!is_ok(next_sq) || distance(s, next_sq) != dist(d))
                          continue;
                      // Check if it's a player piece and not visited.
                      if ((square_bb(next_sq) & playerPieces) && !(square_bb(next_sq) & visited)) {
                          visited |= next_sq;
                          current_group |= next_sq;
                          q.push_back(next_sq);
                      }
                  }
              }

              if (group_size >= targetGroupSize) {
                  result = convert_mate_value(-connect_value(), ply); // ~sideToMove won
                  return true;
              }
          }
      }
  }

  // Check for bikjang rule (Janggi), double passing, or board running full
  if (   (st->pliesFromNull > 0 && ((st->bikjang && st->previous->bikjang) || ((st->pass && st->previous->pass) && !wall_or_move())))
      || (var->adjudicateFullBoard && !(~pieces() & board_bb())))
  {
      result = var->materialCounting ? convert_mate_value(material_counting_result(), ply) : VALUE_DRAW;
      return true;
  }

  // Tsume mode: Assume that side with king wins when not in check
  if (tsumeMode && !count<KING>(~sideToMove) && count<KING>(sideToMove) && !checkers())
  {
      result = mate_in(ply);
      return true;
  }

 // Check for nMoveHardLimitRule (full-move based, like FEN fullmove counter)
 int fullMoveNumber = 1 + (gamePly - (sideToMove == BLACK)) / 2;
 if (n_move_hard_limit_rule() > 0 && fullMoveNumber > n_move_hard_limit_rule()) {

     if (material_counting()) { // VALUE_NONE signifies using material counting
         result = convert_mate_value(material_counting_result(), ply);
     } else {
         result = convert_mate_value(n_move_hard_limit_rule_value(), ply);
     }
     return true;
 }

  // Failing to checkmate with virtual pieces is a loss
  if (two_boards() && !checkers())
  {
      int virtualCount = 0;
      for (PieceSet ps = piece_types(); ps;)
          virtualCount += std::max(-count_in_hand(~sideToMove, pop_lsb(ps)), 0);

      if (virtualCount > 0)
      {
          result = mate_in(ply);
          return true;
      }
  }
  if (var->prisonPawnPromotion &&
      (pawn_attacks_bb(~sideToMove, square<KING>(~sideToMove))
       & pieces(sideToMove, PAWN)
       & ~pawnCannotCheckZone[sideToMove]) ){
      result = mate_in(ply);
      return true;
  }

  return false;
}

// Position::chased() tests whether the last move was a chase.

Bitboard Position::chased() const {
  Bitboard b = 0;
  if (st->move == MOVE_NONE)
      return b;

  Bitboard pins = blockers_for_king(sideToMove);
  if (var->flyingGeneral)
  {
      Bitboard kingFilePieces = file_bb(file_of(square<KING>(~sideToMove))) & pieces(sideToMove);
      if ((kingFilePieces & pieces(sideToMove, KING)) && !more_than_one(kingFilePieces & ~pieces(KING)))
          pins |= kingFilePieces & ~pieces(KING);
  }
  if (var->diagonalGeneral)
  {
      Square enemyKing = square<KING>(~sideToMove);
      Square ourKing = square<KING>(sideToMove);
      int df = int(file_of(enemyKing)) - int(file_of(ourKing));
      int dr = int(rank_of(enemyKing)) - int(rank_of(ourKing));
      if (df == dr || df == -dr)
      {
          Bitboard kingDiagonalPieces = line_bb(enemyKing, ourKing) & pieces(sideToMove);
          if ((kingDiagonalPieces & pieces(sideToMove, KING))
              && !more_than_one(kingDiagonalPieces & ~pieces(KING)))
              pins |= kingDiagonalPieces & ~pieces(KING);
      }
  }
  auto addChased = [&](Square attackerSq, PieceType attackerType, Bitboard attacks) {
      if (attacks & ~b)
      {
          // Exclude attacks on unpromoted soldiers and checks
          attacks &= ~(pieces(sideToMove, KING, SOLDIER) ^ promoted_soldiers(sideToMove));
          // Attacks against stronger pieces
          if (attackerType == HORSE || attackerType == CANNON)
              b |= attacks & pieces(sideToMove, ROOK);
          if (attackerType == ELEPHANT || attackerType == FERS)
              b |= attacks & pieces(sideToMove, ROOK, CANNON, HORSE);
          // Exclude mutual/symmetric attacks
          // Exceptions:
          // - asymmetric pieces ("impaired horse")
          // - pins
          if (attackerType == HORSE && (PseudoAttacks[WHITE][FERS][attackerSq] & pieces()))
          {
              Bitboard horses = attacks & pieces(sideToMove, attackerType);
              while (horses)
              {
                  Square s = pop_lsb(horses);
                  if (attacks_bb(sideToMove, attackerType, s, pieces()) & attackerSq)
                      attacks ^= s;
              }
          }
          else
              attacks &= ~pieces(sideToMove, attackerType) | pins;
          // Attacks against potentially unprotected pieces
          while (attacks)
          {
              Square s = pop_lsb(attacks);
              Bitboard roots = attackers_to(s, pieces() ^ attackerSq, sideToMove) & ~pins;
              if (!roots
                  || (var->flyingGeneral && roots == pieces(sideToMove, KING)
                      && (attacks_bb(sideToMove, ROOK, square<KING>(~sideToMove), pieces() ^ attackerSq) & s))
                  || (var->diagonalGeneral && roots == pieces(sideToMove, KING)
                      && (attacks_bb(sideToMove, BISHOP, square<KING>(~sideToMove), pieces() ^ attackerSq) & s)))
                  b |= s;
          }
      }
  };

  // Direct attacks
  Square from = from_sq(st->move);
  Square to = to_sq(st->move);
  PieceType movedPiece = type_of(piece_on(to));
  if (movedPiece != KING && movedPiece != SOLDIER)
  {
      Bitboard directAttacks = attacks_from(~sideToMove, movedPiece, to) & pieces(sideToMove);
      // Only new attacks count. This avoids expensive comparison of previous and new attacks.
      if (movedPiece == ROOK || movedPiece == CANNON)
          directAttacks &= ~line_bb(from, to);
      addChased(to, movedPiece, directAttacks);
  }

  // Discovered attacks
  Bitboard discoveryCandidates =  (PseudoAttacks[WHITE][WAZIR][from] & pieces(~sideToMove, HORSE))
                                | (PseudoAttacks[WHITE][FERS][from] & pieces(~sideToMove, ELEPHANT))
                                | (PseudoAttacks[WHITE][ROOK][from] & pieces(~sideToMove, CANNON, ROOK))
                                | (PseudoAttacks[WHITE][ROOK][to] & pieces(~sideToMove, CANNON));
  while (discoveryCandidates)
  {
      Square s = pop_lsb(discoveryCandidates);
      PieceType discoveryPiece = type_of(piece_on(s));
      Bitboard discoveries =   pieces(sideToMove)
                            &  attacks_bb(~sideToMove, discoveryPiece, s, pieces())
                            & ~attacks_bb(~sideToMove, discoveryPiece, s, (captured_piece() ? pieces() : pieces() ^ to) ^ from);
      addChased(s, discoveryPiece, discoveries);
  }

  // Changes in real roots and discovered checks
  if (st->pliesFromNull > 0)
  {
      // Fake roots
      Bitboard newPins = st->blockersForKing[sideToMove] & ~st->previous->blockersForKing[sideToMove] & pieces(sideToMove);
      while (newPins)
      {
          Square s = pop_lsb(newPins);
          PieceType pinnedPiece = type_of(piece_on(s));
          Bitboard fakeRooted =  pieces(sideToMove)
                               & ~(pieces(sideToMove, KING, SOLDIER) ^ promoted_soldiers(sideToMove))
                               & attacks_bb(sideToMove, pinnedPiece, s, pieces());
          while (fakeRooted)
          {
              Square s2 = pop_lsb(fakeRooted);
              if (attackers_to(s2, ~sideToMove) & ~blockers_for_king(~sideToMove))
                  b |= s2;
          }
      }
      // Discovered checks
      Bitboard newDiscoverers = st->blockersForKing[sideToMove] & ~st->previous->blockersForKing[sideToMove] & pieces(~sideToMove);
      while (newDiscoverers)
      {
          Square s = pop_lsb(newDiscoverers);
          PieceType discoveryPiece = type_of(piece_on(s));
          Bitboard discoveryAttacks = attacks_from(~sideToMove, discoveryPiece, s) & pieces(sideToMove);
          // Include all captures except where the king can pseudo-legally recapture
          b |= discoveryAttacks & ~attacks_from(sideToMove, KING, square<KING>(sideToMove));
          // Include captures where king can not legally recapture
          discoveryAttacks &= attacks_from(sideToMove, KING, square<KING>(sideToMove));
          while (discoveryAttacks)
          {
              Square s2 = pop_lsb(discoveryAttacks);
              if (attackers_to(s2, pieces() ^ s ^ square<KING>(sideToMove), ~sideToMove) & ~square_bb(s))
                  b |= s2;
          }
      }
  }

  return b;
}

// Position::has_repeated() tests whether there has been at least one repetition
// of positions since the last capture or pawn move.

bool Position::has_repeated() const {

    StateInfo* stc = st;
    int end = captures_to_hand() ? st->pliesFromNull : std::min(st->rule50, st->pliesFromNull);
    while (end-- >= 4)
    {
        if (stc->repetition)
            return true;

        stc = stc->previous;
    }
    return false;
}


/// Position::has_game_cycle() tests if the position has a move which draws by repetition,
/// or an earlier position has a move that directly reaches the current position.

bool Position::has_game_cycle(int ply) const {

  int j;

  int end = captures_to_hand() ? st->pliesFromNull : std::min(st->rule50, st->pliesFromNull);

  if (end < 3 || var->nFoldValue != VALUE_DRAW || var->perpetualCheckIllegal || var->materialCounting || var->moveRepetitionIllegal || walling_rule() == DUCK)
    return false;

  bool useBoardKey = captures_to_hand();
  Key originalKey = useBoardKey ? st->boardKey : st->key;
  StateInfo* stp = st->previous;

  for (int i = 3; i <= end; i += 2)
  {
      stp = stp->previous->previous;

      Key moveKey = originalKey ^ (useBoardKey ? stp->boardKey : stp->key);
      if (   (j = H1(moveKey), cuckoo[j] == moveKey)
          || (j = H2(moveKey), cuckoo[j] == moveKey))
      {
          Move move = cuckooMove[j];
          Square s1 = from_sq(move);
          Square s2 = to_sq(move);

          if (!((between_bb(s1, s2) ^ s2) & pieces()))
          {
              if (ply > i)
                  return true;

              // For nodes before or at the root, check that the move is a
              // repetition rather than a move to the current position.
              // In the cuckoo table, both moves Rc1c5 and Rc5c1 are stored in
              // the same location, so we have to select which square to check.
              Square checkSq = empty(s1) ? s2 : s1;
              if (empty(checkSq) || color_of(piece_on(checkSq)) != side_to_move())
                  continue;

              // For repetitions before or at the root, require one more
              if (useBoardKey ? stp->boardRepetition : stp->repetition)
                  return true;
          }
      }
  }
  return false;
}

bool Position::see_pruning_unreliable() const {

  return points_counting()
      || points_goal() > 0
      || extinction_value() != VALUE_NONE
      || flag_region(WHITE) || flag_region(BLACK)
      || var->castlingWins
      || connect_n() > 0
      || connect_nxn() > 0
      || collinear_n() > 0
      || var->connectRegion1[WHITE] || var->connectRegion2[WHITE]
      || var->connectRegion1[BLACK] || var->connectRegion2[BLACK]
      || !connect_piece_goal_types(WHITE).empty()
      || !connect_piece_goal_types(BLACK).empty()
      || connect_group() != 0;
}


/// Position::count_limit() returns the counting limit in full moves.

int Position::count_limit(Color sideToCount) const {

  assert(counting_rule());

  switch (counting_rule())
  {
  case MAKRUK_COUNTING:
      // No counting for side to move
      if (count<PAWN>() || count<ALL_PIECES>(~sideToCount) == 1)
          return 0;
      // Board's honor rule
      if (count<ALL_PIECES>(sideToCount) > 1)
          return 64;
      // Pieces' honor rule
      if (count<ROOK>(~sideToCount) > 1)
          return 8;
      if (count<ROOK>(~sideToCount) == 1)
          return 16;
      if (count<KHON>(~sideToCount) > 1)
          return 22;
      if (count<KNIGHT>(~sideToCount) > 1)
          return 32;
      if (count<KHON>(~sideToCount) == 1)
          return 44;

      return 64;

  case CAMBODIAN_COUNTING:
      // No counting for side to move
      if (count<ALL_PIECES>(sideToCount) > 3 || count<ALL_PIECES>(~sideToCount) == 1)
          return 0;
      // Board's honor rule
      if (count<ALL_PIECES>(sideToCount) > 1)
          return 63;
      // Pieces' honor rule
      if (count<PAWN>())
          return 0;
      if (count<ROOK>(~sideToCount) > 1)
          return 7;
      if (count<ROOK>(~sideToCount) == 1)
          return 15;
      if (count<KHON>(~sideToCount) > 1)
          return 21;
      if (count<KNIGHT>(~sideToCount) > 1)
          return 31;
      if (count<KHON>(~sideToCount) == 1)
          return 43;

      return 63;

  case ASEAN_COUNTING:
      if (count<PAWN>() || count<ALL_PIECES>(sideToCount) > 1)
          return 0;
      if (count<ROOK>(~sideToCount))
          return 16;
      if (count<KHON>(~sideToCount))
          return 44;
      if (count<KNIGHT>(~sideToCount))
          return 64;

      return 0;

  default:
      assert(false);
      return 0;
  }

}


/// Position::flip() flips position with the white and black sides reversed. This
/// is only useful for debugging e.g. for finding evaluation symmetry bugs.

void Position::flip() {

  string f, token;
  std::stringstream ss(fen());

  for (Rank r = max_rank(); r >= RANK_1; --r) // Piece placement
  {
      std::getline(ss, token, r > RANK_1 ? '/' : ' ');
      size_t bracketPos = token.find('[');
      if (bracketPos != string::npos)
      {
          f.insert(0, token.substr(0, bracketPos) + (f.empty() ? "" : "/"));
          f += token.substr(bracketPos);
      }
      else
          f.insert(0, token + (f.empty() ? "" : "/"));
  }
  f += " ";

  ss >> token; // Active color
  f += (token == "w" ? "B " : "W "); // Will be lowercased later

  ss >> token; // Castling availability
  f += token + " ";

  std::transform(f.begin(), f.end(), f.begin(),
                 [](char c) {
                     unsigned char uc = static_cast<unsigned char>(c);
                     return char(std::islower(uc) ? std::toupper(uc) : std::tolower(uc));
                 });

  ss >> token; // En passant square
  if (token == "-")
      f += token;
  else
  {
      File epFile = File(token[0] - 'a');
      Rank epRank = Rank(std::atoi(token.substr(1).c_str()) - 1);
      Square ep = make_square(epFile, epRank);
      f += char('a' + file_of(ep));
      f += std::to_string(relative_rank(~side_to_move(), ep, max_rank()) + 1);
  }

  std::getline(ss, token); // Half and full moves
  f += token;

  set(variant(), f, is_chess960(), st, this_thread());

  assert(pos_is_ok());
}

void Position::updatePawnCheckZone() {
    if (!var->prisonPawnPromotion) {
        pawnCannotCheckZone[WHITE] = Bitboard(0);
        pawnCannotCheckZone[BLACK] = Bitboard(0);
        return;
    }
    for (Color color : { BLACK, WHITE }) {
        if (count<KING>(~color) == 0) {
            pawnCannotCheckZone[color] = Bitboard(0);
        } else {
            bool canPromotion = false;
            for (PieceSet prom = promotion_piece_types(color) & rescueFor(PAWN); prom; ) {
                PieceType pt = pop_lsb(prom);
                if (count_in_prison(~color, pt) > 0) {
                    canPromotion = true;
                    break;
                }
            }
            Bitboard pz = promotion_zone(color, PAWN);
            pawnCannotCheckZone[color] = canPromotion
                    ? Bitboard(0)
                    : color == WHITE ? shift(SOUTH, pz) : shift(NORTH, pz);
        }
    }
}

/// Position::pos_is_ok() performs some consistency checks for the
/// position object and raises an asserts if something wrong is detected.
/// This is meant to be helpful when debugging.

bool Position::pos_is_ok() const {

  constexpr bool Fast = true; // Quick (default) or full check?

  if (   (sideToMove != WHITE && sideToMove != BLACK)
      || (count<KING>(WHITE) && piece_on(square<KING>(WHITE)) != make_piece(WHITE, KING))
      || (count<KING>(BLACK) && piece_on(square<KING>(BLACK)) != make_piece(BLACK, KING))
      || (ep_squares() & ~(var->enPassantRegion[WHITE] | var->enPassantRegion[BLACK])))
      assert(0 && "pos_is_ok: Default");

  if (Fast)
      return true;

  if (   pieceCount[make_piece(~sideToMove, KING)]
      && (attackers_to_king(square<KING>(~sideToMove), sideToMove) & pieces(sideToMove)))
      assert(0 && "pos_is_ok: Kings");

  if (   pieceCount[make_piece(WHITE, PAWN)] > 64
      || pieceCount[make_piece(BLACK, PAWN)] > 64)
      assert(0 && "pos_is_ok: Pawns");

  if (   (pieces(WHITE) & pieces(BLACK))
      || (pieces(WHITE) | pieces(BLACK)) != pieces()
      || popcount(pieces(WHITE)) > 64
      || popcount(pieces(BLACK)) > 64)
      assert(0 && "pos_is_ok: Bitboards");

  for (PieceType p1 = PAWN; p1 <= KING; ++p1)
      for (PieceType p2 = PAWN; p2 <= KING; ++p2)
          if (p1 != p2 && (pieces(p1) & pieces(p2)))
              assert(0 && "pos_is_ok: Bitboards");

  StateInfo si = *st;
  ASSERT_ALIGNED(&si, Eval::NNUE::CacheLineSize);

  set_state(&si);
  if (std::memcmp(&si, st, sizeof(StateInfo)))
      assert(0 && "pos_is_ok: State");

  for (Color c : {WHITE, BLACK})
      for (PieceType pt = PAWN; pt <= KING; ++pt)
      {
          Piece pc = make_piece(c, pt);
          if (   pieceCount[pc] != popcount(pieces(c, pt))
              || pieceCount[pc] != std::count(board, board + SQUARE_NB, pc))
              assert(0 && "pos_is_ok: Pieces");
      }

  for (Color c : { WHITE, BLACK })
      for (CastlingRights cr : {c & KING_SIDE, c & QUEEN_SIDE})
      {
          if (!can_castle(cr))
              continue;

          if (   !(castling_rook_pieces(c) & type_of(piece_on(castlingRookSquare[cr])))
              || castlingRightsMask[castlingRookSquare[cr]] != cr
              || (count<KING>(c) && (castlingRightsMask[square<KING>(c)] & cr) != cr))
              assert(0 && "pos_is_ok: Castling");
      }

  return true;
}

PieceType Position::committed_piece_type(Move m, bool castlingRook) const {
    PieceType result = NO_PIECE_TYPE;
    if (commit_gates()) {
        Square from = from_sq(m);
        Rank r = rank_of(from);
        if (castlingRook){
            if (type_of(m) == CASTLING){
                from = to_sq(m);
            } else {
                from = SQ_NONE;
            }
        }
        if (from != SQ_NONE){
            if (r == RANK_1){
                result = committed_piece_type(WHITE, file_of(from));
            } else if (r == max_rank()){
                result = committed_piece_type(BLACK, file_of(from));
            } else{
                assert(false);
            }
        }
    }
    return result;
}

} // namespace Stockfish
