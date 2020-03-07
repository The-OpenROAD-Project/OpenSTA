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

#include <cstddef>

namespace sta {

using std::size_t;

const size_t hash_init_value = 5381;

// Dan Bernstein, comp.lang.c.
inline size_t
hashSum(size_t hash,
	size_t add)
{
  // hash * 31 ^ add.
  return ((hash << 5) + hash) ^ add;
}

inline void
hashIncr(size_t &hash,
	 size_t add)
{
  // hash * 31 ^ add.
  hash = ((hash << 5) + hash) ^ add;
}

inline size_t
nextMersenne(size_t n)
{
  return (n + 1) * 2 - 1;
}

// Sadly necessary until c++ std::hash works for char *.
size_t
hashString(const char *str);

} // namespace
