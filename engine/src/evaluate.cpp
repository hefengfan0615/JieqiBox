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

// Evaluation function

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "nnue/evaluate_nnue.h"
#include "nnue/nnue_architecture.h"
#include "nnue/nnue_common.h"
#include "nnue/nnue_feature_transformer.h"
#include "misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"

namespace Stockfish::Eval {

// Trace the evaluation function
std::string trace(Position& pos) {
    std::stringstream ss;

    // Make sure the accumulator is computed
    StateInfo st;
    pos.do_null_move(st);

    auto v = NNUE::evaluate(pos);

    ss << "\nNNUE evaluation" << std::showpos << std::setw(7) << v << "\n";

    return ss.str();
}

// Evaluate the position
Value evaluate(const Position& pos) {
    int  complexity;
    Value v;
    bool nnue = true;

    if (nnue)
        v = NNUE::evaluate(pos, &complexity);
    else
    {
        v          = 0;
        complexity = 0;
    }

    // Dampen evaluation when shuffling
    v -= v * pos.rule40_count() / 217;

    // Guarantee evaluation is not outside the bounds
    v = std::clamp(v, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

    return v;
}

}  // namespace Stockfish::Eval