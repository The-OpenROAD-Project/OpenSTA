// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <vector>

#include "Map.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "DcalcAnalysisPt.hh"
#include "StaState.hh"
#include "Delay.hh"

namespace sta {

using std::vector;

class DelayCalcObserver;
class MultiDrvrNet;
class FindVertexDelays;
class NetCaps;

typedef Map<const Vertex*, MultiDrvrNet*> MultiDrvrNetMap;

// This class traverses the graph calling the arc delay calculator and
// annotating delays on graph edges.
class GraphDelayCalc : public StaState
{
public:
  GraphDelayCalc(StaState *sta);
  virtual ~GraphDelayCalc();
  virtual void copyState(const StaState *sta);
  // Set the observer for edge delay changes.
  virtual void setObserver(DelayCalcObserver *observer);
  // Invalidate all delays/slews.
  virtual void delaysInvalid();
  // Invalidate vertex and downstream delays/slews.
  virtual void delayInvalid(Vertex *vertex);
  virtual void delayInvalid(const Pin *pin);
  virtual void deleteVertexBefore(Vertex *vertex);
  // Reset to virgin state.
  virtual void clear();
  // Find arc delays and vertex slews thru level.
  virtual void findDelays(Level level);
  // Find and annotate drvr_vertex gate and load delays/slews.
  virtual void findDelays(Vertex *drvr_vertex);
  // Returned string is owned by the caller.
  virtual string reportDelayCalc(const Edge *edge,
                                 const TimingArc *arc,
                                 const Corner *corner,
                                 const MinMax *min_max,
                                 int digits);
  // Percentage (0.0:1.0) change in delay that causes downstream
  // delays to be recomputed during incremental delay calculation.
  virtual float incrementalDelayTolerance();
  virtual void setIncrementalDelayTolerance(float tol);
  // Load pin_cap + wire_cap.
  virtual float loadCap(const Pin *drvr_pin,
			const RiseFall *drvr_rf,
			const DcalcAnalysisPt *dcalc_ap) const;
  // Load pin_cap + wire_cap including parasitic min/max for rise/fall.
  virtual float loadCap(const Pin *drvr_pin,
			const DcalcAnalysisPt *dcalc_ap) const;
  // pin_cap  = net pin capacitances + port external pin capacitance,
  // wire_cap = annotated net capacitance + port external wire capacitance.
  virtual void loadCap(const Pin *drvr_pin,
		       const Parasitic *drvr_parasitic,
		       const RiseFall *rf,
		       const DcalcAnalysisPt *dcalc_ap,
		       // Return values.
		       float &pin_cap,
		       float &wire_cap) const;
  // Load pin_cap + wire_cap including parasitic.
  virtual float loadCap(const Pin *drvr_pin,
			const Parasitic *drvr_parasitic,
			const RiseFall *rf,
			const DcalcAnalysisPt *dcalc_ap) const;
  float loadCap(const Pin *drvr_pin,
		const Parasitic *drvr_parasitic,
		const RiseFall *rf,
		const DcalcAnalysisPt *dcalc_ap,
                const MultiDrvrNet *multi_drvr) const;
  virtual void netCaps(const Pin *drvr_pin,
		       const RiseFall *rf,
		       const DcalcAnalysisPt *dcalc_ap,
		       // Return values.
		       float &pin_cap,
		       float &wire_cap,
		       float &fanout,
		       bool &has_set_load) const;
  float ceff(Edge *edge,
	     TimingArc *arc,
	     const DcalcAnalysisPt *dcalc_ap);
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

