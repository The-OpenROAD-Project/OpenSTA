// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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

#ifndef STA_DELAY_FLOAT_H
#define STA_DELAY_FLOAT_H

#include "MinMax.hh"

// Delay values defined as floats.

namespace sta {

typedef float Delay;

const Delay delay_zero = 0.0;

inline Delay
makeDelay(float delay,
	  float,
	  float)
{
  return delay;
}

inline Delay
makeDelay2(float delay,
	   float,
	   float)
{
  return delay;
}

} // namespace
#endif
