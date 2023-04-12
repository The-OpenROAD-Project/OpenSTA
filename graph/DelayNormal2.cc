// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "Delay.hh"

#include <cmath> // sqrt

#include "StaConfig.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"

// SSTA compilation.
#if (SSTA == 2)

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
  sigma2_{0.0, 0.0}
{
}

Delay::Delay(const Delay &delay) :
  mean_(delay.mean_)
{
  sigma2_[EarlyLate::earlyIndex()] = delay.sigma2_[EarlyLate::earlyIndex()];
  sigma2_[EarlyLate::lateIndex()] = delay.sigma2_[EarlyLate::lateIndex()];
}

Delay::Delay(float mean) :
  mean_(mean),
  sigma2_{0.0, 0.0}
{
}

Delay::Delay(float mean,
	     float sigma2_early,
	     float sigma2_late) :
  mean_(mean),
  sigma2_{sigma2_early, sigma2_late}
{
}

float
Delay::sigma(const EarlyLate *early_late) const
{
  float sigma = sigma2_[early_late->index()];
  if (sigma < 0.0)
    // Sigma is negative for crpr to offset sigmas in the common
    // clock path.
    return -sqrt(-sigma);
  else
    return sqrt(sigma);
}

float
Delay::sigma2(const EarlyLate *early_late) const
{
  return sigma2_[early_late->index()];
}

float
Delay::sigma2Early() const
{
  return sigma2_[early_index];
}

float
Delay::sigma2Late() const
{
  return sigma2_[late_index];
}

void
Delay::operator=(const Delay &delay)
{
  mean_ = delay.mean_;
  sigma2_[early_index] = delay.sigma2_[early_index];
  sigma2_[late_index] = delay.sigma2_[late_index];
}

void
Delay::operator=(float delay)
{
  mean_ = delay;
  sigma2_[early_index] = 0.0;
  sigma2_[late_index] = 0.0;
}

void
Delay::operator+=(const Delay &delay)
{
  mean_ += delay.mean_;
  sigma2_[early_index] += delay.sigma2_[early_index];
  sigma2_[late_index] += delay.sigma2_[late_index];
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
	       sigma2_[early_index] + delay.sigma2_[early_index],
	       sigma2_[late_index] + delay.sigma2_[late_index]);
}

Delay
Delay::operator+(float delay) const
{
  return Delay(mean_ + delay, sigma2_[early_index], sigma2_[late_index]);
}

Delay
Delay::operator-(const Delay &delay) const
{
  return Delay(mean_ - delay.mean_,
	       sigma2_[early_index] + delay.sigma2_[late_index],
	       sigma2_[late_index] + delay.sigma2_[early_index]);
}

Delay
Delay::operator-(float delay) const
{
  return Delay(mean_ - delay, sigma2_[early_index], sigma2_[late_index]);
}

Delay
Delay::operator-() const
{
  return Delay(-mean_, sigma2_[late_index], sigma2_[early_index]);
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
  sigma2_[early_index] += delay.sigma2_[early_index];
  sigma2_[late_index] += delay.sigma2_[late_index];
}

bool
Delay::operator==(const Delay &delay) const
{
  return mean_ == delay.mean_
    && sigma2_[early_index] == delay.sigma2_[late_index]
    && sigma2_[late_index]  == delay.sigma2_[early_index];
}

////////////////////////////////////////////////////////////////

Delay
makeDelay(float delay,
	  float sigma_early,
	  float sigma_late)
{
  return Delay(delay, square(sigma_early), square(sigma_late));
}

Delay
makeDelay2(float delay,
	   float sigma2_early,
	   float sigma2_late)
{
  return Delay(delay, sigma2_early, sigma2_late);
}

bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max)
{
  return fuzzyEqual(delay.mean(), min_max->initValue())
    && fuzzyZero(delay.sigma2Early())
    && fuzzyZero(delay.sigma2Late());
}

bool
delayZero(const Delay &delay)
{
  return fuzzyZero(delay.mean())
    && fuzzyZero(delay.sigma2Early())
    && fuzzyZero(delay.sigma2Late());
}

bool
delayInf(const Delay &delay)
{
  return fuzzyInf(delay.mean());
}

bool
delayEqual(const Delay &delay1,
	   const Delay &delay2)
{
  return fuzzyEqual(delay1.mean(), delay2.mean())
    && fuzzyEqual(delay1.sigma2Early(), delay2.sigma2Early())
    && fuzzyEqual(delay1.sigma2Late(),  delay2.sigma2Late());
}

bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const StaState *sta)
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
		   delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
delayLess(const Delay &delay1,
	  float delay2,
	  const StaState *sta)
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
		   delay2);
}

bool
delayLess(const Delay &delay1,
	  const Delay &delay2,
	  const MinMax *min_max,
	  const StaState *sta)
{
  if (min_max == MinMax::max())
    return delayLess(delay1, delay2, sta);
  else
    return delayGreater(delay1, delay2, sta);
}

bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const StaState *sta)
{
  return fuzzyLessEqual(delayAsFloat(delay1, EarlyLate::early(), sta),
			delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
delayLessEqual(const Delay &delay1,
	       float delay2,
	       const StaState *sta)
{
  return fuzzyLessEqual(delayAsFloat(delay1, EarlyLate::early(), sta),
			delay2);
}

bool
delayLessEqual(const Delay &delay1,
	       const Delay &delay2,
	       const MinMax *min_max,
	       const StaState *sta)
{
  if (min_max == MinMax::max())
    return delayLessEqual(delay1, delay2, sta);
  else
    return delayGreaterEqual(delay1, delay2, sta);
}

bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const StaState *sta)
{
  return fuzzyGreater(delayAsFloat(delay1, EarlyLate::late(), sta),
		      delayAsFloat(delay2, EarlyLate::late(), sta));
}

bool
delayGreater(const Delay &delay1,
	     float delay2,
	     const StaState *sta)
{
  return fuzzyGreaterEqual(delayAsFloat(delay1, EarlyLate::late(), sta),
			   delayAsFloat(delay2, EarlyLate::late(), sta));
}

bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const StaState *sta)
{
  return fuzzyGreaterEqual(delayAsFloat(delay1, EarlyLate::late(), sta),
			   delayAsFloat(delay2, EarlyLate::late(), sta));
}

bool
delayGreaterEqual(const Delay &delay1,
		  float delay2,
		  const StaState *sta)
{
  return fuzzyGreaterEqual(delayAsFloat(delay1, EarlyLate::late(), sta),
			   delay2);
}

bool
delayGreater(const Delay &delay1,
	     const Delay &delay2,
	     const MinMax *min_max,
	     const StaState *sta)
{
  if (min_max == MinMax::max())
    return delayGreater(delay1, delay2, sta);
  else
    return delayLess(delay1, delay2, sta);
}

bool
delayGreaterEqual(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max,
		  const StaState *sta)
{
  if (min_max == MinMax::max())
    return delayGreaterEqual(delay1, delay2, sta);
  else
    return delayLessEqual(delay1, delay2, sta);
}

float
delayAsFloat(const Delay &delay,
	     const EarlyLate *early_late,
	     const StaState *sta)
{
  if (sta->pocvEnabled()) {
    if (early_late == EarlyLate::early())
      return delay.mean() - delay.sigma(early_late) * sta->sigmaFactor();
    else if (early_late == EarlyLate::late())
      return delay.mean() + delay.sigma(early_late) * sta->sigmaFactor();
    else
      sta->report()->critical(595, "unknown early/late value.");
  }
  return delay.mean();
}

float
delaySigma2(const Delay &delay,
	    const EarlyLate *early_late)
{
  return delay.sigma2(early_late);
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
    float sigma_early = delay.sigma(EarlyLate::early());
    float sigma_late = delay.sigma(EarlyLate::late());
    return stringPrintTmp("%s[%s:%s]",
			  unit->asString(delay.mean(), digits),
			  unit->asString(sigma_early, digits),
			  unit->asString(sigma_late, digits));
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

Delay
delayRemove(const Delay &delay1,
	    const Delay &delay2)
{
  return Delay(delay1.mean() - delay2.mean(),
	       delay1.sigma2Early() - delay2.sigma2Early(),
	       delay1.sigma2Late() - delay2.sigma2Late());
}

float
delayRatio(const Delay &delay1,
	   const Delay &delay2)
{
  return delay1.mean() / delay2.mean();
}

Delay
operator+(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 + delay2.mean(),
	       delay2.sigma2Early(),
	       delay2.sigma2Late());
}

Delay
operator/(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 / delay2.mean(),
	       delay2.sigma2Early(),
	       delay2.sigma2Late());
}

Delay
operator*(const Delay &delay1,
	  float delay2)
{
  return Delay(delay1.mean() * delay2,
	       delay1.sigma2Early() * delay2 * delay2,
	       delay1.sigma2Late() * delay2 * delay2);
}

} // namespace

#endif // (SSTA == 2)
