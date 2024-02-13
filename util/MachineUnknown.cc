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

#include "Machine.hh"

namespace sta {

int
processorCount()
{
  return 1;
}

void
initElapsedTime()
{
}

double
elapsedRunTime()
{
  return 0.0;
}

double
userRunTime()
{
  return 0.0;
}

double
systemRunTime()
{
  return 0.0;
}

size_t
memoryUsage()
{
  return 0;
}

} // namespace
