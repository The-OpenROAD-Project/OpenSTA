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

#include "DisallowCopyAssign.hh"
#include "Transition.hh"
#include "MinMax.hh"

namespace sta {

// Rise/Fall/Min/Max group of four values common to many constraints.
class RiseFallMinMax
{
public:
  RiseFallMinMax();
  RiseFallMinMax(const RiseFallMinMax *rfmm);
  explicit RiseFallMinMax(float init_value);
  float value(const RiseFall *rf,
	      const MinMax *min_max) const;
  void value(const RiseFall *rf,
	     const MinMax *min_max,
	     float &value,
	     bool &exists) const;
  bool hasValue() const;
  bool empty() const;
  bool hasValue(const RiseFall *rf,
		const MinMax *min_max) const;
  void setValue(const RiseFallBoth *rf,
		const MinMaxAll *min_max,
		float value);
  void setValue(const RiseFallBoth *rf,
		const MinMax *min_max,
		float value);
  void setValue(const RiseFall *rf,
		const MinMax *min_max, float value);
  void setValue(float value);
  void mergeValue(const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float value);
  void setValues(RiseFallMinMax *values);
  void removeValue(const RiseFallBoth *rf,
		   const MinMax *min_max);
  void removeValue(const RiseFallBoth *rf, 
		   const MinMaxAll *min_max);
  // Merge all values of rfmm.
  void mergeWith(RiseFallMinMax *rfmm);
  void clear();
  bool equal(const RiseFallMinMax *values) const;
  bool isOneValue() const;
  bool isOneValue(float &value) const;
  bool isOneValue(const MinMax *min_max,
		  // Return values.
		  float &value) const;

private:
  float values_[RiseFall::index_count][MinMax::index_count];
  bool exists_[RiseFall::index_count][MinMax::index_count];
};

} // namespace
