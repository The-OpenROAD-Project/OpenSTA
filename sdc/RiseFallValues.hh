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

namespace sta {

// Rise/fall group of two values.
class RiseFallValues
{
public:
  RiseFallValues();
  explicit RiseFallValues(float init_value);
  float value(const RiseFall *rf) const;
  void value(const RiseFall *rf,
	     float &value, bool &exists) const;
  bool hasValue(const RiseFall *rf) const;
  void setValue(const RiseFallBoth *rf, float value);
  void setValue(const RiseFall *rf, float value);
  void setValue(float value);
  void setValues(RiseFallValues *values);
  void clear();

private:
  DISALLOW_COPY_AND_ASSIGN(RiseFallValues);

  float values_[RiseFall::index_count];
  bool exists_[RiseFall::index_count];
};

} // namespace
