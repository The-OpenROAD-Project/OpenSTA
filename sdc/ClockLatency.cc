// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "ClockLatency.hh"

namespace sta {

ClockLatency::ClockLatency(const Clock *clk,
			   const Pin *pin) :
  clk_(clk),
  pin_(pin)
{
}

void
ClockLatency::setDelay(const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
		       float delay)
{
  delays_.setValue(rf, min_max, delay);
}

float
ClockLatency::delay(const RiseFall *rf,
		    const MinMax *min_max)
{
  float latency;
  bool exists;
  delays_.value(rf, min_max, latency, exists);
  if (exists)
    return latency;
  else
    return 0.0;
}

void
ClockLatency::delay(const RiseFall *rf,
		    const MinMax *min_max,
		    // Return values.
		    float &latency,
		    bool &exists)

{
  delays_.value(rf, min_max, latency, exists);
  if (!exists)
    latency = 0.0;
}

void
ClockLatency::setDelay(const RiseFall *rf,
		       const MinMax *min_max,
		       float delay)
{
  delays_.setValue(rf, min_max, delay);
}

void
ClockLatency::setDelays(RiseFallMinMax *delays)
{
  delays_.setValues(delays);
}

RiseFallMinMax *
ClockLatency::delays()
{
  return &delays_;
}

} // namespace
