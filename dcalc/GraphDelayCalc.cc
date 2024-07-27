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

#include "GraphDelayCalc.hh"

#include "Debug.hh"
#include "Stats.hh"
#include "MinMax.hh"
#include "Mutex.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "InputDrive.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "Parasitics.hh"
#include "search/Levelize.hh"
#include "Corner.hh"
#include "SearchPred.hh"
#include "Bfs.hh"
#include "ArcDelayCalc.hh"
#include "DcalcAnalysisPt.hh"
#include "NetCaps.hh"
#include "ClkNetwork.hh"

namespace sta {

using std::abs;

static const Slew default_slew = 0.0;

static bool
isLeafDriver(const Pin *pin,
	     const Network *network);

GraphDelayCalc::GraphDelayCalc(StaState *sta) :
  StaState(sta),
  observer_(nullptr),
  delays_seeded_(false),
  incremental_(false),
  delays_exist_(false),
  invalid_delays_(new VertexSet(graph_)),
  search_pred_(new SearchPred1(sta)),
  search_non_latch_pred_(new SearchPredNonLatch2(sta)),
  clk_pred_(new ClkTreeSearchPred(sta)),
  iter_(new BfsFwdIterator(BfsIndex::dcalc, search_non_latch_pred_, sta)),
  incremental_delay_tolerance_(0.0)
{
}

GraphDelayCalc::~GraphDelayCalc()
{
  delete search_pred_;
  delete invalid_delays_;
  delete search_non_latch_pred_;
  delete clk_pred_;
  delete iter_;
  deleteMultiDrvrNets();
  delete observer_;
}

void
GraphDelayCalc::deleteMultiDrvrNets()
{
  Set<MultiDrvrNet*> drvr_nets;
  MultiDrvrNetMap::Iterator multi_iter(multi_drvr_net_map_);
  while (multi_iter.hasNext()) {
    MultiDrvrNet *multi_drvr = multi_iter.next();
    // Multiple drvr pins point to the same drvr PinSet,
    // so collect them into a set.
    drvr_nets.insert(multi_drvr);
  }
  multi_drvr_net_map_.clear();
  drvr_nets.deleteContents();
}

void
GraphDelayCalc::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  // Notify sub-components.
  iter_->copyState(sta);
}

void
GraphDelayCalc::clear()
{
  delaysInvalid();
  deleteMultiDrvrNets();
}

float
GraphDelayCalc::incrementalDelayTolerance()
{
  return incremental_delay_tolerance_;
}

void
GraphDelayCalc::setIncrementalDelayTolerance(float tol)
{
  incremental_delay_tolerance_ = tol;
}

void
GraphDelayCalc::setObserver(DelayCalcObserver *observer)
{
  delete observer_;
  observer_ = observer;
}

void
GraphDelayCalc::delaysInvalid()
{
  debugPrint(debug_, "delay_calc", 1, "delays invalid");
  delays_exist_ = false;
  delays_seeded_ = false;
  incremental_ = false;
  iter_->clear();
  // No need to keep track of incremental updates any more.
  invalid_delays_->clear();
  invalid_check_edges_.clear();
  invalid_latch_edges_.clear();
}

void
GraphDelayCalc::delayInvalid(const Pin *pin)
{
  if (graph_ && incremental_) {
    if (network_->isHierarchical(pin)) {
      EdgesThruHierPinIterator edge_iter(pin, network_, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	delayInvalid(edge->from(graph_));
      }
    }
    else {
      Vertex *vertex, *bidirect_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
      if (vertex)
	delayInvalid(vertex);
      if (bidirect_drvr_vertex)
	delayInvalid(bidirect_drvr_vertex);
    }
  }
}

void
GraphDelayCalc::delayInvalid(Vertex *vertex)
{
  debugPrint(debug_, "delay_calc", 2, "delay invalid %s",
             vertex->name(sdc_network_));
  if (graph_ && incremental_) {
    invalid_delays_->insert(vertex);
    // Invalidate driver that triggers dcalc for multi-driver nets.
    MultiDrvrNet *multi_drvr = multiDrvrNet(vertex);
    if (multi_drvr)
      invalid_delays_->insert(multi_drvr->dcalcDrvr());
  }
}

void
GraphDelayCalc::deleteVertexBefore(Vertex *vertex)
{
  iter_->deleteVertexBefore(vertex);
  if (incremental_)
    invalid_delays_->erase(vertex);
  MultiDrvrNet *multi_drvr = multiDrvrNet(vertex);
  if (multi_drvr) {
    // Don't bother incrementally updating MultiDrvrNet.
    for (Vertex *drvr_vertex : multi_drvr->drvrs())
      multi_drvr_net_map_.erase(drvr_vertex);
    delete multi_drvr;
  }
}

////////////////////////////////////////////////////////////////

class FindVertexDelays : public VertexVisitor
{
public:
  FindVertexDelays(GraphDelayCalc *graph_delay_calc1);
  virtual ~FindVertexDelays();
  virtual void visit(Vertex *vertex);
  virtual VertexVisitor *copy() const;

protected:
  GraphDelayCalc *graph_delay_calc1_;
  ArcDelayCalc *arc_delay_calc_;
};

FindVertexDelays::FindVertexDelays(GraphDelayCalc *graph_delay_calc1) :
  VertexVisitor(),
  graph_delay_calc1_(graph_delay_calc1),
  arc_delay_calc_(graph_delay_calc1_->arc_delay_calc_->copy())
{
}

FindVertexDelays::~FindVertexDelays()
{
  delete arc_delay_calc_;
}

VertexVisitor *
FindVertexDelays::copy() const
{
  // Copy StaState::arc_delay_calc_ because it needs separate state
  // for each thread.
  return new FindVertexDelays(graph_delay_calc1_);
}

void
FindVertexDelays::visit(Vertex *vertex)
{
  graph_delay_calc1_->findVertexDelay(vertex, arc_delay_calc_, true);
}

// The logical structure of incremental delay calculation closely
// resembles the incremental search arrival time algorithm
// (Search::findArrivals).
void
GraphDelayCalc::findDelays(Level level)
{
  if (arc_delay_calc_) {
    Stats stats(debug_, report_);
    int dcalc_count = 0;
    debugPrint(debug_, "delay_calc", 1, "find delays to level %d", level);
    if (!delays_seeded_) {
      iter_->clear();
      seedRootSlews();
      delays_seeded_ = true;
    }
    else
      iter_->ensureSize();
    if (incremental_)
      seedInvalidDelays();

    FindVertexDelays visitor(this);
    dcalc_count += iter_->visitParallel(level, &visitor);

    // Timing checks require slews at both ends of the arc,
    // so find their delays after all slews are known.
    for (Edge *check_edge : invalid_check_edges_)
      findCheckEdgeDelays(check_edge, arc_delay_calc_);
    invalid_check_edges_.clear();

    for (Edge *latch_edge : invalid_latch_edges_)
      findLatchEdgeDelays(latch_edge);
    invalid_latch_edges_.clear();

    delays_exist_ = true;
    incremental_ = true;
    debugPrint(debug_, "delay_calc", 1, "found %d delays", dcalc_count);
    stats.report("Delay calc");
  }
}

