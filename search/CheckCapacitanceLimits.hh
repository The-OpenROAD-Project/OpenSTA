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
#include "Transition.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"

namespace sta {

class StaState;
class Corner;

class CheckCapacitanceLimits
{
public:
  CheckCapacitanceLimits(const StaState *sta);
  // corner=nullptr checks all corners.
  void checkCapacitance(const Pin *pin,
			const Corner *corner1,
			const MinMax *min_max,
			// Return values.
			// Corner is nullptr for no capacitance limit.
			const Corner *&corner,
			const RiseFall *&rf,
			float &capacitance,
			float &limit,
			float &slack) const;
  // corner=nullptr checks all corners.
  PinSeq *pinCapacitanceLimitViolations(const Corner *corner,
					const MinMax *min_max);
  // corner=nullptr checks all corners.
  Pin *pinMinCapacitanceLimitSlack(const Corner *corner,
				   const MinMax *min_max);

protected:
  void checkCapacitance(const Pin *pin,
			const Corner *corner,
			const MinMax *min_max,
			const RiseFall *rf1,
			float limit1,
			// Return values.
			const Corner *&corner1,
			const RiseFall *&rf,
			float &capacitance,
			float &slack,
			float &limit) const;
  void checkCapacitance1(const Pin *pin,
			 const Corner *corner1,
			 const MinMax *min_max,
			 // Return values.
			 const Corner *&corner,
			 const RiseFall *&rf,
			 float &capacitance,
			 float &limit,
			 float &slack) const;
  void findLimit(const Pin *pin,
		 const MinMax *min_max,
		 // Return values.
		 float &limit,
		 bool &limit_exists) const;
  void pinCapacitanceLimitViolations(Instance *inst,
				     const Corner *corner,
				     const MinMax *min_max,
				     PinSeq *violators);
  void pinMinCapacitanceLimitSlack(Instance *inst,
				   const Corner *corner,
				   const MinMax *min_max,
				   // Return values.
				   Pin *&min_slack_pin,
				   float &min_slack);
  bool checkPin(const Pin *pin);

  const StaState *sta_;
};

} // namespace
