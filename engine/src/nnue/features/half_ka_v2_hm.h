/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

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

//Definition of input features HalfKAv2_hm of NNUE evaluation function

#ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
#define NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED

#include <array>
#include <cstdint>
#include <utility>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

// Feature HalfKAv2_hm: Combination of the position of own king and the
// position of pieces. Position mirrored such that king is always on d..e files.
class HalfKAv2_hm {

    // Unique number for each piece type on each square
    enum {
        // clang-format off
        PS_NONE      = 0,
        PS_W_ROOK    = 0,
        PS_B_ROOK    = 1 * SQUARE_NB,
        PS_W_ADVISOR = 2 * SQUARE_NB,
        PS_B_ADVISOR = 3 * SQUARE_NB,
        PS_W_CANNON  = 4 * SQUARE_NB,
        PS_B_CANNON  = 5 * SQUARE_NB,
        PS_W_PAWN    = 6 * SQUARE_NB,
        PS_B_PAWN    = 7 * SQUARE_NB,
        PS_W_KNIGHT  = 8 * SQUARE_NB,
        PS_B_KNIGHT  = 9 * SQUARE_NB,
        PS_W_BISHOP  = 10 * SQUARE_NB,
        PS_B_BISHOP  = 11 * SQUARE_NB,
        PS_KING      = 12 * SQUARE_NB,
        PS_NB        = 13 * SQUARE_NB
        // clang-format on
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB] = {
      // Convention: W - us, B - them
      // Viewed from other side, W and B are reversed
      // clang-format off
      { PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_KING,
        PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_KING },
      { PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_KING,
        PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_KING }
      // clang-format on
    };

    // Mirror a square
    static constexpr int Mirror[SQUARE_NB] = {
        8,  7,  6,  5,  4,  3,  2,  1,  0,
       17, 16, 15, 14, 13, 12, 11, 10,  9,
       26, 25, 24, 23, 22, 21, 20, 19, 18,
       35, 34, 33, 32, 31, 30, 29, 28, 27,
       44, 43, 42, 41, 40, 39, 38, 37, 36,
       53, 52, 51, 50, 49, 48, 47, 46, 45,
       62, 61, 60, 59, 58, 57, 56, 55, 54,
       71, 70, 69, 68, 67, 66, 65, 64, 63,
       80, 79, 78, 77, 76, 75, 74, 73, 72,
       89, 88, 87, 86, 85, 84, 83, 82, 81,
    };

    // Rotate a square by 180
    static constexpr int Rotate[SQUARE_NB] = {
       81, 82, 83, 84, 85, 86, 87, 88, 89,
       72, 73, 74, 75, 76, 77, 78, 79, 80,
       63, 64, 65, 66, 67, 68, 69, 70, 71,
       54, 55, 56, 57, 58, 59, 60, 61, 62,
       45, 46, 47, 48, 49, 50, 51, 52, 53,
       36, 37, 38, 39, 40, 41, 42, 43, 44,
       27, 28, 29, 30, 31, 32, 33, 34, 35,
       18, 19, 20, 21, 22, 23, 24, 25, 26,
        9, 10, 11, 12, 13, 14, 15, 16, 17,
        0,  1,  2,  3,  4,  5,  6,  7,  8,
    };

   public:
    // Feature name
    static constexpr const char* Name = "HalfKAv2_hm";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0xd17b100;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 6 * static_cast<IndexType>(PS_NB);

    // Get king_index and mirror information
    static constexpr auto KingBuckets = []() {
#define M(s) ((1 << 3) | s)
        // Stored as (mirror << 3 | bucket)
        constexpr uint8_t KingBucketsRaw[SQUARE_NB] = {
          // clang-format off
          0,  0,  0,  0,  1, M(0),  0,  0,  0,
          0,  0,  0,  3,  2, M(3),  0,  0,  0,
          0,  0,  0,  4,  5, M(4),  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  4,  5, M(4),  0,  0,  0,
          0,  0,  0,  3,  2, M(3),  0,  0,  0,
          0,  0,  0,  0,  1, M(0),  0,  0,  0,
          // clang-format on
        };
#undef M
        std::array<std::array<std::pair<int, bool>, SQUARE_NB>, SQUARE_NB> v{};
        for (uint8_t ksq = SQ_A0; ksq <= SQ_I9; ++ksq)
            for (uint8_t oksq = SQ_A0; oksq <= SQ_I9; ++oksq)
            {
                uint8_t king_bucket_ = KingBucketsRaw[ksq];
                int     king_bucket  = king_bucket_ & 0x7;
                bool    mirror =
                  (king_bucket_ >> 3) || ((king_bucket & 1) && (KingBucketsRaw[oksq] >> 3));
                v[ksq][oksq].first  = king_bucket;
                v[ksq][oksq].second = mirror;
            }
        return v;
    }();

    // Square index mapping based on condition (Mirror, Rotate)
    static constexpr auto IndexMap = []() {
        std::array<std::array<std::array<std::uint8_t, SQUARE_NB>, 2>, 2> v{};
        for (uint8_t m = 0; m < 2; ++m)
            for (uint8_t r = 0; r < 2; ++r)
                for (uint8_t s = 0; s < SQUARE_NB; ++s)
                {
                    uint8_t ss = s;
                    ss         = m ? Mirror[ss] : ss;
                    ss         = r ? Rotate[ss] : ss;
                    v[m][r][s] = ss;
                }
        return v;
    }();

    static constexpr int LayerStackBuckets[33] = {
      -1, -1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
       3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7,
    };

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList                                = ValueList<IndexType, MaxActiveDimensions>;

    // Index of a feature for a given king position and another piece on some square
    template<Color Perspective>
    static IndexType make_index(Square s, Piece pc, int bucket, bool mirror);

    // Get layer stack bucket
    static IndexType make_layer_stack_bucket(const Position& pos);

    // Get a list of indices for recently changed features
    template<Color Perspective>
    static void append_changed_indices(
      int bucket, bool mirror, const DirtyPiece& dp, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DirtyPiece& dirtyPiece, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED