/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

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

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "search.h"
#include "timeman.h"
#include "uci.h"

namespace Stockfish {

TimeManagement Time; // Our global time management object


/// TimeManagement::init() is called at the beginning of the search and calculates
/// the bounds of time allowed for the current game ply. We currently support:
//      1) x basetime (+ z increment)
//      2) x moves in y seconds (+ z increment)

void TimeManagement::init(Search::LimitsType& limits, Color us, int ply) {

  TimePoint moveOverhead    = TimePoint(Options["Move Overhead"]);
  TimePoint slowMover       = TimePoint(Options["Slow Mover"]);
  TimePoint npmsec          = TimePoint(Options["nodestime"]);

  // optScale is a percentage of available time to use for the current move.
  // maxScale is a multiplier applied to optimumTime.
  double optScale, maxScale;

  // If we have to play in 'nodes as time' mode, then convert from time
  // to nodes, and use resulting values in time management formulas.
  // WARNING: to avoid time losses, the given npmsec (nodes per millisecond)
  // must be much lower than the real engine speed.
  if (npmsec)
  {
      if (!availableNodes) // Only once at game start
          availableNodes = npmsec * limits.time[us]; // Time is in msec

      // Convert from milliseconds to nodes
      limits.time[us] = TimePoint(availableNodes);
      limits.inc[us] *= npmsec;
      limits.npmsec = npmsec;
  }

  startTime = limits.startTime;

  // Pikafish jieqi: Maximum move horizon
  int centiMTG = limits.movestogo ? std::min(limits.movestogo * 100, 6000) : 6000;

  // Pikafish jieqi: If less than one second, gradually reduce mtg
  if (limits.time[us] < 1000)
      centiMTG = limits.time[us] * 6;

  // Make sure timeLeft is > 0 since we may use it as a divisor
  TimePoint timeLeft =  std::max(TimePoint(1),
      limits.time[us]
      + (limits.inc[us] * (centiMTG - 100) - moveOverhead * (200 + centiMTG)) / 100);

  // A user may scale time usage by setting UCI option "Slow Mover"
  // Default is 100 and changing this value will probably lose elo.
  timeLeft = slowMover * timeLeft / 100;

  // x basetime (+ z increment)
  // If there is a healthy increment, timeLeft can exceed actual available
  // game time for the current move, so also cap to 20% of available game time.
  if (limits.movestogo == 0)
  {
      // Pikafish jieqi: logarithmic time constant based on time left
      double logTimeInSec = std::log10(timeLeft / 1000.0);
      double optConstant  = std::min(0.00344000 + 0.000200000 * logTimeInSec, 0.00450000);
      double maxConstant  = std::max(3.9000 + 3.10000 * logTimeInSec, 2.50000);

      optScale = std::min(0.0155000 + std::pow(ply + 3.00000, 0.450000) * optConstant,
                          0.200000 * limits.time[us] / double(timeLeft));

      maxScale = std::min(6.50000, maxConstant + ply / 13.6000);
  }

  // x moves in y seconds (+ z increment)
  else
  {
      optScale = std::min((0.88 + ply / 116.4) / (centiMTG / 100.0),
                           0.88 * limits.time[us] / double(timeLeft));
      maxScale = std::min(6.3, 1.3 + 0.11 * (centiMTG / 100.0));
  }

  // Pikafish jieqi: Never use more than 81% of the available time for this move
  optimumTime = TimePoint(optScale * timeLeft);
  maximumTime = TimePoint(std::min(0.810000 * limits.time[us] - moveOverhead, maxScale * optimumTime)) - 10;

  if (Options["Ponder"])
      optimumTime += optimumTime / 4;
}

} // namespace Stockfish
