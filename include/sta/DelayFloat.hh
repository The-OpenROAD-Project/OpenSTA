// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "MinMax.hh"

// Delay values defined as floats.

namespace sta {

class StaState;

typedef float Delay;

const Delay delay_zero = 0.0;

void
initDelayConstants();

const char *
delayAsString(const Delay &delay,
	      const StaState *sta);
const char *
delayAsString(const Delay &delay,
	      const StaState *sta,
	      int digits);
const char *
delayAsString(const Delay &delay,
	      const EarlyLate *early_late,
	      const StaState *sta,
	      int digits);

inline Delay
makeDelay(float delay,
	  float,
	  float)
{
  return delay;
}

inline Delay
makeDelay2(float delay,
	   float,
	   float)
{
  return delay;
}

inline float
delayAsFloat(const Delay &delay)
{
  return delay;
}

// mean late+/early- sigma
inline float
delayAsFloat(const Delay &delay,
	     const EarlyLate *,
	     const StaState *)
{
  return delay;
}

inline float
delaySigma2(const Delay &,
	    const EarlyLate *)
{
  return 0.0;
}

const Delay &
delayInitValue(const MinMax *min_max);
bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max);
bool
delayZero(const Delay &delay);
bool
delayInf(const Delay &delay);
bool
delayEqual(const Delay &delay1,
	   const Delay &delay2);
bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const StaState *sta);
bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const MinMax *min_max,
	  const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max,
	       const StaState *sta);
bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max,
		  const StaState *sta);
bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const MinMax *min_max,
	     const StaState *sta);

// delay1-delay2 subtracting sigma instead of addiing.
Delay
delayRemove(const Delay &delay1,
	    const Delay &delay2);
float
delayRatio(const Delay &delay1,
	   const Delay &delay2);

} // namespace
