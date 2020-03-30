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
#include "StaState.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "DcalcAnalysisPt.hh"

namespace sta {

using std::string;

class BfsFwdIterator;
class SearchPred;
class DelayCalcObserver;
class Parasitic;
class Corner;

// Base class for graph delay calculator.
// This class annotates the arc delays and slews on the graph by calling
// the timing arc delay calculation primitive through an implementation
// of the ArcDelayCalc abstract class.
// This class does not traverse the graph or call an arc delay
// calculator.  Use it with applications that use an external delay
// calculator and annotate all edge delays.
class GraphDelayCalc : public StaState
{
public:
  explicit GraphDelayCalc(StaState *sta);
  virtual ~GraphDelayCalc() {}
  virtual void copyState(const StaState *sta);
  // Find arc delays and vertex slews thru level.
  virtual void findDelays(Level /* level */) {};
  // Invalidate all delays/slews.
  virtual void delaysInvalid() {};
  // Invalidate vertex and downstream delays/slews.
  virtual void delayInvalid(Vertex * /* vertex */) {}
;
  virtual void delayInvalid(const Pin * /* pin */) {};
  virtual void deleteVertexBefore(Vertex * /* vertex */) {};
  // Reset to virgin state.
  virtual void clear() {}
  // Returned string is owned by the caller.
  virtual string *reportDelayCalc(Edge *edge,
				  TimingArc *arc,
				  const Corner *corner,
				  const MinMax *min_max,
				  int digits);
  // Percentage (0.0:1.0) change in delay that causes downstream
  // delays to be recomputed during incremental delay calculation.
  virtual float incrementalDelayTolerance();
  virtual void setIncrementalDelayTolerance(float /* tol */) {}
  // Set the observer for edge delay changes.
  virtual void setObserver(DelayCalcObserver *observer);
  // pin_cap  = net pin capacitances + port external pin capacitance,
  // wire_cap = annotated net capacitance + port external wire capacitance.
  virtual void loadCap(const Pin *drvr_pin,
		       Parasitic *drvr_parasitic,
		       const RiseFall *rf,
		       const DcalcAnalysisPt *dcalc_ap,
		       // Return values.
		       float &pin_cap,
		       float &wire_cap) const;
  // Load pin_cap + wire_cap including parasitic.
  virtual float loadCap(const Pin *drvr_pin,
			const RiseFall *to_rf,
			const DcalcAnalysisPt *dcalc_ap) const;
  // Load pin_cap + wire_cap including parasitic min/max for rise/fall.
  virtual float loadCap(const Pin *drvr_pin,
			const DcalcAnalysisPt *dcalc_ap) const;
  // Load pin_cap + wire_cap.
  virtual float loadCap(const Pin *drvr_pin,
			Parasitic *drvr_parasitic,
			const RiseFall *rf,
			const DcalcAnalysisPt *dcalc_ap) const;
  virtual void netCaps(const Pin *drvr_pin,
		       const RiseFall *rf,
		       const DcalcAnalysisPt *dcalc_ap,
		       // Return values.
		       float &pin_cap,
		       float &wire_cap,
		       float &fanout,
		       bool &has_set_load) const;
  virtual float ceff(Edge *edge,
		     TimingArc *arc,
		     const DcalcAnalysisPt *dcalc_ap);
  virtual bool isIdealClk(const Vertex *vertex);
  // Precedence:
  //  SDF annotation
  //  Liberty library
  // (ignores set_min_pulse_width constraint)
  void minPulseWidth(const Pin *pin,
		     const RiseFall *hi_low,
		     DcalcAPIndex ap_index,
		     const MinMax *min_max,
		     // Return values.
		     float &min_width,
		     bool &exists);
  // Precedence:
  //  SDF annotation
  //  Liberty library
  void minPeriod(const Pin *pin,
		 // Return values.
		 float &min_period,
		 bool &exists);

private:
  DISALLOW_COPY_AND_ASSIGN(GraphDelayCalc);
};

// Abstract base class for edge delay change observer.
class DelayCalcObserver
{
public:
  DelayCalcObserver() {}
  virtual ~DelayCalcObserver() {}
  virtual void delayChangedFrom(Vertex *vertex) = 0;
  virtual void delayChangedTo(Vertex *vertex) = 0;
  virtual void checkDelayChangedTo(Vertex *vertex) = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(DelayCalcObserver);
};

} // namespace
