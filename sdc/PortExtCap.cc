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
#include "PortExtCap.hh"

namespace sta {

PortExtCap::PortExtCap(Port *port) :
  port_(port)
{
}

void
PortExtCap::pinCap(const RiseFall *rf,
		   const MinMax *min_max,
		   // Return values.
		   float &cap,
		   bool &exists)
{
  pin_cap_.value(rf, min_max, cap, exists);
}

void
PortExtCap::setPinCap(float cap,
		      const RiseFall *rf,
		      const MinMax *min_max)
{
  pin_cap_.setValue(rf, min_max, cap);
}

void
PortExtCap::wireCap(const RiseFall *rf,
		    const MinMax *min_max,
		    // Return values.
		    float &cap,
		    bool &exists)
{
  wire_cap_.value(rf, min_max, cap, exists);
}

void
PortExtCap::setWireCap(float cap,
		       const RiseFall *rf,
		       const MinMax *min_max)
{
  wire_cap_.setValue(rf, min_max, cap);
}

void
PortExtCap::setFanout(int fanout,
		      const MinMax *min_max)
{
  fanout_.setValue(min_max, fanout);
}


void
PortExtCap::fanout(const MinMax *min_max,
		   // Return values.
		   int &fanout,
		   bool &exists)
{
  fanout_.value(min_max, fanout, exists);
}

} // namespace
