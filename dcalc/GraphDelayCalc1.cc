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

#include "Machine.hh"
#include "Debug.hh"
#include "Stats.hh"
#include "Mutex.hh"
#include "MinMax.hh"
#include "PortDirection.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "InputDrive.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "Graph.hh"
#include "Levelize.hh"
#include "SearchPred.hh"
#include "Bfs.hh"
#include "Parasitics.hh"
#include "ArcDelayCalc.hh"
#include "DcalcAnalysisPt.hh"
#include "NetCaps.hh"
#include "GraphDelayCalc1.hh"

namespace sta {

using std::abs;

static const Slew default_slew = 0.0;

typedef Set<MultiDrvrNet*> MultiDrvrNetSet;

static bool
isLeafDriver(const Pin *pin,
	     const Network *network);

// Cache parallel delay/slew values for nets with multiple drivers.
class MultiDrvrNet
{
public:
  MultiDrvrNet(VertexSet *drvrs);
  ~MultiDrvrNet();
  const VertexSet *drvrs() const { return drvrs_; }
  VertexSet *drvrs() { return drvrs_; }
  Vertex *dcalcDrvr() const { return dcalc_drvr_; }
  void setDcalcDrvr(Vertex *drvr);
  void parallelDelaySlew(const RiseFall *drvr_rf,
			 const DcalcAnalysisPt *dcalc_ap,
			 ArcDelayCalc *arc_delay_calc,
			 GraphDelayCalc1 *dcalc,
			 // Return values.
			 ArcDelay &parallel_delay,
			 Slew &parallel_slew);
  void netCaps(const RiseFall *rf,
	       const DcalcAnalysisPt *dcalc_ap,
	       // Return values.
	       float &pin_cap,
	       float &wire_cap,
	       float &fanout,
	       bool &has_set_load);
  void findCaps(const GraphDelayCalc1 *dcalc,
		const Sdc *sdc);

private:
  void findDelaysSlews(ArcDelayCalc *arc_delay_calc,
		       GraphDelayCalc1 *dcalc);

  // Driver that triggers delay calculation for all the drivers on the net.
  Vertex *dcalc_drvr_;
  VertexSet *drvrs_;
  // [drvr_rf->index][dcalc_ap->index]
  ArcDelay *parallel_delays_;
  // [drvr_rf->index][dcalc_ap->index]
  Slew *parallel_slews_;
  // [drvr_rf->index][dcalc_ap->index]
  NetCaps *net_caps_;
  bool delays_valid_:1;
};

MultiDrvrNet::MultiDrvrNet(VertexSet *drvrs) :
  dcalc_drvr_(nullptr),
  drvrs_(drvrs),
  parallel_delays_(nullptr),
  parallel_slews_(nullptr),
  net_caps_(nullptr),
  delays_valid_(false)
{
}

MultiDrvrNet::~MultiDrvrNet()
{
  delete drvrs_;
  if (delays_valid_) {
    delete [] parallel_delays_;
    delete [] parallel_slews_;
  }
  delete [] net_caps_;
}

void
MultiDrvrNet::parallelDelaySlew(const RiseFall *drvr_rf,
				const DcalcAnalysisPt *dcalc_ap,
				ArcDelayCalc *arc_delay_calc,
				GraphDelayCalc1 *dcalc,
				// Return values.
				ArcDelay &parallel_delay,
				Slew &parallel_slew)
{
  if (!delays_valid_) {
    findDelaysSlews(arc_delay_calc, dcalc);
    delays_valid_ = true;
  }

  int index = dcalc_ap->index() * RiseFall::index_count
    + drvr_rf->index();
  parallel_delay = parallel_delays_[index];
  parallel_slew = parallel_slews_[index];
}

void
MultiDrvrNet::findDelaysSlews(ArcDelayCalc *arc_delay_calc,
			      GraphDelayCalc1 *dcalc)
{
  Corners *corners = dcalc->corners();
  int count = RiseFall::index_count * corners->dcalcAnalysisPtCount();
  parallel_delays_ = new ArcDelay[count];
  parallel_slews_ = new Slew[count];
  for (auto dcalc_ap : corners->dcalcAnalysisPts()) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const Pvt *pvt = dcalc_ap->operatingConditions();
    for (auto drvr_rf : RiseFall::range()) {
      int drvr_rf_index = drvr_rf->index();
      int index = ap_index*RiseFall::index_count+drvr_rf_index;
      dcalc->findMultiDrvrGateDelay(this, drvr_rf, pvt, dcalc_ap,
				    arc_delay_calc,
				    parallel_delays_[index],
				    parallel_slews_[index]);
    }
  }
}

void
MultiDrvrNet::netCaps(const RiseFall *drvr_rf,
		      const DcalcAnalysisPt *dcalc_ap,
		      // Return values.
		      float &pin_cap,
		      float &wire_cap,
		      float &fanout,
		      bool &has_set_load)
{
  int index = dcalc_ap->index() * RiseFall::index_count
    + drvr_rf->index();
  NetCaps &net_caps = net_caps_[index];
  pin_cap = net_caps.pinCap();
  wire_cap = net_caps.wireCap();
  fanout = net_caps.fanout();
  has_set_load = net_caps.hasSetLoad();
}

void
MultiDrvrNet::findCaps(const GraphDelayCalc1 *dcalc,
		       const Sdc *sdc)
{
  Corners *corners = dcalc->corners();
  int count = RiseFall::index_count * corners->dcalcAnalysisPtCount();
  net_caps_ = new NetCaps[count];
  const Pin *drvr_pin = dcalc_drvr_->pin();
  for (auto dcalc_ap : corners->dcalcAnalysisPts()) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const Corner *corner = dcalc_ap->corner();
    const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
    const MinMax *min_max = dcalc_ap->constraintMinMax();
    for (auto drvr_rf : RiseFall::range()) {
      int drvr_rf_index = drvr_rf->index();
      int index = ap_index * RiseFall::index_count + drvr_rf_index;
      NetCaps &net_caps = net_caps_[index];
      float pin_cap, wire_cap, fanout;
      bool has_set_load;
      // Find pin and external pin/wire capacitance.
      sdc->connectedCap(drvr_pin, drvr_rf, op_cond, corner, min_max,
			pin_cap, wire_cap, fanout, has_set_load);
      net_caps.init(pin_cap, wire_cap, fanout, has_set_load);
    }
  }
}

void
MultiDrvrNet::setDcalcDrvr(Vertex *drvr)
{
  dcalc_drvr_ = drvr;
}

////////////////////////////////////////////////////////////////


GraphDelayCalc1::GraphDelayCalc1(StaState *sta) :
  GraphDelayCalc(sta),
  observer_(nullptr),
  delays_seeded_(false),
  incremental_(false),
  search_pred_(new SearchPred1(sta)),
  search_non_latch_pred_(new SearchPredNonLatch2(sta)),
  clk_pred_(new ClkTreeSearchPred(sta)),
  iter_(new BfsFwdIterator(BfsIndex::dcalc, search_non_latch_pred_, sta)),
  multi_drvr_nets_found_(false),
  incremental_delay_tolerance_(0.0)
{
}

GraphDelayCalc1::~GraphDelayCalc1()
{
  delete search_pred_;
  delete search_non_latch_pred_;
  delete clk_pred_;
  delete iter_;
  deleteMultiDrvrNets();
  clearIdealClkMap();
  delete observer_;
}

