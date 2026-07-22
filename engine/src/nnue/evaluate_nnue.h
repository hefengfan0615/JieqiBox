/*
  Stockfish - NNUE evaluation function implementation
*/

#ifndef NNUE_EVALUATE_NNUE_H_INCLUDED
#define NNUE_EVALUATE_NNUE_H_INCLUDED

#include <string>

#include "../types.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE {

// Hash value of the evaluation function file
constexpr std::uint32_t HashValue =
  FeatureTransformer<TransformedFeatureDimensionsBig>::get_hash_value()
  ^ NetworkArchitecture<TransformedFeatureDimensionsBig, L2Big, L3Big>::get_hash_value();

constexpr int MAX_PSQT = 256 * (128 * 128);

// The default file name for the NNUE evaluation function
constexpr const char* EvalFileDefaultName = "pikafish.nnue";

// the NNUE evaluation function
Value evaluate(const Position& pos, int* complexity = nullptr);

// Load the NNUE evaluation function from a file
bool load_eval_file(const std::string& evalFile, const std::string& downloadFileName);

// Verify the NNUE evaluation function
bool verify(const std::string& evalFile, const std::string& downloadFileName);

// Initialize the NNUE evaluation function
void init();

// Save the NNUE evaluation function to a file
void save_eval();

}  // namespace Stockfish::Eval::NNUE

#endif  // #ifndef NNUE_EVALUATE_NNUE_H_INCLUDED