void
GraphDelayCalc::seedInvalidDelays()
{
  for (Vertex *vertex : *invalid_delays_) {
    if (vertex->isRoot())
      seedRootSlew(vertex, arc_delay_calc_);
    else {
      if (search_non_latch_pred_->searchFrom(vertex))
	iter_->enqueue(vertex);
    }
  }
  invalid_delays_->clear();
}

void
GraphDelayCalc::seedRootSlews()
{
  for (Vertex *vertex : *levelize_->roots())
    seedRootSlew(vertex, arc_delay_calc_);
}

void
GraphDelayCalc::seedRootSlew(Vertex *vertex,
                             ArcDelayCalc *arc_delay_calc)
{
  if (vertex->isDriver(network_))
    seedDrvrSlew(vertex, arc_delay_calc);
  else
    seedLoadSlew(vertex);
  iter_->enqueueAdjacentVertices(vertex);
}

void
GraphDelayCalc::seedDrvrSlew(Vertex *drvr_vertex,
                             ArcDelayCalc *arc_delay_calc)
{
  const Pin *drvr_pin = drvr_vertex->pin();
  debugPrint(debug_, "delay_calc", 2, "seed driver slew %s",
             drvr_vertex->name(sdc_network_));
  InputDrive *drive = 0;
  if (network_->isTopLevelPort(drvr_pin)) {
    Port *port = network_->port(drvr_pin);
    drive = sdc_->findInputDrive(port);
  }
  for (auto rf : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      if (drive) {
	const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
	const LibertyCell *drvr_cell;
	const LibertyPort *from_port, *to_port;
	float *from_slews;
	drive->driveCell(rf, cnst_min_max, drvr_cell, from_port,
			 from_slews, to_port);
	if (drvr_cell) {
	  if (from_port == nullptr)
	    from_port = driveCellDefaultFromPort(drvr_cell, to_port);
	  findInputDriverDelay(drvr_cell, drvr_pin, drvr_vertex, rf,
			       from_port, from_slews, to_port, dcalc_ap);
	}
	else
	  seedNoDrvrCellSlew(drvr_vertex, drvr_pin, rf, drive, dcalc_ap,
			     arc_delay_calc);
      }
      else
	seedNoDrvrSlew(drvr_vertex, drvr_pin, rf, dcalc_ap, arc_delay_calc);
    }
  }
}

void
GraphDelayCalc::seedNoDrvrCellSlew(Vertex *drvr_vertex,
                                   const Pin *drvr_pin,
                                   const RiseFall *rf,
                                   InputDrive *drive,
                                   DcalcAnalysisPt *dcalc_ap,
                                   ArcDelayCalc *arc_delay_calc)
{
  DcalcAPIndex ap_index = dcalc_ap->index();
  const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
  Slew slew = default_slew;
  float drive_slew;
  bool exists;
  drive->slew(rf, cnst_min_max, drive_slew, exists);
  if (exists)
    slew = drive_slew;
  else {
    // Top level bidirect driver uses load slew unless
    // bidirect instance paths are disabled.
    if (sdc_->bidirectDrvrSlewFromLoad(drvr_pin)) {
      Vertex *load_vertex = graph_->pinLoadVertex(drvr_pin);
      slew = graph_->slew(load_vertex, rf, ap_index);
    }
  }
  Delay drive_delay = delay_zero;
  float drive_res;
  drive->driveResistance(rf, cnst_min_max, drive_res, exists);
  const Parasitic *parasitic;
  float load_cap;
  parasiticLoad(drvr_pin, rf, dcalc_ap, nullptr, arc_delay_calc,
                load_cap, parasitic);
  if (exists) {
    drive_delay = load_cap * drive_res;
    slew = load_cap * drive_res;
  }
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  if (!drvr_vertex->slewAnnotated(rf, slew_min_max))
    graph_->setSlew(drvr_vertex, rf, ap_index, slew);
  LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(drvr_vertex);
  ArcDcalcResult dcalc_result =
    arc_delay_calc->inputPortDelay(drvr_pin, delayAsFloat(slew), rf, parasitic,
                                   load_pin_index_map, dcalc_ap);
  annotateLoadDelays(drvr_vertex, rf, dcalc_result, load_pin_index_map, 
                     drive_delay, false, dcalc_ap);
  arc_delay_calc->finishDrvrPin();
}

void
GraphDelayCalc::seedNoDrvrSlew(Vertex *drvr_vertex,
                               const Pin *drvr_pin,
                               const RiseFall *rf,
                               DcalcAnalysisPt *dcalc_ap,
                               ArcDelayCalc *arc_delay_calc)
{
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  DcalcAPIndex ap_index = dcalc_ap->index();
  Slew slew(default_slew);
  // Top level bidirect driver uses load slew unless
  // bidirect instance paths are disabled.
  if (sdc_->bidirectDrvrSlewFromLoad(drvr_pin)) {
    Vertex *load_vertex = graph_->pinLoadVertex(drvr_pin);
    slew = graph_->slew(load_vertex, rf, ap_index);
  }
  if (!drvr_vertex->slewAnnotated(rf, slew_min_max))
    graph_->setSlew(drvr_vertex, rf, ap_index, slew);
  Parasitic *parasitic = arc_delay_calc->findParasitic(drvr_pin, rf, dcalc_ap);
  LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(drvr_vertex);
  ArcDcalcResult dcalc_result =
    arc_delay_calc->inputPortDelay(drvr_pin, delayAsFloat(slew), rf, parasitic,
                                   load_pin_index_map, dcalc_ap);
  annotateLoadDelays(drvr_vertex, rf, dcalc_result, load_pin_index_map, delay_zero,
                     false, dcalc_ap);
  arc_delay_calc_->finishDrvrPin();
}

void
GraphDelayCalc::seedLoadSlew(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  debugPrint(debug_, "delay_calc", 2, "seed load slew %s",
             vertex->name(sdc_network_));
  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  initSlew(vertex);
  for (auto rf : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      const MinMax *slew_min_max = dcalc_ap->slewMinMax();
      if (!vertex->slewAnnotated(rf, slew_min_max)) {
	float slew = 0.0;
	if (clks) {
	  slew = slew_min_max->initValue();
	  ClockSet::Iterator clk_iter(clks);
	  while (clk_iter.hasNext()) {
	    Clock *clk = clk_iter.next();
	    float clk_slew = clk->slew(rf, slew_min_max);
	    if (slew_min_max->compare(clk_slew, slew))
	      slew = clk_slew;
	  }
	}
	DcalcAPIndex ap_index = dcalc_ap->index();
	graph_->setSlew(vertex, rf, ap_index, slew);
      }
    }
  }
}

// If a driving cell does not specify a -from_pin, the first port
// defined in the cell that has a timing group to the output port
// is used.  Not exactly reasonable, but it's compatible.
LibertyPort *
GraphDelayCalc::driveCellDefaultFromPort(const LibertyCell *cell,
                                         const LibertyPort *to_port)
{
  LibertyPort *from_port = 0;
  int from_port_index = 0;
  for (TimingArcSet *arc_set : cell->timingArcSets(nullptr, to_port)) {
    LibertyPort *set_from_port = arc_set->from();
    int set_from_port_index = findPortIndex(cell, set_from_port);
    if (from_port == nullptr
        || set_from_port_index < from_port_index) {
      from_port = set_from_port;
      from_port_index = set_from_port_index;
    }
  }
  return from_port;
}

