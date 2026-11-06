#include <bitset>
#include <iostream>
#include <random>

#include "../src/bitboard.h"

namespace Stockfish {
uint8_t PopCnt16[1 << 16];
}

using namespace Stockfish;
using Reference = std::bitset<BOARD_BITS>;

Reference reference(Bitboard b) {
    Reference r;
    for (int i = 0; i < BOARD_BITS; ++i)
        r[i] = (b.b64[Bitboard::WordCount - 1 - i / 64] >> (i % 64)) & 1;
    return r;
}

Reference subtract(Reference a, Reference b) {
    Reference r;
    bool borrow = false;
    for (int i = 0; i < BOARD_BITS; ++i) {
        r[i] = a[i] ^ b[i] ^ borrow;
        borrow = (!a[i] && (b[i] || borrow)) || (b[i] && borrow);
    }
    return r;
}

Reference multiply(Reference a, Reference b) {
    Reference r;
    for (int i = 0; i < BOARD_BITS; ++i)
        if (a[i]) {
            Reference add = b << i;
            bool carry = false;
            for (int j = 0; j < BOARD_BITS; ++j) {
                bool bit = r[j] ^ add[j] ^ carry;
                carry = (r[j] && add[j]) || (carry && (r[j] || add[j]));
                r[j] = bit;
            }
        }
    return r;
}

void check(Bitboard b) {
    Reference r = reference(b);
    assert(popcount(b) == int(r.count()));
    assert(bool(b) == r.any());
    if (b) {
        int first = 0, last = BOARD_BITS - 1;
        while (!r[first]) ++first;
        while (!r[last]) --last;
        assert(int(lsb(b)) == first && int(msb(b)) == last);
        assert(reference(least_significant_square_bb(b)).count() == 1);
        Bitboard copy = b;
        assert(int(pop_lsb(copy)) == first);
        r[first] = false;
        assert(reference(copy) == r);
    }
}

void moves() {
    for (int s = 0; s < SQUARE_NB; ++s) {
        Square from = Square(s), to = Square(SQUARE_NB - 1 - s), via = SQ_MAX;
        Move m = make_move(from, to);
        assert(from_sq(m) == from && to_sq(m) == to && type_of(m) == NORMAL);
        for (MoveType mt : {EN_PASSANT, CASTLING, PROMOTION, PIECE_PROMOTION,
                            PIECE_DEMOTION, SPECIAL, SWAP, STACK, UNSTACK, LASER_FIRE}) {
            m = Move(move_bits(make<PROMOTION>(from, to, QUEEN))
                     - uint64_t(PROMOTION) + uint64_t(mt));
            assert(from_sq(m) == from && to_sq(m) == to && type_of(m) == mt);
        }
        m = make_drop(to, PAWN, QUEEN);
        assert(to_sq(m) == to && dropped_piece_type(m) == QUEEN && in_hand_piece_type(m) == PAWN);
        m = make_drop_pair(from, to, PAWN, QUEEN);
        assert(to_sq(m) == from && from_sq(m) == to && type_of(m) == DROP2);
        m = make_insert(from, to, PAWN, QUEEN);
        assert(from_sq(m) == from && to_sq(m) == to && type_of(m) == INSERT);
        m = make_gating<NORMAL>(from, to, QUEEN, via);
        assert(gating_square(m) == via && is_gating(m) && !has_extended_special_payload(m));
        m = make_pull(from, to, via);
        assert(pull_square(m) == via && from_sq(m) == from && to_sq(m) == to);
        m = make_two_step(from, via, to, true);
        assert(is_two_leg(m) && via_sq(m) == via && from_sq(m) == from && to_sq(m) == to);
        assert(!is_gating(m));
        m = make_hook(from, via, to, true);
        assert(is_two_leg(m) && via_sq(m) == via && from_sq(m) == from && to_sq(m) == to);
        m = make_promotion_potion(from, to, QUEEN, 1, via);
        assert(potion_target_square(m) == via && promotion_type(m) == QUEEN && potion_type(m) == 1);
        assert(make_square(file_of(from), rank_of(from)) == from);
    }
    assert(MOVE_NONE != MOVE_NULL && !is_ok(SQ_NONE));
}

void scores() {
    std::mt19937 rng(22);
    const int range = BOARD_BITS == 512 ? 1000000 : 1000;
    std::uniform_int_distribution<int> value(-range, range);
    for (int n = 0; n < 1000; ++n) {
        int am = value(rng), ae = value(rng), bm = value(rng), be = value(rng);
        Score a = make_score(am, ae), b = make_score(bm, be);
        assert(mg_value(a) == am && eg_value(a) == ae);
        assert(mg_value(a + b) == am + bm && eg_value(a + b) == ae + be);
        assert(mg_value(a - b) == am - bm && eg_value(a - b) == ae - be);
        assert(mg_value(-a) == -am && eg_value(-a) == -ae);
        assert(mg_value(a * 5) == am * 5 && eg_value(a * 5) == ae * 5);
        assert(mg_value(Score(5 * a)) == am * 5 && eg_value(Score(5 * a)) == ae * 5);
        assert(mg_value(a / 5) == am / 5 && eg_value(a / 5) == ae / 5);
    }
}

int main() {
    for (int i = 0; i < (1 << 16); ++i) PopCnt16[i] = std::bitset<16>(i).count();
    for (int bit : {0, 1, 63, 64, 127, 128, 191, 192, 255, 256, 319, 320, 447, 448, 483, 511})
        if (bit < BOARD_BITS) check(Bitboard(1) << bit);
    check(Bitboard());
    check(~Bitboard());
    std::mt19937_64 rng(512);
    for (int n = 0; n < 1000; ++n) {
        Bitboard a, b;
        for (int word = 0; word < Bitboard::WordCount; ++word) {
            a.b64[word] = rng();
            b.b64[word] = rng();
        }
        Reference ar = reference(a), br = reference(b);
        check(a);
        assert(reference(a | b) == (ar | br));
        assert(reference(a & b) == (ar & br));
        assert(reference(a ^ b) == (ar ^ br));
        assert(reference(~a) == ~ar);
        assert(reference(a - b) == subtract(ar, br));
        assert(reference(-a) == subtract(Reference(), ar));
        assert((a == b) == (ar == br) && (a != b) == (ar != br));
        for (unsigned shift : {0, 1, 63, 64, 65, 127, 128, 191, 192, 193, 255, 256,
                               319, 320, 447, 448, 511, 512, 513, 1024}) {
            assert(reference(a << shift) == (ar << shift));
            assert(reference(a >> shift) == (ar >> shift));
        }
        assert(reference(a << -1).none() && reference(a >> -1).none());
        if (n < 20) assert(reference(a * b) == multiply(ar, br));
        assert(uint64_t(a) == a.b64[Bitboard::WordCount - 1]);
    }
    moves();
    scores();
    std::cout << BOARD_BITS << "-bit arithmetic and move encoding passed\n";
}
