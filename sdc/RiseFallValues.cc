// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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
  TransRiseFallIterator tr_iter;
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int tr_index = tr->index();
    exists_[tr_index] = false;
  }
}

RiseFallValues::RiseFallValues(float init_value)
{
  TransRiseFallIterator tr_iter;
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int tr_index = tr->index();
    values_[tr_index] = init_value;
    exists_[tr_index] = true;
  }
}

void
RiseFallValues::setValue(float value)
{
  setValue(TransRiseFallBoth::riseFall(), value);
}

void
RiseFallValues::setValue(const TransRiseFallBoth *tr, float value)
{
  TransRiseFallIterator tr_iter(tr);
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int tr_index = tr->index();
    values_[tr_index] = value;
    exists_[tr_index] = true;
  }
}

void
RiseFallValues::setValue(const TransRiseFall *tr, float value)
{
  int tr_index = tr->index();
  values_[tr_index] = value;
  exists_[tr_index] = true;
}

void
RiseFallValues::setValues(RiseFallValues *values)
{
  TransRiseFallIterator tr_iter;
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int tr_index = tr->index();
    values_[tr_index] = values->values_[tr_index];
    exists_[tr_index] = values->exists_[tr_index];
  }
}

void
RiseFallValues::value(const TransRiseFall *tr,
		      float &value, bool &exists) const
{
  int tr_index = tr->index();
  exists = exists_[tr_index];
  if (exists)
    value = values_[tr_index];
}

float
RiseFallValues::value(const TransRiseFall *tr) const
{
  return values_[tr->index()];
}

bool
RiseFallValues::hasValue(const TransRiseFall *tr) const
{
  return exists_[tr->index()];
}

} // namespace
