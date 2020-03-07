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

#include "RCDelayCalc.hh"

namespace sta {

// Liberty table model lumped capacitance arc delay calculator.
// Effective capacitance is the pi model total capacitance (C1+C2).
// Wire delays are elmore delays.
// Driver slews are degraded to loads by rise/fall transition_degradation
// tables.
class SimpleRCDelayCalc : public RCDelayCalc
{
public:
  SimpleRCDelayCalc(StaState *sta);
  virtual ArcDelayCalc *copy();
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const RiseFall *rf,
			      Parasitic *parasitic,
			      const DcalcAnalysisPt *dcalc_ap);
  virtual void gateDelay(const LibertyCell *drvr_cell,
			 TimingArc *arc,
			 const Slew &in_slew,
			 float load_cap,
			 Parasitic *drvr_parasitic,
			 float related_out_cap,
			 const Pvt *pvt,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 ArcDelay &gate_delay,
			 Slew &drvr_slew);
  virtual void loadDelay(const Pin *load_pin,
			 ArcDelay &wire_delay,
			 Slew &load_slew);

  using RCDelayCalc::gateDelay;
  using RCDelayCalc::reportGateDelay;

private:
  const Pvt *pvt_;
};

ArcDelayCalc *
makeSimpleRCDelayCalc(StaState *sta);

} // namespace
