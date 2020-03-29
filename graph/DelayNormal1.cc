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

#include "StaConfig.hh"
#include <cmath> // sqrt
#include "Machine.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"
#include "Delay.hh"

// SSTA compilation.
#if (SSTA == 1)

namespace sta {

inline float
square(float x)
{
  return x * x;
}

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
  mean_(0.0),
  sigma2_(0.0)
{
}

Delay::Delay(float mean) :
  mean_(mean),
  sigma2_(0.0)
{
}

Delay::Delay(float mean,
	     float sigma2) :
  mean_(mean),
  sigma2_(sigma2)
{
}

float
Delay::sigma() const
{
  if (sigma2_ < 0.0)
    // Sigma is negative for crpr to offset sigmas in the common
    // clock path.
    return -sqrt(-sigma2_);
  else
    return sqrt(sigma2_);
}

float
Delay::sigma2() const
{
  return sigma2_;
}

void
Delay::operator=(const Delay &delay)
{
  mean_ = delay.mean_;
  sigma2_ = delay.sigma2_;
}

void
Delay::operator=(float delay)
{
  mean_ = delay;
  sigma2_ = 0.0;
}

void
Delay::operator+=(const Delay &delay)
{
  mean_ += delay.mean_;
  sigma2_ += delay.sigma2_;
}

void
Delay::operator+=(float delay)
{
  mean_ += delay;
}

Delay
Delay::operator+(const Delay &delay) const
{
  return Delay(mean_ + delay.mean_,
	       sigma2_ + delay.sigma2_);
}

Delay
Delay::operator+(float delay) const
{
  return Delay(mean_ + delay, sigma2_);
}

Delay
Delay::operator-(const Delay &delay) const
{
  return Delay(mean_ - delay.mean_,
	       sigma2_ + delay.sigma2_);
}

Delay
Delay::operator-(float delay) const
{
  return Delay(mean_ - delay, sigma2_);
}

Delay
Delay::operator-() const
{
  return Delay(-mean_, sigma2_);
}

void
Delay::operator-=(float delay)
{
  mean_ -= delay;
}

void
Delay::operator-=(const Delay &delay)
{
  mean_ -= delay.mean_;
  sigma2_ += delay.sigma2_;
}

bool
Delay::operator==(const Delay &delay) const
{
  return mean_ == delay.mean_
    && sigma2_ == delay.sigma2_;
}

bool
Delay::operator>(const Delay &delay) const
{
  return mean_ > delay.mean_;
}

bool
Delay::operator>=(const Delay &delay) const
{
  return mean_ >= delay.mean_;
}

bool
Delay::operator<(const Delay &delay) const
{
  return mean_ < delay.mean_;
}

bool
Delay::operator<=(const Delay &delay) const
{
  return mean_ <= delay.mean_;
}

////////////////////////////////////////////////////////////////

Delay
makeDelay(float delay,
	  float sigma,
	  float)
{
  return Delay(delay, square(sigma));
}

Delay
makeDelay2(float delay,
	   float sigma2,
	   float )
{
  return Delay(delay, sigma2);
}

bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max)
{
  return fuzzyEqual(delay.mean(), min_max->initValue())
    && delay.sigma2() == 0.0;
}

bool
fuzzyZero(const Delay &delay)
{
  return fuzzyZero(delay.mean())
    && fuzzyZero(delay.sigma2());
}

bool
fuzzyInf(const Delay &delay)
{
  return fuzzyInf(delay.mean());
}

bool
fuzzyEqual(const Delay &delay1,
	   const Delay &delay2)
{
  return fuzzyEqual(delay1.mean(), delay2.mean())
    && fuzzyEqual(delay1.sigma2(), delay2.sigma2());
}

bool
fuzzyLess(const Delay &delay1,
	  const Delay &delay2)
{
  return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
fuzzyLess(const Delay &delay1,
	  float delay2)
{
  return fuzzyLess(delay1.mean(), delay2);
}

bool
fuzzyLessEqual(const Delay &delay1,
	       const Delay &delay2)
{
  return fuzzyLessEqual(delay1.mean(), delay2.mean());
}

bool
fuzzyLessEqual(const Delay &delay1,
		    float delay2)
{
  return fuzzyLessEqual(delay1.mean(), delay2);
}

bool
fuzzyLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLessEqual(delay1.mean(), delay2.mean());
  else
    return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
}

bool
fuzzyGreater(const Delay &delay1,
	     const Delay &delay2)
{
  return fuzzyGreater(delay1.mean(), delay2.mean());
}

bool
fuzzyGreater(const Delay &delay1,
	     float delay2)
{
  return fuzzyGreater(delay1.mean(), delay2);
}

bool
fuzzyGreaterEqual(const Delay &delay1,
		  const Delay &delay2)
{
  return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
}

bool
fuzzyGreaterEqual(const Delay &delay1,
		  float delay2)
{
  return fuzzyGreaterEqual(delay1.mean(), delay2);
}

bool
fuzzyGreater(const Delay &delay1,
	     const Delay &delay2,
	     const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreater(delay1.mean(), delay2.mean());
  else
    return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
fuzzyGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
  else
    return fuzzyLessEqual(delay1.mean(), delay2.mean());
}

bool
fuzzyLess(const Delay &delay1,
	  const Delay &delay2,
	  const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLess(delay1.mean(), delay2.mean());
  else
    return fuzzyGreater(delay1.mean(), delay2.mean());
}

Delay
operator+(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 + delay2.mean(),
	       delay2.sigma2());
}

Delay
operator/(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 / delay2.mean(),
	       delay2.sigma2());
}

Delay
operator*(const Delay &delay1,
	  float delay2)
{
  return Delay(delay1.mean() * delay2,
	       delay1.sigma2() * delay2 * delay2);
}

float
delayRatio(const Delay &delay1,
	   const Delay &delay2)
{
  return delay1.mean() / delay2.mean();
}

float
delayAsFloat(const Delay &delay,
	     const EarlyLate *early_late,
	     const StaState *sta)
{
  if (sta->pocvEnabled()) {
    if (early_late == EarlyLate::early())
      return delay.mean() - delay.sigma() * sta->sigmaFactor();
    else if (early_late == EarlyLate::late())
      return delay.mean() + delay.sigma() * sta->sigmaFactor();
    else
      internalError("unknown early/late value.");
  }
  else
    return delay.mean();
}

float
delaySigma2(const Delay &delay,
	    const EarlyLate *)
{
  return delay.sigma2();
}

const char *
delayAsString(const Delay &delay,
	      const StaState *sta)
{
  return delayAsString(delay, sta, sta->units()->timeUnit()->digits());
}

const char *
delayAsString(const Delay &delay,
	      const StaState *sta,
	      int digits)
{
  const Unit *unit = sta->units()->timeUnit();
  if (sta->pocvEnabled()) {
    float sigma = delay.sigma();
    return stringPrintTmp("%s[%s]",
			  unit->asString(delay.mean(), digits),
			  unit->asString(sigma, digits));
  }
  else
    return unit->asString(delay.mean(), digits);
}

const char *
delayAsString(const Delay &delay,
	      const EarlyLate *early_late,
	      const StaState *sta,
	      int digits)
{
  float mean_sigma = delayAsFloat(delay, early_late, sta);
  return sta->units()->timeUnit()->asString(mean_sigma, digits);
}

} // namespace

#endif // (SSTA == 1)