void
GraphDelayCalc1::deleteMultiDrvrNets()
{
  MultiDrvrNetSet drvr_nets;
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
GraphDelayCalc1::copyState(const StaState *sta)
{
  GraphDelayCalc::copyState(sta);
  // Notify sub-components.
  iter_->copyState(sta);
}

void
GraphDelayCalc1::clear()
{
  delaysInvalid();
  deleteMultiDrvrNets();
  multi_drvr_nets_found_ = false;
  GraphDelayCalc::clear();
}

float
GraphDelayCalc1::incrementalDelayTolerance()
{
  return incremental_delay_tolerance_;
}

void
GraphDelayCalc1::setIncrementalDelayTolerance(float tol)
{
  incremental_delay_tolerance_ = tol;
}

void
GraphDelayCalc1::setObserver(DelayCalcObserver *observer)
{
  delete observer_;
  observer_ = observer;
}

void
GraphDelayCalc1::delaysInvalid()
{
  debugPrint0(debug_, "delay_calc", 1, "delays invalid\n");
  delays_exist_ = false;
  delays_seeded_ = false;
  incremental_ = false;
  iter_->clear();
  clearIdealClkMap();
  // No need to keep track of incremental updates any more.
  invalid_delays_.clear();
  invalid_checks_.clear();
}

void
GraphDelayCalc1::delayInvalid(const Pin *pin)
{
  if (graph_
      && incremental_) {
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
GraphDelayCalc1::delayInvalid(Vertex *vertex)
{
  debugPrint1(debug_, "delay_calc", 2, "delays invalid %s\n",
	      vertex->name(sdc_network_));
  if (graph_ && incremental_) {
    invalid_delays_.insert(vertex);
    // Invalidate driver that triggers dcalc for multi-driver nets.
    MultiDrvrNet *multi_drvr = multiDrvrNet(vertex);
    if (multi_drvr)
      invalid_delays_.insert(multi_drvr->dcalcDrvr());
  }
}

void
GraphDelayCalc1::deleteVertexBefore(Vertex *vertex)
{
  iter_->deleteVertexBefore(vertex);
  if (incremental_)
    invalid_delays_.erase(vertex);
  MultiDrvrNet *multi_drvr = multiDrvrNet(vertex);
  if (multi_drvr) {
    multi_drvr->drvrs()->erase(vertex);
    multi_drvr_net_map_.erase(vertex);
  }
}

////////////////////////////////////////////////////////////////

class FindVertexDelays : public VertexVisitor
{
public:
  FindVertexDelays(GraphDelayCalc1 *graph_delay_calc1,
		   ArcDelayCalc *arc_delay_calc,
		   bool own_arc_delay_calc);
  virtual ~FindVertexDelays();
  virtual void visit(Vertex *vertex);
  virtual VertexVisitor *copy();
  virtual void levelFinished();

protected:
  GraphDelayCalc1 *graph_delay_calc1_;
  ArcDelayCalc *arc_delay_calc_;
  bool own_arc_delay_calc_;
};

FindVertexDelays::FindVertexDelays(GraphDelayCalc1 *graph_delay_calc1,
				   ArcDelayCalc *arc_delay_calc,
				   bool own_arc_delay_calc) :
  VertexVisitor(),
  graph_delay_calc1_(graph_delay_calc1),
  arc_delay_calc_(arc_delay_calc),
  own_arc_delay_calc_(own_arc_delay_calc)
{
}

FindVertexDelays::~FindVertexDelays()
{
  if (own_arc_delay_calc_)
    delete arc_delay_calc_;
}

VertexVisitor *
FindVertexDelays::copy()
{
  // Copy StaState::arc_delay_calc_ because it needs separate state
  // for each thread.
  return new FindVertexDelays(graph_delay_calc1_,arc_delay_calc_->copy(),true);
}

void
FindVertexDelays::visit(Vertex *vertex)
{
  graph_delay_calc1_->findVertexDelay(vertex, arc_delay_calc_, true);
}

void
FindVertexDelays::levelFinished()
{
  graph_delay_calc1_->mergeIdealClks();
}

void
GraphDelayCalc1::mergeIdealClks()
{
  for (auto vertex_clks : ideal_clks_map_next_) {
    const Vertex *vertex = vertex_clks.first;
    ClockSet *prev_clks = ideal_clks_map_.findKey(vertex);
    delete prev_clks;
    ideal_clks_map_[vertex] = vertex_clks.second;
  }
  ideal_clks_map_next_.clear();
}

// The logical structure of incremental delay calculation closely
// resembles the incremental search arrival time algorithm
// (Search::findArrivals).
void
GraphDelayCalc1::findDelays(Level level)
{
  if (arc_delay_calc_) {
    Stats stats(debug_);
    int dcalc_count = 0;
    debugPrint1(debug_, "delay_calc", 1, "find delays to level %d\n", level);
    if (!delays_seeded_) {
      iter_->clear();
      ensureMultiDrvrNetsFound();
      seedRootSlews();
      delays_seeded_ = true;
    }
    else
      iter_->ensureSize();
    if (incremental_)
      seedInvalidDelays();

    mergeIdealClks();
    FindVertexDelays visitor(this, arc_delay_calc_, false);
    dcalc_count += iter_->visitParallel(level, &visitor);

    // Timing checks require slews at both ends of the arc,
    // so find their delays after all slews are known.
    for (Vertex *check_vertex : invalid_checks_)
      findCheckDelays(check_vertex, arc_delay_calc_);
    invalid_checks_.clear();

    delays_exist_ = true;
    incremental_ = true;
    debugPrint1(debug_, "delay_calc", 1, "found %d delays\n", dcalc_count);
    stats.report("Delay calc");
  }
}

void
GraphDelayCalc1::seedInvalidDelays()
{
  VertexSet::Iterator vertex_iter(invalid_delays_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    if (vertex->isRoot())
      seedRootSlew(vertex, arc_delay_calc_);
    else {
      if (search_non_latch_pred_->searchFrom(vertex))
	iter_->enqueue(vertex);
    }
  }
  invalid_delays_.clear();
}

class FindNetDrvrs : public PinVisitor
{
public:
  FindNetDrvrs(PinSet &drvr_pins,
	       const Network *network,
	       const Graph *graph);
  virtual void operator()(Pin *pin);

protected:
  PinSet &drvr_pins_;
  const Network *network_;
  const Graph *graph_;
};

FindNetDrvrs::FindNetDrvrs(PinSet &drvr_pins,
			   const Network *network,
			   const Graph *graph) :
  drvr_pins_(drvr_pins),
  network_(network),
  graph_(graph)
{
}

void
FindNetDrvrs::operator()(Pin *pin)
{
  Vertex *vertex = graph_->pinDrvrVertex(pin);
  if (isLeafDriver(pin, network_)
      && !vertex->isRoot())
    drvr_pins_.insert(pin);
}

void
GraphDelayCalc1::ensureMultiDrvrNetsFound()
{
  if (!multi_drvr_nets_found_) {
    LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstancePinIterator *pin_iter = network_->pinIterator(inst);
      while (pin_iter->hasNext()) {
	Pin *pin = pin_iter->next();
	Vertex *drvr_vertex = graph_->pinDrvrVertex(pin);
	if (network_->isDriver(pin)
	    && !multi_drvr_net_map_.hasKey(drvr_vertex)) {
	  PinSet drvr_pins;
	  FindNetDrvrs visitor(drvr_pins, network_, graph_);
	  network_->visitConnectedPins(pin, visitor);
	  if (drvr_pins.size() > 1)
	    makeMultiDrvrNet(drvr_pins);
	}
      }
      delete pin_iter;
    }
    delete inst_iter;
    multi_drvr_nets_found_ = true;
  }
}

void
GraphDelayCalc1::makeMultiDrvrNet(PinSet &drvr_pins)
{
  debugPrint0(debug_, "delay_calc", 3, "multi-driver net\n");
  VertexSet *drvr_vertices = new VertexSet;
  MultiDrvrNet *multi_drvr = new MultiDrvrNet(drvr_vertices);
  Level max_drvr_level = 0;
  Vertex *max_drvr = nullptr;
  PinSet::Iterator pin_iter(drvr_pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    Vertex *drvr_vertex = graph_->pinDrvrVertex(pin);
    debugPrint1(debug_, "delay_calc", 3, " %s\n",
		network_->pathName(pin));
    multi_drvr_net_map_[drvr_vertex] = multi_drvr;
    drvr_vertices->insert(drvr_vertex);
    Level drvr_level = drvr_vertex->level();
    if (max_drvr == nullptr
	|| drvr_level > max_drvr_level) {
      max_drvr = drvr_vertex;
      max_drvr_level = drvr_level;
    }
  }
  multi_drvr->setDcalcDrvr(max_drvr);
  multi_drvr->findCaps(this, sdc_);
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
GraphDelayCalc1::multiDrvrNet(const Vertex *drvr_vertex) const
{
  return multi_drvr_net_map_.findKey(drvr_vertex);
}

void
GraphDelayCalc1::seedRootSlews()
{
  VertexSet::Iterator root_iter(levelize_->roots());
  while (root_iter.hasNext()) {
    Vertex *vertex = root_iter.next();
    seedRootSlew(vertex, arc_delay_calc_);
    findIdealClks(vertex);
  }
}

void
GraphDelayCalc1::seedRootSlew(Vertex *vertex,
			      ArcDelayCalc *arc_delay_calc)
{
  if (vertex->isDriver(network_))
    seedDrvrSlew(vertex, arc_delay_calc);
  else
    seedLoadSlew(vertex);
  iter_->enqueueAdjacentVertices(vertex);
}

void
GraphDelayCalc1::seedDrvrSlew(Vertex *drvr_vertex,
			      ArcDelayCalc *arc_delay_calc)
{
  const Pin *drvr_pin = drvr_vertex->pin();
  debugPrint1(debug_, "delay_calc", 2, "seed driver slew %s\n",
	      drvr_vertex->name(sdc_network_));
  InputDrive *drive = 0;
  if (network_->isTopLevelPort(drvr_pin)) {
    Port *port = network_->port(drvr_pin);
    drive = sdc_->findInputDrive(port);
  }
  for (auto tr : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      if (drive) {
	const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
	LibertyCell *drvr_cell;
	LibertyPort *from_port, *to_port;
	float *from_slews;
	drive->driveCell(tr, cnst_min_max, drvr_cell, from_port,
			 from_slews, to_port);
	if (drvr_cell) {
	  if (from_port == nullptr)
	    from_port = driveCellDefaultFromPort(drvr_cell, to_port);
	  findInputDriverDelay(drvr_cell, drvr_pin, drvr_vertex, tr,
			       from_port, from_slews, to_port, dcalc_ap);
	}
	else
	  seedNoDrvrCellSlew(drvr_vertex, drvr_pin, tr, drive, dcalc_ap,
			     arc_delay_calc);
      }
      else
	seedNoDrvrSlew(drvr_vertex, drvr_pin, tr, dcalc_ap, arc_delay_calc);
    }
  }
}

void
GraphDelayCalc1::seedNoDrvrCellSlew(Vertex *drvr_vertex,
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
  Parasitic *parasitic = arc_delay_calc->findParasitic(drvr_pin, rf, dcalc_ap);
  if (exists) {
    float cap = loadCap(drvr_pin, parasitic, rf, dcalc_ap);
    drive_delay = cap * drive_res;
    slew = cap * drive_res;
  }
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  if (!drvr_vertex->slewAnnotated(rf, slew_min_max))
    graph_->setSlew(drvr_vertex, rf, ap_index, slew);
  arc_delay_calc->inputPortDelay(drvr_pin, delayAsFloat(slew), rf,
				 parasitic, dcalc_ap);
  annotateLoadDelays(drvr_vertex, rf, drive_delay, false, dcalc_ap,
		     arc_delay_calc);
}

void
GraphDelayCalc1::seedNoDrvrSlew(Vertex *drvr_vertex,
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
  arc_delay_calc->inputPortDelay(drvr_pin, delayAsFloat(slew), rf,
				 parasitic, dcalc_ap);
  annotateLoadDelays(drvr_vertex, rf, delay_zero, false, dcalc_ap,
		     arc_delay_calc);
}

void
GraphDelayCalc1::seedLoadSlew(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  debugPrint1(debug_, "delay_calc", 2, "seed load slew %s\n",
	      vertex->name(sdc_network_));
  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  initSlew(vertex);
  for (auto tr : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      const MinMax *slew_min_max = dcalc_ap->slewMinMax();
      if (!vertex->slewAnnotated(tr, slew_min_max)) {
	float slew = 0.0;
	if (clks) {
	  slew = slew_min_max->initValue();
	  ClockSet::Iterator clk_iter(clks);
	  while (clk_iter.hasNext()) {
	    Clock *clk = clk_iter.next();
	    float clk_slew = clk->slew(tr, slew_min_max);
	    if (slew_min_max->compare(clk_slew, slew))
	      slew = clk_slew;
	  }
	}
	DcalcAPIndex ap_index = dcalc_ap->index();
	graph_->setSlew(vertex, tr, ap_index, slew);
      }
    }
  }
}

// If a driving cell does not specify a -from_pin, the first port
// defined in the cell that has a timing group to the output port
// is used.  Not exactly reasonable, but it's compatible.
LibertyPort *
GraphDelayCalc1::driveCellDefaultFromPort(LibertyCell *cell,
					  LibertyPort *to_port)
{
  LibertyPort *from_port = 0;
  int from_port_index = 0;
  LibertyCellTimingArcSetIterator set_iter(cell);
  while (set_iter.hasNext()) {
    TimingArcSet *arc_set = set_iter.next();
    if (arc_set->to() == to_port) {
      LibertyPort *set_from_port = arc_set->from();
      int set_from_port_index = findPortIndex(cell, set_from_port);
      if (from_port == nullptr
	  || set_from_port_index < from_port_index) {
	from_port = set_from_port;
	from_port_index = set_from_port_index;
      }
    }
  }
  return from_port;
}

// Find the index that port is defined in cell.
int
GraphDelayCalc1::findPortIndex(LibertyCell *cell,
			       LibertyPort *port)
{
  int index = 0;
  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    LibertyPort *cell_port = port_iter.next();
    if (cell_port == port)
      return index;
    index++;
  }
  internalError("port not found in cell");
  return 0;
}

