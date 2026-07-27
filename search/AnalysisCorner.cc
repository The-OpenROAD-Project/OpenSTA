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

// OpenROAD fork: analysis_corner support.

#include "AnalysisCorner.hh"

#include "ContainerHelpers.hh"
#include "Mode.hh"
#include "PatternMatch.hh"
#include "Property.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Sta.hh"

namespace sta {

AnalysisCorner::AnalysisCorner(std::string_view name,
                               size_t index) :
  name_(name),
  index_(index)
{
}

////////////////////////////////////////////////////////////////

// Register the "analysis_corner" property on scene objects so
// get_property and get_scenes -filter resolve it natively.
void
defineAnalysisCornerProperties(Sta *sta)
{
  sta->properties().defineProperty("analysis_corner",
    [] (const Scene *scene,
        Sta *) -> PropertyValue {
      AnalysisCorner *corner = scene->analysisCorner();
      if (corner)
        return PropertyValue(corner->name());
      return PropertyValue();
    });
}

AnalysisCorner *
Sta::makeAnalysisCorner(const std::string &name)
{
  AnalysisCorner *corner = findAnalysisCorner(name);
  if (corner == nullptr) {
    corner = new AnalysisCorner(name, analysis_corners_.size());
    analysis_corner_name_map_[name] = corner;
    analysis_corners_.push_back(corner);
  }
  return corner;
}

AnalysisCorner *
Sta::findAnalysisCorner(const std::string &name) const
{
  return findKey(analysis_corner_name_map_, name);
}

AnalysisCornerSeq
Sta::findAnalysisCorners(const std::string &pattern) const
{
  AnalysisCornerSeq matches;
  PatternMatch pattern_match(pattern);
  for (AnalysisCorner *corner : analysis_corners_) {
    if (pattern_match.match(corner->name()))
      matches.push_back(corner);
  }
  return matches;
}

void
Sta::setSceneAnalysisCorner(Scene *scene,
                            AnalysisCorner *corner)
{
  scene->setAnalysisCorner(corner);
}

void
Sta::deleteAnalysisCorners()
{
  deleteContents(analysis_corners_);
  analysis_corners_.clear();
  analysis_corner_name_map_.clear();
  cmd_analysis_corner_ = nullptr;
}

////////////////////////////////////////////////////////////////

// Mode overlay Sdc definitions (declared in Mode.hh).

Sdc *
Mode::cornerSdc(const AnalysisCorner *corner) const
{
  return findKey(corner_sdcs_, corner);
}

Sdc *
Mode::makeCornerSdc(const AnalysisCorner *corner)
{
  Sdc *&sdc = corner_sdcs_[corner];
  if (sdc == nullptr)
    sdc = new Sdc(this, sta_);
  return sdc;
}

void
Mode::clearCornerSdcs()
{
  for (const auto [corner, sdc] : corner_sdcs_)
    sdc->clear();
}

// Corner overlay Sdc for derate queries (declared in Scene.hh).
// The overlay overrides the mode Sdc wholesale iff it defines derates.
const Sdc *
Scene::sdcOverlayForDerate() const
{
  if (analysis_corner_) {
    const Sdc *overlay = mode_->cornerSdc(analysis_corner_);
    if (overlay && overlay->hasDeratingFactors())
      return overlay;
  }
  return nullptr;
}

} // namespace sta
