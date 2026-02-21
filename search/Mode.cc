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

#include "Mode.hh"

#include "Sdc.hh"
#include "Sim.hh"
#include "ClkNetwork.hh"
#include "Genclks.hh"
#include "PathGroup.hh"

namespace sta {

Mode::Mode(const std::string &name,
           size_t mode_index,
           StaState *sta) :
  StaState(sta),
  name_(name),
  mode_index_(mode_index),
  sdc_(new Sdc(this, sta)),
  sim_(new Sim(sta)),
  clk_network_(new ClkNetwork(this, sta)),
  genclks_(new Genclks(this, sta)),
  path_groups_(nullptr)
{
}

Mode::~Mode()
{
  delete sdc_;
  delete sim_;
  delete clk_network_;
  delete genclks_;
  delete path_groups_;
}

void
Mode::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  sdc_->copyState(sta);
  sim_->copyState(sta);
  clk_network_->copyState(sta);
  genclks_->copyState(sta);
}

void
Mode::clear()
{
  scenes_.clear();
  sim_->clear();
  clk_network_->clear();
  genclks_->clear();
}

void
Mode::addScene(Scene *scene)
{
  scenes_.push_back(scene);
}

void
Mode::removeScene(Scene *scene)
{
  // std iterators just plain suck
  scenes_.erase(std::remove(scenes_.begin(), scenes_.end(), scene), scenes_.end());
}

const SceneSet
Mode::sceneSet() const
{
  SceneSet scenes;
  for (Scene *scene : scenes_)
    scenes.insert(scene);
  return scenes;
}

////////////////////////////////////////////////////////////////

PathGroups *
Mode::makePathGroups(int group_path_count,
                     int endpoint_path_count,
                     bool unique_pins,
                     bool unique_edges,
                     float slack_min,
                     float slack_max,
                     StdStringSeq &group_names,
                     bool setup,
                     bool hold,
                     bool recovery,
                     bool removal,
                     bool clk_gating_setup,
                     bool clk_gating_hold,
                     bool unconstrained_paths)
{
  path_groups_ = new PathGroups(group_path_count,
                                endpoint_path_count,
                                unique_pins, unique_edges,
                                slack_min, slack_max,
                                group_names,
                                setup, hold,
                                recovery, removal,
                                clk_gating_setup, clk_gating_hold,
                                unconstrained_paths,
                                this);
  return path_groups_;
}

void
Mode::deletePathGroups()
{
  delete path_groups_;
  path_groups_ = nullptr;
}

PathGroupSeq
Mode::pathGroups(const PathEnd *path_end) const
{
  if (path_groups_)
    return path_groups_->pathGroups(path_end);
  else
    return PathGroupSeq();
}

} // namespace
