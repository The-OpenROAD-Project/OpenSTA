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

#pragma once

#include <array>
#include <cstddef>

#include "StaConfig.hh"
#include "MinMax.hh"

namespace sta {

class StaState;

class Delay
{
public:
  Delay();
  Delay(float mean);
  Delay(float mean,
        // std_dev^2
        float std_dev2);
  Delay(float mean,
        float mean_shift,
        // std_dev^2
        float std_dev2,
        float skewness);
  void setValues(float mean,
                 float mean_shift,
                 float std_dev2,
                 float skewnes);
  float mean() const { return values_[0]; }
  void setMean(float mean);
  float meanShift() const { return values_[1]; }
  void setMeanShift(float mean_shift);
  float stdDev() const;
  // std_dev ^ 2
  float stdDev2() const { return values_[2]; }
  void setStdDev(float std_dev);
  float skewness() const { return values_[3]; }
  void setSkewness(float skewness);

  void operator=(float delay);
  // This allows applications that do not support statistical timing
  // to treat Delays as floats without explicitly converting with
  // delayAsFloat.
  operator float() const { return mean(); }

private:
  std::array<float, 4> values_;
};

// Delay with doubles for accumulating Delays.
// Only a subset of operations are required for DelayDbl.
class DelayDbl
{
public:
  DelayDbl();
  DelayDbl(double value);
  double mean() const { return values_[0]; }
  void setMean(double mean);
  double meanShift() const { return values_[1]; }
  // std_dev ^ 2
  double stdDev2() const { return values_[2]; }
  double stdDev() const;
  double skewness() const { return values_[3]; }
  void setValues(double mean,
                 double mean_shift,
                 double std_dev2,
                 double skewnes);

  void operator=(double delay);

private:
  std::array<double, 4> values_;
};

using ArcDelay = Delay;
using Slew = Delay;
using Arrival = Delay;
using Required = Delay;
using Slack = Delay;

const Delay delay_zero(0.0);

class DelayOps
{
public:
  virtual ~DelayOps() {}
  virtual float stdDev2(const Delay &delay,
                        const EarlyLate *early_late) const = 0;
  virtual float asFloat(const Delay &delay,
                        const EarlyLate *early_late,
                        const StaState *sta) const = 0;
  virtual double asFloat(const DelayDbl &delay,
                         const EarlyLate *early_late,
                         const StaState *sta) const = 0;
  virtual bool isZero(const Delay &delay) const = 0;
  virtual bool isInf(const Delay &delay) const = 0;
  virtual bool equal(const Delay &delay1,
                     const Delay &delay2,
                     const StaState *sta) const = 0;
  virtual bool less(const Delay &delay1,
                    const Delay &delay2,
                    const StaState *sta) const = 0;
  virtual bool less(const DelayDbl &delay1,
                    const DelayDbl &delay2,
                    const StaState *sta) const = 0;
  virtual bool lessEqual(const Delay &delay1,
                         const Delay &delay2,
                         const StaState *sta) const = 0;
  virtual bool greater(const Delay &delay1,
                       const Delay &delay2,
                       const StaState *sta) const = 0;
  virtual bool greaterEqual(const Delay &delay1,
                            const Delay &delay2,
                            const StaState *sta) const = 0;
  virtual Delay sum(const Delay &delay1,
                    const Delay &delay2) const = 0;
  virtual Delay sum(const Delay &delay1,
                    float delay2) const = 0;
  virtual Delay diff(const Delay &delay1,
                     const Delay &delay2) const = 0;
  virtual Delay diff(const Delay &delay1,
                     float delay2) const = 0;
  virtual Delay diff(float delay1,
                     const Delay &delay2) const = 0;
  virtual void incr(Delay &delay1,
                    const Delay &delay2) const = 0;
  virtual void incr(DelayDbl &delay1,
                    const Delay &delay2) const = 0;
  virtual void decr(Delay &delay1,
                    const Delay &delay2) const = 0;
  virtual void decr(DelayDbl &delay1,
                    const Delay &delay2) const = 0;
  virtual Delay product(const Delay &delay1,
                        float delay2) const = 0;
  virtual Delay div(float delay1,
                    const Delay &delay2) const = 0;
  virtual std::string asStringVariance(const Delay &delay,
                                       int digits,
                                       const StaState *sta) const = 0;

};

void
initDelayConstants();

inline float
square(float x)
{
  return x * x;
}

inline double
square(double x)
{
  return x * x;
}

inline float
cube(float x)
{
  return x * x * x;
}

inline double
cube(double x)
{
  return x * x * x;
}

Delay
makeDelay(float mean,
          float mean_shift,
          float std_dev,
          float skewness);
Delay
makeDelay(float mean,
          float std_dev);
Delay
makeDelay2(float mean,
           float std_dev);
void
delaySetMean(Delay &delay,
             float mean);

std::string
delayAsString(const Delay &delay,
              const StaState *sta);
std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              const StaState *sta);
std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              int digits,
              const StaState *sta);
