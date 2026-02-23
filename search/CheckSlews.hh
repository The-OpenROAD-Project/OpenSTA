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

#include "MinMax.hh"
#include "Transition.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "BoundedHeap.hh"
#include "Network.hh"

namespace sta {

class Scene;
class Mode;
class SlewCheckSlackLess;

class SlewCheck
{
public:
  SlewCheck();
  SlewCheck(const Pin *pin,
            const RiseFall *rf,
            Slew &slew,
            float limit,
            float slack,
            const Scene *scene);
  bool isNull() { return pin_ == nullptr; }
  const Pin *pin() const { return pin_; }
  Slew slew() const { return slew_; }
  const RiseFall *edge() const { return rf_; }
  float limit() const { return limit_; }
  float slack() const { return slack_; }
  const Scene *scene() const { return scene_; }

private:
  const Pin *pin_;
  const RiseFall *rf_;
  Slew slew_;
  float limit_;
  float slack_;
  const Scene *scene_;
};

class SlewCheckSlackLess
{
public:
  SlewCheckSlackLess(const StaState *sta);
  bool operator()(const SlewCheck &check1,
                  const SlewCheck &check2) const;

private:
  const StaState *sta_;

};

using SlewCheckHeap = BoundedHeap<SlewCheck, SlewCheckSlackLess>;
using SlewCheckSeq = std::vector<SlewCheck>;

class CheckSlews
{
public:
  CheckSlews(const StaState *sta);
  void clear();
  // net=null check all nets
  SlewCheckSeq &check(const Net *net,
                      size_t max_count,
                      bool violators,
                      const SceneSeq &scenes,
                      const MinMax *min_max);
  void check(const Pin *pin,
             const SceneSeq &scenes,
             const MinMax *min_max,
             bool check_clks,
             // Return values.
             // Scene is nullptr for no slew limit.
             Slew &slew,
             float &limit,
             float &slack,
             const RiseFall *&rf,
             const Scene *&scene) const;
  void findLimit(const LibertyPort *port,
                 const Scene *scene,
                 const MinMax *min_max,
                 // Return values.
                 float &limit,
                 bool &exists) const;

protected:
  void checkNet(const Net *net,
                bool violators,
                const SceneSeq &scenes,
                const MinMax *min_max);
  void checkAll(bool violators,
                const SceneSeq &scenes,
                const MinMax *min_max);
  void checkInst(const Instance *inst,
                 bool violators,
                 const SceneSeq &scenes,
                 const MinMax *min_max);
  void checkPin(const Pin *pin,
                bool violators,
                const SceneSeq &scenes,
                const MinMax *min_max);
  void check2(const Vertex *vertex,
              const Scene *scene,
              const MinMax *min_max,
              bool check_clks,
              // Return values.
              const Scene *&scene1,
              const RiseFall *&rf,
              Slew &slew1,
              float &limit1,
              float &slack1) const;
  void check3(const Vertex *vertex,
              const Scene *scene,
              const RiseFall *rf,
              const MinMax *min_max,
              float limit,
              // Return values.
              const Scene *&scene1,
              const RiseFall *&rf1,
              Slew &slew1,
              float &slack1,
              float &limit1) const;
  void findLimit(const Pin *pin,
                 const Scene *scene,
                 const RiseFall *rf,
                 const MinMax *min_max,
                 const ConstClockSet &clks,
                 // Return values.
                 float &limit,
                 bool &limit_exists) const;
  ConstClockSet clockDomains(const Vertex *vertex,
                             const Scene *scene) const;

  SlewCheckSeq checks_;
  SlewCheckHeap heap_;
  const StaState *sta_;
};

} // namespace

