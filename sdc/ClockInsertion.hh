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
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "MinMax.hh"
#include "RiseFallMinMax.hh"
#include "Transition.hh"

namespace sta {

class ClockInsertion
{
public:
  ClockInsertion(const Clock *clk, const Pin *pin);
  const Clock *clock() const { return clk_; }
  const Pin *pin() const { return pin_; }
  float delay(const RiseFall *rf, const MinMax *min_max,
	      const EarlyLate *early_late);
  void delay(const RiseFall *rf, const MinMax *min_max,
	     const EarlyLate *early_late,
	     // Return values.
	     float &insertion, bool &exists);
  RiseFallMinMax *delays(const EarlyLate *early_late);
  void setDelay(const RiseFall *rf, const MinMax *min_max,
		const EarlyLate *early_late, float delay);
  void setDelay(const RiseFallBoth *rf, const MinMaxAll *min_max,
		const EarlyLateAll *early_late, float delay);
  void setDelays(RiseFallMinMax *delays);

private:
  DISALLOW_COPY_AND_ASSIGN(ClockInsertion);

  const Clock *clk_;
  const Pin *pin_;
  RiseFallMinMax delays_[EarlyLate::index_count];
};

} // namespace
