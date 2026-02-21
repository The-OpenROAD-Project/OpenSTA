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

#include "PortExtCap.hh"

namespace sta {

PortExtCap::PortExtCap() :
  port_(nullptr)
{
}

void
PortExtCap::pinCap(const RiseFall *rf,
                   const MinMax *min_max,
                   // Return values.
                   float &cap,
                   bool &exists) const
{
  pin_cap_.value(rf, min_max, cap, exists);
}

void
PortExtCap::setPinCap(const Port *port,
                      float cap,
                      const RiseFall *rf,
                      const MinMax *min_max)
{
  port_ = port;
  pin_cap_.setValue(rf, min_max, cap);
}

void
PortExtCap::wireCap(const RiseFall *rf,
                    const MinMax *min_max,
                    // Return values.
                    float &cap,
                    bool &exists) const
{
  wire_cap_.value(rf, min_max, cap, exists);
}

void
PortExtCap::setWireCap(const Port *port,
                       float cap,
                       const RiseFall *rf,
                       const MinMax *min_max)
{
  port_ = port;
  wire_cap_.setValue(rf, min_max, cap);
}

void
PortExtCap::setFanout(const Port *port,
                      int fanout,
                      const MinMax *min_max)
{
  port_ = port;
  fanout_.setValue(min_max, fanout);
}


void
PortExtCap::fanout(const MinMax *min_max,
                   // Return values.
                   int &fanout,
                   bool &exists) const
{
  fanout_.value(min_max, fanout, exists);
}

} // namespace
