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

#include "Machine.hh"
#include "RiseFallMinMax.hh"

namespace sta {

RiseFallMinMax::RiseFallMinMax()
{
  clear();
}

void
RiseFallMinMax::clear()
{
  for (int tr_index=0; tr_index<RiseFall::index_count; tr_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      exists_[tr_index][mm_index] = false;
    }
  }
}

RiseFallMinMax::RiseFallMinMax(float init_value)
{
  for (int tr_index=0;tr_index<RiseFall::index_count;tr_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      values_[tr_index][mm_index] = init_value;
      exists_[tr_index][mm_index] = true;
    }
  }
}

RiseFallMinMax::RiseFallMinMax(const RiseFallMinMax *rfmm)
{
  for (int tr_index=0;tr_index<RiseFall::index_count;tr_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      values_[tr_index][mm_index] = rfmm->values_[tr_index][mm_index];
      exists_[tr_index][mm_index] = rfmm->exists_[tr_index][mm_index];
    }
  }
}

void
RiseFallMinMax::setValue(float value)
{
  setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), value);
}

void
RiseFallMinMax::setValue(const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 float value)
{
  for (auto rf_index : rf->rangeIndex()) {
    for (auto mm_index : min_max->rangeIndex()) {
      values_[rf_index][mm_index] = value;
      exists_[rf_index][mm_index] = true;
    }
  }
}

void
RiseFallMinMax::removeValue(const RiseFallBoth *rf,
			    const MinMax *min_max)
{
  int mm_index = min_max->index();
  for (auto rf_index : rf->rangeIndex())
    exists_[rf_index][mm_index] = false;
}

void
RiseFallMinMax::removeValue(const RiseFallBoth *rf, 
			    const MinMaxAll *min_max)
{
  for (auto mm : min_max->range())
    removeValue(rf, mm);
}

void
RiseFallMinMax::mergeValue(const RiseFallBoth *rf,
			   const MinMaxAll *min_max,
			   float value)
{
  for (auto rf_index : rf->rangeIndex()) {
    for (auto mm : min_max->range()) {
      int mm_index = mm->index();
      if (!exists_[rf_index][mm_index]
	  || mm->compare(value, values_[rf_index][mm_index])) {
	values_[rf_index][mm_index] = value;
	exists_[rf_index][mm_index] = true;
      }
    }
  }
}

void
RiseFallMinMax::setValue(const RiseFallBoth *rf,
			 const MinMax *min_max,
			 float value)
{
  int mm_index = min_max->index();
  for (auto rf_index : rf->rangeIndex()) {
    values_[rf_index][mm_index] = value;
    exists_[rf_index][mm_index] = true;
  }
}

void
RiseFallMinMax::setValue(const RiseFall *rf,
			 const MinMax *min_max,
			 float value)
{
  int rf_index = rf->index();
  int mm_index = min_max->index();
  values_[rf_index][mm_index] = value;
  exists_[rf_index][mm_index] = true;
}

void
RiseFallMinMax::setValues(RiseFallMinMax *values)
{
  for (int rf_index=0;rf_index<RiseFall::index_count;rf_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      values_[rf_index][mm_index] = values->values_[rf_index][mm_index];
      exists_[rf_index][mm_index] = values->exists_[rf_index][mm_index];
    }
  }
}

void
RiseFallMinMax::value(const RiseFall *rf,
		      const MinMax *min_max,
		      float &value, bool &exists) const
{
  exists = exists_[rf->index()][min_max->index()];
  if (exists)
    value = values_[rf->index()][min_max->index()];
}

float
RiseFallMinMax::value(const RiseFall *rf,
		      const MinMax *min_max) const
{
  return values_[rf->index()][min_max->index()];
}

bool
RiseFallMinMax::hasValue() const
{
  return !empty();
}

bool
RiseFallMinMax::empty() const
{
  for (int rf_index=0;rf_index<RiseFall::index_count;rf_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      if (exists_[rf_index][mm_index])
	return false;
    }
  }
  return true;
}

bool
RiseFallMinMax::hasValue(const RiseFall *rf, const MinMax *min_max) const
{
  return exists_[rf->index()][min_max->index()];
}

void
RiseFallMinMax::mergeWith(RiseFallMinMax *rfmm)
{
  for (auto min_max : MinMax::range()) {
    int mm_index = min_max->index();
    for (auto rf_index : RiseFall::rangeIndex()) {
      bool exists1 = exists_[rf_index][mm_index];
      bool exists2 = rfmm->exists_[rf_index][mm_index];
      if (exists1 && exists2) {
	float rfmm_value = rfmm->values_[rf_index][mm_index];
	if (min_max->compare(rfmm_value, values_[rf_index][mm_index]))
	  values_[rf_index][mm_index] = rfmm_value;
      }
      else if (!exists1 && exists2) {
	values_[rf_index][mm_index] = rfmm->values_[rf_index][mm_index];
	exists_[rf_index][mm_index] = true;
      }
    }
  }
}

bool
RiseFallMinMax::equal(const RiseFallMinMax *values) const
{
  for (int rf_index=0;rf_index<RiseFall::index_count;rf_index++) {
    for (int mm_index = 0; mm_index < MinMax::index_count; mm_index++) {
      bool exists1 = exists_[rf_index][mm_index];
      bool exists2 = values->exists_[rf_index][mm_index];
      if (exists1 != exists2)
	return false;
      if (exists1 && exists2
	  && values_[rf_index][mm_index] != values->values_[rf_index][mm_index])
	return false;
    }
  }
  return true;
}

bool
RiseFallMinMax::isOneValue() const
{
  float value;
  return isOneValue(value);
}

bool
RiseFallMinMax::isOneValue(float &value) const
{
  if (exists_[0][0]) {
    value = values_[0][0];
    for (int rf_index=0;rf_index<RiseFall::index_count;rf_index++) {
      for (int mm_index=0; mm_index<MinMax::index_count;mm_index++) {
	if (!exists_[rf_index][mm_index]
	    || values_[rf_index][mm_index] != value)
	  return false;
      }
    }
    return true;
  }
  else
    return false;
}

bool
RiseFallMinMax::isOneValue(const MinMax *min_max,
			   // Return values.
			   float &value) const
{
  int mm_index = min_max->index();
  if (exists_[0][mm_index]) {
    value = values_[0][mm_index];
    for (int rf_index=0;rf_index<RiseFall::index_count;rf_index++) {
      if (!exists_[rf_index][mm_index]
	  || values_[rf_index][mm_index] != value)
	return false;
    }
    return true;
  }
  else
    return false;
}

} // namespace
