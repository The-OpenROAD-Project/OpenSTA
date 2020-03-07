// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Machine.hh"
#include "StringUtil.hh"
#include "ClockGroups.hh"

namespace sta {

ClockGroup::ClockGroup(ClockSet *clks) :
  clks_(clks)
{
}

ClockGroup::~ClockGroup()
{
  delete clks_;
}

bool
ClockGroup::isMember(const Clock *clk)
{
  return clks_->hasKey(const_cast<Clock*>(clk));
}

////////////////////////////////////////////////////////////////

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

ClockGroup *
ClockGroups::makeClockGroup(ClockSet *clks)
{
  ClockGroup *group = new ClockGroup(clks);
  groups_.insert(group);
  return group;
}

void
ClockGroups::removeClock(Clock *clk)
{
  ClockGroupSet::Iterator group_iter(groups_);
  while (group_iter.hasNext()) {
    ClockGroup *group = group_iter.next();
    ClockSet *clks = group->clks();
    clks->erase(clk);
    if (clks->empty()) {
      groups_.erase(group);
      delete group;
    }
  }
}

} // namespace
