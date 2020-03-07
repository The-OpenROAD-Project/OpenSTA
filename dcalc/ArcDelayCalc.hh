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

#include <string>
#include "DisallowCopyAssign.hh"
#include "MinMax.hh"
#include "Delay.hh"
#include "StaState.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"

namespace sta {

using std::string;

class Parasitic;
class DcalcAnalysisPt;

// Delay calculator class hierarchy.
//  ArcDelayCalc
//   UnitDelayCalc
//   LumpedCapDelayCalc
//    RCDelayCalc
//     SimpleRCDelayCalc
//     DmpCeffDelayCalc
//      DmpCeffElmoreDelayCalc
//      DmpCeffTwoPoleDelayCalc

// Abstract class to interface to a delay calculator primitive.
class ArcDelayCalc : public StaState
{
public:
  explicit ArcDelayCalc(StaState *sta);
  virtual ~ArcDelayCalc() {}
  virtual ArcDelayCalc *copy() = 0;

  // Find the parasitic for drvr_pin that is acceptable to the delay
  // calculator by probing parasitics_.
  virtual Parasitic *findParasitic(const Pin *drvr_pin,
				   const RiseFall *rf,
				   const DcalcAnalysisPt *dcalc_ap) = 0;

  // Find the wire delays and slews for an input port without a driving cell.
  // This call primarily initializes the load delay/slew iterator.
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const RiseFall *rf,
			      Parasitic *parasitic,
			      const DcalcAnalysisPt *dcalc_ap) = 0;

  // Find the delay and slew for arc driving drvr_pin.
  virtual void gateDelay(const LibertyCell *drvr_cell,
			 TimingArc *arc,
			 const Slew &in_slew,
			 // Pass in load_cap or drvr_parasitic.
			 float load_cap,
			 Parasitic *drvr_parasitic,
			 float related_out_cap,
			 const Pvt *pvt,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 ArcDelay &gate_delay,
			 Slew &drvr_slew) = 0;
  // Find the wire delay and load slew of a load pin.
  // Called after inputPortDelay or gateDelay.
  virtual void loadDelay(const Pin *load_pin,
			 // Return values.
			 ArcDelay &wire_delay,
			 Slew &load_slew) = 0;
  virtual void setMultiDrvrSlewFactor(float factor) = 0;
  // Ceff for parasitics with pi models.
  virtual float ceff(const LibertyCell *drvr_cell,
		     TimingArc *arc,
		     const Slew &in_slew,
		     float load_cap,
		     Parasitic *drvr_parasitic,
		     float related_out_cap,
		     const Pvt *pvt,
		     const DcalcAnalysisPt *dcalc_ap) = 0;

  // Find the delay for a timing check arc given the arc's
  // from/clock, to/data slews and related output pin parasitic.
  virtual void checkDelay(const LibertyCell *drvr_cell,
			  TimingArc *arc,
			  const Slew &from_slew,
			  const Slew &to_slew,
			  float related_out_cap,
			  const Pvt *pvt,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  ArcDelay &margin) = 0;
  // Report delay and slew calculation.
  virtual void reportGateDelay(const LibertyCell *drvr_cell,
			       TimingArc *arc,
			       const Slew &in_slew,
			       // Pass in load_cap or drvr_parasitic.
			       float load_cap,
			       Parasitic *drvr_parasitic,
			       float related_out_cap,
			       const Pvt *pvt,
			       const DcalcAnalysisPt *dcalc_ap,
			       int digits,
			       string *result) = 0;
  // Report timing check delay calculation.
  virtual void reportCheckDelay(const LibertyCell *cell,
				TimingArc *arc,
				const Slew &from_slew,
				const char *from_slew_annotation,
				const Slew &to_slew,
				float related_out_cap,
				const Pvt *pvt,
				const DcalcAnalysisPt *dcalc_ap,
				int digits,
				string *result) = 0;
  virtual void finishDrvrPin() = 0;

protected:
  GateTimingModel *gateModel(TimingArc *arc,
			     const DcalcAnalysisPt *dcalc_ap) const;
  CheckTimingModel *checkModel(TimingArc *arc,
			       const DcalcAnalysisPt *dcalc_ap) const;
  TimingModel *model(TimingArc *arc,
		     const DcalcAnalysisPt *dcalc_ap) const;

private:
  DISALLOW_COPY_AND_ASSIGN(ArcDelayCalc);
};

} // namespace