// Find the index that port is defined in cell.
int
GraphDelayCalc::findPortIndex(const LibertyCell *cell,
                              const LibertyPort *port)
{
  int index = 0;
  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    LibertyPort *cell_port = port_iter.next();
    if (cell_port == port)
      return index;
    index++;
  }
  report_->critical(1100, "port not found in cell.");
  return 0;
}

void
GraphDelayCalc::findInputDriverDelay(const LibertyCell *drvr_cell,
                                     const Pin *drvr_pin,
                                     Vertex *drvr_vertex,
                                     const RiseFall *rf,
                                     const LibertyPort *from_port,
                                     float *from_slews,
                                     const LibertyPort *to_port,
                                     const DcalcAnalysisPt *dcalc_ap)
{
  debugPrint(debug_, "delay_calc", 2, "  driver cell %s %s",
             drvr_cell->name(),
             rf->asString());
  for (TimingArcSet *arc_set : drvr_cell->timingArcSets(from_port, to_port)) {
    for (TimingArc *arc : arc_set->arcs()) {
      if (arc->toEdge()->asRiseFall() == rf) {
        float from_slew = from_slews[arc->fromEdge()->index()];
        findInputArcDelay(drvr_pin, drvr_vertex, arc, from_slew, dcalc_ap);
      }
    }
  }
  arc_delay_calc_->finishDrvrPin();
}

// Driving cell delay is the load dependent delay, which is the gate
// delay minus the intrinsic delay.  Driving cell delays are annotated
// to the wire arcs from the input port pin to the load pins.
void
GraphDelayCalc::findInputArcDelay(const Pin *drvr_pin,
                                  Vertex *drvr_vertex,
                                  const TimingArc *arc,
                                  float from_slew,
                                  const DcalcAnalysisPt *dcalc_ap)
{
  debugPrint(debug_, "delay_calc", 3, "  %s %s -> %s %s (%s)",
             arc->from()->name(),
             arc->fromEdge()->asString(),
             arc->to()->name(),
             arc->toEdge()->asString(),
             arc->role()->asString());
  const RiseFall *drvr_rf = arc->toEdge()->asRiseFall();
  if (drvr_rf) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const Parasitic *parasitic;
    float load_cap;
    parasiticLoad(drvr_pin, drvr_rf, dcalc_ap, nullptr, arc_delay_calc_,
                  load_cap, parasitic);

    LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(drvr_vertex);
    ArcDcalcResult intrinsic_result =
      arc_delay_calc_->gateDelay(drvr_pin, arc, Slew(from_slew), 0.0, nullptr,
                                 load_pin_index_map, dcalc_ap);
    ArcDelay intrinsic_delay = intrinsic_result.gateDelay();

    ArcDcalcResult gate_result = arc_delay_calc_->gateDelay(drvr_pin, arc,
                                                            Slew(from_slew), load_cap,
                                                            parasitic, 
                                                            load_pin_index_map,
                                                            dcalc_ap);
    ArcDelay gate_delay = gate_result.gateDelay();
    Slew gate_slew = gate_result.drvrSlew();

    ArcDelay load_delay = gate_delay - intrinsic_delay;
    debugPrint(debug_, "delay_calc", 3,
               "    gate delay = %s intrinsic = %s slew = %s",
               delayAsString(gate_delay, this),
               delayAsString(intrinsic_delay, this),
               delayAsString(gate_slew, this));
    graph_->setSlew(drvr_vertex, drvr_rf, ap_index, gate_slew);
    annotateLoadDelays(drvr_vertex, drvr_rf, gate_result, load_pin_index_map, 
                       load_delay, false, dcalc_ap);
    arc_delay_calc_->finishDrvrPin();
  }
}

void
GraphDelayCalc::findDelays(Vertex *drvr_vertex)
{
  findVertexDelay(drvr_vertex, arc_delay_calc_, true);
}

void
GraphDelayCalc::findVertexDelay(Vertex *vertex,
                                ArcDelayCalc *arc_delay_calc,
                                bool propagate)
{
  const Pin *pin = vertex->pin();
  debugPrint(debug_, "delay_calc", 2, "find delays %s (%s)",
             vertex->name(sdc_network_),
             network_->cellName(network_->instance(pin)));
  if (vertex->isRoot()) {
    seedRootSlew(vertex, arc_delay_calc);
    if (propagate)
      iter_->enqueueAdjacentVertices(vertex);
  }
  else {
    if (network_->isLeaf(pin)) {
      if (vertex->isDriver(network_)) {
	bool delay_changed = findDriverDelays(vertex, arc_delay_calc);
	if (propagate) {
	  if (network_->direction(pin)->isInternal())
	    enqueueTimingChecksEdges(vertex);
	  // Enqueue adjacent vertices even if the delays did not
	  // change when non-incremental to stride past annotations.
	  if (delay_changed || !incremental_)
	    iter_->enqueueAdjacentVertices(vertex);
	}
      }
      else {
	// Load vertex.
	enqueueTimingChecksEdges(vertex);
	// Enqueue driver vertices from this input load.
	if (propagate)
	  iter_->enqueueAdjacentVertices(vertex);
      }
    }
    // Bidirect port drivers are enqueued by their load vertex in
    // annotateLoadDelays.
    else if (vertex->isBidirectDriver()
	     && network_->isTopLevelPort(pin))
      seedRootSlew(vertex, arc_delay_calc);
  }
}

void
GraphDelayCalc::enqueueTimingChecksEdges(Vertex *vertex)
{
  if (vertex->hasChecks()) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    LockGuard lock(invalid_edge_lock_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->isTimingCheck())
        invalid_check_edges_.insert(edge);
    }
  }
  if (vertex->isCheckClk()) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    LockGuard lock(invalid_edge_lock_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->isTimingCheck())
        invalid_check_edges_.insert(edge);
    }
  }
  if (network_->isLatchData(vertex->pin())) {
    // Latch D->Q arcs have to be re-evaled if level(D) > level(E)
    // because levelization does not traverse D->Q arcs to break loops.
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    LockGuard lock(invalid_edge_lock_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role() == TimingRole::latchDtoQ())
        invalid_latch_edges_.insert(edge);
    }
  }
}

bool
GraphDelayCalc::findDriverDelays(Vertex *drvr_vertex,
                                 ArcDelayCalc *arc_delay_calc)
{
  bool delay_changed = false;
  MultiDrvrNet *multi_drvr = findMultiDrvrNet(drvr_vertex);
  if (multi_drvr == nullptr
      || (multi_drvr
          && (!multi_drvr->parallelGates(network_)
              || drvr_vertex == multi_drvr->dcalcDrvr()))) {
    initLoadSlews(drvr_vertex);
    delay_changed |= findDriverDelays1(drvr_vertex, multi_drvr, arc_delay_calc);
  }
  arc_delay_calc_->finishDrvrPin();
  return delay_changed;
}

