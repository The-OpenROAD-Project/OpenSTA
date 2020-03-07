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

#include "LumpedCapDelayCalc.hh"

namespace sta {

// Base class for delay calculators with RC wire delay.
class RCDelayCalc : public LumpedCapDelayCalc
{
public:
  RCDelayCalc(StaState *sta);
  virtual ArcDelayCalc *copy();
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const RiseFall *rf,
			      Parasitic *parasitic,
			      const DcalcAnalysisPt *dcalc_ap);

protected:
  // Helper function for input ports driving dspf parasitic.
  void dspfWireDelaySlew(const Pin *load_pin,
			 float elmore,
			 ArcDelay &wire_delay,
			 Slew &load_slew);

  const LibertyCell *drvr_cell_;
  Parasitic *drvr_parasitic_;
};

} // namespace
