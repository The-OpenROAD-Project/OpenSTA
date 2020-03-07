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

#include "MinMax.hh"
#include "Error.hh"

namespace sta {

template <class TYPE>
class MinMaxValues
{
public:
  MinMaxValues()
  {
    clear();
  }

  MinMaxValues(TYPE init_value)
  {
    int mm_index;

    mm_index = MinMax::minIndex();
    values_[mm_index] = init_value;
    exists_[mm_index] = true;

    mm_index = MinMax::maxIndex();
    values_[mm_index] = init_value;
    exists_[mm_index] = true;
  }

  void
  clear()
  {
    exists_[MinMax::minIndex()] = false;
    exists_[MinMax::maxIndex()] = false;
  }

  void
  clear(const MinMaxAll *min_max)
  {
    exists_[min_max->index()] = false;
  }

  bool
  empty()
  {
    return !exists_[MinMax::minIndex()]
      && !exists_[MinMax::maxIndex()];
  }

  void
  setValue(TYPE value)
  {
    for (auto mm_index : MinMax::rangeIndex()) {
      values_[mm_index] = value;
      exists_[mm_index] = true;
    }
  }

  void
  setValue(const MinMaxAll *min_max,
	   TYPE value)
  {
    for (auto mm_index : min_max->rangeIndex()) {
      values_[mm_index] = value;
      exists_[mm_index] = true;
    }
  }

  void
  setValue(const MinMax *min_max,
	   TYPE value)
  {
    int mm_index = min_max->index();
    values_[mm_index] = value;
    exists_[mm_index] = true;
  }

  void
  mergeValue(const MinMax *min_max,
	     TYPE value)
  {
    int mm_index = min_max->index();
    if (!exists_[mm_index]
	|| min_max->compare(value, values_[mm_index])) {
      values_[mm_index] = value;
      exists_[mm_index] = true;
    }
  }

  TYPE
  value(const MinMax *min_max) const
  {
    int mm_index = min_max->index();
    bool exists = exists_[mm_index];
    if (exists)
      return values_[mm_index];
    else
      internalError("uninitialized value reference");
  }

  void
  value(const MinMax *min_max,
	// Return values.
	TYPE &value,
	bool &exists) const
  {
    int mm_index = min_max->index();
    exists = exists_[mm_index];
    value = values_[mm_index];
  }

  bool
  hasValue(const MinMax *min_max) const
  {
    int mm_index = min_max->index();
    return exists_[mm_index];
  }

  void
  removeValue(const MinMaxAll *min_max)
  {
    for (auto mm_index : min_max->rangeIndex())
      exists_[mm_index] = false;
  }

  static bool equal(MinMaxValues *values1,
		    MinMaxValues *values2)
  {
    return ((!values1->exists_[MinMax::minIndex()]
	     && !values2->exists_[MinMax::minIndex()])
	    || (values1->exists_[MinMax::minIndex()]
		&& values2->exists_[MinMax::minIndex()]
		&& values1->values_[MinMax::minIndex()] 
		== values2->values_[MinMax::minIndex()]))
      && ((!values1->exists_[MinMax::maxIndex()]
	   && !values2->exists_[MinMax::maxIndex()])
	 || (values1->exists_[MinMax::maxIndex()]
	     && values2->exists_[MinMax::maxIndex()]
	     && values1->values_[MinMax::maxIndex()] 
	     == values2->values_[MinMax::maxIndex()]));
  }

private:
  TYPE values_[MinMax::index_count];
  bool exists_[MinMax::index_count];
};

typedef MinMaxValues<float> MinMaxFloatValues;
typedef MinMaxValues<int> MinMaxIntValues;

} // namespace