MultiDrvrNet *
GraphDelayCalc::findMultiDrvrNet(Vertex *drvr_vertex)
{
  // Avoid locking for single driver nets.
  if (hasMultiDrvrs(drvr_vertex)) {
    LockGuard lock(multi_drvr_lock_);
    MultiDrvrNet *multi_drvr = multiDrvrNet(drvr_vertex);
    if (multi_drvr)
      return multi_drvr;
    multi_drvr = makeMultiDrvrNet(drvr_vertex);
    return multi_drvr;
  }
  return nullptr;
}

bool
GraphDelayCalc::hasMultiDrvrs(Vertex *drvr_vertex)
{
  Vertex *load_vertex = firstLoad(drvr_vertex);
  if (load_vertex) {
    int drvr_count = 0;
    VertexInEdgeIterator edge_iter(load_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()) {
        Vertex *drvr = edge->from(graph_);
        if (isLeafDriver(drvr->pin(), network_))
          drvr_count++;
      }
      if (drvr_count > 1)
        return true;
    }
    return false;
  }
  return false;
}

Vertex *
GraphDelayCalc::firstLoad(Vertex *drvr_vertex)
{
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire())
      return wire_edge->to(graph_);
  }
  return nullptr;
}

static bool
isLeafDriver(const Pin *pin,
	     const Network *network)
{
  PortDirection *dir = network->direction(pin);
  const Instance *inst = network->instance(pin);
  return network->isLeaf(inst) && dir->isAnyOutput();
}

MultiDrvrNet *
GraphDelayCalc::multiDrvrNet(const Vertex *drvr_vertex) const
{
  return multi_drvr_net_map_.findKey(drvr_vertex);
}

MultiDrvrNet *
GraphDelayCalc::makeMultiDrvrNet(Vertex *drvr_vertex)
{
  Vertex *load_vertex = firstLoad(drvr_vertex);
  if (load_vertex) {
    debugPrint(debug_, "delay_calc", 3, "multi-driver net");
    MultiDrvrNet *multi_drvr = new MultiDrvrNet;
    VertexSeq &drvr_vertices = multi_drvr->drvrs();
    Level max_drvr_level = 0;
    Vertex *max_drvr = nullptr;
    VertexInEdgeIterator edge_iter(load_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()) {
        Vertex *drvr = edge->from(graph_);
        const Pin *drvr_pin = drvr->pin();
        if (isLeafDriver(drvr_pin, network_)) {
          debugPrint(debug_, "delay_calc", 3, " %s",
                     network_->pathName(drvr_pin));
          multi_drvr_net_map_[drvr] = multi_drvr;
          drvr_vertices.push_back(drvr);
          Level drvr_level = drvr->level();
          if (max_drvr == nullptr
              || drvr_level > max_drvr_level) {
            max_drvr = drvr;
            max_drvr_level = drvr_level;
          }
        }
      }
    }
    multi_drvr->setDcalcDrvr(max_drvr);
    multi_drvr->findCaps(sdc_);
    return multi_drvr;
  }
  report_->critical(1101, "mult_drvr missing load.");
  return nullptr;
}

void
GraphDelayCalc::initLoadSlews(Vertex *drvr_vertex)
{
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
        const MinMax *slew_min_max = dcalc_ap->slewMinMax();
        Slew slew_init_value(slew_min_max->initValue());
        DcalcAPIndex ap_index = dcalc_ap->index();
        for (auto rf : RiseFall::range()) {
          if (!load_vertex->slewAnnotated(rf, slew_min_max))
            graph_->setSlew(load_vertex, rf, ap_index, slew_init_value);
        }
      }
    }
  }
}

bool
GraphDelayCalc::findDriverDelays1(Vertex *drvr_vertex,
                                  MultiDrvrNet *multi_drvr,
                                  ArcDelayCalc *arc_delay_calc)
{
  initSlew(drvr_vertex);
  if (multi_drvr
      && multi_drvr->parallelGates(network_)) {
    // Only init on the trigger driver.
    if (drvr_vertex == multi_drvr->dcalcDrvr()) {
      for (auto vertex : multi_drvr->drvrs())
        initWireDelays(vertex);
    }
  }
  else
    initWireDelays(drvr_vertex);
  bool delay_changed = false;
  array<bool, RiseFall::index_count> delay_exists = {false, false};
  VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    // Don't let disabled edges set slews that influence downstream delays.
    if (search_pred_->searchFrom(from_vertex)
	&& search_pred_->searchThru(edge)
        && !edge->role()->isLatchDtoQ())
      delay_changed |= findDriverEdgeDelays(drvr_vertex, multi_drvr, edge,
                                            arc_delay_calc, delay_exists);
  }
  for (auto rf : RiseFall::range()) {
    if (!delay_exists[rf->index()])
      zeroSlewAndWireDelays(drvr_vertex, rf);
  }
  if (delay_changed && observer_)
    observer_->delayChangedTo(drvr_vertex);
  return delay_changed;
}

// Init slews to zero on root vertices that are not inputs, such as
// floating input pins.
void
GraphDelayCalc::initRootSlews(Vertex *vertex)
{
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    const MinMax *slew_min_max = dcalc_ap->slewMinMax();
    DcalcAPIndex ap_index = dcalc_ap->index();
    for (auto rf : RiseFall::range()) {
      if (!vertex->slewAnnotated(rf, slew_min_max))
	graph_->setSlew(vertex, rf, ap_index, default_slew);
    }
  }
}

void
GraphDelayCalc::findLatchEdgeDelays(Edge *edge)
{
  Vertex *drvr_vertex = edge->to(graph_);
  const Pin *drvr_pin = drvr_vertex->pin();
  Instance *drvr_inst = network_->instance(drvr_pin);
  debugPrint(debug_, "delay_calc", 2, "find latch D->Q %s",
             sdc_network_->pathName(drvr_inst));
  array<bool, RiseFall::index_count> delay_exists = {false, false};
  bool delay_changed = findDriverEdgeDelays(drvr_vertex, nullptr, edge,
                                            arc_delay_calc_, delay_exists);
  if (delay_changed && observer_)
    observer_->delayChangedTo(drvr_vertex);
}

bool
GraphDelayCalc::findDriverEdgeDelays(Vertex *drvr_vertex,
                                     const MultiDrvrNet *multi_drvr,
                                     Edge *edge,
                                     ArcDelayCalc *arc_delay_calc,
                                     array<bool, RiseFall::index_count> &delay_exists)
{
  Vertex *from_vertex = edge->from(graph_);
  const TimingArcSet *arc_set = edge->timingArcSet();
  bool delay_changed = false;
  LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(drvr_vertex);
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    for (const TimingArc *arc : arc_set->arcs()) {
      delay_changed |= findDriverArcDelays(drvr_vertex, multi_drvr, edge, arc,
                                           load_pin_index_map, dcalc_ap,
                                           arc_delay_calc);
      delay_exists[arc->toEdge()->asRiseFall()->index()] = true;
    }
  }
  if (delay_changed && observer_) {
    observer_->delayChangedFrom(from_vertex);
    observer_->delayChangedFrom(drvr_vertex);
  }
  return delay_changed;
}

void
GraphDelayCalc::findDriverArcDelays(Vertex *drvr_vertex,
                                    Edge *edge,
                                    const TimingArc *arc,
                                    const DcalcAnalysisPt *dcalc_ap,
                                    ArcDelayCalc *arc_delay_calc)
{
  MultiDrvrNet *multi_drvr = multiDrvrNet(drvr_vertex);
  LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(drvr_vertex);
  findDriverArcDelays(drvr_vertex, multi_drvr, edge, arc,
                      load_pin_index_map, dcalc_ap,
                      arc_delay_calc);
}

