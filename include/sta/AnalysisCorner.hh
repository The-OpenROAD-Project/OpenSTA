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

#pragma once

#include <map>
#include <string>
#include <vector>

#include "StringUtil.hh"

namespace sta {

class AnalysisCorner;
class Sta;

using AnalysisCornerSeq = std::vector<AnalysisCorner*>;
using AnalysisCornerNameMap = std::map<std::string, AnalysisCorner*, std::less<>>;

// Register the "analysis_corner" property on scene objects.
// Called once from Sta::makeComponents.
void
defineAnalysisCornerProperties(Sta *sta);

// PVT/RC analysis corner. Groups the scenes that share an operating
// point across modes and bundles the liberty/spef authoring data
// define_scene composes from. Liberty maps, parasitics bindings and
// SDC remain on Scene/Mode.
class AnalysisCorner
{
public:
  AnalysisCorner(std::string_view name,
                 size_t index);
  const std::string &name() const { return name_; }
  size_t index() const { return index_; }
  const StringSeq &libertyMinFiles() const { return liberty_min_files_; }
  const StringSeq &libertyMaxFiles() const { return liberty_max_files_; }
  const std::string &spefMinName() const { return spef_min_name_; }
  const std::string &spefMaxName() const { return spef_max_name_; }
  void setLiberty(const StringSeq &min_files,
                  const StringSeq &max_files)
  {
    liberty_min_files_ = min_files;
    liberty_max_files_ = max_files;
  }
  void setSpef(const std::string &min_name,
               const std::string &max_name)
  {
    spef_min_name_ = min_name;
    spef_max_name_ = max_name;
  }

private:
  std::string name_;
  size_t index_;
  // Liberty names/filenames and read_spef -name keys, composed into
  // define_scene when the scene names this corner.
  StringSeq liberty_min_files_;
  StringSeq liberty_max_files_;
  std::string spef_min_name_;
  std::string spef_max_name_;
};

} // namespace sta
