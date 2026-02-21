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

#include "Scene.hh"

#include "ContainerHelpers.hh"
#include "Parasitics.hh"
#include "Sdc.hh"
#include "Mode.hh"

namespace sta {

Scene::Scene(const std::string &name,
             size_t index,
             Mode *mode,
             Parasitics *parasitics_min,
             Parasitics *parasitics_max) :
  name_(name),
  index_(index),
  mode_(mode),
  parasitics_{parasitics_min, parasitics_max}
{
}

Scene::Scene(const std::string &name,
             size_t index,
             Mode *mode,
             Parasitics *parasitics) :
  name_(name),
  index_(index),
  mode_(mode)
{
  for (size_t mm_index : MinMax::rangeIndex())
    parasitics_[mm_index] = parasitics;
}
 
size_t
Scene::pathIndex(const MinMax *min_max) const
{
  return index_ * MinMax::index_count + min_max->index();
}

void
Scene::setMode(Mode *mode)
{
  mode_ = mode;
}

Sdc *
Scene::sdc()
{
  return mode_->sdc();
}

Sdc *
Scene::sdc() const
{
  return mode_->sdc();
}

Parasitics *
Scene::parasitics(const MinMax *min_max) const
{
  return parasitics_[min_max->index()];
}

void
Scene::setParasitics(Parasitics *parasitics,
                     const MinMaxAll *min_max)
{
  for (size_t mm : min_max->rangeIndex())
    parasitics_[mm] = parasitics;
}

DcalcAPIndex
Scene::dcalcAnalysisPtIndex(const MinMax *min_max) const
{
  return index_ * MinMax::index_count + min_max->index();
}

const MinMax *
Scene::checkClkSlewMinMax(const MinMax *min_max) const
{
  switch (mode_->sdc()->analysisType()) {
  case AnalysisType::single:
    return MinMax::min();
  case AnalysisType::bc_wc:
    return min_max;
  case AnalysisType::ocv:
    return min_max->opposite();
  default:
    // suppress gcc warning
    return min_max;
  }
}

DcalcAPIndex
Scene::checkClkSlewIndex(const MinMax *min_max) const
{
  return dcalcAnalysisPtIndex(checkClkSlewMinMax(min_max));
}

void
Scene::addLiberty(LibertyLibrary *lib,
                  const MinMax *min_max)
{
  liberty_[min_max->index()].push_back(lib);
}

const LibertySeq &
Scene::libertyLibraries(const MinMax *min_max) const
{
  return liberty_[min_max->index()];
}

int
Scene::libertyIndex(const MinMax *min_max) const
{
  return index_ * MinMax::index_count + min_max->index();
}

////////////////////////////////////////////////////////////////

SceneSet
Scene::sceneSet(const SceneSeq &scenes)
{
  SceneSet scenes_set;
  for (Scene *scene : scenes)
    scenes_set.insert(scene);
  return scenes_set;
}

ModeSeq
Scene::modes(const SceneSeq &scenes)
{
  ModeSet mode_set;
  for (const Scene *scene : scenes)
    mode_set.insert(scene->mode());

  ModeSeq modes;
  for (Mode *mode : mode_set)
    modes.push_back(mode);
  return modes;
}

ModeSet
Scene::modeSet(const SceneSeq &scenes)
{
  ModeSet modes;
  for (const Scene *scene : scenes)
    modes.insert(scene->mode());
  return modes;
}

ModeSeq
Scene::modesSorted(const SceneSeq &scenes)
{
  ModeSeq modes = Scene::modes(scenes);
  sort(modes, [] (const Mode *mode1,
                  const Mode *mode2) {
    return mode1->name() < mode2->name();
  });
  return modes;
}  

} // namespace
