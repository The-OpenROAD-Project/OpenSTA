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

#include <string>
#include <vector>
#include <functional>

#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"
#include "Path.hh"
#include "BoundedHeap.hh"

namespace sta {

class RiseFall;

class MinPulseWidthSlackLess
{
public:
  MinPulseWidthSlackLess(const StaState *sta);
  bool operator()(const MinPulseWidthCheck &check1,
                  const MinPulseWidthCheck &check2) const;

private:
  const StaState *sta_;
};

class MinPulseWidthCheck
{
public:
  MinPulseWidthCheck();
  MinPulseWidthCheck(Path *open_path);
  std::string to_string(const StaState *sta);
  bool isNull() const { return open_path_ == nullptr; }
  Pin *pin(const StaState *sta) const;
  const RiseFall *openTransition(const StaState *sta) const;
  Arrival width(const StaState *sta) const;
  float minWidth(const StaState *sta) const;
  Slack slack(const StaState *sta) const;
  Path *openPath() { return open_path_; }
  Scene *scene(const StaState *sta) const;
  const Path *openPath() const { return open_path_; }
  Arrival openArrival(const StaState *sta) const;
  Path *closePath(const StaState *sta) const;
  Arrival closeArrival(const StaState *sta) const;
  Arrival openDelay(const StaState *sta) const;
  Arrival closeDelay(const StaState *sta) const;
  float closeOffset(const StaState *sta) const;
  const ClockEdge *openClkEdge(const StaState *sta) const;
  const ClockEdge *closeClkEdge(const StaState *sta) const;
  Crpr checkCrpr(const StaState *sta) const;

protected:
  // Open path of the pulse.
  Path *open_path_;
};

using MinPulseWidthCheckSeq = std::vector<MinPulseWidthCheck>;
using MinPulseWidthCheckHeap = BoundedHeap<MinPulseWidthCheck, MinPulseWidthSlackLess>;

class CheckMinPulseWidths
{
public:
  CheckMinPulseWidths(StaState *sta);
  void clear();
  MinPulseWidthCheckSeq &check(const Net *net,
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

  MinPulseWidthCheckSeq checks_;
  MinPulseWidthCheckHeap heap_;
  StaState *sta_;
};

} // namespace
