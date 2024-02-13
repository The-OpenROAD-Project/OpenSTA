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

#pragma once

#include "SdcCmdComment.hh"
#include "SdcClass.hh"

namespace sta {

class ClockGroups : public SdcCmdComment
{
public:
  ClockGroups(const char *name,
	      bool logically_exclusive,
	      bool physically_exclusive,
	      bool asynchronous,
	      bool allow_paths,
	      const char *comment);
  ~ClockGroups();
  void makeClockGroup(ClockSet *clks);
  const char *name() const { return name_; }
  ClockGroupSet *groups() { return &groups_; }
  bool logicallyExclusive() const { return logically_exclusive_; }
  bool physicallyExclusive() const { return physically_exclusive_; }
  bool asynchronous() const { return asynchronous_; }
  bool allowPaths() const { return allow_paths_; }
  void removeClock(Clock *clk);

private:
  const char *name_;
  bool logically_exclusive_;
  bool physically_exclusive_;
  bool asynchronous_;
  bool allow_paths_;
  ClockGroupSet groups_;
};

} // namespace
