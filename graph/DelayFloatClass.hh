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

#ifndef STA_DELAY_FLOAT_CLASS_H
#define STA_DELAY_FLOAT_CLASS_H

#include "MinMax.hh"
#include "Pool.hh"

namespace sta {

// Delay values defined as objects that hold a float value.

class Units;
class Delay;
class StaState;

typedef Delay ArcDelay;
typedef Delay Slew;
typedef Delay Arrival;
typedef Delay Required;
typedef Delay Slack;
typedef Pool<Delay> DelayPool;

class Delay
{
public:
  Delay();
  Delay(float delay);
  float asFloat() const;
  void operator=(const Delay &delay);
  void operator=(float delay);
  void operator+=(const Delay &delay);
  void operator+=(float delay);
  Delay operator+(const Delay &delay) const;
  Delay operator+(float delay) const;
  Delay operator-(const Delay &delay) const;
  Delay operator-(float delay) const;
  Delay operator-() const;
  void operator-=(float delay);
  bool operator==(const Delay &delay) const;
  bool operator>(const Delay &delay) const;
  bool operator>=(const Delay &delay) const;
  bool operator<(const Delay &delay) const;
  bool operator<=(const Delay &delay) const;

private:
  float delay_;
};

const Delay delay_zero(0.0);

void
initDelayConstants();

// Most non-operator functions on Delay are not defined as member
// functions so they can be defined on floats, where there is no class
// to define them.

Delay operator+(float delay1,
		const Delay &delay2);
Delay operator/(float delay1,
		const Delay &delay2);
Delay operator*(float delay1,
		const Delay &delay2);
Delay operator*(const Delay &delay1,
		float delay2);
inline float
delayAsFloat(const Delay &delay) { return delay.asFloat(); }
const char *delayAsString(const Delay &delay,
			  Units *units);
const char *delayAsString(const Delay &delay,
			  const StaState *sta);
const Delay &delayInitValue(const MinMax *min_max);
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
	       float delay2);
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
delayFuzzyLessEqual(const Delay &delay1,
		    float delay2);
bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2);
bool
delayFuzzyGreater(const Delay &delay1,
		  float delay2);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2);
bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       float delay2);
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