void
GraphDelayCalc1::findInputDriverDelay(LibertyCell *drvr_cell,
				      const Pin *drvr_pin,
				      Vertex *drvr_vertex,
				      const RiseFall *rf,
				      LibertyPort *from_port,
				      float *from_slews,
				      LibertyPort *to_port,
				      DcalcAnalysisPt *dcalc_ap)
{
  debugPrint2(debug_, "delay_calc", 2, "  driver cell %s %s\n",
	      drvr_cell->name(),
	      rf->asString());
  LibertyCellTimingArcSetIterator set_iter(drvr_cell);
  while (set_iter.hasNext()) {
    TimingArcSet *arc_set = set_iter.next();
    if (arc_set->from() == from_port
	&& arc_set->to() == to_port) {
      TimingArcSetArcIterator arc_iter(arc_set);
      while (arc_iter.hasNext()) {
	TimingArc *arc = arc_iter.next();
	if (arc->toTrans()->asRiseFall() == rf) {
	  float from_slew = from_slews[arc->fromTrans()->index()];
	  findInputArcDelay(drvr_cell, drvr_pin, drvr_vertex,
			    arc, from_slew, dcalc_ap);
	}
      }
    }
  }
}

// Driving cell delay is the load dependent delay, which is the gate
// delay minus the intrinsic delay.  Driving cell delays are annotated
// to the wire arcs from the input port pin to the load pins.
void
GraphDelayCalc1::findInputArcDelay(LibertyCell *drvr_cell,
				   const Pin *drvr_pin,
				   Vertex *drvr_vertex,
				   TimingArc *arc,
				   float from_slew,
				   DcalcAnalysisPt *dcalc_ap)
{
  debugPrint5(debug_, "delay_calc", 3, "  %s %s -> %s %s (%s)\n",
	      arc->from()->name(),
	      arc->fromTrans()->asString(),
	      arc->to()->name(),
	      arc->toTrans()->asString(),
	      arc->role()->asString());
  RiseFall *drvr_rf = arc->toTrans()->asRiseFall();
  if (drvr_rf) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    const Pvt *pvt = dcalc_ap->operatingConditions();
    Parasitic *drvr_parasitic = arc_delay_calc_->findParasitic(drvr_pin, drvr_rf,
							       dcalc_ap);
    float load_cap = loadCap(drvr_pin, drvr_parasitic, drvr_rf, dcalc_ap);

    ArcDelay intrinsic_delay;
    Slew intrinsic_slew;
    arc_delay_calc_->gateDelay(drvr_cell, arc, Slew(from_slew),
			       0.0, 0, 0.0, pvt, dcalc_ap,
			       intrinsic_delay, intrinsic_slew);

    // For input drivers there is no instance to find a related_output_pin.
    ArcDelay gate_delay;
    Slew gate_slew;
    arc_delay_calc_->gateDelay(drvr_cell, arc,
			       Slew(from_slew), load_cap,
			       drvr_parasitic, 0.0, pvt, dcalc_ap,
			       gate_delay, gate_slew);
    ArcDelay load_delay = gate_delay - intrinsic_delay;
    debugPrint3(debug_, "delay_calc", 3,
		"    gate delay = %s intrinsic = %s slew = %s\n",
		delayAsString(gate_delay, this),
		delayAsString(intrinsic_delay, this),
		delayAsString(gate_slew, this));
    graph_->setSlew(drvr_vertex, drvr_rf, ap_index, gate_slew);
    annotateLoadDelays(drvr_vertex, drvr_rf, load_delay, false, dcalc_ap,
		       arc_delay_calc_);
  }
}

