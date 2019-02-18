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

#ifndef STA_POWER_H
#define STA_POWER_H

#include "Sta.hh"

namespace sta {

class PowerResult;

// The Power class has access to Sta components directly for
// convenience but also requires access to the Sta class member functions.
class Power : public StaState
{
public:
  Power(Sta *sta);
  void power(const Corner *corner,
	     // Return values.
	     PowerResult &total,
	     PowerResult &sequential,
  	     PowerResult &combinational,
	     PowerResult &macro,
	     PowerResult &pad);
  void power(const Instance *inst,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result);
  float defaultSignalToggleRate();
  void setDefaultSignalToggleRate(float rate);

protected:
  void power(const Instance *inst,
	     LibertyCell *cell,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result);
  void findInternalPower(const Instance *inst,
			 LibertyCell *cell,
			 const LibertyPort *to_port,
			 float activity,
			 float load_cap,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 PowerResult &result);
  void findLeakagePower(const Instance *inst,
			LibertyCell *cell,
			// Return values.
			PowerResult &result);
  void findSwitchingPower(LibertyCell *cell,
			  const LibertyPort *to_port,
			  float activity,
			  float load_cap,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  PowerResult &result);
  void findClk(const Pin *to_pin,
	       // Return values.
	       const Clock *&clk,
	       bool &is_clk);
  float loadCap(const Pin *to_pin,
		const DcalcAnalysisPt *dcalc_ap);;
  void activity(const Pin *pin,
		// Return values.
		float &activity,
		bool &is_clk);
  float voltage(LibertyCell *cell,
		const LibertyPort *port,
		const DcalcAnalysisPt *dcalc_ap);

private:
  Sta *sta_;
  float default_signal_toggle_rate_;
};

class PowerResult
{
public:
  PowerResult();
  void clear();
  float internal() const { return internal_; }
  void setInternal(float internal);
  float switching() const { return switching_; }
  void setSwitching(float switching);
  float leakage() const { return leakage_; }
  void setLeakage(float leakage);
  float total() const;
  void incr(PowerResult &result);
  
private:
  float internal_;
  float switching_;
  float leakage_;
};

} // namespace
#endif