bool
GraphDelayCalc::findDriverArcDelays(Vertex *drvr_vertex,
                                    const MultiDrvrNet *multi_drvr,
                                    Edge *edge,
                                    const TimingArc *arc,
                                    LoadPinIndexMap &load_pin_index_map,
                                    const DcalcAnalysisPt *dcalc_ap,
                                    ArcDelayCalc *arc_delay_calc)
{
  bool delay_changed = false;
  const RiseFall *from_rf = arc->fromEdge()->asRiseFall();
  const RiseFall *drvr_rf = arc->toEdge()->asRiseFall();
  if (from_rf && drvr_rf) {
    const Pin *drvr_pin = drvr_vertex->pin();
    const Parasitic *parasitic;
    float load_cap;
    parasiticLoad(drvr_pin, drvr_rf, dcalc_ap, multi_drvr, arc_delay_calc,
                  load_cap, parasitic);

    if (multi_drvr
        && multi_drvr->parallelGates(network_)) {
      ArcDcalcArgSeq dcalc_args = makeArcDcalcArgs(drvr_vertex, multi_drvr,
                                                   edge, arc, dcalc_ap,
                                                   arc_delay_calc);
      ArcDcalcResultSeq dcalc_results =
        arc_delay_calc->gateDelays(dcalc_args, load_pin_index_map, dcalc_ap);
      for (size_t drvr_idx = 0; drvr_idx < dcalc_args.size(); drvr_idx++) {
        ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
        ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
        delay_changed |= annotateDelaysSlews(dcalc_arg.edge(), dcalc_arg.arc(),
                                             dcalc_result, load_pin_index_map,
                                             dcalc_ap);
      }
    }
    else {
      Vertex *from_vertex = edge->from(graph_);
      const Slew in_slew = edgeFromSlew(from_vertex, from_rf, edge, dcalc_ap);
      ArcDcalcResult dcalc_result = arc_delay_calc->gateDelay(drvr_pin, arc, in_slew,
                                                              load_cap, parasitic,
                                                              load_pin_index_map,
                                                              dcalc_ap);
      delay_changed |= annotateDelaysSlews(edge, arc, dcalc_result,
                                           load_pin_index_map, dcalc_ap);
    }
    arc_delay_calc->finishDrvrPin();
  }
  return delay_changed;
}

ArcDcalcArgSeq
GraphDelayCalc::makeArcDcalcArgs(Vertex *drvr_vertex,
                                 const MultiDrvrNet *multi_drvr,
                                 Edge *edge,
                                 const TimingArc *arc,
                                 const DcalcAnalysisPt *dcalc_ap,
                                 ArcDelayCalc *arc_delay_calc)
{
  ArcDcalcArgSeq dcalc_args;
  for (auto drvr_vertex1 : multi_drvr->drvrs()) {
    Edge *edge1 = nullptr;
    const TimingArc *arc1 = nullptr;
    if (drvr_vertex1 == drvr_vertex) {
      edge1 = edge;
      arc1 = arc;
    }
    else
      findParallelEdge(drvr_vertex1, edge, arc, edge1, arc1);
    // Shockingly one fpga vendor connects outputs with no timing arcs together.
    if (edge1) {
      Vertex *from_vertex = edge1->from(graph_);
      const Pin *from_pin = from_vertex->pin();
      const RiseFall *from_rf = arc1->fromEdge()->asRiseFall();
      const RiseFall *drvr_rf = arc1->toEdge()->asRiseFall();
      const Slew in_slew = edgeFromSlew(from_vertex, from_rf, edge1, dcalc_ap);
      const Pin *drvr_pin1 = drvr_vertex1->pin();
      float load_cap;
      const Parasitic *parasitic;
      parasiticLoad(drvr_pin1, drvr_rf, dcalc_ap, multi_drvr, arc_delay_calc,
                    load_cap, parasitic);
      dcalc_args.emplace_back(from_pin, drvr_pin1, edge1, arc1, in_slew,
                              load_cap, parasitic);
    }
  }
  return dcalc_args;
}

// Find an edge/arc for parallel driver vertex to go along with the
// primary driver drvr_edge/drvr_arc.
void
GraphDelayCalc::findParallelEdge(Vertex *vertex,
                                 Edge *drvr_edge,
                                 const TimingArc *drvr_arc,
                                 // Return values.
                                 Edge *&edge,
                                 const TimingArc *&arc)
{
  LibertyCell *drvr_cell = drvr_arc->from()->libertyCell();
  LibertyCell *vertex_cell = network_->libertyCell(network_->instance(vertex->pin()));
  if (vertex_cell == drvr_cell) {
    // Homogeneous drivers.
    arc = drvr_arc;
    LibertyPort *from_port = network_->libertyPort(drvr_edge->from(graph_)->pin());
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      edge = edge_iter.next();
      if (network_->libertyPort(edge->from(graph_)->pin()) == from_port)
        return;
    }
  }
  else {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      edge = edge_iter.next();
      for (TimingArc *arc1 : edge->timingArcSet()->arcs()) {
        if (arc1->fromEdge() == drvr_arc->fromEdge()
            && arc1->toEdge() == drvr_arc->toEdge()) {
          arc = arc1;
          return;
        }
      }
    }
  }
  edge = nullptr;
  arc = nullptr;
}

bool
GraphDelayCalc::annotateDelaysSlews(Edge *edge,
                                    const TimingArc *arc,
                                    ArcDcalcResult &dcalc_result,
                                    LoadPinIndexMap &load_pin_index_map,
                                    const DcalcAnalysisPt *dcalc_ap)
{
  bool delay_changed = annotateDelaySlew(edge, arc,
                                         dcalc_result.gateDelay(),
                                         dcalc_result.drvrSlew(), dcalc_ap);
  if (!edge->role()->isLatchDtoQ()) {
    Vertex *drvr_vertex = edge->to(graph_);
    delay_changed |= annotateLoadDelays(drvr_vertex, arc->toEdge()->asRiseFall(),
                                        dcalc_result,
                                        load_pin_index_map, delay_zero, true,
                                        dcalc_ap);
  }
  return delay_changed;
}

