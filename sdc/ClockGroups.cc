// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "ClockGroups.hh"

#include "StringUtil.hh"

namespace sta {

ClockGroups::ClockGroups(const char *name,
			 bool logically_exclusive,
			 bool physically_exclusive,
			 bool asynchronous,
			 bool allow_paths,
			 const char *comment) :
  SdcCmdComment(comment),
  name_(stringCopy(name)),
  logically_exclusive_(logically_exclusive),
  physically_exclusive_(physically_exclusive),
  asynchronous_(asynchronous),
  allow_paths_(allow_paths)
{
}

ClockGroups::~ClockGroups()
{
  stringDelete(name_);
  groups_.deleteContentsClear();
}

void
ClockGroups::makeClockGroup(ClockSet *clks)
{
  groups_.insert(clks);
}

void
ClockGroups::removeClock(Clock *clk)
{
  for (auto itr = groups_.cbegin(); itr != groups_.cend(); ) {
    ClockGroup *group = *itr;
    group->erase(clk);
    if (group->empty()) {
      itr = groups_.erase(itr);
      delete group;
    }
    else
      itr++;
  }
}

} // namespace
