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

#pragma once

#include <stddef.h>  // size_t
#include "DisallowCopyAssign.hh"

namespace sta {

class Debug;

// Show run time and memory statistics if the "stats" debug flag is on.
class Stats
{
public:
  explicit Stats(Debug *debug);
  void report(const char *step);

private:
  DISALLOW_COPY_AND_ASSIGN(Stats);

  double elapsed_begin_;
  double user_begin_;
  double system_begin_;
  size_t memory_begin_;
  Debug *debug_;
};

} // namespace
