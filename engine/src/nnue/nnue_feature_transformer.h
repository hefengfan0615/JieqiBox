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

// A class that converts the input features of the NNUE evaluation function

#ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED
#define NNUE_FEATURE_TRANSFORMER_H_INCLUDED

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iosfwd>

#include "../position.h"
#include "../types.h"
#include "nnue_accumulator.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "layers/simd.h"

namespace Stockfish::Eval::NNUE {

// Input feature converter
template<IndexType TransformedFeatureDimensions>
class FeatureTransformer {

    // Number of output dimensions for one side
    static constexpr IndexType HalfDimensions = TransformedFeatureDimensions;

   public:
    // Output type
    using OutputType = TransformedFeatureType;

    // Number of input/output dimensions
    static constexpr IndexType InputDimensions  = FeatureSet::Dimensions;
    static constexpr IndexType OutputDimensions = HalfDimensions;

    // Size of forward propagation buffer
    static constexpr std::size_t BufferSize = OutputDimensions * sizeof(OutputType);

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t get_hash_value() {
        return FeatureSet::HashValue ^ (OutputDimensions * 2);
    }

    // Read network parameters
    bool read_parameters(std::istream& stream) {
        for (IndexType i = 0; i < HalfDimensions; ++i)
            biases[i] = read_little_endian<BiasType>(stream);
        for (IndexType i = 0; i < HalfDimensions * InputDimensions; ++i)
            weights[i] = read_little_endian<WeightType>(stream);
        for (IndexType i = 0; i < PSQTBuckets * InputDimensions; ++i)
            psqtWeights[i] = read_little_endian<PSQTWeightType>(stream);
        return !stream.fail();
    }

    // Write network parameters
    bool write_parameters(std::ostream& stream) const {
        for (IndexType i = 0; i < HalfDimensions; ++i)
            write_little_endian<BiasType>(stream, biases[i]);
        for (IndexType i = 0; i < HalfDimensions * InputDimensions; ++i)
            write_little_endian<WeightType>(stream, weights[i]);
        for (IndexType i = 0; i < PSQTBuckets * InputDimensions; ++i)
            write_little_endian<PSQTWeightType>(stream, psqtWeights[i]);
        return !stream.fail();
    }

    // Proceeds with the difference of an accumulator
    std::int32_t transform(const Position& pos, OutputType* output, int bucket) const {
        update_accumulator<WHITE>(pos);
        update_accumulator<BLACK>(pos);

        const Color perspectives[2] = {pos.side_to_move(), ~pos.side_to_move()};
        const auto& psqtAccumulation = static_cast<const Accumulator*>(pos.state()->accumulator.state)
                                         ->psqtAccumulation;
        const auto  psqt =
          (psqtAccumulation[perspectives[0]][bucket] - psqtAccumulation[perspectives[1]][bucket])
          / 2;

        const auto& accumulation = static_cast<const Accumulator*>(pos.state()->accumulator.state)
                                     ->accumulation;

        for (IndexType p = 0; p < 2; ++p)
        {
            const IndexType offset = (HalfDimensions / 2) * p;

#if defined(VECTOR)

            constexpr IndexType OutputChunkSize = MaxChunkSize;
            static_assert((HalfDimensions / 2) % OutputChunkSize == 0);
            constexpr IndexType NumOutputChunks = HalfDimensions / 2 / OutputChunkSize;

            const vec_t Zero = vec_zero();
            const vec_t One  = vec_set_16(127);

            const vec_t* in0 = reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][0]));
            const vec_t* in1 =
              reinterpret_cast<const vec_t*>(&(accumulation[perspectives[p]][HalfDimensions / 2]));
            vec_t* out = reinterpret_cast<vec_t*>(output + offset);

            for (IndexType j = 0; j < NumOutputChunks; ++j)
            {
                const vec_t sum0a =
                  vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 0], One), Zero), 7);
                const vec_t sum0b =
                  vec_slli_16(vec_max_16(vec_min_16(in0[j * 2 + 1], One), Zero), 7);
                const vec_t sum1a = vec_min_16(in1[j * 2 + 0], One);
                const vec_t sum1b = vec_min_16(in1[j * 2 + 1], One);

                const vec_t pa = vec_mulhi_16(sum0a, sum1a);
                const vec_t pb = vec_mulhi_16(sum0b, sum1b);

                out[j] = vec_packus_16(pa, pb);
            }

