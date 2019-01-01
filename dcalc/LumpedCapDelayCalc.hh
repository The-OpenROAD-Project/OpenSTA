// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#ifndef STA_LUMPED_CAP_DELAY_CALC_H
#define STA_LUMPED_CAP_DELAY_CALC_H

#include "ArcDelayCalc.hh"

namespace sta {

// Liberty table model lumped capacitance arc delay calculator.
// Wire delays are zero.
class LumpedCapDelayCalc : public ArcDelayCalc
{
public:
  LumpedCapDelayCalc(StaState *sta);
  virtual ArcDelayCalc *copy();
  virtual void findParasitic(const Pin *drvr_pin,
			     const TransRiseFall *tr,
			     const DcalcAnalysisPt *dcalc_ap,
			     // Return values.
			     Parasitic *&parasitic,
			     bool &delete_at_finish);
  virtual void finish(const Pin *drvr_pin,
		      const TransRiseFall *tr,
		      const DcalcAnalysisPt *dcalc_ap,
		      Parasitic *parasitic,
		      bool delete_at_finish);
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const TransRiseFall *tr,
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
  virtual void setMultiDrvrSlewFactor(float factor);
  virtual void loadDelay(const Pin *load_pin,
			 // Return values.
			 ArcDelay &wire_delay,
			 Slew &load_slew);
  virtual void checkDelay(const LibertyCell *cell,
			  TimingArc *arc,
			  const Slew &from_slew,
			  const Slew &to_slew,
			  float related_out_cap,
			  const Pvt *pvt,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  ArcDelay &margin);
  virtual float ceff(const LibertyCell *drvr_cell,
		     TimingArc *arc,
		     const Slew &in_slew,
		     float load_cap,
		     Parasitic *drvr_parasitic,
		     float related_out_cap,
		     const Pvt *pvt,
		     const DcalcAnalysisPt *dcalc_ap);
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

protected:
  // Find the liberty library to use for logic/slew thresholds.
  LibertyLibrary *thresholdLibrary(const Pin *load_pin);
  // Adjust load_delay and load_slew from driver thresholds to load thresholds.
  void thresholdAdjust(const Pin *load_pin,
		       ArcDelay &load_delay,
		       Slew &load_slew);

  Slew drvr_slew_;
  float multi_drvr_slew_factor_;
  const LibertyLibrary *drvr_library_;
  const TransRiseFall *drvr_tr_;
};

ArcDelayCalc *
makeLumpedCapDelayCalc(StaState *sta);

} // namespace
#endif
