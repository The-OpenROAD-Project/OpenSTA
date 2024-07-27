// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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
#include <mutex>
#include <array>

#include "Map.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "DcalcAnalysisPt.hh"
#include "StaState.hh"
#include "Delay.hh"
#include "ArcDelayCalc.hh"

namespace sta {

using std::vector;
using std::map;
using std::array;

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

  float loadCap(const Pin *drvr_pin,
                const DcalcAnalysisPt *dcalc_ap) const;
  float loadCap(const Pin *drvr_pin,
                const RiseFall *rf,
                const DcalcAnalysisPt *dcalc_ap) const;
  void loadCap(const Pin *drvr_pin,
               const RiseFall *rf,
               const DcalcAnalysisPt *dcalc_ap,
               // Return values.
               float &pin_cap,
               float &wire_cap) const;
  void netCaps(const Pin *drvr_pin,
               const RiseFall *rf,
               const DcalcAnalysisPt *dcalc_ap,
               // Return values.
               float &pin_cap,
               float &wire_cap,
               float &fanout,
               bool &has_set_load) const;
  void parasiticLoad(const Pin *drvr_pin,
                     const RiseFall *rf,
                     const DcalcAnalysisPt *dcalc_ap,
                     const MultiDrvrNet *multi_drvr,
                     ArcDelayCalc *arc_delay_calc,
                     // Return values.
                     float &cap,
                     const Parasitic *&parasitic) const;
  LoadPinIndexMap makeLoadPinIndexMap(Vertex *drvr_vertex);
  void findDriverArcDelays(Vertex *drvr_vertex,
                           Edge *edge,
                           const TimingArc *arc,
                           const DcalcAnalysisPt *dcalc_ap,
                           ArcDelayCalc *arc_delay_calc);
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
  Slew edgeFromSlew(const Vertex *from_vertex,
		    const RiseFall *from_rf,
                    const TimingRole *role,
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
  MultiDrvrNet *multiDrvrNet(const Vertex *drvr_vertex) const;
  MultiDrvrNet *findMultiDrvrNet(Vertex *drvr_pin);
  MultiDrvrNet *makeMultiDrvrNet(Vertex *drvr_vertex);
  bool hasMultiDrvrs(Vertex *drvr_vertex);
  Vertex *firstLoad(Vertex *drvr_vertex);
  bool findDriverDelays1(Vertex *drvr_vertex,
			 MultiDrvrNet *multi_drvr,
			 ArcDelayCalc *arc_delay_calc);
  void initLoadSlews(Vertex *drvr_vertex);
  bool findDriverEdgeDelays(Vertex *drvr_vertex,
			    const MultiDrvrNet *multi_drvr,
			    Edge *edge,
			    ArcDelayCalc *arc_delay_calc,
                            array<bool, RiseFall::index_count> &delay_exists);
  bool findDriverArcDelays(Vertex *drvr_vertex,
                           const MultiDrvrNet *multi_drvr,
                           Edge *edge,
                           const TimingArc *arc,
                           LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap,
                           ArcDelayCalc *arc_delay_calc);
  ArcDcalcArgSeq makeArcDcalcArgs(Vertex *drvr_vertex,
                                  const MultiDrvrNet *multi_drvr,
                                  Edge *edge,
                                  const TimingArc *arc,
                                  const DcalcAnalysisPt *dcalc_ap,
                                  ArcDelayCalc *arc_delay_calc);
  void findParallelEdge(Vertex *vertex,
                        Edge *drvr_edge,
                        const TimingArc *drvr_arc,
                        // Return values.
                        Edge *&edge,
                        const TimingArc *&arc);
  void initWireDelays(Vertex *drvr_vertex);
  void initRootSlews(Vertex *vertex);
  void zeroSlewAndWireDelays(Vertex *drvr_vertex,
                             const RiseFall *rf);
  void findVertexDelay(Vertex *vertex,
		       ArcDelayCalc *arc_delay_calc,
		       bool propagate);
  void enqueueTimingChecksEdges(Vertex *vertex);
  bool annotateDelaysSlews(Edge *edge,
                           const TimingArc *arc,
                           ArcDcalcResult &dcalc_result,
                           LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap);

  bool annotateDelaySlew(Edge *edge,
                         const TimingArc *arc,
                         ArcDelay &gate_delay,
                         Slew &gate_slew,
                         const DcalcAnalysisPt *dcalc_ap);
  bool annotateLoadDelays(Vertex *drvr_vertex,
                          const RiseFall *drvr_rf,
                          ArcDcalcResult &dcalc_result,
                          LoadPinIndexMap &load_pin_index_map,
                          const ArcDelay &extra_delay,
                          bool merge,
                          const DcalcAnalysisPt *dcalc_ap);
  void findLatchEdgeDelays(Edge *edge);
  void findCheckEdgeDelays(Edge *edge,
			   ArcDelayCalc *arc_delay_calc);
  void deleteMultiDrvrNets();
  Slew checkEdgeClkSlew(const Vertex *from_vertex,
			const RiseFall *from_rf,
			const DcalcAnalysisPt *dcalc_ap);
  bool bidirectDrvrSlewFromLoad(const Vertex *vertex) const;
  float loadCap(const Pin *drvr_pin,
                const RiseFall *rf,
                const DcalcAnalysisPt *dcalc_ap,
                ArcDelayCalc *arc_delay_calc) const;
  void parasiticLoad(const Pin *drvr_pin,
                     const RiseFall *rf,
                     const DcalcAnalysisPt *dcalc_ap,
                     const MultiDrvrNet *multi_drvr,
                     ArcDelayCalc *arc_delay_calc,
                     // Return values.
                     float &pin_cap,
                     float &wire_cap,
                     const Parasitic *&parasitic) const;
  void netCaps(const Pin *drvr_pin,
               const RiseFall *rf,
               const DcalcAnalysisPt *dcalc_ap,
               const MultiDrvrNet *multi_drvr,
               // Return values.
               float &pin_cap,
               float &wire_cap,
               float &fanout,
               bool &has_net_load) const;

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
  std::mutex multi_drvr_lock_;
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
  MultiDrvrNet();
  VertexSeq &drvrs() { return drvrs_; }
  const VertexSeq &drvrs() const { return drvrs_; }
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
  VertexSeq drvrs_;
  // [drvr_rf->index][dcalc_ap->index]
  vector<NetCaps> net_caps_;
};

} // namespace
