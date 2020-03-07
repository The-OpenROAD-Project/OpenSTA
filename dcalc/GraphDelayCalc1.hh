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

#include <mutex>
#include "GraphDelayCalc.hh"

namespace sta {

class MultiDrvrNet;
class FindVertexDelays;
class Corner;

typedef Map<const Vertex*, MultiDrvrNet*> MultiDrvrNetMap;
typedef Map<const Vertex*, ClockSet*> VertexIdealClksMap;

// This class traverses the graph calling the arc delay calculator and
// annotating delays on graph edges.
class GraphDelayCalc1 : public GraphDelayCalc
{
public:
  GraphDelayCalc1(StaState *sta);
  virtual ~GraphDelayCalc1();
  virtual void copyState(const StaState *sta);
  virtual void delaysInvalid();
  virtual void delayInvalid(Vertex *vertex);
  virtual void delayInvalid(const Pin *pin);
  virtual void deleteVertexBefore(Vertex *vertex);
  virtual void clear();
  virtual void findDelays(Level level);
  virtual string *reportDelayCalc(Edge *edge,
				  TimingArc *arc,
				  const Corner *corner,
				  const MinMax *min_max,
				  int digits);
  virtual float incrementalDelayTolerance();
  virtual void setIncrementalDelayTolerance(float tol);
  virtual void setObserver(DelayCalcObserver *observer);
  // Load pin_cap + wire_cap.
  virtual float loadCap(const Pin *drvr_pin,
			const RiseFall *drvr_rf,
			const DcalcAnalysisPt *dcalc_ap) const;
  virtual float loadCap(const Pin *drvr_pin,
			const DcalcAnalysisPt *dcalc_ap) const;
  virtual void loadCap(const Pin *drvr_pin,
		       Parasitic *drvr_parasitic,
		       const RiseFall *rf,
		       const DcalcAnalysisPt *dcalc_ap,
		       // Return values.
		       float &pin_cap,
		       float &wire_cap) const;
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
  float ceff(Edge *edge,
	     TimingArc *arc,
	     const DcalcAnalysisPt *dcalc_ap);

protected:
  void seedInvalidDelays();
  void ensureMultiDrvrNetsFound();
  void makeMultiDrvrNet(PinSet &drvr_pins);
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
  void findInputDriverDelay(LibertyCell *drvr_cell,
			    const Pin *drvr_pin,
			    Vertex *drvr_vertex,
			    const RiseFall *rf,
			    LibertyPort *from_port,
			    float *from_slews,
			    LibertyPort *to_port,
			    DcalcAnalysisPt *dcalc_ap);
  LibertyPort *driveCellDefaultFromPort(LibertyCell *cell,
					LibertyPort *to_port);
  int findPortIndex(LibertyCell *cell,
		    LibertyPort *port);
  void findInputArcDelay(LibertyCell *drvr_cell,
			 const Pin *drvr_pin,
			 Vertex *drvr_vertex,
			 TimingArc *arc,
			 float from_slew,
			 DcalcAnalysisPt *dcalc_ap);
  bool findDriverDelays(Vertex *drvr_vertex,
			ArcDelayCalc *arc_delay_calc);
  bool findDriverDelays1(Vertex *drvr_vertex,
			 bool init_load_slews,
			 MultiDrvrNet *multi_drvr,
			 ArcDelayCalc *arc_delay_calc);
  bool findDriverEdgeDelays(LibertyCell *drvr_cell,
			    Instance *drvr_inst,
			    const Pin *drvr_pin,
			    Vertex *drvr_vertex,
			    MultiDrvrNet *multi_drvr,
			    Edge *edge,
			    ArcDelayCalc *arc_delay_calc);
  void initWireDelays(Vertex *drvr_vertex,
		      bool init_load_slews);
  void initRootSlews(Vertex *vertex);
  void findVertexDelay(Vertex *vertex,
		       ArcDelayCalc *arc_delay_calc,
		       bool propagate);
  void enqueueTimingChecksEdges(Vertex *vertex);
  bool findIdealClks(Vertex *vertex);
  bool findArcDelay(LibertyCell *drvr_cell,
		    const Pin *drvr_pin,
		    Vertex *drvr_vertex,
		    MultiDrvrNet *multi_drvr,
		    TimingArc *arc,
		    Parasitic *drvr_parasitic,
		    float related_out_cap,
		    Vertex *from_vertex,
		    Edge *edge,
		    const Pvt *pvt,
		    const DcalcAnalysisPt *dcalc_ap,
		    ArcDelayCalc *arc_delay_calc);
  void annotateLoadDelays(Vertex *drvr_vertex,
			  const RiseFall *drvr_rf,
			  const ArcDelay &extra_delay,
			  bool merge,
			  const DcalcAnalysisPt *dcalc_ap,
			  ArcDelayCalc *arc_delay_calc);
  void findCheckDelays(Vertex *vertex,
		       ArcDelayCalc *arc_delay_calc);
  void findCheckEdgeDelays(Edge *edge,
			   ArcDelayCalc *arc_delay_calc);
  void findMultiDrvrGateDelay(MultiDrvrNet *multi_drvr,
			      const RiseFall *drvr_rf,
			      const Pvt *pvt,
			      const DcalcAnalysisPt *dcalc_ap,
			      ArcDelayCalc *arc_delay_calc,
			      // Return values.
			      ArcDelay &parallel_delay,
			      Slew &parallel_slew);
  void multiDrvrGateDelay(MultiDrvrNet *multi_drvr,
			  LibertyCell *drvr_cell,
			  const Pin *drvr_pin,
			  TimingArc *arc,
			  const Pvt *pvt,
			  const DcalcAnalysisPt *dcalc_ap,
			  const Slew from_slew,
			  Parasitic *drvr_parasitic,
			  float related_out_cap,
			  ArcDelayCalc *arc_delay_calc,
			  // Return values.
			  ArcDelay &gate_delay,
			  Slew &gate_slew);
  void deleteMultiDrvrNets();
  Slew edgeFromSlew(const Vertex *from_vertex,
		    const RiseFall *from_rf,
		    const Edge *edge,
		    const DcalcAnalysisPt *dcalc_ap);
  Slew checkEdgeClkSlew(const Vertex *from_vertex,
			const RiseFall *from_rf,
			const DcalcAnalysisPt *dcalc_ap);
  bool bidirectDrvrSlewFromLoad(const Vertex *vertex) const;
  void clearIdealClkMap();
  bool setIdealClks(const Vertex *vertex,
		    ClockSet *clks);
  ClockSet *idealClks(const Vertex *vertex);
  virtual bool isIdealClk(const Vertex *vertex);
  Slew idealClkSlew(const Vertex *vertex,
		    const RiseFall *rf,
		    const MinMax *min_max);
  MultiDrvrNet *multiDrvrNet(const Vertex *drvr_vertex) const;
  void loadCap(Parasitic *drvr_parasitic,
	       bool has_set_load,
	       // Return values.
	       float &pin_cap,
	       float &wire_cap) const;
  float loadCap(const Pin *drvr_pin,
		MultiDrvrNet *multi_drvr,
		Parasitic *drvr_parasitic,
		const RiseFall *rf,
		const DcalcAnalysisPt *dcalc_ap) const;
  void mergeIdealClks();

  // Observer for edge delay changes.
  DelayCalcObserver *observer_;
  bool delays_seeded_;
  bool incremental_;
  bool delays_exist_;
  // Vertices with invalid -to delays.
  VertexSet invalid_delays_;
  // Vertices with invalid -from/-to timing checks.
  VertexSet invalid_checks_;
  std::mutex check_vertices_lock_;
  SearchPred *search_pred_;
  SearchPred *search_non_latch_pred_;
  SearchPred *clk_pred_;
  BfsFwdIterator *iter_;
  MultiDrvrNetMap multi_drvr_net_map_;
  bool multi_drvr_nets_found_;
  // Percentage (0.0:1.0) change in delay that causes downstream
  // delays to be recomputed during incremental delay calculation.
  float incremental_delay_tolerance_;
  VertexIdealClksMap ideal_clks_map_;
  VertexIdealClksMap ideal_clks_map_next_;
  std::mutex ideal_clks_map_next_lock_;

  friend class FindVertexDelays;
  friend class MultiDrvrNet;
};

} // namespace
