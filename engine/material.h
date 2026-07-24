/*
  Pikafish, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2023 The Pikafish developers (see AUTHORS file)

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

#ifndef MATERIAL_H_INCLUDED
#define MATERIAL_H_INCLUDED

#include "misc.h"
#include "position.h"
#include "score.h"
#include "types.h"

namespace Stockfish::Material {

// Material::Entry contains various information about a material configuration.
// It contains a material imbalance evaluation.
struct Entry {

    Score imbalance() const { return score; }

    Key   key;
    Score score;
};

using Table = HashTable<Entry, 8192>;

Entry* probe(const Position& pos, Table& table);

}  // namespace Stockfish::Material

#endif  // MATERIAL_H_INCLUDED