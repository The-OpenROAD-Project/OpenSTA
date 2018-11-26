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

#include "Machine.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"
#include "Delay.hh"

// Conditional compilation based on delay abstraction from Delay.hh.
#ifdef DELAY_FLOAT_CLASS

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

Delay::Delay() :
  delay_(0.0)
{
}

Delay::Delay(float delay) :
  delay_(delay)
{
}

void
Delay::operator=(const Delay &delay)
{
  delay_ = delay.delay_;
}

void
Delay::operator=(float delay)
{
  delay_ = delay;
}

void
Delay::operator+=(const Delay &delay)
{
  delay_ += delay.delay_;
}

void
Delay::operator+=(float delay)
{
  delay_ += delay;
}

Delay
Delay::operator+(const Delay &delay) const
{
  return Delay(delay_ + delay.delay_);
}

Delay
Delay::operator+(float delay) const
{
  return Delay(delay_ + delay);
}

Delay
Delay::operator-(const Delay &delay) const
{
  return Delay(delay_ - delay.delay_);
}

Delay
Delay::operator-(float delay) const
{
  return Delay(delay_ - delay);
}

Delay
Delay::operator-() const
{
  return Delay(-delay_);
}

void
Delay::operator-=(float delay)
{
  delay_ -= delay;
}

bool
Delay::operator==(const Delay &delay) const
{
  return delay_ == delay.delay_;
}

bool
Delay::operator>(const Delay &delay) const
{
  return delay_ > delay.delay_;
}

bool
Delay::operator>=(const Delay &delay) const
{
  return delay_ >= delay.delay_;
}

bool
Delay::operator<(const Delay &delay) const
{
  return delay_ < delay.delay_;
}

bool
Delay::operator<=(const Delay &delay) const
{
  return delay_ <= delay.delay_;
}

bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max)
{
  return fuzzyEqual(delayAsFloat(delay), min_max->initValue());
}

bool
delayFuzzyZero(const Delay &delay)
{
  return fuzzyZero(delayAsFloat(delay));
}

bool
delayFuzzyEqual(const Delay &delay1,
		const Delay &delay2)
{
  return fuzzyEqual(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2)
{
  return fuzzyLess(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyLess(const Delay &delay1,
	       float delay2)
{
  return fuzzyLess(delayAsFloat(delay1), delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2)
{
  return fuzzyLessEqual(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    float delay2)
{
  return fuzzyLessEqual(delayAsFloat(delay1), delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2,
		    const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLessEqual(delayAsFloat(delay1), delayAsFloat(delay2));
  else
    return fuzzyGreaterEqual(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2)
{
  return fuzzyGreater(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyGreater(const Delay &delay1,
		  float delay2)
{
  return fuzzyGreater(delayAsFloat(delay1), delay2);
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2)
{
  return fuzzyGreaterEqual(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       float delay2)
{
  return fuzzyGreaterEqual(delayAsFloat(delay1), delay2);
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreater(delayAsFloat(delay1), delayAsFloat(delay2));
  else
    return fuzzyLess(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2,
		       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreaterEqual(delayAsFloat(delay1), delayAsFloat(delay2));
  else
    return fuzzyLessEqual(delayAsFloat(delay1), delayAsFloat(delay2));
}

bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLess(delayAsFloat(delay1), delayAsFloat(delay2));
  else
    return fuzzyGreater(delayAsFloat(delay1), delayAsFloat(delay2));
}

Delay
operator+(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 + delayAsFloat(delay2));
}

Delay
operator-(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 - delayAsFloat(delay2));
}

Delay
operator/(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 / delayAsFloat(delay2));
}

Delay
operator*(const Delay &delay1,
	  float delay2)
{
  return Delay(delayAsFloat(delay1) * delay2);
}

float
delayRatio(const Delay &delay1,
	   const Delay &delay2)
{
  return delayAsFloat(delay1) / delayAsFloat(delay2);
}

const char *
delayAsString(const Delay &delay,
	      const Units *units,
	      int digits)
{
  return units->timeUnit()->asString(delay.delay(), digits);
}

float
delayMeanSigma(const Delay &delay,
	       const EarlyLate *)
{
  return delay.delay();
}

const char *
delayMeanSigmaString(const Delay &delay,
		     const EarlyLate *,
		     const Units *units,
		     int digits)
{
  return units->timeUnit()->asString(delay.delay(), digits);
}

} // namespace
#endif
