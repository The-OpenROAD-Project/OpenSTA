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

#pragma once

#include "Delay.hh"

namespace sta {

class DelayOpsSkewNormal : public DelayOps
{
public:
  float stdDev2(const Delay &delay,
                const EarlyLate *early_late) const override;
  float asFloat(const Delay &delay,
                const EarlyLate *early_late,
                const StaState *sta) const override;
  double asFloat(const DelayDbl &delay,
                 const EarlyLate *early_late,
                 const StaState *sta) const override;

  bool isZero(const Delay &delay) const override;
  bool isInf(const Delay &delay) const override;
  bool equal(const Delay &delay1,
             const Delay &delay2,
             const StaState *sta) const override;
  bool less(const Delay &delay1,
            const Delay &delay2,
            const StaState *sta) const override;
  bool less(const DelayDbl &delay1,
            const DelayDbl &delay2,
            const StaState *sta) const override;
  bool lessEqual(const Delay &delay1,
                 const Delay &delay2,
                 const StaState *) const override;
  bool greater(const Delay &delay1,
               const Delay &delay2,
               const StaState *) const override;
  bool greaterEqual(const Delay &delay1,
                    const Delay &delay2,
                    const StaState *) const override;
  Delay sum(const Delay &delay1,
            const Delay &delay2) const override;
  Delay sum(const Delay &delay1,
            float delay2) const override;
  Delay diff(const Delay &delay1,
             const Delay &delay2) const override;
  Delay diff(const Delay &delay1,
             float delay2) const override;
  Delay diff(float delay1,
             const Delay &delay2) const override;
  void incr(Delay &delay1,
            const Delay &delay2) const override;
  void incr(DelayDbl &delay1,
            const Delay &delay2) const override;
  void decr(Delay &delay1,
            const Delay &delay2) const override;
  void decr(DelayDbl &delay1,
            const Delay &delay2) const override;
  Delay product(const Delay &delay1,
                float delay2) const override;
  Delay div(float delay1,
            const Delay &delay2) const override;
  std::string asStringVariance(const Delay &delay,
                               int digits,
                               const StaState *sta) const override;

private:
  float skewnessSum(const Delay &delay1,
                    const Delay &delay2) const;
  double skewnessSum(double std_dev1,
                     double skewness1,
                     double std_dev2,
                     double skewness2) const;
};

} // namespace
