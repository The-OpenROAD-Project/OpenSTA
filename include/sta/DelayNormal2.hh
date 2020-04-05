// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#pragma once

#include "MinMax.hh"

namespace sta {

class Delay;
class StaState;

// Normal distribution with early(left)/late(right) std deviations.
class Delay
{
public:
  Delay();
  Delay(float mean);
  Delay(float mean,
	float sigma2_early,
	float sigma2_late);
  float mean() const { return mean_; }
  float sigma(const EarlyLate *early_late) const;
  // sigma^2
  float sigma2(const EarlyLate *early_late) const;
  float sigma2Early() const;
  float sigma2Late() const;
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
  void operator-=(const Delay &delay);
  bool operator==(const Delay &delay) const;
  bool operator>(const Delay &delay) const;
  bool operator>=(const Delay &delay) const;
  bool operator<(const Delay &delay) const;
  bool operator<=(const Delay &delay) const;

protected:
  static const int early_index = 0;
  static const int late_index = 1;

private:
  float mean_;
  // Sigma^2
  float sigma2_[EarlyLate::index_count];
};

const Delay delay_zero(0.0);

void
initDelayConstants();

Delay
makeDelay(float delay,
	  float sigma_early,
	  float sigma_late);

Delay
makeDelay2(float delay,
	   // sigma^2
	   float sigma_early,
	   float sigma_late);

inline float
delayAsFloat(const Delay &delay) { return delay.mean(); }

// Most non-operator functions on Delay are not defined as member
// functions so they can be defined on floats, where there is no class
// to define them.

Delay operator+(float delay1,
		const Delay &delay2);
// Used for parallel gate delay calc.
Delay operator/(float delay1,
		const Delay &delay2);
// Used for parallel gate delay calc.
Delay operator*(const Delay &delay1,
		float delay2);

// mean late+/early- sigma
float
delayAsFloat(const Delay &delay,
	     const EarlyLate *early_late,
	     const StaState *sta);
float
delaySigma2(const Delay &delay,
	    const EarlyLate *early_late);
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
const Delay &
delayInitValue(const MinMax *min_max);
bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max);
bool
fuzzyZero(const Delay &delay);
bool
fuzzyInf(const Delay &delay);
bool
fuzzyEqual(const Delay &delay1,
	   const Delay &delay2);
bool
fuzzyLess(const Delay &delay1,
	  const Delay &delay2);
bool
fuzzyLess(const Delay &delay1,
	  const Delay &delay2,
	  const MinMax *min_max);
bool
fuzzyLessEqual(const Delay &delay1,
	       const Delay &delay2);
bool
fuzzyLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max);
bool
fuzzyGreater(const Delay &delay1,
	     const Delay &delay2);
bool
fuzzyGreaterEqual(const Delay &delay1,
		  const Delay &delay2);
bool
fuzzyGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max);
bool
fuzzyGreater(const Delay &delay1,
	     const Delay &delay2,
	     const MinMax *min_max);
float
delayRatio(const Delay &delay1,
	   const Delay &delay2);

} // namespace
