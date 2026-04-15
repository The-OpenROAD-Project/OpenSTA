// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "DelayNormal.hh"

#include <cmath> // sqrt

#include "Error.hh"
#include "Format.hh"
#include "Fuzzy.hh"
#include "StaState.hh"
#include "Units.hh"
#include "Variables.hh"

namespace sta {

float
DelayOpsNormal::stdDev2(const Delay &delay,
                        const EarlyLate *) const
{
  return delay.stdDev2();
}

float
DelayOpsNormal::asFloat(const Delay &delay,
                        const EarlyLate *early_late,
                        const StaState *sta) const
{
  float quantile = sta->variables()->pocvQuantile();
  if (early_late == EarlyLate::early())
    return delay.mean() - delay.stdDev() * quantile;
  else // (early_late == EarlyLate::late())
    return delay.mean() + delay.stdDev() * quantile;
}

double
DelayOpsNormal::asFloat(const DelayDbl &delay,
                        const EarlyLate *early_late,
                        const StaState *sta) const
{
  double quantile = sta->variables()->pocvQuantile();
  if (early_late == EarlyLate::early())
    return delay.mean() - delay.stdDev() * quantile;
  else // (early_late == EarlyLate::late())
    return delay.mean() + delay.stdDev() * quantile;
}

bool
DelayOpsNormal::isZero(const Delay &delay) const
{
  return fuzzyZero(delay.mean())
    && fuzzyZero(delay.stdDev2());
}

bool
DelayOpsNormal::isInf(const Delay &delay) const
{
  return fuzzyInf(delay.mean());
}

bool
DelayOpsNormal::equal(const Delay &delay1,
                      const Delay &delay2,
                      const StaState *) const
{
  return fuzzyEqual(delay1.mean(), delay2.mean())
    && fuzzyEqual(delay1.stdDev2(), delay2.stdDev2());
}

bool
DelayOpsNormal::less(const Delay &delay1,
                     const Delay &delay2,
                     const StaState *sta) const
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
                   delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsNormal::less(const DelayDbl &delay1,
                     const DelayDbl &delay2,
                     const StaState *sta) const
{
  return fuzzyLess(delayAsFloat(delay1, EarlyLate::early(), sta),
                   delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsNormal::lessEqual(const Delay &delay1,
                          const Delay &delay2,
                          const StaState *sta) const
{
  return fuzzyLessEqual(delayAsFloat(delay1, EarlyLate::early(), sta),
                        delayAsFloat(delay2, EarlyLate::early(), sta));
}

bool
DelayOpsNormal::greater(const Delay &delay1,
                        const Delay &delay2,
                        const StaState *sta) const
{
  return fuzzyGreater(delayAsFloat(delay1, EarlyLate::late(), sta),
		      delayAsFloat(delay2, EarlyLate::late(), sta));
}

bool
DelayOpsNormal::greaterEqual(const Delay &delay1,
                             const Delay &delay2,
                             const StaState *sta) const
{
  return fuzzyGreaterEqual(delayAsFloat(delay1, EarlyLate::late(), sta),
			   delayAsFloat(delay2, EarlyLate::late(), sta));
}

Delay
DelayOpsNormal::sum(const Delay &delay1,
                    const Delay &delay2) const
{
  return Delay(delay1.mean() + delay2.mean(),
	       delay1.stdDev2() + delay2.stdDev2());
}

Delay
DelayOpsNormal::sum(const Delay &delay1,
                        float delay2) const
{
  return Delay(delay1.mean() + delay2,
	       delay1.stdDev2());
}

Delay
DelayOpsNormal::diff(const Delay &delay1,
                     const Delay &delay2) const
{
  return Delay(delay1.mean() - delay2.mean(), 
	       delay1.stdDev2() + delay2.stdDev2());
}

Delay
DelayOpsNormal::diff(const Delay &delay1,
                     float delay2) const
{
  return Delay(delay1.mean() - delay2, 
	       delay1.stdDev2());
}

Delay
DelayOpsNormal::diff(float delay1,
                     const Delay &delay2) const
{
  return Delay(delay1 - delay2.mean(), 
	       delay2.stdDev2());
}

void
DelayOpsNormal::incr(Delay &delay1,
                     const Delay &delay2) const
{
  delay1.setValues(delay1.mean() + delay2.mean(), 0.0,
                   delay1.stdDev2() + delay2.stdDev2(), 0.0);
}

void
DelayOpsNormal::incr(DelayDbl &delay1,
                     const Delay &delay2) const
{
  delay1.setValues(delay1.mean() + delay2.mean(), 0.0,
                   delay1.stdDev2() + delay2.stdDev2(), 0.0);
}

void
DelayOpsNormal::decr(Delay &delay1,
                     const Delay &delay2) const
{
  delay1.setMean(delay1.mean() - delay2.mean());
}

void
DelayOpsNormal::decr(DelayDbl &delay1,
                     const Delay &delay2) const
{
  delay1.setMean(delay1.mean() - delay2.mean());
}

Delay
DelayOpsNormal::product(const Delay &delay1,
                        float delay2) const
{
  return Delay(delay1.mean() * delay2,
	       delay1.stdDev2() * square(delay2));
}

Delay
DelayOpsNormal::div(float delay1,
                    const Delay &delay2) const
{
  return Delay(delay1 / delay2.mean());
}

std::string
DelayOpsNormal::asStringVariance(const Delay &delay,
                                 int digits,
                                 const StaState *sta) const
{
  const Unit *unit = sta->units()->timeUnit();
  return sta::format("{}[{}]",
                     unit->asString(delay.mean(), digits),
                     unit->asString(delay.stdDev(), digits));
}

} // namespace sta
