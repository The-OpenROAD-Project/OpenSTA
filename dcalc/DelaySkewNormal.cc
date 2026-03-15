// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "DelaySkewNormal.hh"

#include <cmath> // sqrt

#include "Error.hh"
#include "Fuzzy.hh"
#include "Units.hh"
#include "Format.hh"
#include "StaState.hh"
#include "Variables.hh"

namespace sta {

float
DelayOpsSkewNormal::stdDev2(const Delay &delay,
                            const EarlyLate *) const
{
  return delay.stdDev2();
}

float
DelayOpsSkewNormal::asFloat(const Delay &delay,
                            const EarlyLate *early_late,
                            const StaState *sta) const
{
  // LVF: mean + mean_shift + sigma * sigma_factor with skewness consideration.
  float quantile = sta->variables()->pocvQuantile();
  if (early_late == EarlyLate::early())
    return delay.mean() + delay.meanShift()
      - delay.stdDev() * (quantile + delay.skewness() * (square(quantile)-1.0) / 6.0);
  else // (early_late == EarlyLate::late())
    return delay.mean() + delay.meanShift()
      + delay.stdDev() * (quantile + delay.skewness() * (square(quantile)-1.0) / 6.0);
}

double
DelayOpsSkewNormal::asFloat(const DelayDbl &delay,
                            const EarlyLate *early_late,
                            const StaState *sta) const
{
  // LVF: mean + mean_shift + sigma * sigma_factor with skewness consideration.
  double quantile = sta->variables()->pocvQuantile();
  if (early_late == EarlyLate::early())
    return delay.mean() + delay.meanShift()
      - delay.stdDev() * (quantile + delay.skewness() * (square(quantile)-1.0) / 6.0);
  else // (early_late == EarlyLate::late())
    return delay.mean() + delay.meanShift()
      + delay.stdDev() * (quantile + delay.skewness() * (square(quantile)-1.0) / 6.0);
}

bool
DelayOpsSkewNormal::isZero(const Delay &delay) const
{
  return fuzzyZero(delay.mean())
    && fuzzyZero(delay.meanShift())
    && fuzzyZero(delay.stdDev2())
    && fuzzyZero(delay.skewness());
}

bool
DelayOpsSkewNormal::isInf(const Delay &delay) const
{
  return fuzzyInf(delay.mean());
}

bool
DelayOpsSkewNormal::equal(const Delay &delay1,
                          const Delay &delay2,
                          const StaState *) const
{
  return fuzzyEqual(delay1.mean(), delay2.mean())
    && fuzzyEqual(delay1.meanShift(), delay2.meanShift())
    && fuzzyEqual(delay1.stdDev2(), delay2.stdDev2())
    && fuzzyEqual(delay1.skewness(), delay2.skewness());
}

bool
DelayOpsSkewNormal::less(const Delay &delay1,
                         const Delay &delay2,
                         const StaState *sta) const
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
                   delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsSkewNormal::less(const DelayDbl &delay1,
                         const DelayDbl &delay2,
                         const StaState *sta) const
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
                   delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsSkewNormal::lessEqual(const Delay &delay1,
                              const Delay &delay2,
                              const StaState *sta) const
{
  return fuzzyLessEqual(delayAsFloat(delay1, EarlyLate::early(), sta),
                        delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsSkewNormal::greater(const Delay &delay1,
                            const Delay &delay2,
                            const StaState *sta) const
{
  return fuzzyGreater(delayAsFloat(delay1, EarlyLate::late(), sta),
		      delayAsFloat(delay2, EarlyLate::late(), sta));
}

bool
DelayOpsSkewNormal::greaterEqual(const Delay &delay1,
                                 const Delay &delay2,
                                 const StaState *sta) const
{
  return fuzzyGreaterEqual(delayAsFloat(delay1, EarlyLate::late(), sta),
			   delayAsFloat(delay2, EarlyLate::late(), sta));
}

Delay
DelayOpsSkewNormal::sum(const Delay &delay1,
                        const Delay &delay2) const
{
  return Delay(delay1.mean() + delay2.mean(),
	       delay1.meanShift() + delay2.meanShift(),
	       delay1.stdDev2() + delay2.stdDev2(),
               skewnessSum(delay1, delay2));
}

float
DelayOpsSkewNormal::skewnessSum(const Delay &delay1,
                                const Delay &delay2) const
{
  return skewnessSum(delay1.stdDev(), delay1.skewness(),
                     delay2.stdDev(), delay2.skewness());
}

// Helper function to compute combined skewness from std dev and skewness values.
double
DelayOpsSkewNormal::skewnessSum(double std_dev1,
                                double skewness1,
                                double std_dev2,
                                double skewness2) const
{
  double std_dev_sum = square(std_dev1) + square(std_dev2);
  if (std_dev_sum == 0.0)
    return 0.0;
  else {
    // Un-normalize the skews so they are third moments so they can be added.
    double skew = (skewness1 * cube(std_dev1) + skewness2 * cube(std_dev2))
      // std_dev_sum^(3/2)
      / (std_dev_sum * std::sqrt(std_dev_sum));
    return skew;
  }
}

Delay
DelayOpsSkewNormal::sum(const Delay &delay1,
                        float delay2) const
{
  return Delay(delay1.mean() + delay2,
	       delay1.meanShift(),
	       delay1.stdDev2(),
               delay1.skewness());
}

Delay
DelayOpsSkewNormal::diff(const Delay &delay1,
                         const Delay &delay2) const
{
  return Delay(delay1.mean() - delay2.mean(), 
	       delay1.meanShift() - delay2.meanShift(),
	       delay1.stdDev2() + delay2.stdDev2(),
               skewnessSum(delay1, delay2));
}

Delay
DelayOpsSkewNormal::diff(const Delay &delay1,
                         float delay2) const
{
  return Delay(delay1.mean() - delay2, 
	       delay1.meanShift(),
	       delay1.stdDev2(),
               delay1.skewness());
}

Delay
DelayOpsSkewNormal::diff(float delay1,
                         const Delay &delay2) const
{
  return Delay(delay1 - delay2.mean(), 
	       delay2.meanShift(),
	       delay2.stdDev2(),
               delay2.skewness());
}

void
DelayOpsSkewNormal::incr(Delay &delay1,
                         const Delay &delay2) const
{
  delay1.setValues(delay1.mean() + delay2.mean(),
                   delay1.meanShift() + delay2.meanShift(),
                   delay1.stdDev2() + delay2.stdDev2(),
                   skewnessSum(delay1, delay2));
}

void
DelayOpsSkewNormal::incr(DelayDbl &delay1,
                         const Delay &delay2) const
{
  delay1.setValues(delay1.mean() + delay2.mean(),
                   delay1.meanShift() + delay2.meanShift(),
                   delay1.stdDev2() + delay2.stdDev2(),
                   skewnessSum(delay1.stdDev(), delay1.skewness()));
}

void
DelayOpsSkewNormal::decr(Delay &delay1,
                         const Delay &delay2) const
{
  delay1.setValues(delay1.mean() - delay2.mean(),
                   delay1.meanShift() + delay2.meanShift(),
                   delay1.stdDev2() + delay2.stdDev2(),
                   skewnessSum(delay1, delay2));
}

void
DelayOpsSkewNormal::decr(DelayDbl &delay1,
                         const Delay &delay2) const
{
  delay1.setValues(delay1.mean() - delay2.mean(),
                   delay1.meanShift() + delay2.meanShift(),
                   delay1.stdDev2() + delay2.stdDev2(),
                   skewnessSum(delay1.stdDev(), delay1.skewness()));
}

Delay
DelayOpsSkewNormal::product(const Delay &delay1,
                            float delay2) const
{
  return Delay(delay1.mean() * delay2,
	       delay1.meanShift() * delay2,
	       delay1.stdDev2() * square(delay2),
	       delay1.skewness() * cube(delay2));
}

Delay
DelayOpsSkewNormal::div(float delay1,
                        const Delay &delay2) const
{
  return Delay(delay1 / delay2.mean());
}

std::string
DelayOpsSkewNormal::asStringVariance(const Delay &delay,
                                     int digits,
                                     const StaState *sta) const
{
  const Unit *unit = sta->units()->timeUnit();
  return sta::format("{}[{},{},{}]",
                     unit->asString(delay.mean(), digits),
                     unit->asString(delay.meanShift(), digits),
                     unit->asString(delay.stdDev(), digits),
                     sta->units()->scalarUnit()->asString(delay.skewness(), digits));
}

} // namespace
