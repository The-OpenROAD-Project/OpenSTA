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
#include "Debug.hh"
#include "Stats.hh"

namespace sta {

Stats::Stats(Debug *debug) :
  debug_(debug)
{
  if (debug->statsLevel() > 0) {
    elapsed_begin_ = elapsedRunTime();
    user_begin_ = userRunTime();
    system_begin_ = systemRunTime();
    memory_begin_ = memoryUsage();
  }
}

void
Stats::report(const char *step)
{
  if (debug_->statsLevel() > 0) {
    double elapsed_end = elapsedRunTime();
    double user_end = userRunTime();
    double memory_begin = static_cast<double>(memory_begin_);
    double memory_end = static_cast<double>(memoryUsage());
    double memory_delta = memory_end - memory_begin;
    debug_->print("stats: %5.1f/%5.1fe %5.1f/%5.1fu %5.1f/%5.1fMB %s\n",
		  elapsed_end - elapsed_begin_, elapsed_end,
		  user_end - user_begin_, user_end,
		  memory_delta * 1e-6, memory_end * 1e-6,
		  step);
  }
}

} // namespace