void
GraphDelayCalc1::findVertexDelay(Vertex *vertex,
				 ArcDelayCalc *arc_delay_calc,
				 bool propagate)
{
  const Pin *pin = vertex->pin();
  bool ideal_clks_changed = findIdealClks(vertex);
  // Don't clobber root slews.
  if (!vertex->isRoot()) {
    debugPrint2(debug_, "delay_calc", 2, "find delays %s (%s)\n",
		vertex->name(sdc_network_),
		network_->cellName(network_->instance(pin)));
    if (network_->isLeaf(pin)) {
      if (vertex->isDriver(network_)) {
	bool delay_changed = findDriverDelays(vertex, arc_delay_calc);
	if (propagate) {
	  if (network_->direction(pin)->isInternal())
	    enqueueTimingChecksEdges(vertex);
	  // Enqueue adjacent vertices even if the delays did not
	  // change when non-incremental to stride past annotations.
	  if (delay_changed || ideal_clks_changed || !incremental_)
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
GraphDelayCalc1::enqueueTimingChecksEdges(Vertex *vertex)
{
  if (vertex->hasChecks()
      || vertex->isCheckClk()) {
    UniqueLock lock(check_vertices_lock_);
    invalid_checks_.insert(vertex);
  }
}

bool
GraphDelayCalc1::findDriverDelays(Vertex *drvr_vertex,
				  ArcDelayCalc *arc_delay_calc)
{
  bool delay_changed = false;
  MultiDrvrNet *multi_drvr = multiDrvrNet(drvr_vertex);
  if (multi_drvr) {
    Vertex *dcalc_drvr = multi_drvr->dcalcDrvr();
    if (drvr_vertex == dcalc_drvr) {
      bool init_load_slews = true;
      VertexSet::Iterator drvr_iter(multi_drvr->drvrs());
      while (drvr_iter.hasNext()) {
	Vertex *drvr_vertex = drvr_iter.next();
	// Only init load slews once so previous driver dcalc results
	// aren't clobbered.
	delay_changed |= findDriverDelays1(drvr_vertex, init_load_slews,
					   multi_drvr, arc_delay_calc);
	init_load_slews = false;
      }
    }
  }
  else
    delay_changed = findDriverDelays1(drvr_vertex, true, nullptr, arc_delay_calc);
  arc_delay_calc->finishDrvrPin();
  return delay_changed;
}

bool
GraphDelayCalc1::findDriverDelays1(Vertex *drvr_vertex,
				   bool init_load_slews,
				   MultiDrvrNet *multi_drvr,
				   ArcDelayCalc *arc_delay_calc)
{
  const Pin *drvr_pin = drvr_vertex->pin();
  Instance *drvr_inst = network_->instance(drvr_pin);
  LibertyCell *drvr_cell = network_->libertyCell(drvr_inst);
  initSlew(drvr_vertex);
  initWireDelays(drvr_vertex, init_load_slews);
  bool delay_changed = false;
  VertexInEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Vertex *from_vertex = edge->from(graph_);
    // Don't let disabled edges set slews that influence downstream delays.
    if (search_pred_->searchFrom(from_vertex)
	&& search_pred_->searchThru(edge))
      delay_changed |= findDriverEdgeDelays(drvr_cell, drvr_inst, drvr_pin,
					    drvr_vertex, multi_drvr, edge,
					    arc_delay_calc);
  }
  if (delay_changed && observer_)
    observer_->delayChangedTo(drvr_vertex);
  return delay_changed;
}

// Init slews to zero on root vertices that are not inputs, such as
// floating input pins.
void
GraphDelayCalc1::initRootSlews(Vertex *vertex)
{
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    const MinMax *slew_min_max = dcalc_ap->slewMinMax();
    DcalcAPIndex ap_index = dcalc_ap->index();
    for (auto tr : RiseFall::range()) {
      if (!vertex->slewAnnotated(tr, slew_min_max))
	graph_->setSlew(vertex, tr, ap_index, default_slew);
    }
  }
}

bool
GraphDelayCalc1::findDriverEdgeDelays(LibertyCell *drvr_cell,
				      Instance *drvr_inst,
				      const Pin *drvr_pin,
				      Vertex *drvr_vertex,
				      MultiDrvrNet *multi_drvr,
				      Edge *edge,
				      ArcDelayCalc *arc_delay_calc)
{
  Vertex *in_vertex = edge->from(graph_);
  TimingArcSet *arc_set = edge->timingArcSet();
  const LibertyPort *related_out_port = arc_set->relatedOut();
  const Pin *related_out_pin = 0;
  bool delay_changed = false;
  if (related_out_port)
    related_out_pin = network_->findPin(drvr_inst, related_out_port);
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    const Pvt *pvt = sdc_->pvt(drvr_inst, dcalc_ap->constraintMinMax());
    if (pvt == nullptr)
      pvt = dcalc_ap->operatingConditions();
    TimingArcSetArcIterator arc_iter(arc_set);
    while (arc_iter.hasNext()) {
      TimingArc *arc = arc_iter.next();
      const RiseFall *rf = arc->toTrans()->asRiseFall();
      Parasitic *parasitic = arc_delay_calc->findParasitic(drvr_pin, rf,
							   dcalc_ap);
      float related_out_cap = 0.0;
      if (related_out_pin) {
	Parasitic *related_out_parasitic = 
	  arc_delay_calc->findParasitic(related_out_pin, rf, dcalc_ap);
	related_out_cap = loadCap(related_out_pin,
				  related_out_parasitic,
				  rf, dcalc_ap);
      }
      delay_changed |= findArcDelay(drvr_cell, drvr_pin, drvr_vertex,
				    multi_drvr, arc, parasitic,
				    related_out_cap,
				    in_vertex, edge, pvt, dcalc_ap,
				    arc_delay_calc);
    }
  }

  if (delay_changed && observer_) {
    observer_->delayChangedFrom(in_vertex);
    observer_->delayChangedFrom(drvr_vertex);
  }
  return delay_changed;
}

float
GraphDelayCalc1::loadCap(const Pin *drvr_pin,
			 const DcalcAnalysisPt *dcalc_ap) const
{
  const MinMax *min_max = dcalc_ap->constraintMinMax();
  float load_cap = 0.0;
  for (auto drvr_rf : RiseFall::range()) {
    Parasitic *drvr_parasitic = arc_delay_calc_->findParasitic(drvr_pin, drvr_rf,
							       dcalc_ap);
    float cap = loadCap(drvr_pin, nullptr, drvr_parasitic, drvr_rf, dcalc_ap);
    if (min_max->compare(cap, load_cap))
      load_cap = cap;
  }
  return load_cap;
}

float
GraphDelayCalc1::loadCap(const Pin *drvr_pin,
			 const RiseFall *drvr_rf,
			 const DcalcAnalysisPt *dcalc_ap) const
{
  Parasitic *drvr_parasitic = arc_delay_calc_->findParasitic(drvr_pin, drvr_rf,
							     dcalc_ap);
  float cap = loadCap(drvr_pin, nullptr, drvr_parasitic, drvr_rf, dcalc_ap);
  return cap;
}

float
GraphDelayCalc1::loadCap(const Pin *drvr_pin,
			 Parasitic *drvr_parasitic,
			 const RiseFall *rf,
			 const DcalcAnalysisPt *dcalc_ap) const
{
  return loadCap(drvr_pin, nullptr, drvr_parasitic, rf, dcalc_ap);
}

float
GraphDelayCalc1::loadCap(const Pin *drvr_pin,
			 MultiDrvrNet *multi_drvr,
			 Parasitic *drvr_parasitic,
			 const RiseFall *rf,
			 const DcalcAnalysisPt *dcalc_ap) const
{
  float pin_cap, wire_cap;
  bool has_set_load;
  float fanout;
  if (multi_drvr)
    multi_drvr->netCaps(rf, dcalc_ap,
			pin_cap, wire_cap, fanout, has_set_load);
  else
    netCaps(drvr_pin, rf, dcalc_ap,
	    pin_cap, wire_cap, fanout, has_set_load);
  loadCap(drvr_parasitic, has_set_load, pin_cap, wire_cap);
  return wire_cap + pin_cap;
}

void
GraphDelayCalc1::loadCap(const Pin *drvr_pin,
			 Parasitic *drvr_parasitic,
			 const RiseFall *rf,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 float &pin_cap,
			 float &wire_cap) const
{
  bool has_set_load;
  float fanout;
  // Find pin and external pin/wire capacitance.
  netCaps(drvr_pin, rf, dcalc_ap,
	  pin_cap, wire_cap, fanout, has_set_load);
  loadCap(drvr_parasitic, has_set_load, pin_cap, wire_cap);
}

void
GraphDelayCalc1::loadCap(Parasitic *drvr_parasitic,
			 bool has_set_load,
			 // Return values.
			 float &pin_cap,
			 float &wire_cap) const
{
  // set_load has precidence over parasitics.
  if (!has_set_load && drvr_parasitic) {
    if (parasitics_->isParasiticNetwork(drvr_parasitic))
      wire_cap += parasitics_->capacitance(drvr_parasitic);
    else {
      // PiModel includes both pin and external caps.
      float cap = parasitics_->capacitance(drvr_parasitic);
      if (pin_cap > cap) {
	pin_cap = 0.0;
	wire_cap = cap;
      }
      else
	wire_cap = cap - pin_cap;
    }
  }
}

void
GraphDelayCalc1::netCaps(const Pin *drvr_pin,
			 const RiseFall *rf,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 float &pin_cap,
			 float &wire_cap,
			 float &fanout,
			 bool &has_set_load) const
{
  MultiDrvrNet *multi_drvr = 0;
  if (graph_) {
    Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
    multi_drvr = multiDrvrNet(drvr_vertex);
  }
  if (multi_drvr)
    multi_drvr->netCaps(rf, dcalc_ap,
			pin_cap, wire_cap, fanout, has_set_load);
  else {
    const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
    const Corner *corner = dcalc_ap->corner();
    const MinMax *min_max = dcalc_ap->constraintMinMax();
    // Find pin and external pin/wire capacitance.
    sdc_->connectedCap(drvr_pin, rf, op_cond, corner, min_max,
		       pin_cap, wire_cap, fanout, has_set_load);
  }
}

void
GraphDelayCalc1::initSlew(Vertex *vertex)
{
  for (auto tr : RiseFall::range()) {
    for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
      const MinMax *slew_min_max = dcalc_ap->slewMinMax();
      if (!vertex->slewAnnotated(tr, slew_min_max)) {
	DcalcAPIndex ap_index = dcalc_ap->index();
	graph_->setSlew(vertex, tr, ap_index, slew_min_max->initValue());
      }
    }
  }
}

// Init wire delays and load slews.
void
GraphDelayCalc1::initWireDelays(Vertex *drvr_vertex,
				bool init_load_slews)
{
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
	const MinMax *delay_min_max = dcalc_ap->delayMinMax();
	const MinMax *slew_min_max = dcalc_ap->slewMinMax();
	Delay delay_init_value(delay_min_max->initValue());
	Slew slew_init_value(slew_min_max->initValue());
	DcalcAPIndex ap_index = dcalc_ap->index();
	for (auto tr : RiseFall::range()) {
	  if (!graph_->wireDelayAnnotated(wire_edge, tr, ap_index))
	    graph_->setWireArcDelay(wire_edge, tr, ap_index, delay_init_value);
	  // Init load vertex slew.
	  if (init_load_slews
	      && !load_vertex->slewAnnotated(tr, slew_min_max))
	    graph_->setSlew(load_vertex, tr, ap_index, slew_init_value);
	}
      }
    }
  }
}

