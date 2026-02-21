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

#include <vector>

#include "MinMax.hh"
#include "Transition.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "BoundedHeap.hh"

namespace sta {

class StaState;
class Scene;
class RiseFall;
class CapacitanceCheckSlackLess;

class CapacitanceCheck
{
public:
  CapacitanceCheck();
  CapacitanceCheck(const Pin *pin,
                   float capacitance,
                   float limit,
                   float slack,
                   const Scene *scene,
                   const RiseFall *rf);
  bool isNull() { return pin_ == nullptr; }
  const Pin *pin() const { return pin_; }
  float capacitance() const { return capacitance_; }
  float limit() const { return limit_; }
  float slack() const { return slack_; }
  const Scene *scene() const { return scene_; }
  const RiseFall *rf() const { return rf_; }

private:
  const Pin *pin_;
  float capacitance_;
  float limit_;
  float slack_;
  const Scene *scene_;
  const RiseFall *rf_;
};

using CapacitanceCheckSeq = std::vector<CapacitanceCheck>;
using CapacitanceCheckHeap = BoundedHeap<CapacitanceCheck, CapacitanceCheckSlackLess>;

class CheckCapacitances
{
public:
  CheckCapacitances(const StaState *sta);
  void clear();
  // Return pins with the min/max cap limit slack.
  // net=null check all nets
  CapacitanceCheckSeq &check(const Net *net,
                             size_t max_count,
                             bool violators,
                             const SceneSeq &scenes,
                             const MinMax *min_max);
  // Return min slack check across scenes.
  CapacitanceCheck check(const Pin *pin,
                         const SceneSeq &scenes,
                         const MinMax *min_max) const;

protected:
  CapacitanceCheck check(const Pin *pin,
                         bool violators,
                         const SceneSeq &scenes,
                         const MinMax *min_max) const;
  void findLimit(const Pin *pin,
                 const Scene *scene,
                 const MinMax *min_max,
                 // Return values.
                 float &limit,
                 bool &limit_exists) const;
  void checkCapLimits(const Instance *inst,
                      bool violators,
                      const SceneSeq &scenes,
                      const MinMax *min_max);
  void checkCapLimits(const Instance *inst,
                      const SceneSeq &scenes,
                      const MinMax *min_max,
                      CapacitanceCheckHeap &heap);
  void check(const Pin *pin,
             const SceneSeq &scenes,
             const MinMax *min_max,
             CapacitanceCheckHeap &heap);
  CapacitanceCheckSeq &checkViolations(const Net *net,
                                       const SceneSeq &scenes,
                                       const MinMax *min_max);
  CapacitanceCheckSeq &checkMaxCount(const Net *net,
                                     size_t max_count,
                                     const SceneSeq &scenes,
                                     const MinMax *min_max);
  bool checkPin(const Pin *pin,
                const Scene *scene) const;

  const StaState *sta_;
  CapacitanceCheckSeq checks_;
};

} // namespace

