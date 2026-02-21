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

#include "Debug.hh"

#include "ContainerHelpers.hh"
#include "Report.hh"

namespace sta {

bool debug_on = false;

Debug::Debug(Report *report) :
  report_(report),
  debug_on_(false),
  stats_level_(0)
{
}

bool
Debug::check(const char *what,
             int level) const
{
  if (debug_on_) {
    int dbg_level;
    bool exists;
    findKeyValue(debug_map_, what, dbg_level, exists);
    if (exists)
      return dbg_level >= level;
  }
  return false;
}

int
Debug::level(const char *what)
{
  if (debug_on_) {
    int dbg_level;
    bool exists;
    findKeyValue(debug_map_, what, dbg_level, exists);
    if (exists)
      return dbg_level;
  }
  return 0;
}

void
Debug::setLevel(const char *what,
                int level)
{
  if (stringEq(what, "stats"))
    stats_level_ = level;
  else if (level == 0) {
    debug_map_.erase(what);
    debug_on_ = !debug_map_.empty();
  }
  else {
    debug_map_[what] = level;
    debug_on_ = true;
  }
}

void
Debug::reportLine(const char *what,
                  const char *fmt,
                  ...)
{
  va_list args;
  va_start(args, fmt);
  std::unique_lock<std::mutex> lock(buffer_lock_);
  report_->printToBuffer("%s", what);
  report_->printToBufferAppend(": ");
  report_->printToBufferAppend(fmt, args);
  report_->printBufferLine();
  va_end(args);
}

} // namespace
