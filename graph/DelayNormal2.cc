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

#include <cmath> // sqrt
#include "Machine.hh"
#include "StringUtil.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"
#include "Delay.hh"

// Conditional compilation based on delay abstraction from Delay.hh.
#ifdef DELAY_NORMAL2

namespace sta {

#define square(x) ((x)*(x))

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
  sigma_{0.0, 0.0}
{
}

Delay::Delay(float mean) :
  mean_(mean),
  sigma_{0.0, 0.0}
{
}

Delay::Delay(float mean,
	     float sigma_early,
	     float sigma_late) :
  mean_(mean),
  sigma_{sigma_early, sigma_late}
{
}

float
Delay::sigma(const EarlyLate *early_late) const
{
  return sigma_[early_late->index()];
}

void
Delay::operator=(const Delay &delay)
{
  mean_ = delay.mean_;
  sigma_[early_index] = delay.sigma_[early_index];
  sigma_[late_index] = delay.sigma_[late_index];
}

void
Delay::operator=(float delay)
{
  mean_ = delay;
  sigma_[early_index] = 0.0;
  sigma_[late_index] = 0.0;
}

void
Delay::operator+=(const Delay &delay)
{
  mean_ += delay.mean_;
  sigma_[early_index] = sqrt(square(sigma_[early_index])
					 + square(delay.sigma_[early_index]));
  sigma_[late_index] = sqrt(square(sigma_[late_index])
					+ square(delay.sigma_[late_index]));
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
	       sqrt(square(sigma_[early_index])
		    + square(delay.sigma_[early_index])),
	       sqrt(square(sigma_[late_index])
		    + square(delay.sigma_[late_index])));
}

Delay
Delay::operator+(float delay) const
{
  return Delay(mean_ + delay, sigma_[early_index], sigma_[late_index]);
}

Delay
Delay::operator-(const Delay &delay) const
{
  return Delay(mean_ - delay.mean_,
	       sqrt(square(sigma_[early_index])
		    + square(delay.sigma_[early_index])),
	       sqrt(square(sigma_[late_index])
		    + square(delay.sigma_[late_index])));
}

Delay
Delay::operator-(float delay) const
{
  return Delay(mean_ - delay, sigma_[early_index], sigma_[late_index]);
}

Delay
Delay::operator-() const
{
  return Delay(-mean_, sigma_[early_index], sigma_[late_index]);
}

void
Delay::operator-=(float delay)
{
  mean_ -= - delay;
}

bool
Delay::operator==(const Delay &delay) const
{
  return mean_ == delay.mean_
    && sigma_[early_index] == delay.sigma_[early_index]
    && sigma_[late_index]  == delay.sigma_[late_index];
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

bool
delayIsInitValue(const Delay &delay,
		 const MinMax *min_max)
{
  return fuzzyEqual(delay.mean(), min_max->initValue())
    && delay.sigmaEarly() == 0.0
    && delay.sigmaLate()  == 0.0;
}

bool
delayFuzzyZero(const Delay &delay)
{
  return fuzzyZero(delay.mean())
    && fuzzyZero(delay.sigmaEarly())
    && fuzzyZero(delay.sigmaLate());
}

bool
delayFuzzyEqual(const Delay &delay1,
		const Delay &delay2)
{
  return fuzzyEqual(delay1.mean(), delay2.mean())
    && fuzzyEqual(delay1.sigmaEarly(), delay2.sigmaEarly())
    && fuzzyEqual(delay1.sigmaLate(),  delay2.sigmaLate());
}

bool
delayFuzzyLess(const Delay &delay1,
	       const Delay &delay2)
{
  return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
delayFuzzyLess(const Delay &delay1,
	       float delay2)
{
  return fuzzyLess(delay1.mean(), delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2)
{
  return fuzzyLessEqual(delay1.mean(), delay2.mean());
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    float delay2)
{
  return fuzzyLessEqual(delay1.mean(), delay2);
}

bool
delayFuzzyLessEqual(const Delay &delay1,
		    const Delay &delay2,
		    const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyLessEqual(delay1.mean(), delay2.mean());
  else
    return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2)
{
  return fuzzyGreater(delay1.mean(), delay2.mean());
}

bool
delayFuzzyGreater(const Delay &delay1,
		  float delay2)
{
  return fuzzyGreater(delay1.mean(), delay2);
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2)
{
  return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       float delay2)
{
  return fuzzyGreaterEqual(delay1.mean(), delay2);
}

bool
delayFuzzyGreater(const Delay &delay1,
		  const Delay &delay2,
		  const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreater(delay1.mean(), delay2.mean());
  else
    return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
delayFuzzyGreaterEqual(const Delay &delay1,
		       const Delay &delay2,
		       const MinMax *min_max)
{
  if (min_max == MinMax::max())
    return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
  else
    return fuzzyLessEqual(delay1.mean(), delay2.mean());
}

bool
delayFuzzyLess(const Delay &delay1,
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
	       delay2.sigmaEarly(),
	       delay2.sigmaLate());
}

Delay
operator/(float delay1,
	  const Delay &delay2)
{
  return Delay(delay1 / delay2.mean(),
	       delay2.sigmaEarly(),
	       delay2.sigmaLate());
}

Delay
operator*(const Delay &delay1,
	  float delay2)
{
  return Delay(delay1.mean() * delay2,
	       delay1.sigmaEarly() * delay2,
	       delay1.sigmaLate() * delay2);
}

float
delayRatio(const Delay &delay1,
	   const Delay &delay2)
{
  return delay1.mean() / delay2.mean();
}

const char *
delayAsString(const Delay &delay,
	      const Units *units,
	      int digits)
{
  const Unit *unit = units->timeUnit();
  float sigma_early = delay.sigmaEarly();
  float sigma_late = delay.sigmaLate();
  if (fuzzyEqual(sigma_early, sigma_late))
    return stringPrintTmp((digits + 2) * 2 + 2,
			  "%s|%s",
			  unit->asString(delay.mean(), digits),
			  unit->asString(sigma_early, digits));
  else
    return stringPrintTmp((digits + 2) * 3 + 3,
			  "%s|%s:%s",
			  unit->asString(delay.mean(), digits),
			  unit->asString(sigma_early, digits),
			  unit->asString(sigma_late, digits));
}

const char *
delayMeanSigmaString(const Delay &delay,
		     const EarlyLate *early_late,
		     const Units *units,
		     int digits)
{
  float mean_sigma = delay.mean();
  if (early_late == EarlyLate::early())
    mean_sigma -= delay.sigmaEarly();
  else if (early_late == EarlyLate::late())
    mean_sigma += delay.sigmaLate();
  return units->timeUnit()->asString(mean_sigma, digits);
}

float
delayMeanSigma(const Delay &delay,
	       const EarlyLate *early_late)
{
  if (early_late == EarlyLate::early())
    return delay.mean() - delay.sigmaEarly();
  else if (early_late == EarlyLate::late())
    return delay.mean() + delay.sigmaLate();
  else
    return delay.mean();
}

} // namespace
#endif