  Slew edgeFromSlew(const Vertex *from_vertex,
		    const RiseFall *from_rf,
		    const Edge *edge,
		    const DcalcAnalysisPt *dcalc_ap);

protected:
  void seedInvalidDelays();
  void initSlew(Vertex *vertex);
  void seedRootSlew(Vertex *vertex,
		    ArcDelayCalc *arc_delay_calc);
  void seedRootSlews();
  void seedDrvrSlew(Vertex *vertex,
		    ArcDelayCalc *arc_delay_calc);
  void seedNoDrvrSlew(Vertex *drvr_vertex,
		      const Pin *drvr_pin,
		      const RiseFall *rf,
		      DcalcAnalysisPt *dcalc_ap,
		      ArcDelayCalc *arc_delay_calc);
  void seedNoDrvrCellSlew(Vertex *drvr_vertex,
			  const Pin *drvr_pin,
			  const RiseFall *rf,
			  InputDrive *drive,
			  DcalcAnalysisPt *dcalc_ap,
			  ArcDelayCalc *arc_delay_calc);
  void seedLoadSlew(Vertex *vertex);
  void setInputPortWireDelays(Vertex *vertex);
  void findInputDriverDelay(const LibertyCell *drvr_cell,
			    const Pin *drvr_pin,
			    Vertex *drvr_vertex,
			    const RiseFall *rf,
			    const LibertyPort *from_port,
			    float *from_slews,
			    const LibertyPort *to_port,
			    const DcalcAnalysisPt *dcalc_ap);
  LibertyPort *driveCellDefaultFromPort(const LibertyCell *cell,
					const LibertyPort *to_port);
  int findPortIndex(const LibertyCell *cell,
		    const LibertyPort *port);
  void findInputArcDelay(const Pin *drvr_pin,
			 Vertex *drvr_vertex,
			 const TimingArc *arc,
			 float from_slew,
			 const DcalcAnalysisPt *dcalc_ap);
  bool findDriverDelays(Vertex *drvr_vertex,
			ArcDelayCalc *arc_delay_calc);
  MultiDrvrNet *findMultiDrvrNet(Vertex *drvr_pin);
  MultiDrvrNet *makeMultiDrvrNet(PinSet &drvr_pins);
  bool findDriverDelays1(Vertex *drvr_vertex,
			 MultiDrvrNet *multi_drvr,
			 ArcDelayCalc *arc_delay_calc);
  void initLoadSlews(Vertex *drvr_vertex);
  bool findDriverEdgeDelays(const Instance *drvr_inst,
			    const Pin *drvr_pin,
			    Vertex *drvr_vertex,
			    const MultiDrvrNet *multi_drvr,
			    Edge *edge,
			    ArcDelayCalc *arc_delay_calc);
  void initWireDelays(Vertex *drvr_vertex);
  void initRootSlews(Vertex *vertex);
  void zeroSlewAndWireDelays(Vertex *drvr_vertex);
  void findVertexDelay(Vertex *vertex,
		       ArcDelayCalc *arc_delay_calc,
		       bool propagate);
  void enqueueTimingChecksEdges(Vertex *vertex);
  bool findArcDelay(const Pin *drvr_pin,
		    Vertex *drvr_vertex,
		    const TimingArc *arc,
		    const Parasitic *drvr_parasitic,
		    float related_out_cap,
		    Vertex *from_vertex,
		    Edge *edge,
		    const Pvt *pvt,
		    const DcalcAnalysisPt *dcalc_ap,
		    const MultiDrvrNet *multi_drvr,
		    ArcDelayCalc *arc_delay_calc);
  void annotateLoadDelays(Vertex *drvr_vertex,
			  const RiseFall *drvr_rf,
			  const ArcDelay &extra_delay,
			  bool merge,
			  const DcalcAnalysisPt *dcalc_ap,
			  ArcDelayCalc *arc_delay_calc);
  void findLatchEdgeDelays(Edge *edge);
  void findCheckEdgeDelays(Edge *edge,
			   ArcDelayCalc *arc_delay_calc);
  void deleteMultiDrvrNets();
  Slew checkEdgeClkSlew(const Vertex *from_vertex,
			const RiseFall *from_rf,
			const DcalcAnalysisPt *dcalc_ap);
  bool bidirectDrvrSlewFromLoad(const Vertex *vertex) const;
  MultiDrvrNet *multiDrvrNet(const Vertex *drvr_vertex) const;
  void loadCap(const Parasitic *drvr_parasitic,
	       bool has_set_load,
	       // Return values.
	       float &pin_cap,
	       float &wire_cap) const;

  // Observer for edge delay changes.
  DelayCalcObserver *observer_;
  bool delays_seeded_;
  bool incremental_;
  bool delays_exist_;
  // Vertices with invalid -to delays.
  VertexSet *invalid_delays_;
  // Timing check edges with invalid delays.
  EdgeSet invalid_check_edges_;
  // Latch D->Q edges with invalid delays.
  EdgeSet invalid_latch_edges_;
  // shared by invalid_check_edges_ and invalid_latch_edges_
  std::mutex invalid_edge_lock_;
  SearchPred *search_pred_;
  SearchPred *search_non_latch_pred_;
  SearchPred *clk_pred_;
  BfsFwdIterator *iter_;
  MultiDrvrNetMap multi_drvr_net_map_;
  bool multi_drvr_nets_found_;
  // Percentage (0.0:1.0) change in delay that causes downstream
  // delays to be recomputed during incremental delay calculation.
  float incremental_delay_tolerance_;

  friend class FindVertexDelays;
  friend class MultiDrvrNet;
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
};

// Nets with multiple drivers (tristate, bidirect or output).
// Cache net caps to prevent N^2 net pin walk.
class MultiDrvrNet
{
public:
  MultiDrvrNet(VertexSet *drvrs);
  ~MultiDrvrNet();
  const VertexSet *drvrs() const { return drvrs_; }
  VertexSet *drvrs() { return drvrs_; }
  bool parallelGates(const Network *network) const;
  Vertex *dcalcDrvr() const { return dcalc_drvr_; }
  void setDcalcDrvr(Vertex *drvr);
  void netCaps(const RiseFall *rf,
	       const DcalcAnalysisPt *dcalc_ap,
	       // Return values.
	       float &pin_cap,
	       float &wire_cap,
	       float &fanout,
	       bool &has_net_load) const;
  void findCaps(const Sdc *sdc);

private:
  // Driver that triggers delay calculation for all the drivers on the net.
  Vertex *dcalc_drvr_;
  VertexSet *drvrs_;
  // [drvr_rf->index][dcalc_ap->index]
  vector<NetCaps> net_caps_;
};

} // namespace
