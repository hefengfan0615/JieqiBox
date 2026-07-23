/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)

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

// Code for calculating NNUE evaluation function

#include "evaluate_nnue.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <optional>

#include "../evaluate.h"
#include "../misc.h"
#include "../position.h"
#include "../types.h"
#include "../uci.h"

#include "network.h"
#include "nnue_accumulator.h"
#include "nnue_misc.h"

namespace Stockfish::Eval::NNUE {

// Global network
static Network network(EvalFile{EvalFileDefaultName});

// Global accumulator caches
static std::unique_ptr<AccumulatorCaches> caches;

// Initialize the global network and caches
void init_network() {
    caches = std::make_unique<AccumulatorCaches>(network);
}

// Load the network from the default location
void load_network() {
    std::string eval_file = std::string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    network.load(CommandLine::binaryDirectory, eval_file);
}

// Verify the network
void verify_network() {
    std::string eval_file = std::string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    network.verify(eval_file);
}

// External access to the global network
const Network& get_network() { return network; }
AccumulatorCaches& get_caches() { return *caches; }

// Load eval, from a file stream or a memory stream
bool load_eval(std::string name, std::istream& stream) {

    auto description = network.load(stream);
    return description.has_value();
}

// Save eval, to a file stream or a memory stream
bool save_eval(std::ostream& stream) {

    return network.save(stream, network.evalFile.current, network.evalFile.netDescription);
}

/// Save eval, to a file given by its name
bool save_eval(const std::optional<std::string>& filename) {

    return network.save(filename);
}

// Evaluate a position using the global network
Value evaluate(const Position& pos, int* complexity) {

    auto [psqt, positional] = network.evaluate(pos, &caches->cache);
    Value v = psqt + positional;

    if (complexity)
        *complexity = std::abs(psqt + positional - psqt);

    return v;
}

// trace() returns a string with the value of each piece on a board
std::string trace(Position& pos) {

    return NNUE::trace(pos, network, *caches);
}

} // namespace Stockfish::Eval::NNUE