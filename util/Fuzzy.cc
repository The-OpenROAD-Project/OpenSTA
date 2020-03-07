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

#include <algorithm> // max
#include <cmath> // abs
#include "Machine.hh"
#include "MinMax.hh" // INF

namespace sta {

using std::max;
using std::abs;

bool
fuzzyEqual(float v1,
	   float v2)
{
  return v1 == v2
    || abs(v1 - v2) < 1E-6F * max(abs(v1), abs(v2));
}

bool
fuzzyZero(float v)
{
  return v == 0.0
    || abs(v) < 1E-15F;
}

bool
fuzzyLess(float v1,
	  float v2)
{
  return v1 < v2 && !fuzzyEqual(v1, v2);
}

bool
fuzzyLessEqual(float v1,
	       float v2)
{
  return v1 < v2 || fuzzyEqual(v1, v2);
}

bool
fuzzyGreater(float v1,
	     float v2)
{
  return v1 > v2 && !fuzzyEqual(v1, v2);
}

bool
fuzzyGreaterEqual(float v1,
		  float v2)
{
  return v1 > v2 || fuzzyEqual(v1, v2);
}

bool
fuzzyInf(float value)
{
  return fuzzyEqual(value, INF)
    || fuzzyEqual(value, -INF);
}

} // namespace
