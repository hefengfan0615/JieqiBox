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

// Code for calculating the NNUE evaluation function

#include "evaluate_nnue.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "../misc.h"
#include "../position.h"
#include "../types.h"
#include "../uci.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_feature_transformer.h"

namespace Stockfish::Eval::NNUE {

// Network definition
using Network = NetworkArchitecture<TransformedFeatureDimensionsBig, L2Big, L3Big>;

// The feature transformer
static FeatureTransformer<TransformedFeatureDimensionsBig> featureTransformer;

// The network
alignas(CacheLineSize) static Network network[LayerStacks];

// Evaluation function file
static std::string evalFile;

// Load the evaluation function file
bool load_eval_file(const std::string& evalFile, const std::string& downloadFileName) {
    std::ifstream stream(evalFile, std::ios::binary);
    if (!stream.is_open())
    {
        // Try to download the file
        if (!downloadFileName.empty())
        {
            std::string cmd = "sh " + downloadFileName + " " + evalFile;
            int       ret  = std::system(cmd.c_str());
            if (ret != 0)
                return false;
            stream.open(evalFile, std::ios::binary);
            if (!stream.is_open())
                return false;
        }
        else
            return false;
    }

    bool success = featureTransformer.read_parameters(stream);

    for (IndexType i = 0; i < LayerStacks; ++i)
        success &= network[i].read_parameters(stream);

    if (!success)
        return false;

    if (stream.peek() != std::ios::traits_type::eof())
        return false;

    return true;
}

// Verify the evaluation function file
bool verify(const std::string& evalFile, const std::string& downloadFileName) {
    std::ifstream stream(evalFile, std::ios::binary);
    if (!stream.is_open())
    {
        // Try to download the file
        if (!downloadFileName.empty())
        {
            std::string cmd = "sh " + downloadFileName + " " + evalFile;
            int       ret  = std::system(cmd.c_str());
            if (ret != 0)
                return false;
            stream.open(evalFile, std::ios::binary);
            if (!stream.is_open())
                return false;
        }
        else
            return false;
    }

    std::uint32_t hashValue = read_little_endian<std::uint32_t>(stream);
    if (hashValue != HashValue)
    {
        std::cerr << "Network evaluation file hash mismatch. Got " << std::hex << hashValue
                  << " Expected " << HashValue << std::dec << std::endl;
        return false;
    }

    return true;
}

// Initialize the evaluation function
void init() {
    evalFile = std::string(Utility::map_path(EvalFileDefaultName));

    if (!verify(evalFile, std::string(Utility::map_path("scripts/net.sh"))))
    {
        std::cerr << "Network evaluation file " << evalFile << " is not valid. "
                  << "Attempting to download..." << std::endl;
        if (!load_eval_file(evalFile, std::string(Utility::map_path("scripts/net.sh"))))
        {
            std::cerr << "Failed to load network evaluation file." << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }
    else
    {
        load_eval_file(evalFile, "");
    }
}

// Save the evaluation function
void save_eval() {
    std::optional<std::string> savedir;
    std::string                filename;

    // If the user has a custom net name, save the net to the same directory
    if (!(evalFile).empty())
    {
        savedir  = evalFile;
        filename = evalFile;
    }

    if (savedir.has_value())
    {
        std::ofstream stream(filename, std::ios::binary);
        if (!stream)
            return;

        write_little_endian<std::uint32_t>(stream, HashValue);
        featureTransformer.write_parameters(stream);
        for (IndexType i = 0; i < LayerStacks; ++i)
            network[i].write_parameters(stream);
    }
}

// Calculate the evaluation value
Value evaluate(const Position& pos, int* complexity) {
    // We manually align the arrays on the stack because with gcc < 9.3
    // overaligning stack variables with alignas() doesn't work correctly.

    constexpr std::size_t alignment =
      CacheLineSize / std::max<IndexType>(sizeof(TransformedFeatureType), std::size_t(1));

    constexpr IndexType bufferSize = FeatureTransformer<TransformedFeatureDimensionsBig>::BufferSize;

    alignas(alignment) std::byte transformedFeatures[bufferSize];

    int  bucket = FeatureSet::make_layer_stack_bucket(pos);
    auto psqt   = featureTransformer.transform(pos, reinterpret_cast<TransformedFeatureType*>(transformedFeatures), bucket);
    auto positional = network[bucket].propagate(reinterpret_cast<TransformedFeatureType*>(transformedFeatures));

    Value nnue = (psqt + positional) * 0.711;

    // Calculate complexity
    int nnueComplexity = std::abs(psqt - positional);
    nnueComplexity = (90 * nnueComplexity + 121 * std::abs(nnue - pos.material_diff())) / 256;

    if (complexity)
        *complexity = nnueComplexity;

    // Apply optimism scaling
    int   scale    = 1035 + 126 * pos.material_sum() / 4214;
    Value optimism = pos.this_thread()->optimism[pos.side_to_move()];
    optimism = optimism * (281 + nnueComplexity) / 256;
    nnue     = (nnue * scale + optimism * (scale - 780)) / 1024;

    // Clamp the evaluation
    nnue = std::clamp(nnue, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

    return nnue;
}

}  // namespace Stockfish::Eval::NNUE