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

class ClockLatency
{
public:
  ClockLatency(const Clock *clk,
	       const Pin *pin);
  const Clock *clock() const { return clk_; }
  const Pin *pin() const { return pin_; }
  float delay(const RiseFall *rf,
	      const MinMax *min_max);
  void delay(const RiseFall *rf,
	     const MinMax *min_max,
	     // Return values.
	     float &latency,
	     bool &exists);
  RiseFallMinMax *delays();
  void setDelay(const RiseFall *rf,
		const MinMax *min_max,
		float delay);
  void setDelay(const RiseFallBoth *rf,
		const MinMaxAll *min_max,
		float delay);
  void setDelays(RiseFallMinMax *delays);

private:
  DISALLOW_COPY_AND_ASSIGN(ClockLatency);

  const Clock *clk_;
  const Pin *pin_;
  RiseFallMinMax delays_;
};

} // namespace