// Annotate the gate delay and merge the slew at the driver pin.
// Annotate the wire delays from the gate output to
// each load pin, and the merge the slews at each load pin. 
bool
GraphDelayCalc::annotateDelaySlew(Edge *edge,
                                  const TimingArc *arc,
                                  ArcDelay &gate_delay,
                                  Slew &gate_slew,
                                  const DcalcAnalysisPt *dcalc_ap)
{
  bool delay_changed = false;
  DcalcAPIndex ap_index = dcalc_ap->index();
  debugPrint(debug_, "delay_calc", 3,
             "  %s %s -> %s %s (%s) corner:%s/%s",
             arc->from()->name(),
             arc->fromEdge()->asString(),
             arc->to()->name(),
             arc->toEdge()->asString(),
             arc->role()->asString(),
             dcalc_ap->corner()->name(),
             dcalc_ap->delayMinMax()->asString());
  debugPrint(debug_, "delay_calc", 3,
             "    gate delay = %s slew = %s",
             delayAsString(gate_delay, this),
             delayAsString(gate_slew, this));
  Vertex *drvr_vertex = edge->to(graph_);
  const RiseFall *drvr_rf = arc->toEdge()->asRiseFall();
  // Merge slews.
  const Slew &drvr_slew = graph_->slew(drvr_vertex, drvr_rf, ap_index);
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  if (delayGreater(gate_slew, drvr_slew, slew_min_max, this)
      && !drvr_vertex->slewAnnotated(drvr_rf, slew_min_max)
      && !edge->role()->isLatchDtoQ())
    graph_->setSlew(drvr_vertex, drvr_rf, ap_index, gate_slew);
  if (!graph_->arcDelayAnnotated(edge, arc, ap_index)) {
    const ArcDelay &prev_gate_delay = graph_->arcDelay(edge,arc,ap_index);
    float gate_delay1 = delayAsFloat(gate_delay);
    float prev_gate_delay1 = delayAsFloat(prev_gate_delay);
    if (prev_gate_delay1 == 0.0
        || (abs(gate_delay1 - prev_gate_delay1) / prev_gate_delay1
            > incremental_delay_tolerance_))
      delay_changed = true;
    graph_->setArcDelay(edge, arc, ap_index, gate_delay);
  }
  return delay_changed;
}

// Annotate wire arc delays and load pin slews.
// extra_delay is additional wire delay to add to delay returned
// by the delay calculator.
bool
GraphDelayCalc::annotateLoadDelays(Vertex *drvr_vertex,
                                   const RiseFall *drvr_rf,
                                   ArcDcalcResult &dcalc_result,
                                   LoadPinIndexMap &load_pin_index_map,
                                   const ArcDelay &extra_delay,
                                   bool merge,
                                   const DcalcAnalysisPt *dcalc_ap)
{
  bool changed = false;
  DcalcAPIndex ap_index = dcalc_ap->index();
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      Pin *load_pin = load_vertex->pin();
      size_t load_idx = load_pin_index_map[load_pin];
      ArcDelay wire_delay = dcalc_result.wireDelay(load_idx);
      Slew load_slew = dcalc_result.loadSlew(load_idx);
      debugPrint(debug_, "delay_calc", 3,
                 "    %s load delay = %s slew = %s",
                 load_vertex->name(sdc_network_),
                 delayAsString(wire_delay, this),
                 delayAsString(load_slew, this));
      bool load_changed = false;
      if (!load_vertex->slewAnnotated(drvr_rf, slew_min_max)) {
	if (drvr_vertex->slewAnnotated(drvr_rf, slew_min_max)) {
	  // Copy the driver slew to the load if it is annotated.
	  const Slew &drvr_slew = graph_->slew(drvr_vertex,drvr_rf,ap_index);
	  graph_->setSlew(load_vertex, drvr_rf, ap_index, drvr_slew);
          load_changed = true;
	}
	else {
	  const Slew &slew = graph_->slew(load_vertex, drvr_rf, ap_index);
	  if (!merge
	      || delayGreater(load_slew, slew, slew_min_max, this)) {
	    graph_->setSlew(load_vertex, drvr_rf, ap_index, load_slew);
            load_changed = true;
          }
	}
      }
      if (!graph_->wireDelayAnnotated(wire_edge, drvr_rf, ap_index)) {
	// Multiple timing arcs with the same output transition
	// annotate the same wire edges so they must be combined
	// rather than set.
	const ArcDelay &delay = graph_->wireArcDelay(wire_edge, drvr_rf, ap_index);
	Delay wire_delay_extra = extra_delay + wire_delay;
	const MinMax *delay_min_max = dcalc_ap->delayMinMax();
	if (!merge
	    || delayGreater(wire_delay_extra, delay, delay_min_max, this)) {
	  graph_->setWireArcDelay(wire_edge, drvr_rf, ap_index, wire_delay_extra);
          load_changed = true;
	}
      }
      if (load_changed && observer_)
        observer_->delayChangedTo(load_vertex);
      // Enqueue bidirect driver from load vertex.
      if (sdc_->bidirectDrvrSlewFromLoad(load_pin))
	iter_->enqueue(graph_->pinDrvrVertex(load_pin));
      changed |= load_changed;
    }
  }
  return changed;
}

LoadPinIndexMap
GraphDelayCalc::makeLoadPinIndexMap(Vertex *drvr_vertex)
{
  LoadPinIndexMap load_pin_index_map(network_);
  size_t load_idx = 0;
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      const Pin *load_pin = load_vertex->pin();
      load_pin_index_map[load_pin] = load_idx;
      load_idx++;
    }
  }
  return load_pin_index_map;
}

// External
float
GraphDelayCalc::loadCap(const Pin *drvr_pin,
                        const DcalcAnalysisPt *dcalc_ap) const
{
  const MinMax *min_max = dcalc_ap->constraintMinMax();
  float load_cap = min_max->initValue();
  for (auto drvr_rf : RiseFall::range()) {
    float cap = loadCap(drvr_pin, drvr_rf, dcalc_ap);
    load_cap = min_max->minMax(cap, load_cap);
  }
  arc_delay_calc_->finishDrvrPin();
  return load_cap;
}

// External
float
GraphDelayCalc::loadCap(const Pin *drvr_pin,
                        const RiseFall *rf,
                        const DcalcAnalysisPt *dcalc_ap) const
{
  float pin_cap, wire_cap;
  loadCap(drvr_pin, rf, dcalc_ap, pin_cap, wire_cap);
  return pin_cap + wire_cap;
}

// External
void
GraphDelayCalc::loadCap(const Pin *drvr_pin,
                        const RiseFall *rf,
                        const DcalcAnalysisPt *dcalc_ap,
                        float &pin_cap,
                        float &wire_cap) const
{
  MultiDrvrNet *multi_drvr = nullptr;
  if (graph_) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    multi_drvr = multiDrvrNet(drvr_vertex);
  }
  const Parasitic *parasitic;
  parasiticLoad(drvr_pin, rf, dcalc_ap, multi_drvr, arc_delay_calc_,
                pin_cap, wire_cap, parasitic);
  arc_delay_calc_->finishDrvrPin();
}

float
GraphDelayCalc::loadCap(const Pin *drvr_pin,
                        const RiseFall *rf,
                        const DcalcAnalysisPt *dcalc_ap,
                        ArcDelayCalc *arc_delay_calc) const
{
  const Parasitic *parasitic;
  float pin_cap,  wire_cap;
  parasiticLoad(drvr_pin, rf, dcalc_ap, nullptr, arc_delay_calc,
                pin_cap, wire_cap, parasitic);
  return pin_cap + wire_cap;
}

void
GraphDelayCalc::parasiticLoad(const Pin *drvr_pin,
                              const RiseFall *rf,
                              const DcalcAnalysisPt *dcalc_ap,
                              const MultiDrvrNet *multi_drvr,
                              ArcDelayCalc *arc_delay_calc,
                              // Return values.
                              float &load_cap,
                              const Parasitic *&parasitic) const
{
  float pin_cap,  wire_cap;
  parasiticLoad(drvr_pin, rf, dcalc_ap, multi_drvr, arc_delay_calc,
                pin_cap, wire_cap, parasitic);
  load_cap = pin_cap + wire_cap;
}

