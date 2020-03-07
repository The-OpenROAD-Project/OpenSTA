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

#include "ArcDelayCalc.hh"

namespace sta {

// Unit delay calculator.
class UnitDelayCalc : public ArcDelayCalc
{
public:
  UnitDelayCalc(StaState *sta);
  virtual ArcDelayCalc *copy();
  virtual Parasitic *findParasitic(const Pin *drvr_pin,
				   const RiseFall *rf,
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
			 // Return values.
			 ArcDelay &wire_delay,
			 Slew &load_slew);
  virtual void setMultiDrvrSlewFactor(float) {}
  virtual float ceff(const LibertyCell *drvr_cell,
		     TimingArc *arc,
		     const Slew &in_slew,
		     float load_cap,
		     Parasitic *drvr_parasitic,
		     float related_out_cap,
		     const Pvt *pvt,
		     const DcalcAnalysisPt *dcalc_ap);
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const RiseFall *rf,
			      Parasitic *parasitic,
			      const DcalcAnalysisPt *dcalc_ap);
  virtual void checkDelay(const LibertyCell *cell,
			  TimingArc *arc,
			  const Slew &from_slew,
			  const Slew &to_slew,
			  float related_out_cap,
			  const Pvt *pvt,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  ArcDelay &margin);
  virtual void reportGateDelay(const LibertyCell *drvr_cell,
			       TimingArc *arc,
			       const Slew &in_slew,
			       float load_cap,
			       Parasitic *drvr_parasitic,
			       float related_out_cap,
			       const Pvt *pvt,
			       const DcalcAnalysisPt *dcalc_ap,
			       int digits,
			       string *result);
  virtual void reportCheckDelay(const LibertyCell *cell,
				TimingArc *arc,
				const Slew &from_slew,
				const char *from_slew_annotation,
				const Slew &to_slew,
				float related_out_cap,
				const Pvt *pvt,
				const DcalcAnalysisPt *dcalc_ap,
				int digits,
				string *result);
  virtual void finishDrvrPin();
};

ArcDelayCalc *
makeUnitDelayCalc(StaState *sta);

} // namespace
