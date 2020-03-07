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
#include "MinMax.hh"
#include "Transition.hh"
#include "RiseFallMinMax.hh"
#include "MinMaxValues.hh"
#include "NetworkClass.hh"

namespace sta {

typedef MinMaxIntValues FanoutValues;

// Port external pin and wire capacitance (set_load -pin_load -wire_load).
class PortExtCap
{
public:
  explicit PortExtCap(Port *port);
  Port *port() { return port_; }
  void pinCap(const RiseFall *rf,
	      const MinMax *min_max,
	      // Return values.
	      float &cap, bool &exists);
  RiseFallMinMax *pinCap() { return &pin_cap_; }
  void setPinCap(float cap,
		 const RiseFall *rf,
		 const MinMax *min_max);
  void wireCap(const RiseFall *rf,
	       const MinMax *min_max,
	       // Return values.
	       float &cap, bool &exists);
  RiseFallMinMax *wireCap() { return &wire_cap_; }
  void setWireCap(float cap,
		  const RiseFall *rf,
		  const MinMax *min_max);
  void setFanout(int fanout,
		 const MinMax *min_max);
  void fanout(const MinMax *min_max,
	      // Return values.
	      int &fanout,
	      bool &exists);
  FanoutValues *fanout() { return &fanout_; }

private:
  DISALLOW_COPY_AND_ASSIGN(PortExtCap);

  Port *port_;
  RiseFallMinMax pin_cap_;
  RiseFallMinMax wire_cap_;
  FanoutValues fanout_;
};

} // namespace
