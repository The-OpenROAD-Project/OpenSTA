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

#include <algorithm>
#include "Machine.hh"
#include "StringUtil.hh"
#include "MinMax.hh"

namespace sta {

const float INF = 1E+30F;

static bool
compareMin(float value1,
	   float value2)
{
  return value1 < value2;
}

static bool
compareMax(float value1,
	   float value2)
{
  return value1 > value2;
}

////////////////////////////////////////////////////////////////

MinMax MinMax::min_("min", 0,  INF, compareMin);
MinMax MinMax::max_("max", 1, -INF, compareMax);
const std::array<MinMax*, 2> MinMax::range_{&min_, &max_};
const std::array<int, 2> MinMax::range_index_{min_.index(), max_.index()};

MinMax::MinMax(const char *name,
	       int index,
	       float init_value,
	       bool (*compare)(float value1, float value2)) :
  name_(name),
  index_(index),
  init_value_(init_value),
  compare_(compare)
{
}

MinMaxAll *
MinMax::asMinMaxAll() const
{
  if (this == &min_)
    return MinMaxAll::min();
  else
    return MinMaxAll::max();
}

MinMax *
MinMax::opposite() const
{
  if (this == &max_)
    return &min_;
  else
    return &max_;
}

MinMax *
MinMax::find(const char *min_max)
{
  if (stringEq(min_max, "min")
      || stringEq(min_max, "early"))
    return &min_;
  else if (stringEq(min_max, "max")
	   || stringEq(min_max, "late"))
    return &max_;
  else
    return nullptr;
}

MinMax *
MinMax::find(int index)
{
  if (index == min_.index())
    return &min_;
  else if (index == max_.index())
    return &max_;
  else
    return nullptr;
}

bool
MinMax::compare(float value1,
		float value2) const
{
  return compare_(value1, value2);
}

////////////////////////////////////////////////////////////////

MinMaxAll MinMaxAll::min_("min", 0, {MinMax::min()}, {MinMax::min()->index()});
MinMaxAll MinMaxAll::max_("max", 1, {MinMax::max()}, {MinMax::max()->index()});
MinMaxAll MinMaxAll::all_("all", 2, {MinMax::min(), MinMax::max()},
			  {MinMax::min()->index(), MinMax::max()->index()});

MinMaxAll::MinMaxAll(const char *name,
		     int index,
		     std::vector<MinMax*> range,
		     std::vector<int> range_index) :
  name_(name),
  index_(index),
  range_(range),
  range_index_(range_index)
{
}

MinMax *
MinMaxAll::asMinMax() const
{
  if (this == &min_)
    return MinMax::min();
  else
    return MinMax::max();
}

bool
MinMaxAll::matches(const MinMax *min_max) const
{
  return this == &all_ || asMinMax() == min_max;
}

bool
MinMaxAll::matches(const MinMaxAll *min_max) const
{
  return this == &all_ || this == min_max;
}

MinMaxAll *
MinMaxAll::find(const char *min_max)
{
  if (stringEq(min_max, "min")
      || stringEq(min_max, "early"))
    return &min_;
  else if (stringEq(min_max, "max")
	   || stringEq(min_max, "late"))
    return &max_;
  else if (stringEq(min_max, "all")
	   || stringEq(min_max, "min_max")
	   || stringEq(min_max, "minmax"))
    return &all_;
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

MinMaxIterator::MinMaxIterator(const MinMaxAll *min_max)
{
  if (min_max == MinMaxAll::all()) {
    index_ = 0;
    index_max_ = MinMax::index_max;
  }
  else {
    index_ = min_max->asMinMax()->index();
    index_max_ = index_;
  }
}

} // namespace