// Call the arc delay calculator to find the delay thru a single gate
// input to output timing arc, the wire delays from the gate output to
// each load pin, and the slew at each load pin.  Annotate the graph
// with the results.
bool
GraphDelayCalc1::findArcDelay(LibertyCell *drvr_cell,
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
			      ArcDelayCalc *arc_delay_calc)
{
  bool delay_changed = false;
  RiseFall *from_rf = arc->fromTrans()->asRiseFall();
  RiseFall *drvr_rf = arc->toTrans()->asRiseFall();
  if (from_rf && drvr_rf) {
    DcalcAPIndex ap_index = dcalc_ap->index();
    debugPrint7(debug_, "delay_calc", 3,
		"  %s %s -> %s %s (%s) corner:%s/%s\n",
		arc->from()->name(),
		arc->fromTrans()->asString(),
		arc->to()->name(),
		arc->toTrans()->asString(),
		arc->role()->asString(),
		dcalc_ap->corner()->name(),
		dcalc_ap->delayMinMax()->asString());
    // Delay calculation is done even when the gate delays/slews are
    // annotated because the wire delays may not be annotated.
    const Slew from_slew = edgeFromSlew(from_vertex, from_rf, edge, dcalc_ap);
    ArcDelay gate_delay;
    Slew gate_slew;
    if (multi_drvr
	&& network_->direction(drvr_pin)->isOutput())
      multiDrvrGateDelay(multi_drvr, drvr_cell, drvr_pin, arc,
			 pvt, dcalc_ap, from_slew, drvr_parasitic,
			 related_out_cap,
			 arc_delay_calc,
			 gate_delay, gate_slew);
    else {
      float load_cap = loadCap(drvr_pin, multi_drvr, drvr_parasitic,
			       drvr_rf, dcalc_ap);
      arc_delay_calc->gateDelay(drvr_cell, arc,
				from_slew, load_cap, drvr_parasitic,
				related_out_cap, pvt, dcalc_ap,
				gate_delay, gate_slew);
    }
    debugPrint2(debug_, "delay_calc", 3,
		"    gate delay = %s slew = %s\n",
		delayAsString(gate_delay, this),
		delayAsString(gate_slew, this));
    // Merge slews.
    const Slew &drvr_slew = graph_->slew(drvr_vertex, drvr_rf, ap_index);
    const MinMax *slew_min_max = dcalc_ap->slewMinMax();
    if (fuzzyGreater(gate_slew, drvr_slew, dcalc_ap->slewMinMax())
	&& !drvr_vertex->slewAnnotated(drvr_rf, slew_min_max))
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
    annotateLoadDelays(drvr_vertex, drvr_rf, delay_zero, true, dcalc_ap,
		       arc_delay_calc);
  }
  return delay_changed;
}