std::string
delayAsString(const Delay &delay,
              const EarlyLate *early_late,
              bool report_variance,
              int digits,
              const StaState *sta);

float
delayAsFloat(const Delay &delay);
float
delayAsFloat(const Delay &delay,
             const EarlyLate *early_late,
             const StaState *sta);
float
delayAsFloat(const DelayDbl &delay,
             const EarlyLate *early_late,
             const StaState *sta);

Delay
delayDblAsDelay(DelayDbl &delay);

Delay
delaySum(const Delay &delay1,
         const Delay &delay2,
         const StaState *sta);
Delay
delaySum(const Delay &delay1,
         float delay2,
         const StaState *sta);
Delay
delayDiff(const Delay &delay1,
          const Delay &delay2,
          const StaState *sta);
Delay
delayDiff(const Delay &delay1,
          float delay2,
          const StaState *sta);
Delay
delayDiff(float delay1,
          const Delay &delay2,
          const StaState *sta);
void
delayIncr(Delay &delay1,
          const Delay &delay2,
          const StaState *sta);
void
delayIncr(DelayDbl &delay1,
          const Delay &delay2,
          const StaState *sta);
void
delayIncr(Delay &delay1,
          float delay2,
          const StaState *sta);
void
delayDecr(Delay &delay1,
          const Delay &delay2,
          const StaState *sta);
void
delayDecr(DelayDbl &delay1,
          const Delay &delay2,
          const StaState *sta);
Delay
delayProduct(const Delay &delay1,
             float delay2,
             const StaState *sta);
Delay
delayDiv(float delay1,
         const Delay &delay2,
         const StaState *sta);

const Delay &
delayInitValue(const MinMax *min_max);
bool
delayIsInitValue(const Delay &delay,
                 const MinMax *min_max);
bool
delayZero(const Delay &delay,
          const StaState *sta);
bool
delayInf(const Delay &delay,
         const StaState *sta);
bool
delayEqual(const Delay &delay1,
           const Delay &delay2,
           const StaState *sta);
bool
delayLess(const Delay &delay1,
          const Delay &delay2,
          const StaState *sta);
bool
delayLess(const DelayDbl &delay1,
          const DelayDbl &delay2,
          const StaState *sta);
bool
delayLess(const Delay &delay1,
          const Delay &delay2,
          const MinMax *min_max,
          const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
               const Delay &delay2,
               const StaState *sta);
bool
delayLessEqual(const Delay &delay1,
               const Delay &delay2,
               const MinMax *min_max,
               const StaState *sta);
bool
delayGreater(const Delay &delay1,
             const Delay &delay2,
             const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
                  const Delay &delay2,
                  const StaState *sta);
bool
delayGreaterEqual(const Delay &delay1,
                  const Delay &delay2,
                  const MinMax *min_max,
                  const StaState *sta);
bool
delayGreater(const Delay &delay1,
             const Delay &delay2,
             const MinMax *min_max,
             const StaState *sta);

// delay1-delay2 subtracting sigma instead of addiing.
Delay
delayRemove(const Delay &delay1,
            const Delay &delay2);

} // namespace
