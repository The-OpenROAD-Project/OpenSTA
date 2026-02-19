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

#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"
#include "BoundedHeap.hh"

namespace sta {

class MinPeriodSlackLess
{
public:
  MinPeriodSlackLess(const StaState *sta);
  bool operator()(const MinPeriodCheck &check1,
                  const MinPeriodCheck &check2) const;

private:
  const StaState *sta_;
};

using MinPeriodHeap = BoundedHeap<MinPeriodCheck, MinPeriodSlackLess>;

class MinPeriodCheck
{
public:
  MinPeriodCheck();
  MinPeriodCheck(Pin *pin,
                 Clock *clk,
                 const Scene *scene);
  bool isNull() { return pin_ == nullptr; }
  Pin *pin() const { return pin_; }
  Clock *clk() const { return clk_; }
  float period() const;
  float minPeriod(const StaState *sta) const;
  Slack slack(const StaState *sta) const;

private:
  Pin *pin_;
  Clock *clk_;
  const Scene *scene_;
};

using MinPeriodCheckSeq = std::vector<MinPeriodCheck>;

class CheckMinPeriods
{
public:
  CheckMinPeriods(StaState *sta);
  void clear();
  MinPeriodCheckSeq &check(const Net *net,
                           size_t max_count,
                           bool violators,
                           const SceneSeq &scenes);

protected:
  void checkNet(const Net *net,
                bool violators,
                const SceneSeq &scenes);
  void checkAll(bool violators,
              const SceneSeq &scenes);
  void checkVertex(Vertex *vertex,
                   bool violators,
                   const SceneSeq &scenes);
  MinPeriodCheck check(Vertex *vertex,
                       const SceneSeq &scenes);

  MinPeriodCheckSeq checks_;
  MinPeriodHeap heap_;
  StaState *sta_;
};

} // namespace