void
GraphDelayCalc1::multiDrvrGateDelay(MultiDrvrNet *multi_drvr,
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
				    Slew &gate_slew)
{
  ArcDelay intrinsic_delay;
  Slew intrinsic_slew;
  arc_delay_calc->gateDelay(drvr_cell, arc, from_slew,
			    0.0, 0, 0.0, pvt, dcalc_ap,
			    intrinsic_delay, intrinsic_slew);
  ArcDelay parallel_delay;
  Slew parallel_slew;
  const RiseFall *drvr_rf = arc->toTrans()->asRiseFall();
  multi_drvr->parallelDelaySlew(drvr_rf, dcalc_ap, arc_delay_calc, this,
				parallel_delay, parallel_slew);

  gate_delay = parallel_delay + intrinsic_delay;
  gate_slew = parallel_slew;

  float load_cap = loadCap(drvr_pin, multi_drvr, drvr_parasitic,
			   drvr_rf, dcalc_ap);
  Delay gate_delay1;
  Slew gate_slew1;
  arc_delay_calc->gateDelay(drvr_cell, arc,
			    from_slew, load_cap, drvr_parasitic,
			    related_out_cap, pvt, dcalc_ap,
			    gate_delay1, gate_slew1);
  float factor = delayRatio(gate_slew, gate_slew1);
  arc_delay_calc->setMultiDrvrSlewFactor(factor);
}

void
GraphDelayCalc1::findMultiDrvrGateDelay(MultiDrvrNet *multi_drvr,
					const RiseFall *drvr_rf,
					const Pvt *pvt,
					const DcalcAnalysisPt *dcalc_ap,
					ArcDelayCalc *arc_delay_calc,
					// Return values.
					ArcDelay &parallel_delay,
					Slew &parallel_slew)
{
  ArcDelay delay_sum = 1.0;
  Slew slew_sum = 1.0;
  VertexSet::Iterator drvr_iter(multi_drvr->drvrs());
  while (drvr_iter.hasNext()) {
    Vertex *drvr_vertex1 = drvr_iter.next();
    Pin *drvr_pin1 = drvr_vertex1->pin();
    Instance *drvr_inst1 = network_->instance(drvr_pin1);
    LibertyCell *drvr_cell1 = network_->libertyCell(drvr_inst1);
    if (network_->isDriver(drvr_pin1)) {
      VertexInEdgeIterator edge_iter(drvr_vertex1, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge1 = edge_iter.next();
	TimingArcSet *arc_set1 = edge1->timingArcSet();
	const LibertyPort *related_out_port = arc_set1->relatedOut();
	TimingArcSetArcIterator arc_iter(arc_set1);
	while (arc_iter.hasNext()) {
	  TimingArc *arc1 = arc_iter.next();
	  RiseFall *drvr_rf1 = arc1->toTrans()->asRiseFall();
	  if (drvr_rf1 == drvr_rf) {
	    Vertex *from_vertex1 = edge1->from(graph_);
	    RiseFall *from_rf1 = arc1->fromTrans()->asRiseFall();
	    Slew from_slew1 = edgeFromSlew(from_vertex1, from_rf1, edge1, dcalc_ap);
	    ArcDelay intrinsic_delay1;
	    Slew intrinsic_slew1;
	    arc_delay_calc->gateDelay(drvr_cell1, arc1, from_slew1,
				      0.0, 0, 0.0, pvt, dcalc_ap,
				      intrinsic_delay1, intrinsic_slew1);
	    Parasitic *parasitic1 =
	      arc_delay_calc->findParasitic(drvr_pin1, drvr_rf1, dcalc_ap);
	    const Pin *related_out_pin1 = 0;
	    float related_out_cap1 = 0.0;
	    if (related_out_port) {
	      Instance *inst1 = network_->instance(drvr_pin1);
	      related_out_pin1 = network_->findPin(inst1, related_out_port);
	      if (related_out_pin1) {
		Parasitic *related_out_parasitic1 =
		  arc_delay_calc->findParasitic(related_out_pin1, drvr_rf,
						dcalc_ap);
		related_out_cap1 = loadCap(related_out_pin1,
					   related_out_parasitic1,
					   drvr_rf, dcalc_ap);
	      }
	    }
	    float load_cap1 = loadCap(drvr_pin1, parasitic1,
				      drvr_rf, dcalc_ap);
	    ArcDelay gate_delay1;
	    Slew gate_slew1;
	    arc_delay_calc->gateDelay(drvr_cell1, arc1,
				      from_slew1, load_cap1, parasitic1,
				      related_out_cap1, pvt, dcalc_ap,
				      gate_delay1, gate_slew1);
	    delay_sum += 1.0F / (gate_delay1 - intrinsic_delay1);
	    slew_sum += 1.0F / gate_slew1;
	  }
	}
      }
    }
  }
  parallel_delay = 1.0F / delay_sum;
  parallel_slew = 1.0F / slew_sum;
}

// Use clock slew for register/latch clk->q edges.
Slew
GraphDelayCalc1::edgeFromSlew(const Vertex *from_vertex,
			      const RiseFall *from_rf,
			      const Edge *edge,
			      const DcalcAnalysisPt *dcalc_ap)
{
  const TimingRole *role = edge->role();
  if (role->genericRole() == TimingRole::regClkToQ()
      && isIdealClk(from_vertex))
    return idealClkSlew(from_vertex, from_rf, dcalc_ap->slewMinMax());
  else
    return graph_->slew(from_vertex, from_rf, dcalc_ap->index());
}

