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
#include <set>

#include "StringSeq.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"

namespace sta {

class Mode;
class Sdc;
class MinMax;
class Scene;
class LibertyLibrary;
class Parasitics;

using SceneSet = std::set<const Scene*>;
using SceneSeq = std::vector<Scene*>;
using LibertySeq = std::vector<LibertyLibrary*>;
using ModeSeq = std::vector<Mode*>;
using ModeSet = std::set<Mode*>;

class Scene
{
public:
  Scene(const std::string &name,
        size_t index,
        Mode *mode,
        Parasitics *parasitics);
  Scene(const std::string &name,
        size_t index,
        Mode *mode,
        Parasitics *parasitics_min,
        Parasitics *parasitics_max);
  const std::string &name() const { return name_; }
  size_t index() const { return index_; }
  Mode *mode() { return mode_; }
  Mode *mode() const { return mode_; }
  void setMode(Mode *mode);
  Sdc *sdc();
  Sdc *sdc() const;
  Parasitics *parasitics(const MinMax *min_max) const;
  void setParasitics(Parasitics *parasitics,
                     const MinMaxAll *min_max);
  size_t pathIndex(const MinMax *min_max) const;

  DcalcAPIndex dcalcAnalysisPtIndex(const MinMax *min_max) const;
  const MinMax *checkClkSlewMinMax(const MinMax *min_max) const;
  // Slew index of timing check clock.
  DcalcAPIndex checkClkSlewIndex(const MinMax *min_max) const;

  const LibertySeq &libertyLibraries(const MinMax *min_max) const;
  int libertyIndex(const MinMax *min_max) const;
  void addLiberty(LibertyLibrary *lib,
                  const MinMax *min_max);

  static SceneSet sceneSet(const SceneSeq &scenes);
  static ModeSeq modes(const SceneSeq &scenes);
  static ModeSet modeSet(const SceneSeq &scenes);
  static ModeSeq modesSorted(const SceneSeq &scenes);

protected:
  std::string name_;
  size_t index_;
  Mode *mode_;
  LibertySeq liberty_[MinMax::index_count];
  std::array<Parasitics*, MinMax::index_count> parasitics_;

  friend class Scenes;
};

} // namespace
