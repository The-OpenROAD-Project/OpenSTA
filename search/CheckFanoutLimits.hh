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

#pragma once

#include "MinMax.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "Sta.hh"

namespace sta {

class StaState;

class CheckFanoutLimits
{
public:
  CheckFanoutLimits(const Sta *sta);
  void checkFanout(const Pin *pin,
		   const MinMax *min_max,
		   // Return values.
		   float &fanout,
		   float &limit,
		   float &slack) const;
  // Return pins with the min/max fanout limit slack.
  // net=null check all nets
  // corner=nullptr checks all corners.
  PinSeq checkFanoutLimits(const Net *net,
                           bool violators,
                           const MinMax *min_max);

protected:
  void checkFanout(const Pin *pin,
		   const MinMax *min_max,
		   float limit1,
		   // Return values.
		   float &fanout,
		   float &limit,
		   float &slack) const;
  void findLimit(const Pin *pin,
		 const MinMax *min_max,
		 // Return values.
		 float &limit,
		 bool &limit_exists) const;
  float fanoutLoad(const Pin *pin) const;
  void checkFanoutLimits(const Instance *inst,
                         bool violators,
                         const MinMax *min_max,
                         PinSeq &fanout_pins,
                         float &min_slack);
  void checkFanoutLimits(const Pin *pin,
                         bool violators,
                         const MinMax *min_max,
                         PinSeq &fanout_pins,
                         float &min_slack);
  bool checkPin(const Pin *pin);

  const Sta *sta_;
};

} // namespace
