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

#include "Delay.hh"

#include <cmath>

#include "Fuzzy.hh"
#include "StaConfig.hh"
#include "StaState.hh"
#include "Units.hh"
#include "Variables.hh"

namespace sta {

static Delay delay_init_values[MinMax::index_count];

void
initDelayConstants()
{
  delay_init_values[MinMax::minIndex()] = MinMax::min()->initValue();
  delay_init_values[MinMax::maxIndex()] = MinMax::max()->initValue();
}

Delay::Delay() noexcept :
  values_{0.0, 0.0, 0.0, 0.0}
{
}

Delay::Delay(float mean) noexcept :
  values_{mean, 0.0, 0.0, 0.0}
{
}

Delay::Delay(float mean,
             float std_dev2) noexcept :
  values_{mean, 0.0, std_dev2, 0.0}
{
}

Delay::Delay(float mean,
             float mean_shift,
             float std_dev2,
             float skewness) noexcept :
  values_{mean, mean_shift, std_dev2, skewness}
{
}

Delay &
Delay::operator=(float delay)
{
  values_[0] = delay;
  values_[1] = 0.0;
  values_[2] = 0.0;
  values_[3] = 0.0;
  return *this;
}

void
Delay::setValues(float mean,
                 float mean_shift,
                 float std_dev2,
                 float skewnes)
{
  values_[0] = mean;
  values_[1] = mean_shift;
  values_[2] = std_dev2;
  values_[3] = skewnes;  
}

void
Delay::setMean(float mean)
{
  values_[0] = mean;
}

void
Delay::setMeanShift(float mean_shift)
{
  values_[1] = mean_shift;
}

float
Delay::stdDev() const
{
  float std_dev2 = values_[2];
  if (std_dev2 < 0.0)
    // std_dev is negative for crpr to offset std_dev in the common
    // clock path.
    return -std::sqrt(-std_dev2);
  else
    return std::sqrt(std_dev2);
}

void
Delay::setStdDev(float std_dev)
{
  values_[2] = square(std_dev);
}

void
Delay::setSkewness(float skewness)
{
  values_[3] = skewness;
}

////////////////////////////////////////////////////////////////

DelayDbl::DelayDbl() noexcept :
  values_{0.0, 0.0, 0.0, 0.0}
{
}

DelayDbl::DelayDbl(double mean) noexcept :
  values_{mean, 0.0, 0.0, 0.0}
{
}

void
DelayDbl::setMean(double mean)
{
  values_[0] = mean;
}

double
DelayDbl::stdDev() const
{
  float std_dev2 = values_[2];
  if (std_dev2 < 0.0)
    // std_dev is negative for crpr to offset std_dev in the common
    // clock path.
    return -std::sqrt(-std_dev2);
  else
    return std::sqrt(std_dev2);
}

void
DelayDbl::setValues(double mean,
                    double mean_shift,
                    double std_dev2,
                    double skewnes)
{
  values_[0] = mean;
  values_[1] = mean_shift;
  values_[2] = std_dev2;
  values_[3] = skewnes;  
}

DelayDbl &
DelayDbl::operator=(double delay)
{
  values_[0] = delay;
  values_[1] = 0.0;
  values_[2] = 0.0;
  values_[3] = 0.0;
  return *this;
}

////////////////////////////////////////////////////////////////

Delay
makeDelay(float mean,
          float mean_shift,
          float std_dev,
          float skewness)
{
  return Delay(mean, mean_shift, square(std_dev), skewness);
}

Delay
makeDelay(float mean,
          float std_dev)
{
  return Delay(mean, 0.0, square(std_dev), 0.0);
}

Delay
makeDelay2(float mean,
           float std_dev)
{
  return Delay(mean, 0.0, std_dev, 0.0);
}

void
delaySetMean(Delay &delay,
             float mean)
{
  delay.setMean(mean);
}

////////////////////////////////////////////////////////////////

Delay
delayDblAsDelay(DelayDbl &delay)
{
  return Delay(delay.mean(), delay.meanShift(), delay.stdDev2(), delay.skewness());
}

std::string
delayAsString(const Delay &delay,
              const StaState *sta)
{
  return delayAsString(delay, EarlyLate::late(),
                       sta->units()->timeUnit()->digits(), sta);
}

std::string
delayAsString(const Delay &delay,
              int digits,
              const StaState *sta)
{
  return delayAsString(delay, EarlyLate::late(), digits, sta);
}

std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              const StaState *sta)
{
  return delayAsString(delay, early_late, sta->units()->timeUnit()->digits(), sta);
}

std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              int digits,
              const StaState *sta)
{
  const Unit *unit = sta->units()->timeUnit();
  float mean_std_dev = delayAsFloat(delay, early_late, sta);
  return unit->asString(mean_std_dev, digits);
}

std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              bool report_variance,
              int digits,
              const StaState *sta)
{
  if (report_variance)
    return sta->delayOps()->asStringVariance(delay, digits, sta);
  else
    return delayAsString(delay, early_late, digits, sta);
}

float
delayAsFloat(const Delay &delay,
             const EarlyLate *early_late,
             const StaState *sta)
{
  return sta->delayOps()->asFloat(delay, early_late, sta);
}

