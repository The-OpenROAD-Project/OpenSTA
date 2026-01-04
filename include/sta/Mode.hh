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

#include "StaState.hh"

namespace sta {

class Sdc;
class Sim;
class ClkNetwork;
class Genclks;
class PathGroups;

using PathGroupSeq = std::vector<PathGroup*>;

// Sdc and dependent state.
class Mode : public StaState
{
public:
  Mode(const std::string &name,
       size_t mode_index,
       StaState *sta);
  virtual ~Mode();
  virtual void copyState(const StaState *sta);
  void clear();
  const std::string &name() const { return name_; }
  size_t modeIndex() const { return mode_index_; }
  const SceneSeq &scenes() const { return scenes_; }
  const SceneSet sceneSet() const;
  void addScene(Scene *scene);
  void removeScene(Scene *scene);
  Sdc *sdc() { return sdc_; }
  Sdc *sdc() const { return sdc_; }
  Sim *sim() { return sim_; }
  Sim *sim() const { return sim_; }
  ClkNetwork *clkNetwork() { return clk_network_; }
  ClkNetwork *clkNetwork() const { return clk_network_; }
  Genclks *genclks() { return genclks_; }
  Genclks *genclks() const { return genclks_; }
  PathGroups *pathGroups() { return path_groups_; }
  PathGroups *pathGroups() const { return path_groups_; }
  PathGroupSeq pathGroups(const PathEnd *path_end) const;
  PathGroups *makePathGroups(int group_path_count,
                             int endpoint_path_count,
                             bool unique_pins,
                             bool unique_edges,
                             float min_slack,
                             float max_slack,
                             StdStringSeq &group_names,
                             bool setup,
                             bool hold,
                             bool recovery,
                             bool removal,
                             bool clk_gating_setup,
                             bool clk_gating_hold,
                             bool unconstrained_paths);
  void deletePathGroups();

private:
  std::string name_;
  size_t mode_index_;
  SceneSeq scenes_;
  Sdc *sdc_;
  Sim *sim_;
  ClkNetwork *clk_network_;
  Genclks *genclks_;
  PathGroups *path_groups_;
};

} // namespace
