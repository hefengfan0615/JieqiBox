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

#ifndef SCORE_H_INCLUDED
#define SCORE_H_INCLUDED

#include "types.h"

namespace Stockfish {

struct Score {
    Value mg, eg;
};

constexpr Score make_score(Value mg, Value eg) { return {mg, eg}; }
constexpr Value mg_value(Score s) { return s.mg; }
constexpr Value eg_value(Score s) { return s.eg; }

constexpr Score SCORE_ZERO = {0, 0};

constexpr Score operator+(Score s1, Score s2) { return {s1.mg + s2.mg, s1.eg + s2.eg}; }
constexpr Score operator-(Score s1, Score s2) { return {s1.mg - s2.mg, s1.eg - s2.eg}; }
constexpr Score operator-(Score s) { return {-s.mg, -s.eg}; }
constexpr Score& operator+=(Score& s1, Score s2) { s1 = s1 + s2; return s1; }
constexpr Score& operator-=(Score& s1, Score s2) { s1 = s1 - s2; return s1; }
constexpr Score operator*(int i, Score s) { return {i * s.mg, i * s.eg}; }
constexpr Score operator*(Score s, int i) { return {s.mg * i, s.eg * i}; }
constexpr Score operator/(Score s, int i) { return {s.mg / i, s.eg / i}; }

}

#endif  // #ifndef SCORE_H_INCLUDED