#else

            for (IndexType j = 0; j < HalfDimensions / 2; ++j)
            {
                BiasType sum0 = accumulation[static_cast<int>(perspectives[p])][j + 0];
                BiasType sum1 =
                  accumulation[static_cast<int>(perspectives[p])][j + HalfDimensions / 2];
                sum0               = std::max<BiasType>(0, std::min<BiasType>(127, sum0));
                sum1               = std::max<BiasType>(0, std::min<BiasType>(127, sum1));
                output[offset + j] = static_cast<OutputType>(unsigned(sum0 * sum1) / 128);
            }

#endif
        }

        return psqt;
    }  // end of function transform()

    void update_accumulator(const Position& pos) const {
        // The size must be enough to contain the
        // largest possible update from one move.
        FeatureSet::IndexList removed_indices, added_indices;

        for (Color color : {WHITE, BLACK})
        {
            auto [bucket, mirror] = FeatureSet::KingBuckets[king_square(pos, color)][king_square(pos, ~color)];

            auto& accumulator = static_cast<Accumulator*>(pos.state()->accumulator.state);
            auto& prevAccumulator =
              static_cast<Accumulator*>(pos.state()->previous->accumulator.state);

            if (accumulator->computed_accumulation)
                return;

            if (prevAccumulator->computed_accumulation)
            {
                removed_indices.clear();
                added_indices.clear();
                FeatureSet::append_changed_indices<color>(bucket, mirror, pos.state()->dirtyPiece,
                                                          removed_indices, added_indices);

                // Reset accumulator to the previous state
                std::memcpy(accumulator->accumulation[color], prevAccumulator->accumulation[color],
                            HalfDimensions * sizeof(BiasType));
                std::memcpy(accumulator->psqtAccumulation[color],
                            prevAccumulator->psqtAccumulation[color],
                            PSQTBuckets * sizeof(PSQTWeightType));

                // Difference calculation for the deactivated features
                for (const auto index : removed_indices)
                {
                    const IndexType offset = HalfDimensions * index;
                    for (IndexType k = 0; k < HalfDimensions; ++k)
                        accumulator->accumulation[color][k] -= weights[offset + k];
                    for (IndexType k = 0; k < PSQTBuckets; ++k)
                        accumulator->psqtAccumulation[color][k] -=
                          psqtWeights[index * PSQTBuckets + k];
                }

                // Difference calculation for the activated features
                for (const auto index : added_indices)
                {
                    const IndexType offset = HalfDimensions * index;
                    for (IndexType k = 0; k < HalfDimensions; ++k)
                        accumulator->accumulation[color][k] += weights[offset + k];
                    for (IndexType k = 0; k < PSQTBuckets; ++k)
                        accumulator->psqtAccumulation[color][k] +=
                          psqtWeights[index * PSQTBuckets + k];
                }

                accumulator->computed_accumulation = true;
            }
            else
            {
                // If the previous accumulator is not available, initialize from scratch
                std::memset(accumulator->accumulation[color], 0, HalfDimensions * sizeof(BiasType));
                std::memset(accumulator->psqtAccumulation[color], 0,
                            PSQTBuckets * sizeof(PSQTWeightType));

                // Fill the accumulator with the biases
                for (IndexType k = 0; k < HalfDimensions; ++k)
                    accumulator->accumulation[color][k] = biases[k];

                // Accumulate for the king
                Square ksq = mirsq(color, king_square(pos, color));
                IndexType ks_index = FeatureSet::make_index<color>(ksq, make_piece(color, KING), bucket, mirror);

                const IndexType ks_offset = HalfDimensions * ks_index;
                for (IndexType k = 0; k < HalfDimensions; ++k)
                    accumulator->accumulation[color][k] += weights[ks_offset + k];
                for (IndexType k = 0; k < PSQTBuckets; ++k)
                    accumulator->psqtAccumulation[color][k] +=
                      psqtWeights[ks_index * PSQTBuckets + k];

                // Accumulate for each piece
                pos.accumulate<color>(bucket, mirror, *this, accumulator);

                accumulator->computed_accumulation = true;
            }
        }
    }

    alignas(CacheLineSize) BiasType biases[HalfDimensions];
    alignas(CacheLineSize) WeightType weights[HalfDimensions * InputDimensions];
    alignas(CacheLineSize) PSQTWeightType psqtWeights[InputDimensions * PSQTBuckets];
};

}  // namespace Stockfish::Eval::NNUE

#endif  // #ifndef NNUE_FEATURE_TRANSFORMER_H_INCLUDED