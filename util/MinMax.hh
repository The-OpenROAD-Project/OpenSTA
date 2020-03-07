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

#include <array>
#include <vector>
#include "DisallowCopyAssign.hh"
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
  static MinMax *min() { return &min_; }
  static MinMax *max() { return &max_; }
  static EarlyLate *early() { return &min_; }
  static EarlyLate *late() { return &max_; }
  static int minIndex() { return min_.index_; }
  static int earlyIndex() { return min_.index_; }
  static int maxIndex() { return max_.index_; }
  static int lateIndex() { return max_.index_; }
  const char *asString() const { return name_; }
  int index() const { return index_; }
  float initValue() const { return init_value_; }
  // Max value1 > value2, Min value1 < value2.
  bool compare(float value1,
	       float value2) const;
  MinMaxAll *asMinMaxAll() const;
  MinMax *opposite() const;
  // for range support.
  // for (auto min_max : MinMax::range()) {}
  static const std::array<MinMax*, 2> &range() { return range_; }
  // for (auto mm_index : MinMax::rangeIndex()) {}
  static const std::array<int, 2> &rangeIndex() { return range_index_; }
  static MinMax *find(const char *min_max);
  // Find transition from index.
  static MinMax *find(int index);
  static const int index_max = 1;
  static const int index_count = 2;
  static const int index_bit_count = 1;

private:
  DISALLOW_COPY_AND_ASSIGN(MinMax);
  MinMax(const char *name,
	 int index,
	 float init_value,
	 bool (*compare)(float value1,
			 float value2));

  const char *name_;
  int index_;
  float init_value_;
  bool (*compare_)(float value1,
		   float value2);

  static MinMax min_;
  static MinMax max_;
  static const std::array<MinMax*, 2> range_;
  static const std::array<int, 2> range_index_;
};

// Min/Max/All, where "All" means use both min and max.
class MinMaxAll
{
public:
  // Singleton accessors.
  static MinMaxAll *min() { return &min_; }
  static MinMaxAll *early() { return &min_; }
  static MinMaxAll *max() { return &max_; }
  static MinMaxAll *late() { return &max_; }
  static MinMaxAll *all() { return &all_; }
  const char *asString() const { return name_; }
  int index() const { return index_; }
  MinMax *asMinMax() const;
  bool matches(const MinMax *min_max) const;
  bool matches(const MinMaxAll *min_max) const;
  static MinMaxAll *find(const char *min_max);
  // for (auto min_max : min_max->range()) {}
  const std::vector<MinMax*> &range() const { return range_; }
  // for (auto mm_index : min_max->rangeIndex()) {}
  const std::vector<int> &rangeIndex() const { return range_index_; }

private:
  DISALLOW_COPY_AND_ASSIGN(MinMaxAll);
  MinMaxAll(const char *name,
	    int index,
	    std::vector<MinMax*> range,
	    std::vector<int> range_index);

  const char *name_;
  int index_;
  const std::vector<MinMax*> range_;
  const std::vector<int> range_index_;

  static MinMaxAll min_;
  static MinMaxAll max_;
  static MinMaxAll all_;
};

////////////////////////////////////////////////////////////////

// Obsolete. Use range iteration.
class MinMaxIterator : public Iterator<MinMax*>
{
public:
  MinMaxIterator() : index_(0), index_max_(MinMax::index_max) {}
  explicit MinMaxIterator(const MinMaxAll *min_max);
  bool hasNext() { return index_ <= index_max_; }
  MinMax *next()
  { return (index_++ == 0) ? MinMax::min() : MinMax::max(); }

private:
  DISALLOW_COPY_AND_ASSIGN(MinMaxIterator);

  int index_;
  int index_max_;
};

typedef MinMaxIterator EarlyLateIterator;

} // namespace
