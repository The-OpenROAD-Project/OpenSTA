// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef STA_DELAY_FLOAT_H
#define STA_DELAY_FLOAT_H

#include "MinMax.hh"
#include "Pool.hh"

// Delay values defined as floats.

namespace sta {

class Units;
class StaState;

typedef float Delay;
typedef Delay ArcDelay;
typedef Delay Slew;
typedef Delay Arrival;
typedef Delay Required;
typedef Delay Slack;
typedef Pool<float> DelayPool;

const Delay delay_zero = 0.0;

void initDelayConstants();

inline float delayAsFloat(const Delay &delay) { return delay; }
const char *
delayAsString(const Delay &delay,
	      Units *units);
const char *
delayAsString(const Delay &delay,
	      const StaState *sta);

const Delay &
delayInitValue(const MinMax *min_max);
bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max);
bool
delayFuzzyZero(const Delay &delay);
bool
delayFuzzyEqual(const Delay &delay1,
		const Delay &delay2);
bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2);
bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2);
bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2,
		    const MinMax *min_max);
bool delayFuzzyGreater(const Delay &delay1,
		       const Delay &delay2);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2);
bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2,
		       const MinMax *min_max);
bool
delayFuzzyLess(const Delay &delay1, const
	       Delay &delay2,
	       const MinMax *min_max);
float
delayRatio(const Delay &delay1,
	   const Delay &delay2);

} // namespace
#endif