Slew
GraphDelayCalc1::idealClkSlew(const Vertex *vertex,
			      const RiseFall *rf,
			      const MinMax *min_max)
{
  float slew = min_max->initValue();
  const ClockSet *clks = idealClks(vertex);
  ClockSet::ConstIterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    float clk_slew = clk->slew(rf, min_max);
    if (min_max->compare(clk_slew, slew))
      slew = clk_slew;
  }
  return slew;
}

// Annotate wire arc delays and load pin slews.
// extra_delay is additional wire delay to add to delay returned
// by the delay calculator.
void
GraphDelayCalc1::annotateLoadDelays(Vertex *drvr_vertex,
				    const RiseFall *drvr_rf,
				    const ArcDelay &extra_delay,
				    bool merge,
				    const DcalcAnalysisPt *dcalc_ap,
				    ArcDelayCalc *arc_delay_calc)
{
  DcalcAPIndex ap_index = dcalc_ap->index();
  const MinMax *slew_min_max = dcalc_ap->slewMinMax();
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *wire_edge = edge_iter.next();
    if (wire_edge->isWire()) {
      Vertex *load_vertex = wire_edge->to(graph_);
      Pin *load_pin = load_vertex->pin();
      ArcDelay wire_delay;
      Slew load_slew;
      arc_delay_calc->loadDelay(load_pin, wire_delay, load_slew);
      debugPrint3(debug_, "delay_calc", 3,
		  "    %s load delay = %s slew = %s\n",
		  load_vertex->name(sdc_network_),
		  delayAsString(wire_delay, this),
		  delayAsString(load_slew, this));
      if (!load_vertex->slewAnnotated(drvr_rf, slew_min_max)) {
	if (drvr_vertex->slewAnnotated(drvr_rf, slew_min_max)) {
	  // Copy the driver slew to the load if it is annotated.
	  const Slew &drvr_slew = graph_->slew(drvr_vertex,drvr_rf,ap_index);
	  graph_->setSlew(load_vertex, drvr_rf, ap_index, drvr_slew);
	}
	else {
	  const Slew &slew = graph_->slew(load_vertex, drvr_rf, ap_index);
	  if (!merge
	      || fuzzyGreater(load_slew, slew, slew_min_max))
	    graph_->setSlew(load_vertex, drvr_rf, ap_index, load_slew);
	}
      }
      if (!graph_->wireDelayAnnotated(wire_edge, drvr_rf, ap_index)) {
	// Multiple timing arcs with the same output transition
	// annotate the same wire edges so they must be combined
	// rather than set.
	const ArcDelay &delay = graph_->wireArcDelay(wire_edge, drvr_rf,
						     ap_index);
	Delay wire_delay_extra = extra_delay + wire_delay;
	const MinMax *delay_min_max = dcalc_ap->delayMinMax();
	if (!merge
	    || fuzzyGreater(wire_delay_extra, delay, delay_min_max)) {
	  graph_->setWireArcDelay(wire_edge, drvr_rf, ap_index,
				  wire_delay_extra);
	  if (observer_)
	    observer_->delayChangedTo(load_vertex);
	}
      }
      // Enqueue bidirect driver from load vertex.
      if (sdc_->bidirectDrvrSlewFromLoad(load_pin))
	iter_->enqueue(graph_->pinDrvrVertex(load_pin));
    }
  }
}

void
GraphDelayCalc1::findCheckDelays(Vertex *vertex,
				 ArcDelayCalc *arc_delay_calc)
{
  debugPrint2(debug_, "delay_calc", 2, "find checks %s (%s)\n",
	      vertex->name(sdc_network_),
	      network_->cellName(network_->instance(vertex->pin())));
  if (vertex->hasChecks()) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->isTimingCheck())
	findCheckEdgeDelays(edge, arc_delay_calc);
    }
    if (network_->isLatchData(vertex->pin())) {
      // Latch D->Q arcs have to be re-evaled if level(D) > level(E)
      // because levelization does not traverse D->Q arcs to break loops.
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	Vertex *to_vertex = edge->to(graph_);
	if (edge->role() == TimingRole::latchDtoQ())
	  findVertexDelay(to_vertex, arc_delay_calc_, false);
      }
    }
  }
  if (vertex->isCheckClk()) {
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->isTimingCheck())
	findCheckEdgeDelays(edge, arc_delay_calc);
    }
  }
}

void
GraphDelayCalc1::findCheckEdgeDelays(Edge *edge,
				     ArcDelayCalc *arc_delay_calc)
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  TimingArcSet *arc_set = edge->timingArcSet();
  const Pin *to_pin = to_vertex->pin();
  Instance *inst = network_->instance(to_pin);
  const LibertyCell *cell = network_->libertyCell(inst);
  bool delay_changed = false;
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    RiseFall *from_rf = arc->fromTrans()->asRiseFall();
    RiseFall *to_rf = arc->toTrans()->asRiseFall();
    if (from_rf && to_rf) {
      const LibertyPort *related_out_port = arc_set->relatedOut();
      const Pin *related_out_pin = 0;
      if (related_out_port)
	related_out_pin = network_->findPin(inst, related_out_port);
      for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
	DcalcAPIndex ap_index = dcalc_ap->index();
	if (!graph_->arcDelayAnnotated(edge, arc, ap_index)) {
	  const Pvt *pvt = sdc_->pvt(inst,dcalc_ap->constraintMinMax());
	  if (pvt == nullptr)
	    pvt = dcalc_ap->operatingConditions();
	  const Slew &from_slew = checkEdgeClkSlew(from_vertex, from_rf,
						   dcalc_ap);
	  int slew_index = dcalc_ap->checkDataSlewIndex();
	  const Slew &to_slew = graph_->slew(to_vertex, to_rf, slew_index);
	  debugPrint5(debug_, "delay_calc", 3,
		      "  %s %s -> %s %s (%s)\n",
		      arc_set->from()->name(),
		      arc->fromTrans()->asString(),
		      arc_set->to()->name(),
		      arc->toTrans()->asString(),
		      arc_set->role()->asString());
	  debugPrint2(debug_, "delay_calc", 3,
		      "    from_slew = %s to_slew = %s\n",
		      delayAsString(from_slew, this),
		      delayAsString(to_slew, this));
	  float related_out_cap = 0.0;
	  if (related_out_pin) {
	    Parasitic *related_out_parasitic =
	      arc_delay_calc->findParasitic(related_out_pin, to_rf, dcalc_ap);
	    related_out_cap = loadCap(related_out_pin,
				      related_out_parasitic,
				      to_rf, dcalc_ap);
	  }
	  ArcDelay check_delay;
	  arc_delay_calc->checkDelay(cell, arc,
				     from_slew, to_slew,
				     related_out_cap,
				     pvt, dcalc_ap,
				     check_delay);
	  debugPrint1(debug_, "delay_calc", 3,
		      "    check_delay = %s\n",
		      delayAsString(check_delay, this));
	  graph_->setArcDelay(edge, arc, ap_index, check_delay);
	  delay_changed = true;
	}
      }
    }
  }

  if (delay_changed && observer_)
    observer_->checkDelayChangedTo(to_vertex);
}

// Use clock slew for timing check clock edges.
Slew
GraphDelayCalc1::checkEdgeClkSlew(const Vertex *from_vertex,
				  const RiseFall *from_rf,
				  const DcalcAnalysisPt *dcalc_ap)
{
  if (isIdealClk(from_vertex))
    return idealClkSlew(from_vertex, from_rf, dcalc_ap->checkClkSlewMinMax());
  else 
    return graph_->slew(from_vertex, from_rf, dcalc_ap->checkClkSlewIndex());
}

