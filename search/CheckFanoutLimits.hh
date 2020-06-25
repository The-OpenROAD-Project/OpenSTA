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
  PinSeq *pinFanoutLimitViolations(const MinMax *min_max);
  Pin *pinMinFanoutLimitSlack(const MinMax *min_max);

protected:
  void checkFanout(const Pin *pin,
		   const MinMax *min_max,
		   float limit1,
		   // Return values.
		   float &fanout,
		   float &slack,
		   float &limit) const;
  void findLimit(const Pin *pin,
		 const MinMax *min_max,
		 // Return values.
		 float &limit,
		 bool &limit_exists) const;
  void pinFanoutLimitViolations(Instance *inst,
				const MinMax *min_max,
				PinSeq *violators);
  void pinMinFanoutLimitSlack(Instance *inst,
			      const MinMax *min_max,
			      // Return values.
			      Pin *&min_slack_pin,
			      float &min_slack);
  float fanoutLoad(const Pin *pin) const;
  bool checkPin(Pin *pin);

  const Sta *sta_;
};

} // namespace
