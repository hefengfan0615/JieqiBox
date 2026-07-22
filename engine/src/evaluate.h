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

#ifndef EVALUATE_H_INCLUDED
#define EVALUATE_H_INCLUDED

#include <string>

#include "types.h"

namespace Stockfish {

class Position;

namespace Eval {

std::string trace(Position& pos);
Value       evaluate(const Position& pos);

// The default net name MUST follow the format nn-[SHA256 first 12 digits].nnue
// for the build process (profile-build and meson) to work.
constexpr const char* EvalFileDefaultName = "pikafish.nnue";

namespace NNUE {

// Hash value of the evaluation function file
constexpr std::uint32_t HashValue =
  FeatureTransformer<TransformedFeatureDimensionsBig>::get_hash_value()
  ^ NetworkArchitecture<TransformedFeatureDimensionsBig, L2Big, L3Big>::get_hash_value();

void        init();
void        verify();
void        save_eval();
std::string trace(Position& pos);
Value       evaluate(const Position& pos, int* complexity = nullptr);

}  // namespace NNUE

}  // namespace Eval

}  // namespace Stockfish

#endif  // #ifndef EVALUATE_H_INCLUDED