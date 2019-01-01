// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "config.h"

#ifndef STA_DELAY_H
#define STA_DELAY_H

#if SSTA
  // Delays are Normal PDFs with early/late sigma.
  #include "DelayNormal2.hh"
#else
  // Delays are floats.
 #include "DelayFloat.hh"
#endif

// API common to DelayFloat and DelayNormal2.
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
// sigma^2
Delay
makeDelay2(float delay,
	  float sigma2_early,
	  float sigma2_late);
float
delayAsFloat(const Delay &delay);
// mean late+/early- sigma
float
delayAsFloat(const Delay &delay,
	     const EarlyLate *early_late);
float
delaySigma(const Delay &delay,
	   const EarlyLate *early_late);
float
delaySigma2(const Delay &delay,
	    const EarlyLate *early_late);
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
const char *
delayAsString(const Delay &delay,
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
