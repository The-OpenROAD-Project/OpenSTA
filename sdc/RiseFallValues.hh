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

#ifndef STA_RISE_FALL_VALUES_H
#define STA_RISE_FALL_VALUES_H

#include "DisallowCopyAssign.hh"
#include "Transition.hh"

namespace sta {

// Rise/fall group of two values.
class RiseFallValues
{
public:
  RiseFallValues();
  explicit RiseFallValues(float init_value);
  float value(const TransRiseFall *tr) const;
  void value(const TransRiseFall *tr,
	     float &value, bool &exists) const;
  bool hasValue(const TransRiseFall *tr) const;
  void setValue(const TransRiseFallBoth *tr, float value);
  void setValue(const TransRiseFall *tr, float value);
  void setValue(float value);
  void setValues(RiseFallValues *values);
  void clear();

private:
  DISALLOW_COPY_AND_ASSIGN(RiseFallValues);

  float values_[TransRiseFall::index_count];
  bool exists_[TransRiseFall::index_count];
};

} // namespace
#endif
