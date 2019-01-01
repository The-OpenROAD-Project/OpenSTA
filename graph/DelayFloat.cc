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
#include "Machine.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"
#include "Delay.hh"

// Non-SSTA compilation.
#if !SSTA

namespace sta {

static Delay delay_init_values[MinMax::index_count];

void
initDelayConstants()
{
  delay_init_values[MinMax::minIndex()] = MinMax::min()->initValue();
  delay_init_values[MinMax::maxIndex()] = MinMax::max()->initValue();
}

const Delay &
delayInitValue(const MinMax *min_max)
{
  return delay_init_values[min_max->index()];
}

bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max)
{
  return fuzzyEqual(delay, min_max->initValue());
}

float
delayAsFloat(const Delay &delay)
{
  return delay;
}

bool
delayFuzzyZero(const Delay &delay)
{
  return fuzzyZero(delay);
}

bool
delayFuzzyEqual(const Delay &delay1,
		const Delay &delay2)
{
  return fuzzyEqual(delay1, delay2);
}

bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2)
{
  return fuzzyLess(delay1, delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2)
{
  return fuzzyLessEqual(delay1, delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2,
		    const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLessEqual(delay1, delay2);
  else
    return fuzzyGreaterEqual(delay1, delay2);
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2)
{
  return fuzzyGreater(delay1, delay2);
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2)
{
  return fuzzyGreaterEqual(delay1, delay2);
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreater(delay1, delay2);
  else
    return fuzzyLess(delay1, delay2);
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2,
		       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreaterEqual(delay1, delay2);
  else
    return fuzzyLessEqual(delay1, delay2);
}

bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLess(delay1, delay2);
  else
    return fuzzyGreater(delay1, delay2);
}

float
delayRatio(const Delay &delay1,
	   const Delay &delay2)
{
  return delay1 / delay2;
}

const char *
delayAsString(const Delay &delay,
	      const Units *units,
	      int digits)
{
  return units->timeUnit()->asString(delay, digits);
}

float
delayAsFloat(const Delay &delay,
	     const EarlyLate *)
{
  return delay;
}

const char *
delayAsString(const Delay &delay,
	      const EarlyLate *,
	      const Units *units,
	      int digits)
{
  const Unit *unit = units->timeUnit();
  return unit->asString(delay, digits);
}

float
delaySigma(const Delay &,
	   const EarlyLate *)
{
  return 0.0;
}

float
delaySigma2(const Delay &,
	   const EarlyLate *)
{
  return 0.0;
}

} // namespace
#endif