float
delayAsFloat(const DelayDbl &delay,
             const EarlyLate *early_late,
             const StaState *sta)
{
  return sta->delayOps()->asFloat(delay, early_late, sta);
}

float
delayAsFloat(const Delay &delay)
{
  return delay.mean();
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
  return fuzzyEqual(delay.mean(), min_max->initValue());
}

bool
delayZero(const Delay &delay,
          const StaState *sta)
{
  return sta->delayOps()->isZero(delay);
}

bool
delayInf(const Delay &delay,
         const StaState *sta)
{
  return sta->delayOps()->isInf(delay);
}

bool
delayEqual(const Delay &delay1,
           const Delay &delay2,
           const StaState *sta)
{
  return sta->delayOps()->equal(delay1, delay2, sta);
}

bool
delayLess(const Delay &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  return sta->delayOps()->less(delay1, delay2, sta);
}

bool
delayLess(const DelayDbl &delay1,
          const DelayDbl &delay2,
          const StaState *sta)
{
  return sta->delayOps()->less(delay1, delay2, sta);
}

bool
delayLess(const Delay &delay1,
          const Delay &delay2,
          const MinMax *min_max,
          const StaState *sta)
{
  if (min_max == MinMax::max())
    return sta->delayOps()->less(delay1, delay2, sta);
  else
    return sta->delayOps()->greater(delay1, delay2, sta);
}

bool
delayLessEqual(const Delay &delay1,
               const Delay &delay2,
               const StaState *sta)
{
  return sta->delayOps()->lessEqual(delay1, delay2, sta);
}

bool
delayLessEqual(const Delay &delay1,
               const Delay &delay2,
               const MinMax *min_max,
               const StaState *sta)
{
  if (min_max == MinMax::max())
    return sta->delayOps()->lessEqual(delay1, delay2, sta);
  else
    return sta->delayOps()->greaterEqual(delay1, delay2, sta);
}

bool
delayGreater(const Delay &delay1,
             const Delay &delay2,
             const StaState *sta)
{
  return sta->delayOps()->greater(delay1, delay2, sta);
}

bool
delayGreater(const Delay &delay1,
             const Delay &delay2,
             const MinMax *min_max,
             const StaState *sta)
{
  if (min_max == MinMax::max())
    return sta->delayOps()->greater(delay1, delay2, sta);
  else
    return sta->delayOps()->less(delay1, delay2, sta);
}

bool
delayGreaterEqual(const Delay &delay1,
                  const Delay &delay2,
                  const StaState *sta)
{
  return sta->delayOps()->greaterEqual(delay1, delay2, sta);
}

bool
delayGreaterEqual(const Delay &delay1,
                  const Delay &delay2,
                  const MinMax *min_max,
                  const StaState *sta)
{
  if (min_max == MinMax::max())
    return sta->delayOps()->greaterEqual(delay1, delay2, sta);
  else
    return sta->delayOps()->lessEqual(delay1, delay2, sta);
}

Delay
delayRemove(const Delay &delay1,
            const Delay &delay2)
{
  return makeDelay2(delay1.mean() - delay2.mean(),
                    delay1.stdDev2() - delay2.stdDev2());
}

Delay
delaySum(const Delay &delay1,
         const Delay &delay2,
         const StaState *sta)
{
  return sta->delayOps()->sum(delay1, delay2);
}

Delay
delaySum(const Delay &delay1,
         float delay2,
         const StaState *sta)
{
  return sta->delayOps()->sum(delay1, delay2);
}

Delay
delayDiff(const Delay &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  return sta->delayOps()->diff(delay1, delay2);
}

Delay
delayDiff(const Delay &delay1,
          float delay2,
          const StaState *sta)
{
  return sta->delayOps()->diff(delay1, delay2);
}

Delay
delayDiff(float delay1,
          const Delay &delay2,
          const StaState *sta)
{
  return sta->delayOps()->diff(delay1, delay2);
}

void
delayIncr(Delay &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  sta->delayOps()->incr(delay1, delay2);
}

void
delayIncr(DelayDbl &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  sta->delayOps()->incr(delay1, delay2);
}

void
delayIncr(Delay &delay1,
          float delay2,
          const StaState *sta)
{
  sta->delayOps()->incr(delay1, delay2);
}

void
delayDecr(Delay &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  sta->delayOps()->decr(delay1, delay2);
}

void
delayDecr(DelayDbl &delay1,
          const Delay &delay2,
          const StaState *sta)
{
  sta->delayOps()->decr(delay1, delay2);
}

Delay
delayProduct(const Delay &delay1,
             float delay2,
             const StaState *sta)
{
  return sta->delayOps()->product(delay1, delay2);
}

Delay
delayDiv(float delay1,
         const Delay &delay2,
         const StaState *sta)
{
  return sta->delayOps()->div(delay1, delay2);
}

float
delayStdDev2(const Delay &delay,
             const EarlyLate *early_late,
             const StaState *sta)
{
  return sta->delayOps()->stdDev2(delay, early_late);
}

} // namespace sta
