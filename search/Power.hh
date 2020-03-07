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

#include "Sta.hh"

namespace sta {

class PowerResult;
class PwrActivity;
class PropActivityVisitor;
class BfsFwdIterator;

typedef UnorderedMap<const Pin*,PwrActivity> PwrActivityMap;

enum class PwrActivityOrigin
{
 global,
 input,
 user,
 propagated,
 clock,
 constant,
 defaulted,
 unknown
};

class PwrActivity
{
public:
  PwrActivity();
  PwrActivity(float activity,
		float duty,
		PwrActivityOrigin origin);
  float activity() const { return activity_; }
  float duty() const { return duty_; }
  PwrActivityOrigin origin() { return origin_; }
  const char *originName() const;
  void set(float activity,
	   float duty,
	   PwrActivityOrigin origin);
  bool isSet() const;

private:
  // In general activity is per clock cycle, NOT per second.
  float activity_;
  float duty_;
  PwrActivityOrigin origin_;
};

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
  void setGlobalActivity(float activity,
			 float duty);
  void setInputActivity(float activity,
			float duty);
  void setInputPortActivity(const Port *input_port,
			    float activity,
			    float duty);
  PwrActivity &pinActivity(const Pin *pin);
  bool hasPinActivity(const Pin *pin);
  void setPinActivity(const Pin *pin,
		      PwrActivity &activity);
  void setPinActivity(const Pin *pin,
		      float activity,
		      float duty,
		      PwrActivityOrigin origin);
  // Activity is toggles per second.
  PwrActivity findClkedActivity(const Pin *pin);

protected:
  void preamble();
  void ensureActivities();

  void power(const Instance *inst,
	     LibertyCell *cell,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result);
  void findInternalPower(const Pin *to_pin,
			 const LibertyPort *to_port,
			 const Instance *inst,
			 LibertyCell *cell,
			 PwrActivity &to_activity,
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
			  PwrActivity &activity,
			  float load_cap,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  PowerResult &result);
  const Clock *findInstClk(const Instance *inst);
  const Clock *findClk(const Pin *to_pin);
  PwrActivity findClkedActivity(const Pin *pin,
				const Clock *inst_clk);
  PwrActivity findActivity(const Pin *pin);
  float portVoltage(LibertyCell *cell,
		    const LibertyPort *port,
		    const DcalcAnalysisPt *dcalc_ap);
  float pgNameVoltage(LibertyCell *cell,
		      const char *pg_port_name,
		      const DcalcAnalysisPt *dcalc_ap);
  void seedActivities(BfsFwdIterator &bfs);
  void seedRegOutputActivities(const Instance *reg,
			       Sequential *seq,
			       LibertyPort *output,
			       bool invert);
  void seedRegOutputActivities(const Instance *inst,
			       BfsFwdIterator &bfs);
  PwrActivity evalActivity(FuncExpr *expr,
			   const Instance *inst);
  bool internalPowerMissingWhen(LibertyCell *cell,
				const LibertyPort *to_port,
				const char *related_pg_pin);
  FuncExpr *inferedWhen(FuncExpr *expr,
			const LibertyPort *from_port);

private:
  PwrActivity global_activity_;
  PwrActivity input_activity_;
  PwrActivityMap activity_map_;
  bool activities_valid_;

  friend class PropActivityVisitor;
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
