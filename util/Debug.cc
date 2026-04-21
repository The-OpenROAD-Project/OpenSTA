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

#include "Debug.hh"

#include "ContainerHelpers.hh"
#include "Report.hh"

namespace sta {

bool debug_on = false;

Debug::Debug(Report *report) :
  report_(report)
{
}

bool
Debug::check(std::string_view what,
             int level) const
{
  if (debug_on_) {
    auto itr = debug_map_.find(what);
    if (itr != debug_map_.end()) {
      int dbg_level = itr->second;
      return dbg_level >= level;
    }
  }
  return false;
}

int
Debug::level(std::string_view what)
{
  if (debug_on_) {
    auto itr = debug_map_.find(what);
    if (itr != debug_map_.end()) {
      int dbg_level = itr->second;
      return dbg_level;
    }
  }
  return 0;
}

void
Debug::setLevel(std::string_view what,
                int level)
{
  if (what == "stats")
    stats_level_ = level;
  else if (level == 0) {
    debug_map_.erase(std::string(what));
    debug_on_ = !debug_map_.empty();
  }
  else {
    debug_map_[std::string(what)] = level;
    debug_on_ = true;
  }
}

} // namespace sta