void
GraphDelayCalc::parasiticLoad(const Pin *drvr_pin,
                              const RiseFall *rf,
                              const DcalcAnalysisPt *dcalc_ap,
                              const MultiDrvrNet *multi_drvr,
                              ArcDelayCalc *arc_delay_calc,
                              // Return values.
                              float &pin_cap,
                              float &wire_cap,
                              const Parasitic *&parasitic) const
{
  bool has_net_load;
  float fanout;
  netCaps(drvr_pin, rf, dcalc_ap, multi_drvr,
          pin_cap, wire_cap, fanout, has_net_load);

  parasitic = arc_delay_calc->findParasitic(drvr_pin, rf, dcalc_ap);
  // set_load net has precedence over parasitics.
  if (!has_net_load && parasitic) {
    if (parasitics_->isParasiticNetwork(parasitic))
      wire_cap += parasitics_->capacitance(parasitic);
    else {
      // PiModel includes both pin and external caps.
      float parasitic_cap = parasitics_->capacitance(parasitic);
      if (parasitic_cap >= pin_cap)
	wire_cap = parasitic_cap - pin_cap;
      else {
        wire_cap = 0.0;
        // Ignore parasitic if pin cap is greater.
        parasitic = nullptr;
      }
    }
  }
}

void
GraphDelayCalc::netCaps(const Pin *drvr_pin,
                        const RiseFall *rf,
                        const DcalcAnalysisPt *dcalc_ap,
                        // Return values.
                        float &pin_cap,
                        float &wire_cap,
                        float &fanout,
                        bool &has_net_load) const
{
  MultiDrvrNet *multi_drvr = nullptr;
  if (graph_) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    multi_drvr = multiDrvrNet(drvr_vertex);
  }
  netCaps(drvr_pin, rf, dcalc_ap, multi_drvr,
          pin_cap, wire_cap, fanout, has_net_load);
}

void
GraphDelayCalc::netCaps(const Pin *drvr_pin,
                        const RiseFall *rf,
                        const DcalcAnalysisPt *dcalc_ap,
                        const MultiDrvrNet *multi_drvr,
                        // Return values.
                        float &pin_cap,
                        float &wire_cap,
                        float &fanout,
                        bool &has_net_load) const
{
  if (multi_drvr)
    multi_drvr->netCaps(rf, dcalc_ap,
			pin_cap, wire_cap, fanout, has_net_load);
  else {
    const Corner *corner = dcalc_ap->corner();
    const MinMax *min_max = dcalc_ap->constraintMinMax();
    // Find pin and external pin/wire capacitance.
    sdc_->connectedCap(drvr_pin, rf, corner, min_max,
		       pin_cap, wire_cap, fanout, has_net_load);
  }
}

void
GraphDelayCalc::initSlew(Vertex *vertex)
{
  for (auto rf : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      const MinMax *slew_min_max = dcalc_ap->slewMinMax();
      if (!vertex->slewAnnotated(rf, slew_min_max)) {
	DcalcAPIndex ap_index = dcalc_ap->index();
	graph_->setSlew(vertex, rf, ap_index, slew_min_max->initValue());
      }
    }
  }
}

void
GraphDelayCalc::zeroSlewAndWireDelays(Vertex *drvr_vertex,
                                      const RiseFall *rf)
{
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const MinMax *slew_min_max = dcalc_ap->slewMinMax();
    // Init drvr slew.
    if (!drvr_vertex->slewAnnotated(rf, slew_min_max)) {
      DcalcAPIndex ap_index = dcalc_ap->index();
      graph_->setSlew(drvr_vertex, rf, ap_index, slew_min_max->initValue());
    }

    // Init wire delays and slews.
    VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *wire_edge = edge_iter.next();
      if (wire_edge->isWire()) {
        Vertex *load_vertex = wire_edge->to(graph_);
        if (!graph_->wireDelayAnnotated(wire_edge, rf, ap_index))
          graph_->setWireArcDelay(wire_edge, rf, ap_index, 0.0);
        // Init load vertex slew.
        if (!load_vertex->slewAnnotated(rf, slew_min_max))
          graph_->setSlew(load_vertex, rf, ap_index, 0.0);
      }
    }
  }
}

void
GraphDelayCalc::initWireDelays(Vertex *drvr_vertex)
{
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
	const MinMax *delay_min_max = dcalc_ap->delayMinMax();
	Delay delay_init_value(delay_min_max->initValue());
	DcalcAPIndex ap_index = dcalc_ap->index();
	for (auto rf : RiseFall::range()) {
	  if (!graph_->wireDelayAnnotated(wire_edge, rf, ap_index))
	    graph_->setWireArcDelay(wire_edge, rf, ap_index, delay_init_value);
	}
      }
    }
  }
}

Slew
GraphDelayCalc::edgeFromSlew(const Vertex *from_vertex,
                             const RiseFall *from_rf,
                             const Edge *edge,
                             const DcalcAnalysisPt *dcalc_ap)
{
  return edgeFromSlew(from_vertex, from_rf, edge->role(), dcalc_ap);
}

// Use clock slew for register/latch clk->q edges.
Slew
GraphDelayCalc::edgeFromSlew(const Vertex *from_vertex,
                             const RiseFall *from_rf,
                             const TimingRole *role,
                             const DcalcAnalysisPt *dcalc_ap)
{

  if (role->genericRole() == TimingRole::regClkToQ()
      && clk_network_->isIdealClock(from_vertex->pin()))
    return clk_network_->idealClkSlew(from_vertex->pin(), from_rf,
                                      dcalc_ap->slewMinMax());
  else
    return graph_->slew(from_vertex, from_rf, dcalc_ap->index());
}

void
GraphDelayCalc::findCheckEdgeDelays(Edge *edge,
                                    ArcDelayCalc *arc_delay_calc)
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  TimingArcSet *arc_set = edge->timingArcSet();
  const Pin *to_pin = to_vertex->pin();
  Instance *inst = network_->instance(to_pin);
  debugPrint(debug_, "delay_calc", 2, "find check %s %s -> %s",
             sdc_network_->pathName(inst),
             network_->portName(from_vertex->pin()),
             network_->portName(to_pin));
  bool delay_changed = false;
  for (TimingArc *arc : arc_set->arcs()) {
    RiseFall *from_rf = arc->fromEdge()->asRiseFall();
    RiseFall *to_rf = arc->toEdge()->asRiseFall();
    if (from_rf && to_rf) {
      const LibertyPort *related_out_port = arc_set->relatedOut();
      const Pin *related_out_pin = 0;
      if (related_out_port)
	related_out_pin = network_->findPin(inst, related_out_port);
      for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
	DcalcAPIndex ap_index = dcalc_ap->index();
	if (!graph_->arcDelayAnnotated(edge, arc, ap_index)) {
	  const Slew &from_slew = checkEdgeClkSlew(from_vertex, from_rf,
						   dcalc_ap);
	  int slew_index = dcalc_ap->checkDataSlewIndex();
	  const Slew &to_slew = graph_->slew(to_vertex, to_rf, slew_index);
	  debugPrint(debug_, "delay_calc", 3,
                     "  %s %s -> %s %s (%s) corner:%s/%s",
                     arc_set->from()->name(),
                     arc->fromEdge()->asString(),
                     arc_set->to()->name(),
                     arc->toEdge()->asString(),
                     arc_set->role()->asString(),
                     dcalc_ap->corner()->name(),
                     dcalc_ap->delayMinMax()->asString());
	  debugPrint(debug_, "delay_calc", 3,
                     "    from_slew = %s to_slew = %s",
                     delayAsString(from_slew, this),
                     delayAsString(to_slew, this));
	  float related_out_cap = 0.0;
	  if (related_out_pin)
	    related_out_cap = loadCap(related_out_pin, to_rf,dcalc_ap,arc_delay_calc);
          ArcDelay check_delay = arc_delay_calc->checkDelay(to_pin, arc, from_slew,
                                                            to_slew, related_out_cap,
                                                            dcalc_ap);
	  debugPrint(debug_, "delay_calc", 3,
                     "    check_delay = %s",
                     delayAsString(check_delay, this));
	  graph_->setArcDelay(edge, arc, ap_index, check_delay);
	  delay_changed = true;
          arc_delay_calc_->finishDrvrPin();
	}
      }
    }
  }

  if (delay_changed && observer_)
    observer_->checkDelayChangedTo(to_vertex);
}

