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

// Delay as floats, non-SSTA.

#include "DelayScalar.hh"

#include "Fuzzy.hh"
#include "Units.hh"
#include "StaState.hh"

namespace sta {

float
DelayOpsScalar::stdDev2(const Delay &,
                       const EarlyLate *) const
{
  return 0.0;
}

float
DelayOpsScalar::asFloat(const Delay &delay,
                        const EarlyLate *,
                        const StaState *) const
{
  return delay.mean();
}

double
DelayOpsScalar::asFloat(const DelayDbl &delay,
                        const EarlyLate *,
                        const StaState *) const
{
  return delay.mean();
}

bool
DelayOpsScalar::isZero(const Delay &delay) const
{
  return fuzzyZero(delay.mean());
}

bool
DelayOpsScalar::isInf(const Delay &delay) const
{
  return fuzzyInf(delay.mean());
}

bool
DelayOpsScalar::equal(const Delay &delay1,
                      const Delay &delay2,
                      const StaState *) const
{
  return fuzzyEqual(delay1.mean(), delay2.mean());
}

bool
DelayOpsScalar::less(const Delay &delay1,
                     const Delay &delay2,
                     const StaState *) const
{
  return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
DelayOpsScalar::less(const DelayDbl &delay1,
                     const DelayDbl &delay2,
                     const StaState *) const
{
  return fuzzyLess(delay1.mean(), delay2.mean());
}

bool
DelayOpsScalar::lessEqual(const Delay &delay1,
                          const Delay &delay2,
                          const StaState *) const
{
  return fuzzyLessEqual(delay1.mean(), delay2.mean());
}

bool
DelayOpsScalar::greater(const Delay &delay1,
                        const Delay &delay2,
                        const StaState *) const
{
  return fuzzyGreater(delay1.mean(), delay2.mean());
}

bool
DelayOpsScalar::greaterEqual(const Delay &delay1,
                             const Delay &delay2,
                             const StaState *) const
{
  return fuzzyGreaterEqual(delay1.mean(), delay2.mean());
}

Delay
DelayOpsScalar::sum(const Delay &delay1,
                    const Delay &delay2) const
{
  return Delay(delay1.mean() + delay2.mean());
}

Delay
DelayOpsScalar::sum(const Delay &delay1,
                    float delay2) const
{
  return Delay(delay1.mean() + delay2);
}

Delay
DelayOpsScalar::diff(const Delay &delay1,
                     const Delay &delay2) const
{
  return Delay(delay1.mean() - delay2.mean());
}

Delay
DelayOpsScalar::diff(const Delay &delay1,
                     float delay2) const
{
  return Delay(delay1.mean() - delay2);
}

Delay
DelayOpsScalar::diff(float delay1,
                     const Delay &delay2) const
{
  return Delay(delay1 - delay2.mean());
}


void
DelayOpsScalar::incr(Delay &delay1,
                    const Delay &delay2) const
{
  delay1.setMean(delay1.mean() + delay2.mean());
}

void
DelayOpsScalar::incr(DelayDbl &delay1,
                    const Delay &delay2) const
{
  delay1.setMean(delay1.mean() + delay2.mean());
}

void
DelayOpsScalar::decr(Delay &delay1,
                     const Delay &delay2) const
{
  delay1.setMean(delay1.mean() - delay2.mean());
}

void
DelayOpsScalar::decr(DelayDbl &delay1,
                     const Delay &delay2) const
{
  delay1.setMean(delay1.mean() - delay2.mean());
}

Delay
DelayOpsScalar::product(const Delay &delay1,
                        float delay2) const
{
  return Delay(delay1.mean() * delay2);
}

Delay
DelayOpsScalar::div(float delay1,
                    const Delay &delay2) const
{
  return Delay(delay1 / delay2.mean());
}

const char *
DelayOpsScalar::asStringVariance(const Delay &delay,
                                 int digits,
                                 const StaState *sta) const
{
  const Unit *unit = sta->units()->timeUnit();
  return unit->asString(delay.mean(), digits);
}

} // namespace

