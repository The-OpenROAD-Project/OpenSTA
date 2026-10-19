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

#include "PathStages.hh"

#include "Liberty.hh"
#include "Network.hh"

namespace sta {

PathStages::PathStages(PathExpanded &path_expanded,
                       const StaState *sta) :
  StaState(sta),
  path_expanded_(path_expanded)
{
}

Stage
PathStages::stageFirst() const
{
  return 1;
}

Stage
PathStages::stageLast() const
{
  return (path_expanded_.size() + 1) / 2;
}

std::string
PathStages::stageName(Stage stage) const
{
  return sta::format("stage{}", stage);
}

int
PathStages::stageGateInputPathIndex(Stage stage) const
{
  return stage * 2 - 3;
}

int
PathStages::stageDrvrPathIndex(Stage stage) const
{
  return stage * 2 - 2;
}

int
PathStages::stageLoadPathIndex(Stage stage) const
{
  return stage * 2 - 1;
}

const Path *
PathStages::stageGateInputPath(Stage stage) const
{
  int path_index = stageGateInputPathIndex(stage);
  return path_expanded_.path(path_index);
}

const Path *
PathStages::stageDrvrPath(Stage stage) const
{
  int path_index = stageDrvrPathIndex(stage);
  return path_expanded_.path(path_index);
}

const Path *
PathStages::stageLoadPath(Stage stage) const
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index);
}

const TimingArc *
PathStages::stageGateArc(Stage stage) const
{
  int path_index = stageDrvrPathIndex(stage);
  if (path_index >= 0)
    return path_expanded_.path(path_index)->prevArc(this);
  else
    return nullptr;
}

const TimingArc *
PathStages::stageWireArc(Stage stage) const
{
  int path_index = stageLoadPathIndex(stage);
  return path_expanded_.path(path_index)->prevArc(this);
}

Edge *
PathStages::stageGateEdge(Stage stage) const
{
  const Path *path = stageDrvrPath(stage);
  return path->prevEdge(this);
}

const Edge *
PathStages::stageWireEdge(Stage stage) const
{
  const Path *path = stageLoadPath(stage);
  return path->prevEdge(this);
}

const Pin *
PathStages::stageGateInputPin(Stage stage) const
{
  const Path *path = stageGateInputPath(stage);
  if (path)
    return path->pin(this);
  else
    return nullptr;
}

const LibertyPort *
PathStages::stageGateInputPort(Stage stage) const
{
  const Pin *pin = stageGateInputPin(stage);
  return network_->libertyPort(pin);
}

const Pin *
PathStages::stageDrvrPin(Stage stage) const
{
  const Path *path = stageDrvrPath(stage);
  return path->pin(this);
}

const LibertyPort *
PathStages::stageDrvrPort(Stage stage) const
{
  const Pin *pin = stageDrvrPin(stage);
  return network_->libertyPort(pin);
}

const Pin *
PathStages::stageLoadPin(Stage stage) const
{
  const Path *path = stageLoadPath(stage);
  return path->pin(this);
}

std::string
PathStages::stageGateInputPinName(Stage stage) const
{
  const Pin *pin = stageGateInputPin(stage);
  return network_->pathName(pin);
}

std::string
PathStages::stageDrvrPinName(Stage stage) const
{
  const Pin *pin = stageDrvrPin(stage);
  return network_->pathName(pin);
}

std::string
PathStages::stageLoadPinName(Stage stage) const
{
  const Pin *pin = stageLoadPin(stage);
  return network_->pathName(pin);
}

const Instance *
PathStages::stageInstance(Stage stage) const
{
  const Pin *pin = stageDrvrPin(stage);
  return network_->instance(pin);
}

const LibertyCell *
PathStages::stageLibertyCell(Stage stage) const
{
  const Pin *pin = stageDrvrPin(stage);
  return network_->libertyPort(pin)->libertyCell();
}

} // namespace sta
