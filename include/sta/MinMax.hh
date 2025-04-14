// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <array>
#include <vector>
#include <string>

#include "Iterator.hh"

namespace sta {

class MinMax;
class MinMaxAll;

// Use typedefs to make early/late functional equivalents to min/max.
typedef MinMax EarlyLate;
typedef MinMaxAll EarlyLateAll;

// Large value used for min/max initial values.
extern const float INF;

class MinMax
{
public:
  static void init();
  static void destroy();
  // Singleton accessors.
  static const MinMax *min() { return &min_; }
  static const MinMax *max() { return &max_; }
  static const EarlyLate *early() { return &min_; }
  static const EarlyLate *late() { return &max_; }
  static int minIndex() { return min_.index_; }
  static int earlyIndex() { return min_.index_; }
  static int maxIndex() { return max_.index_; }
  static int lateIndex() { return max_.index_; }
  const std::string &to_string() const { return name_; }
  int index() const { return index_; }
  float initValue() const { return init_value_; }
  int initValueInt() const { return init_value_int_; }
  // Max value1 > value2, Min value1 < value2.
  bool compare(float value1,
	       float value2) const;
  // min/max(value1, value2)
  float minMax(float value1,
	       float value2) const;
  const MinMaxAll *asMinMaxAll() const;
  const MinMax *opposite() const;
  // for range support.
  // for (auto min_max : MinMax::range()) {}
  static const std::array<const MinMax*, 2> &range() { return range_; }
  // for (auto mm_index : MinMax::rangeIndex()) {}
  static const std::array<int, 2> &rangeIndex() { return range_index_; }
  // Find MinMax from name.
  static const MinMax *find(const char *min_max);
  // Find MinMax from index.
  static const MinMax *find(int index);
  static const int index_max = 1;
  static const int index_count = 2;
  static const int index_bit_count = 1;

private:
  MinMax(const char *name,
	 int index,
	 float init_value,
         int init_value_int,
	 bool (*compare)(float value1,
			 float value2));

  const std::string name_;
  int index_;
  float init_value_;
  int init_value_int_;
  bool (*compare_)(float value1,
		   float value2);

  static const MinMax min_;
  static const MinMax max_;
  static const std::array<const MinMax*, 2> range_;
  static const std::array<int, 2> range_index_;
};

// Min/Max/All, where "All" means use both min and max.
class MinMaxAll
{
public:
  // Singleton accessors.
  static const MinMaxAll *min() { return &min_; }
  static const MinMaxAll *early() { return &min_; }
  static const MinMaxAll *max() { return &max_; }
  static const MinMaxAll *late() { return &max_; }
  static const MinMaxAll *all() { return &all_; }
  const std::string &to_string() const { return name_; }
  int index() const { return index_; }
  const MinMax *asMinMax() const;
  bool matches(const MinMax *min_max) const;
  bool matches(const MinMaxAll *min_max) const;
  static const MinMaxAll *find(const char *min_max);
  // for (const auto min_max : min_max->range()) {}
  const std::vector<const MinMax*> &range() const { return range_; }
  // for (const auto mm_index : min_max->rangeIndex()) {}
  const std::vector<int> &rangeIndex() const { return range_index_; }

private:
  MinMaxAll(const char *name,
	    int index,
	    std::vector<const MinMax*> range,
	    std::vector<int> range_index);

  const std::string name_;
  int index_;
  const std::vector<const MinMax*> range_;
  const std::vector<int> range_index_;

  static const MinMaxAll min_;
  static const MinMaxAll max_;
  static const MinMaxAll all_;
};

} // namespace
