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

#ifndef STA_DELAY_H
#define STA_DELAY_H

// Define one of the following:

// Define DELAY_FLOAT to use the float definitions.
#define DELAY_FLOAT

// Define DELAY_FLOAT_CLASS to use the DelayClass definitions.
//#define DELAY_FLOAT_CLASS

// Define DELAY_NORMAL2 to use the DelayNormal2 definitions.
//#define DELAY_NORMAL2

#ifdef DELAY_FLOAT
 #include "DelayFloat.hh"
#endif

#ifdef DELAY_FLOAT_CLASS
 #include "DelayFloatClass.hh"
#endif

#ifdef DELAY_NORMAL2
 #include "DelayNormal2.hh"
#endif

namespace sta {

class Units;
class StaState;

typedef Delay ArcDelay;
typedef Delay Slew;
typedef Delay Arrival;
typedef Delay Required;
typedef Delay Slack;

void
initDelayConstants();
Delay
makeDelay(float delay,
	  float sigma_early,
	  float sigma_late);
float
delayAsFloat(const Delay &delay);
const char *
delayAsString(const Delay &delay,
	      const Units *units);
const char *
delayAsString(const Delay &delay,
	      const StaState *sta);
const char *
delayAsString(const Delay &delay,
	      const Units *units,
	      int digits);
// mean late+/early- sigma
// early_late = NULL returns mean.
float
delayMeanSigma(const Delay &delay,
	       const EarlyLate *early_late);
const char *
delayMeanSigmaString(const Delay &delay,
		     const EarlyLate *early_late,
		     const Units *units,
		     int digits);
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
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max);
bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2);
bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2,
		    const MinMax *min_max);
bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2,
		       const MinMax *min_max);
bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max);
float
delayRatio(const Delay &delay1,
	   const Delay &delay2);

} // namespace
#endif