////////////////////////////////////////////////////////////////

bool
GraphDelayCalc1::findIdealClks(Vertex *vertex)
{
  const Pin *pin = vertex->pin();
  ClockSet *ideal_clks = nullptr;
  if (sdc_->isLeafPinClock(pin)) {
    // Seed ideal clocks pins.
    if (!sdc_->isPropagatedClock(pin)) {
      ClockSet *clks = sdc_->findLeafPinClocks(pin);
      ClockSet::ConstIterator clk_iter(clks);
      while (clk_iter.hasNext()) {
	Clock *clk = clk_iter.next();
	if (!clk->isPropagated()) {
	  if (ideal_clks == nullptr) {
	    ideal_clks = new ClockSet;
	    debugPrint1(debug_, "ideal_clks", 1, " %s\n",
			vertex->name(sdc_network_));
	  }
	  ideal_clks->insert(clk);
	  debugPrint1(debug_, "ideal_clks", 1, "  %s\n", clk->name());
	}
      }
    }
  }
  else {
    if (!sdc_->isPropagatedClock(pin)) {
      VertexInEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (clk_pred_->searchThru(edge)) {
	  Vertex *from_vertex = edge->from(graph_);
	  ClockSet *from_clks = idealClks(from_vertex);
	  if (from_clks) {
	    ClockSet::ConstIterator from_clk_iter(from_clks);
	    while (from_clk_iter.hasNext()) {
	      Clock *from_clk = from_clk_iter.next();
	      if (ideal_clks == nullptr) {
		ideal_clks = new ClockSet;
		debugPrint1(debug_, "ideal_clks", 1, " %s\n",
			    vertex->name(sdc_network_));
	      }
	      ideal_clks->insert(from_clk);
	      debugPrint1(debug_, "ideal_clks", 1, "  %s\n", from_clk->name());
	    }
	  }
	}
      }
    }
  }
  return setIdealClks(vertex, ideal_clks);
}

void
GraphDelayCalc1::clearIdealClkMap()
{
  ideal_clks_map_.deleteContentsClear();
  ideal_clks_map_next_.deleteContentsClear();
}

bool
GraphDelayCalc1::setIdealClks(const Vertex *vertex,
			      ClockSet *clks)
{
  bool changed = false;
  ClockSet *clks1 = ideal_clks_map_.findKey(vertex);
  if (!ClockSet::equal(clks, clks1)) {
    // Only lock for updates to vertex ideal clks.
    // Finding ideal clks by level means only changes at the current
    // delay calc level are changed.
    UniqueLock lock(ideal_clks_map_next_lock_);
    ideal_clks_map_next_[vertex] = clks;
    changed = true;
  }
  else
    delete clks;
  return changed;
}

ClockSet *
GraphDelayCalc1::idealClks(const Vertex *vertex)
{
  return ideal_clks_map_.findKey(vertex);
}

bool
GraphDelayCalc1::isIdealClk(const Vertex *vertex)
{
  const ClockSet *clks = idealClks(vertex);
  return clks != 0
    && clks->size() > 0;
}

float
GraphDelayCalc1::ceff(Edge *edge,
		      TimingArc *arc,
		      const DcalcAnalysisPt *dcalc_ap)
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  Instance *inst = network_->instance(to_pin);
  LibertyCell *cell = network_->libertyCell(inst);
  TimingArcSet *arc_set = edge->timingArcSet();
  float ceff = 0.0;
  const Pvt *pvt = sdc_->pvt(inst, dcalc_ap->constraintMinMax());
  if (pvt == nullptr)
    pvt = dcalc_ap->operatingConditions();
  RiseFall *from_rf = arc->fromTrans()->asRiseFall();
  RiseFall *to_rf = arc->toTrans()->asRiseFall();
  if (from_rf && to_rf) {
    const LibertyPort *related_out_port = arc_set->relatedOut();
    const Pin *related_out_pin = 0;
    if (related_out_port)
      related_out_pin = network_->findPin(inst, related_out_port);
    float related_out_cap = 0.0;
    if (related_out_pin) {
      Parasitic *related_out_parasitic =
	arc_delay_calc_->findParasitic(related_out_pin, to_rf, dcalc_ap);
      related_out_cap = loadCap(related_out_pin, related_out_parasitic,
				to_rf, dcalc_ap);
    }
    Parasitic *to_parasitic = arc_delay_calc_->findParasitic(to_pin, to_rf,
							     dcalc_ap);
    const Slew &from_slew = edgeFromSlew(from_vertex, from_rf, edge, dcalc_ap);
    float load_cap = loadCap(to_pin, to_parasitic, to_rf, dcalc_ap);
    ceff = arc_delay_calc_->ceff(cell, arc,
				 from_slew, load_cap, to_parasitic,
				 related_out_cap, pvt, dcalc_ap);
    arc_delay_calc_->finishDrvrPin();
  }
  return ceff;
}

////////////////////////////////////////////////////////////////

string *
GraphDelayCalc1::reportDelayCalc(Edge *edge,
				 TimingArc *arc,
				 const Corner *corner,
				 const MinMax *min_max,
				 int digits)
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  TimingRole *role = arc->role();
  Instance *inst = network_->instance(to_pin);
  LibertyCell *cell = network_->libertyCell(inst);
  TimingArcSet *arc_set = edge->timingArcSet();
  string *result = new string;
  DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  const Pvt *pvt = sdc_->pvt(inst, dcalc_ap->constraintMinMax());
  if (pvt == nullptr)
    pvt = dcalc_ap->operatingConditions();
  RiseFall *from_rf = arc->fromTrans()->asRiseFall();
  RiseFall *to_rf = arc->toTrans()->asRiseFall();
  if (from_rf && to_rf) {
    const LibertyPort *related_out_port = arc_set->relatedOut();
    const Pin *related_out_pin = 0;
    if (related_out_port)
      related_out_pin = network_->findPin(inst, related_out_port);
    float related_out_cap = 0.0;
    if (related_out_pin) {
      Parasitic *related_out_parasitic = 
	arc_delay_calc_->findParasitic(related_out_pin, to_rf, dcalc_ap);
      related_out_cap = loadCap(related_out_pin, related_out_parasitic,
				to_rf, dcalc_ap);
    }
    if (role->isTimingCheck()) {
      const Slew &from_slew = checkEdgeClkSlew(from_vertex, from_rf, dcalc_ap);
      int slew_index = dcalc_ap->checkDataSlewIndex();
      const Slew &to_slew = graph_->slew(to_vertex, to_rf, slew_index);
      bool from_ideal_clk = isIdealClk(from_vertex);
      const char *from_slew_annotation = from_ideal_clk ? " (ideal clock)" : nullptr;
      arc_delay_calc_->reportCheckDelay(cell, arc, from_slew, from_slew_annotation,
					to_slew, related_out_cap, pvt, dcalc_ap,
					digits, result);
    }
    else {
      Parasitic *to_parasitic =
	arc_delay_calc_->findParasitic(to_pin, to_rf, dcalc_ap);
      const Slew &from_slew = edgeFromSlew(from_vertex, from_rf, edge, dcalc_ap);
      float load_cap = loadCap(to_pin, to_parasitic, to_rf, dcalc_ap);
      arc_delay_calc_->reportGateDelay(cell, arc,
				       from_slew, load_cap, to_parasitic,
				       related_out_cap, pvt, dcalc_ap,
				       digits, result);
    }
    arc_delay_calc_->finishDrvrPin();
  }
  return result;
}

} // namespace
