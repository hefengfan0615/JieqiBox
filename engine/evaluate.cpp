/*
  Pikafish, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2022 The Pikafish developers (see AUTHORS file)

  Pikafish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Pikafish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <iostream>

#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "psqt.h"
#include "types.h"

namespace Stockfish {

namespace Eval {

namespace {

#define S(mg, eg) make_score(mg, eg)

Score ConnectedPieces[PIECE_TYPE_NB] = {
  S(0, 0), S(-16, 106), S(-4, 23), S(185, -3), S(-33, 93), S(-60, -8), S(84, -144)
};

Score ThreatByPawn[PIECE_TYPE_NB] = {
  S(0, 0), S(-11, 2), S(39, -26), S(-57, -4), S(-47, 64), S(20, 72), S(-8, -61)
};

Score ThreatByAdvisor[PIECE_TYPE_NB] = {
  S(0, 0), S(23, 4), S(11, 39), S(-66, 143), S(-35, -49), S(70, 78), S(26, 67)
};

Score ThreatByBishop[PIECE_TYPE_NB] = {
  S(0, 0), S(-6, 44), S(15, -56), S(13, -19), S(54, -82), S(-38, 14), S(-46, 47)
};

Score ThreatByMinor[PIECE_TYPE_NB] = {
  S(0, 0), S(-169, -94), S(-58, -109), S(73, -110), S(37, -89), S(-36, 319), S(-115, -96)
};

Score ThreatByRook[PIECE_TYPE_NB] = {
  S(0, 0), S(59, -229), S(-118, 45), S(-50, 84), S(56, 46), S(-13, -152), S(91, 22)
};

Score Hanging            = S(119, 9);
Score RestrictedPiece    = S(-14, 109);
Score ThreatByKing       = S(-135, -37);
Score ThreatByPawnPush   = S(-96, 277);
Score ThreatBySafePawn   = S(11, 39);
Score ProtectedByPawn    = S(51, -166);
Score RestrictedDarkPawn = S(87, -54);

#undef S

// Threshold for space evaluation
constexpr Value SpaceThreshold = Value(8943);

// Compute PSQT score for the position
Score compute_psq(const Position& pos) {
    Score score = SCORE_ZERO;
    for (Piece pc : {W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING}) {
        Bitboard b = pos.pieces(color_of(pc), type_of(pc));
        while (b) {
            Square s = pop_lsb(b);
            if (!pos.is_dark(s))
                score += PSQT::psq[pc][s];
        }
    }
    for (Piece pc : {B_ROOK, B_ADVISOR, B_CANNON, B_PAWN, B_KNIGHT, B_BISHOP, B_KING}) {
        Bitboard b = pos.pieces(color_of(pc), type_of(pc));
        while (b) {
            Square s = pop_lsb(b);
            if (!pos.is_dark(s))
                score += PSQT::psq[pc][s];
        }
    }
    return score;
}

// Evaluation class computes and stores attacks tables and other working data
class Evaluation {

public:
    Evaluation() = delete;
    explicit Evaluation(const Position& p) : pos(p) {}
    Evaluation& operator=(const Evaluation&) = delete;
    Value value(Material::Table& materialTable);

private:
    template<Color Us> void initialize();
    template<Color Us, PieceType Pt> Score pieces();
    template<Color Us> Score pair();
    template<Color Us> Score threats() const;
    template<Color Us> Score space() const;
    Value winnable(Score score) const;

    const Position& pos;

    // attackedBy[color][piece type] is a bitboard representing all squares
    // attacked by a given color and piece type.
    Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB];

    // attackedBy2[color] are the squares attacked by at least 2 units
    Bitboard attackedBy2[COLOR_NB];

    // Dark pieces by color and piece type
    Bitboard DarkPieces[COLOR_NB][PIECE_TYPE_NB];
};


// Evaluation::initialize() computes king and pawn attacks
template<Color Us>
void Evaluation::initialize() {

    constexpr Color     Them = ~Us;
    const Square        ksq  = pos.king_square(Us);
    constexpr Bitboard  LowRanks = (Us == WHITE ? Rank0BB | Rank1BB : Rank8BB | Rank9BB);

    // Initialize attackedBy[] for king and pawns
    attackedBy[Us][KING] = attacks_bb<KING>(ksq);
    attackedBy[Us][PAWN] = pawn_attacks_bb<Us>(pos.pieces(Us, PAWN));
    attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
    attackedBy2[Us] = attackedBy[Us][KING] & attackedBy[Us][PAWN];
    std::memset(DarkPieces, 0, sizeof DarkPieces);

    for (PieceType i = ROOK; i <= BISHOP; ++i) {
        Bitboard b = pos.pieces(Us, i);
        while (b) {
            Square s = pop_lsb(b);
            if (pos.is_dark(s)) {
                DarkPieces[Us][i] |= square_bb(s);
            }
        }
        DarkPieces[Us][ALL_PIECES] |= DarkPieces[Us][i];
    }
}


// Evaluation::pieces() scores pieces of a given color and type
template<Color Us, PieceType Pt>
Score Evaluation::pieces() {

    Bitboard b1 = pos.pieces(Us, Pt);
    Bitboard b;

    attackedBy[Us][Pt] = 0;

    while (b1)
    {
        Square s = pop_lsb(b1);

        // Find attacked squares, including x-ray attacks for bishops and rooks
        b = attacks_bb<Pt>(s, pos.pieces());

        if (pos.blockers_for_king(Us) & s)
            b &= line_bb(pos.king_square(Us), s);

        attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & b;
        attackedBy[Us][Pt] |= b;
        attackedBy[Us][ALL_PIECES] |= b;
    }
    return SCORE_ZERO;
}

template<Color Us>
Score Evaluation::pair() {
    Score score = SCORE_ZERO;
    for (PieceType i = ROOK; i <= BISHOP; ++i) {
        Bitboard b = attackedBy[Us][i];
        b &= pos.pieces(Us, i);
        if (popcount(b) >= 2) {
            score += ConnectedPieces[i];
        }
    }
    return score;
}

// Evaluation::threats() assigns bonuses according to the types of the
// attacking and the attacked pieces.
template<Color Us>
Score Evaluation::threats() const {

    constexpr Color     Them     = ~Us;
    constexpr Direction Up       = (Us == WHITE ? NORTH : SOUTH);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);

    Bitboard b, weak, defended, nonPawnEnemies, stronglyProtected, safe;
    Score score = SCORE_ZERO;

    // Non-pawn enemies
    nonPawnEnemies = pos.pieces(Them) & ~pos.pieces(PAWN);

    // Squares strongly protected by the enemy
    stronglyProtected =  attackedBy[Them][PAWN]
                       | (attackedBy2[Them] & ~attackedBy2[Us]);

    // Non-pawn enemies, strongly protected
    defended = nonPawnEnemies & stronglyProtected;

    // Enemies not strongly protected and under our attack
    weak = pos.pieces(Them) & ~stronglyProtected & attackedBy[Us][ALL_PIECES];

    // Bonus according to the kind of attacking pieces
    if (defended | weak)
    {
        b = (defended | weak) & (attackedBy[Us][KNIGHT] | attackedBy[Us][BISHOP]);
        while (b)
            score += ThreatByMinor[type_of(pos.piece_on(pop_lsb(b)))];

        b = weak & attackedBy[Us][ROOK];
        while (b)
            score += ThreatByRook[type_of(pos.piece_on(pop_lsb(b)))];

        if (weak & attackedBy[Us][KING])
            score += ThreatByKing;

        b =  ~attackedBy[Them][ALL_PIECES]
           | (nonPawnEnemies & attackedBy2[Us]);
        score += Hanging * popcount(weak & b);
    }

    // Bonus for restricting their piece moves
    b =   attackedBy[Them][ALL_PIECES]
       & ~stronglyProtected
       &  attackedBy[Us][ALL_PIECES];
    score += RestrictedPiece * popcount(b);

    // Protected or unattacked squares
    safe = ~attackedBy[Them][ALL_PIECES] | attackedBy[Us][ALL_PIECES];

    // Bonus for attacking enemy pieces with our relatively safe pawns
    b = pos.pieces(Us, PAWN) & safe;
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatBySafePawn * popcount(b);

    // Find squares where our pawns can push on the next move
    b  = shift<Up>(pos.pieces(Us, PAWN)) & ~pos.pieces();
    b |= shift<Up>(b & TRank3BB) & ~pos.pieces();

    // Keep only the squares which are relatively safe
    b &= ~attackedBy[Them][PAWN] & safe;

    // Bonus for safe pawn threats on the next move
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatByPawnPush * popcount(b);

    b = attackedBy[Us][PAWN];
    b &= DarkPieces[Us][PAWN];

    score += popcount(b) * ProtectedByPawn;

    b = pawn_attacks_bb<Us>(DarkPieces[Us][PAWN]) & attackedBy[Them][PAWN];
    score -= popcount(b) * RestrictedDarkPawn;

    for (PieceType i = ROOK; i <= BISHOP; ++i) {
        b = attackedBy[Us][PAWN] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByPawn[i];

        b = attackedBy[Us][ADVISOR] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByAdvisor[i];

        b = attackedBy[Us][BISHOP] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByBishop[i];
    }

    return score;
}


// Evaluation::space() computes a space evaluation for a given side
template<Color Us>
Score Evaluation::space() const {

    // Early exit if too few pieces
    if (pos.major_material(WHITE) + pos.major_material(BLACK) < SpaceThreshold)
        return SCORE_ZERO;

    constexpr Color Them     = ~Us;
    constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);
    constexpr Bitboard SpaceMask =
      Us == WHITE ? (FileDBB | FileEBB | FileFBB) & (Rank2BB | Rank3BB | Rank4BB)
                  : (FileDBB | FileEBB | FileFBB) & (Rank7BB | Rank6BB | Rank5BB);

    // Find the available squares for our pieces inside the area defined by SpaceMask
    Bitboard safe =   SpaceMask
                   & ~pos.pieces(Us, PAWN)
                   & ~pawn_attacks_bb<Them>(pos.pieces(Us, PAWN));

    // Find all squares which are at most three squares behind some friendly pawn
    Bitboard behind = pos.pieces(Us, PAWN);
    behind |= shift<Down>(behind);
    behind |= shift<Down+Down>(behind);

    // Compute space score
    int bonus = popcount(safe) + popcount(behind & safe & ~attackedBy[Them][ALL_PIECES]);
    int weight = pos.count<ALL_PIECES>(Us) - 3;
    Score score = make_score(bonus * weight * weight / 16, 0);

    return score;
}


// Evaluation::winnable() adjusts the midgame and endgame score components
Value Evaluation::winnable(Score score) const {
    int totalPieces = pos.count<ALL_PIECES>();
    int mgWeight = totalPieces * 1000 / 32;
    int egWeight = 1000 - mgWeight;
    return (mg_value(score) * mgWeight + eg_value(score) * egWeight) / 1000;
}


// Evaluation::value() is the main function of the class
Value Evaluation::value(Material::Table& materialTable) {

    // Compute PSQT and material imbalance
    Score score = compute_psq(pos) + Material::probe(pos, materialTable)->imbalance();

    // Main evaluation begins here
    initialize<WHITE>();
    initialize<BLACK>();

    score += pieces<WHITE, KNIGHT>() - pieces<BLACK, KNIGHT>()
          + pieces<WHITE, BISHOP>() - pieces<BLACK, BISHOP>()
          + pieces<WHITE, ROOK>() - pieces<BLACK, ROOK>()
          + pieces<WHITE, ADVISOR>() - pieces<BLACK, ADVISOR>()
          + pieces<WHITE, CANNON>() - pieces<BLACK, CANNON>();

    score += pair<WHITE>() - pair<BLACK>();

    score +=  threats<WHITE>() - threats<BLACK>()
            + space<  WHITE>() - space<  BLACK>();

    // Derive single value from mg and eg parts of score
    Value v = winnable(score);

    // Side to move point of view
    v = (pos.side_to_move() == WHITE ? v : -v);

    return v;
}

}  // namespace


// evaluate() is the evaluator for the outer world. It returns a static
// evaluation of the position from the point of view of the side to move.
Value Eval::evaluate(const Position& pos, Material::Table& materialTable) {

    Value v = Evaluation(pos).value(materialTable);

    // Damp down the evaluation linearly when shuffling
    v -= (v * pos.rule40_count()) / 217;

    // Guarantee evaluation does not hit the mate range
    v = std::clamp(v, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

    return v;
}


// trace() is like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term.
std::string Eval::trace(Position& pos) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);

    Value v = Evaluation(pos).value(materialTable);

    v = pos.side_to_move() == WHITE ? v : -v;

    ss << "HCE evaluation        " << v << " (white side)\n";
    ss << "Final evaluation       " << v << " (white side) [HCE]\n";

    return ss.str();
}

}  // namespace Eval

}  // namespace Stockfish