// Use clock slew for timing check clock edges.
Slew
GraphDelayCalc::checkEdgeClkSlew(const Vertex *from_vertex,
                                 const RiseFall *from_rf,
                                 const DcalcAnalysisPt *dcalc_ap)
{
  if (clk_network_->isIdealClock(from_vertex->pin()))
    return clk_network_->idealClkSlew(from_vertex->pin(), from_rf,
                                      dcalc_ap->checkClkSlewMinMax());
  else 
    return graph_->slew(from_vertex, from_rf, dcalc_ap->checkClkSlewIndex());
}

////////////////////////////////////////////////////////////////

string
GraphDelayCalc::reportDelayCalc(const Edge *edge,
                                const TimingArc *arc,
                                const Corner *corner,
                                const MinMax *min_max,
                                int digits)
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  TimingRole *role = arc->role();
  const Instance *inst = network_->instance(to_pin);
  const TimingArcSet *arc_set = edge->timingArcSet();
  string result;
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  RiseFall *from_rf = arc->fromEdge()->asRiseFall();
  RiseFall *to_rf = arc->toEdge()->asRiseFall();
  if (from_rf && to_rf) {
    const LibertyPort *related_out_port = arc_set->relatedOut();
    const Pin *related_out_pin = 0;
    if (related_out_port)
      related_out_pin = network_->findPin(inst, related_out_port);
    float related_out_cap = 0.0;
    if (related_out_pin)
      related_out_cap = loadCap(related_out_pin, to_rf, dcalc_ap, arc_delay_calc_);
    if (role->isTimingCheck()) {
      const Slew &from_slew = checkEdgeClkSlew(from_vertex, from_rf, dcalc_ap);
      int slew_index = dcalc_ap->checkDataSlewIndex();
      const Slew &to_slew = graph_->slew(to_vertex, to_rf, slew_index);
      bool from_ideal_clk = clk_network_->isIdealClock(from_vertex->pin());
      const char *from_slew_annotation = from_ideal_clk ? " (ideal clock)" : nullptr;
      result = arc_delay_calc_->reportCheckDelay(to_pin, arc, from_slew,
                                                 from_slew_annotation, to_slew,
                                                 related_out_cap, dcalc_ap, digits);
    }
    else {
      const Slew &from_slew = edgeFromSlew(from_vertex, from_rf, edge, dcalc_ap);
      const Parasitic *to_parasitic;
      float load_cap;
      parasiticLoad(to_pin, to_rf, dcalc_ap, nullptr, arc_delay_calc_,
                    load_cap, to_parasitic);
      LoadPinIndexMap load_pin_index_map = makeLoadPinIndexMap(to_vertex);
      result = arc_delay_calc_->reportGateDelay(to_pin, arc, from_slew, load_cap,
                                                to_parasitic, load_pin_index_map,
                                                dcalc_ap, digits);
    }
    arc_delay_calc_->finishDrvrPin();
  }
  return result;
}

////////////////////////////////////////////////////////////////

void
GraphDelayCalc::minPeriod(const Pin *pin,
			  // Return values.
			  float &min_period,
			  bool &exists)
{
  exists = false;
  const MinMax *min_max = MinMax::max();
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    // Sdf annotation.
    float min_period1 = 0.0;
    bool exists1 = false;
    graph_->periodCheckAnnotation(pin, dcalc_ap->index(),
				  min_period, exists);
    if (exists1 
	&& (!exists || min_period1 < min_period)) {
      min_period = min_period1;
      exists = true;
    }
  }
  if (!exists) {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      // Liberty library.
      Instance *inst = network_->instance(pin);
      OperatingConditions *op_cond = sdc_->operatingConditions(min_max);
      const Pvt *pvt = inst ? sdc_->pvt(inst, min_max) : nullptr;
      port->minPeriod(op_cond, pvt, min_period, exists);
    }
  }
}

////////////////////////////////////////////////////////////////

MultiDrvrNet::MultiDrvrNet() :
  dcalc_drvr_(nullptr)
{
}

void
MultiDrvrNet::netCaps(const RiseFall *drvr_rf,
		      const DcalcAnalysisPt *dcalc_ap,
		      // Return values.
		      float &pin_cap,
		      float &wire_cap,
		      float &fanout,
		      bool &has_net_load) const
{
  int index = dcalc_ap->index() * RiseFall::index_count
    + drvr_rf->index();
  const NetCaps &net_caps = net_caps_[index];
  pin_cap = net_caps.pinCap();
  wire_cap = net_caps.wireCap();
  fanout = net_caps.fanout();
  has_net_load = net_caps.hasNetLoad();
}

void
MultiDrvrNet::findCaps(const Sdc *sdc)
{
  Corners *corners = sdc->corners();
  int count = RiseFall::index_count * corners->dcalcAnalysisPtCount();
  net_caps_.resize(count);
  const Pin *drvr_pin = dcalc_drvr_->pin();
  for (auto dcalc_ap : corners->dcalcAnalysisPts()) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const Corner *corner = dcalc_ap->corner();
    const MinMax *min_max = dcalc_ap->constraintMinMax();
    for (auto drvr_rf : RiseFall::range()) {
      int drvr_rf_index = drvr_rf->index();
      int index = ap_index * RiseFall::index_count + drvr_rf_index;
      NetCaps &net_caps = net_caps_[index];
      float pin_cap, wire_cap, fanout;
      bool has_net_load;
      // Find pin and external pin/wire capacitance.
      sdc->connectedCap(drvr_pin, drvr_rf, corner, min_max,
			pin_cap, wire_cap, fanout, has_net_load);
      net_caps.init(pin_cap, wire_cap, fanout, has_net_load);
    }
  }
}

void
MultiDrvrNet::setDcalcDrvr(Vertex *drvr)
{
  dcalc_drvr_ = drvr;
}

bool
MultiDrvrNet::parallelGates(const Network *network) const
{
  return network->direction(dcalc_drvr_->pin())->isOutput();
}

} // namespace
