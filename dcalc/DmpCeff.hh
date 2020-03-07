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

#include "LibertyClass.hh"
#include "RCDelayCalc.hh"

namespace sta {

class DmpAlg;
class DmpCap;
class DmpPi;
class DmpZeroC2;

// Delay calculator using Dartu/Menezes/Pileggi effective capacitance
// algorithm for RSPF loads.
class DmpCeffDelayCalc : public RCDelayCalc
{
public:
  DmpCeffDelayCalc(StaState *sta);
  virtual ~DmpCeffDelayCalc();
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
			 // return values
			 ArcDelay &gate_delay,
			 Slew &drvr_slew);
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
  virtual void copyState(const StaState *sta);

protected:
  void gateDelaySlew(double &delay,
		     double &slew);
  void loadDelaySlew(const Pin *load_pin,
		     double elmore,
		     ArcDelay &delay,
		     Slew &slew);
  // Select the appropriate special case Dartu/Menezes/Pileggi algorithm.
  void setCeffAlgorithm(const LibertyLibrary *library,
			const LibertyCell *cell,
			const Pvt *pvt,
			GateTableModel *gate_model,
			double in_slew,
			float related_out_cap,
			double c2,
			double rpi,
			double c1);

  bool input_port_;
  static bool unsuppored_model_warned_;

private:
  // Dmp algorithms for each special pi model case.
  // These objects are reused to minimize make/deletes.
  DmpCap *dmp_cap_;
  DmpPi *dmp_pi_;
  DmpZeroC2 *dmp_zero_c2_;
  DmpAlg *dmp_alg_;
};

} // namespace
