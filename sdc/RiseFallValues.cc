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
#include "RiseFallValues.hh"

namespace sta {

RiseFallValues::RiseFallValues()
{
  clear();
}

void
RiseFallValues::clear()
{
  for (auto tr_index : RiseFall::rangeIndex())
    exists_[tr_index] = false;
}

RiseFallValues::RiseFallValues(float init_value)
{
  for (auto tr_index : RiseFall::rangeIndex()) {
    values_[tr_index] = init_value;
    exists_[tr_index] = true;
  }
}

void
RiseFallValues::setValue(float value)
{
  setValue(RiseFallBoth::riseFall(), value);
}

void
RiseFallValues::setValue(const RiseFallBoth *rf,
			 float value)
{
  for (auto rf_index : rf->rangeIndex()) {
    values_[rf_index] = value;
    exists_[rf_index] = true;
  }
}

void
RiseFallValues::setValue(const RiseFall *rf,
			 float value)
{
  int rf_index = rf->index();
  values_[rf_index] = value;
  exists_[rf_index] = true;
}

void
RiseFallValues::setValues(RiseFallValues *values)
{
  for (auto rf_index : RiseFall::rangeIndex()) {
    values_[rf_index] = values->values_[rf_index];
    exists_[rf_index] = values->exists_[rf_index];
  }
}

void
RiseFallValues::value(const RiseFall *rf,
		      float &value, bool &exists) const
{
  int rf_index = rf->index();
  exists = exists_[rf_index];
  if (exists)
    value = values_[rf_index];
}

float
RiseFallValues::value(const RiseFall *rf) const
{
  return values_[rf->index()];
}

bool
RiseFallValues::hasValue(const RiseFall *rf) const
{
  return exists_[rf->index()];
}

} // namespace
