// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include <cstdarg>

#include "Map.hh"
#include "StringUtil.hh"

namespace sta {

class Report;
class Pin;

typedef Map<const char *, int, CharPtrLess> DebugMap;

class Debug
{
public:
  explicit Debug(Report *report);
  ~Debug();
  int level(const char *what);
  void setLevel(const char *what,
		int level);
  bool check(const char *what,
	     int level) const;
  int statsLevel() const { return stats_level_; }
  void reportLine(const char *what,
                  const char *fmt,
                  ...) const
    __attribute__((format (printf, 3, 4)));

protected:
  Report *report_;
  bool debug_on_;
  DebugMap *debug_map_;
  int stats_level_;
};

// Inlining a varargs function would eval the args, which can
// be expensive, so use a macro.
// Note that "##__VA_ARGS__" is a gcc extension to support zero arguments (no comma).
// clang -Wno-gnu-zero-variadic-macro-arguments suppresses the warning.
// c++20 has "__VA_OPT__" to deal with the zero arg case so this is temporary.
#define debugPrint(debug, what, level, ...) \
  if (debug->check(what, level)) {  \
    debug->reportLine(what, ##__VA_ARGS__); \
  }

} // namespace
