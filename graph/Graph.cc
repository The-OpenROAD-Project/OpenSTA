// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "Graph.hh"

#include "Debug.hh"
#include "Stats.hh"
#include "MinMax.hh"
#include "Mutex.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "DcalcAnalysisPt.hh"
#include "FuncExpr.hh"

namespace sta {

using std::string;

////////////////////////////////////////////////////////////////
//
// Graph
//
////////////////////////////////////////////////////////////////

Graph::Graph(StaState *sta,
	     int slew_rf_count,
	     DcalcAPIndex ap_count) :
  StaState(sta),
  vertices_(nullptr),
  edges_(nullptr),
  slew_rf_count_(slew_rf_count),
  ap_count_(ap_count),
  period_check_annotations_(nullptr),
  reg_clk_vertices_(new VertexSet(graph_))
{
  // For the benifit of reg_clk_vertices_ that references graph_.
  graph_ = this;
}

Graph::~Graph()
{
  edges_->clear();
  delete edges_;
  vertices_->clear();
  delete vertices_;
  delete reg_clk_vertices_;
  removePeriodCheckAnnotations();
}

void
Graph::makeGraph()
{
  Stats stats(debug_, report_);
  makeVerticesAndEdges();
  makeWireEdges();
  stats.report("Make graph");
}

// Make vertices for each pin.
// Iterate over instances and top level port pins rather than nets
// because network may not connect floating pins to a net
// (ie, Intime occurence tree bleachery).
void
Graph::makeVerticesAndEdges()
{
  vertices_ = new VertexTable;
  edges_ = new EdgeTable;

  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    makePinVertices(inst);
    makeInstanceEdges(inst);
  }
  delete leaf_iter;
  makePinVertices(network_->topInstance());
}

class FindNetDrvrLoadCounts : public PinVisitor
{
public:
  FindNetDrvrLoadCounts(Pin *drvr_pin,
			PinSet &visited_drvrs,
			int &drvr_count,
			int &bidirect_count,
			int &load_count,
			const Network *network);
  virtual void operator()(const Pin *pin);

protected:
  Pin *drvr_pin_;
  PinSet &visited_drvrs_;
  int &drvr_count_;
  int &bidirect_count_;
  int &load_count_;
  const Network *network_;
};

FindNetDrvrLoadCounts::FindNetDrvrLoadCounts(Pin *drvr_pin,
					     PinSet &visited_drvrs,
					     int &drvr_count,
					     int &bidirect_count,
					     int &load_count,
					     const Network *network) :
  drvr_pin_(drvr_pin),
  visited_drvrs_(visited_drvrs),
  drvr_count_(drvr_count),
  bidirect_count_(bidirect_count),
  load_count_(load_count),
  network_(network)
{
}

void
FindNetDrvrLoadCounts::operator()(const Pin *pin)
{
  if (network_->isDriver(pin)) {
    if (pin != drvr_pin_)
      visited_drvrs_.insert(pin);
    if (network_->direction(pin)->isBidirect())
      bidirect_count_++;
    else
      drvr_count_++;
  }
  if (network_->isLoad(pin))
    load_count_++;
}

void
Graph::makePinVertices(const Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    makePinVertices(pin);
  }
  delete pin_iter;
}

// Make edges corresponding to library timing arcs.
void
Graph::makeInstanceEdges(const Instance *inst)
{
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell)
    makePortInstanceEdges(inst, cell, nullptr);
}

void
Graph::makePinInstanceEdges(const Pin *pin)
{
  const Instance *inst = network_->instance(pin);
  if (inst) {
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      LibertyPort *port = network_->libertyPort(pin);
      makePortInstanceEdges(inst, cell, port);
    }
  }
}

void
Graph::makePortInstanceEdges(const Instance *inst,
			     LibertyCell *cell,
			     LibertyPort *from_to_port)
{
  for (TimingArcSet *arc_set : cell->timingArcSets()) {
    LibertyPort *from_port = arc_set->from();
    LibertyPort *to_port = arc_set->to();
    if ((from_to_port == nullptr
	 || from_port == from_to_port
	 || to_port == from_to_port)
	&& from_port) {
      Pin *from_pin = network_->findPin(inst, from_port);
      Pin *to_pin = network_->findPin(inst, to_port);
      if (from_pin && to_pin) {
	Vertex *from_vertex, *from_bidirect_drvr_vertex;
	Vertex *to_vertex, *to_bidirect_drvr_vertex;
	pinVertices(from_pin, from_vertex, from_bidirect_drvr_vertex);
	pinVertices(to_pin, to_vertex, to_bidirect_drvr_vertex);
	// From pin and/or to pin can be bidirect.
	//  For combinational arcs edge is to driver.
	//  For timing checks edge is to load.
	// Vertices can be missing from the graph if the pins
	// are power or ground.
	if (from_vertex) {
          const TimingRole *role = arc_set->role();
  	  bool is_check = role->isTimingCheckBetween();
	  if (to_bidirect_drvr_vertex && !is_check)
	    makeEdge(from_vertex, to_bidirect_drvr_vertex, arc_set);
	  else if (to_vertex) {
	    makeEdge(from_vertex, to_vertex, arc_set);
	    if (is_check) {
	      to_vertex->setHasChecks(true);
	      from_vertex->setIsCheckClk(true);
	    }
	  }
	  if (from_bidirect_drvr_vertex && to_vertex) {
	    // Internal path from bidirect output back into the
	    // instance.
	    Edge *edge = makeEdge(from_bidirect_drvr_vertex, to_vertex,
				  arc_set);
	    edge->setIsBidirectInstPath(true);
	  }
	}
      }
    }
  }
}

void
Graph::makeWireEdges()
{
  PinSet visited_drvrs(network_);
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    makeInstDrvrWireEdges(inst, visited_drvrs);
  }
  delete inst_iter;
  makeInstDrvrWireEdges(network_->topInstance(), visited_drvrs);
}

void
Graph::makeInstDrvrWireEdges(const Instance *inst,
			     PinSet &visited_drvrs)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)
	&& !visited_drvrs.hasKey(pin))
      makeWireEdgesFromPin(pin, visited_drvrs);
  }
  delete pin_iter;
}

void
Graph::makeWireEdgesFromPin(const Pin *drvr_pin)
{
  PinSeq loads, drvrs;
  PinSet visited_drvrs(network_);
  FindNetDrvrLoads visitor(drvr_pin, visited_drvrs, loads, drvrs, network_);
  network_->visitConnectedPins(drvr_pin, visitor);

  for (auto load_pin : loads) {
    if (drvr_pin != load_pin)
      makeWireEdge(drvr_pin, load_pin);
  }
}

void
Graph::makeWireEdgesFromPin(const Pin *drvr_pin,
			    PinSet &visited_drvrs)
{
  // Find all drivers and loads on the net to avoid N*M run time
  // for large fanin/fanout nets.
  PinSeq drvrs, loads;
  FindNetDrvrLoads visitor(drvr_pin, visited_drvrs, loads, drvrs, network_);
  network_->visitConnectedPins(drvr_pin, visitor);

  if (isIsolatedNet(drvrs, loads)) {
    for (auto drvr_pin : drvrs) {
      visited_drvrs.insert(drvr_pin);
      debugPrint(debug_, "graph", 1, "ignoring isolated driver %s",
                 network_->pathName(drvr_pin));
    }
    return;
  }

  for (auto drvr_pin : drvrs) {
    for (auto load_pin : loads) {
      if (drvr_pin != load_pin)
	makeWireEdge(drvr_pin, load_pin);
    }
  }
}

// Check for nets with bidirect drivers that have no fanin or
// fanout. One example of these nets are bidirect pad ring pins
// are connected together but have no function but are marked
// as signal nets.
// These nets tickle N^2 behaviors that have no function.
bool
Graph::isIsolatedNet(PinSeq &drvrs,
                     PinSeq &loads) const
{
  if (drvrs.size() < 10)
    return false;
  // Check that all drivers have no fanin.
  for (auto drvr_pin : drvrs) {
    Vertex *drvr_vertex = pinDrvrVertex(drvr_pin);
    if (network_->isTopLevelPort(drvr_pin)
        || drvr_vertex->hasFanin())
      return false;
  }
  // Check for fanout on the load pins.
  for (auto load_pin : loads) {
    Vertex *load_vertex = pinLoadVertex(load_pin);
    if (load_vertex->hasFanout()
        || load_vertex->hasChecks()) {
      return false;
    }
  }
  return true;
}

void
Graph::makeWireEdgesToPin(const Pin *to_pin)
{
  PinSet *drvrs = network_->drivers(to_pin);
  if (drvrs) {
    for (auto drvr : *drvrs) {
      if (drvr != to_pin)
	makeWireEdge(drvr, to_pin);
    }
  }
}

class MakeEdgesThruHierPin : public HierPinThruVisitor
{
public:
  MakeEdgesThruHierPin(Graph *graph);

private:
  virtual void visit(const Pin *drvr,
		     const Pin *load);

  Graph *graph_;
};

MakeEdgesThruHierPin::MakeEdgesThruHierPin(Graph *graph) :
  HierPinThruVisitor(),
  graph_(graph)
{
}

void
MakeEdgesThruHierPin::visit(const Pin *drvr,
			    const Pin *load)
{
  graph_->makeWireEdge(drvr, load);
}

void
Graph::makeWireEdgesThruPin(const Pin *hpin)
{
  MakeEdgesThruHierPin visitor(this);
  visitDrvrLoadsThruHierPin(hpin, network_, &visitor);
}

void
Graph::makeWireEdge(const Pin *from_pin,
		    const Pin *to_pin)
{
  TimingArcSet *arc_set = TimingArcSet::wireTimingArcSet();
  Vertex *from_vertex, *from_bidirect_drvr_vertex;
  pinVertices(from_pin, from_vertex, from_bidirect_drvr_vertex);
  Vertex *to_vertex = pinLoadVertex(to_pin);
  if (from_vertex && to_vertex) {
    // From and/or to can be bidirect, but edge is always from driver to load.
    if (from_bidirect_drvr_vertex)
      makeEdge(from_bidirect_drvr_vertex, to_vertex, arc_set);
    else
      makeEdge(from_vertex, to_vertex, arc_set);
  }
}

////////////////////////////////////////////////////////////////

Vertex *
Graph::vertex(VertexId vertex_id) const
{
  return vertices_->pointer(vertex_id);
}

VertexId
Graph::id(const Vertex *vertex) const
{
  return vertices_->objectId(vertex);
}

void
Graph::makePinVertices(Pin *pin)
{
  Vertex *vertex, *bidir_drvr_vertex;
  makePinVertices(pin, vertex, bidir_drvr_vertex);
}

void
Graph::makePinVertices(Pin *pin,
		       Vertex *&vertex,
		       Vertex *&bidir_drvr_vertex)
{
  PortDirection *dir = network_->direction(pin);
  if (!dir->isPowerGround()) {
    bool is_reg_clk = network_->isRegClkPin(pin);
    vertex = makeVertex(pin, false, is_reg_clk);
    network_->setVertexId(pin, id(vertex));
    if (dir->isBidirect()) {
      bidir_drvr_vertex = makeVertex(pin, true, is_reg_clk);
      pin_bidirect_drvr_vertex_map_[pin] = bidir_drvr_vertex;
    }
    else
      bidir_drvr_vertex = nullptr;
  }
}

Vertex *
Graph::makeVertex(Pin *pin,
		  bool is_bidirect_drvr,
		  bool is_reg_clk)
{
  Vertex *vertex = vertices_->make();
  vertex->init(pin, is_bidirect_drvr, is_reg_clk);
  initSlews(vertex);
  if (is_reg_clk)
    reg_clk_vertices_->insert(vertex);
  return vertex;
}

void
Graph::pinVertices(const Pin *pin,
		   // Return values.
		   Vertex *&vertex,
		   Vertex *&bidirect_drvr_vertex)  const
{
  vertex = Graph::vertex(network_->vertexId(pin));
  if (network_->direction(pin)->isBidirect())
    bidirect_drvr_vertex = pin_bidirect_drvr_vertex_map_.findKey(pin);
  else
    bidirect_drvr_vertex = nullptr;
}

Vertex *
Graph::pinDrvrVertex(const Pin *pin) const
{
  if (network_->direction(pin)->isBidirect())
    return pin_bidirect_drvr_vertex_map_.findKey(pin);
  else
    return Graph::vertex(network_->vertexId(pin));
}

Vertex *
Graph::pinLoadVertex(const Pin *pin) const
{
  return vertex(network_->vertexId(pin));
}

void
Graph::deleteVertex(Vertex *vertex)
{
  if (vertex->isRegClk())
    reg_clk_vertices_->erase(vertex);
  Pin *pin = vertex->pin_;
  if (vertex->isBidirectDriver())
    pin_bidirect_drvr_vertex_map_.erase(pin_bidirect_drvr_vertex_map_
					.find(pin));
  else
    network_->setVertexId(pin, vertex_id_null);
  // Delete edges to vertex.
  EdgeId edge_id, next_id;
  for (edge_id = vertex->in_edges_; edge_id; edge_id = next_id) {
    Edge *edge = Graph::edge(edge_id);
    next_id = edge->vertex_in_link_;
    deleteOutEdge(edge->from(this), edge);
    edge->clear();
    edges_->destroy(edge);
  }
  // Delete edges from vertex.
  for (edge_id = vertex->out_edges_; edge_id; edge_id = next_id) {
    Edge *edge = Graph::edge(edge_id);
    next_id = edge->vertex_out_next_;
    deleteInEdge(edge->to(this), edge);
    edge->clear();
    edges_->destroy(edge);
  }
  vertex->clear();
  vertices_->destroy(vertex);
}

bool
Graph::hasFaninOne(Vertex *vertex) const
{
  return vertex->in_edges_
    && edge(vertex->in_edges_)->vertex_in_link_ == 0;
}

void
Graph::deleteInEdge(Vertex *vertex,
		    Edge *edge)
{
  EdgeId edge_id = id(edge);
  EdgeId prev = 0;
  for (EdgeId i = vertex->in_edges_;
       i && i != edge_id;
       i = Graph::edge(i)->vertex_in_link_)
    prev = i;
  if (prev)
    Graph::edge(prev)->vertex_in_link_ = edge->vertex_in_link_;
  else
    vertex->in_edges_ = edge->vertex_in_link_;
}

void
Graph::deleteOutEdge(Vertex *vertex,
		     Edge *edge)
{
  EdgeId next = edge->vertex_out_next_;
  EdgeId prev = edge->vertex_out_prev_;
  if (prev)
    Graph::edge(prev)->vertex_out_next_ = next;
  else
    vertex->out_edges_ = next;
  if (next)
    Graph::edge(next)->vertex_out_prev_ = prev;
}

void
Graph::gateEdgeArc(const Pin *in_pin,
                   const RiseFall *in_rf,
                   const Pin *drvr_pin,
                   const RiseFall *drvr_rf,
                   // Return values.
                   Edge *&edge,
                   const TimingArc *&arc) const
{
  Vertex *in_vertex = pinLoadVertex(in_pin);
  Vertex *drvr_vertex = pinDrvrVertex(drvr_pin);
  // Iterate over load drivers to avoid driver fanout^2.
  VertexInEdgeIterator edge_iter(drvr_vertex, this);
  while (edge_iter.hasNext()) {
    Edge *edge1 = edge_iter.next();
    if (edge1->from(this) == in_vertex) {
      TimingArcSet *arc_set = edge1->timingArcSet();
      for (TimingArc *arc1 : arc_set->arcs()) {
        if (arc1->fromEdge()->asRiseFall() == in_rf
            && arc1->toEdge()->asRiseFall() == drvr_rf) {
          edge = edge1;
          arc = arc1;
          return;
        }
      }
    }
  }
  edge = nullptr;
  arc = nullptr;
}

////////////////////////////////////////////////////////////////

Path *
Graph::makePaths(Vertex *vertex,
                 uint32_t count)
{
  Path *paths = new Path[count];
  vertex->setPaths(paths);
  return paths;
}

Path *
Graph::paths(const Vertex *vertex) const
{
  return vertex->paths();
}

void
Graph::deletePaths(Vertex *vertex)
{
  vertex->setPaths(nullptr);
  vertex->tag_group_index_ = tag_group_index_max;
}

////////////////////////////////////////////////////////////////

const Slew &
Graph::slew(const Vertex *vertex,
            const RiseFall *rf,
            DcalcAPIndex ap_index)
{
  if (slew_rf_count_) {
    const Slew *slews = vertex->slews();
    size_t slew_index = (slew_rf_count_ == 1)
      ? ap_index
      : ap_index*slew_rf_count_+rf->index();
    return slews[slew_index];
  }
  else {
    static Slew slew(0.0);
    return slew;
  }
}

void
Graph::setSlew(Vertex *vertex,
               const RiseFall *rf,
               DcalcAPIndex ap_index,
               const Slew &slew)
{
  if (slew_rf_count_) {
    Slew *slews = vertex->slews();
    if (slews == nullptr) {
      int slew_count = slew_rf_count_ * ap_count_;
      slews = new Slew[slew_count];
      vertex->setSlews(slews);
    }
    size_t slew_index = (slew_rf_count_ == 1)
      ? ap_index
      : ap_index*slew_rf_count_+rf->index();
    slews[slew_index] = slew;
  }
}

////////////////////////////////////////////////////////////////

Edge *
Graph::edge(EdgeId edge_id) const
{
  return edges_->pointer(edge_id);
}

EdgeId
Graph::id(const Edge *edge) const
{
  return edges_->objectId(edge);
}

Edge *
Graph::makeEdge(Vertex *from,
		Vertex *to,
		TimingArcSet *arc_set)
{
  Edge *edge = edges_->make();
  edge->init(id(from), id(to), arc_set);
  // Add out edge to from vertex.
  EdgeId next = from->out_edges_;
  edge->vertex_out_next_ = next;
  edge->vertex_out_prev_ = edge_id_null;
  EdgeId edge_id = edges_->objectId(edge);
  if (next)
    Graph::edge(next)->vertex_out_prev_ = edge_id;
  from->out_edges_ = edge_id;

  // Add in edge to to vertex.
  edge->vertex_in_link_ = to->in_edges_;
  to->in_edges_ = edge_id;

  initArcDelays(edge);
  return edge;
}

void
Graph::deleteEdge(Edge *edge)
{
  Vertex *from = edge->from(this);
  Vertex *to = edge->to(this);
  deleteOutEdge(from, edge);
  deleteInEdge(to, edge);
  edge->clear();
  edges_->destroy(edge);
}

ArcDelay
Graph::arcDelay(const Edge *edge,
		const TimingArc *arc,
		DcalcAPIndex ap_index) const
{
  ArcDelay *delays = edge->arcDelays();
  size_t index = arc->index() * ap_count_ + ap_index;
  return delays[index];
}

void
Graph::setArcDelay(Edge *edge,
		   const TimingArc *arc,
		   DcalcAPIndex ap_index,
		   ArcDelay delay)
{
  ArcDelay *arc_delays = edge->arcDelays();
  size_t index = arc->index() * ap_count_ + ap_index;
  arc_delays[index] = delay;
}

const ArcDelay &
Graph::wireArcDelay(const Edge *edge,
		    const RiseFall *rf,
		    DcalcAPIndex ap_index)
{
  ArcDelay *delays = edge->arcDelays();
  size_t index = rf->index() * ap_count_ + ap_index;
  return delays[index];
}

void
Graph::setWireArcDelay(Edge *edge,
		       const RiseFall *rf,
		       DcalcAPIndex ap_index,
		       const ArcDelay &delay)
{
  ArcDelay *delays = edge->arcDelays();
  size_t index = rf->index() * ap_count_ + ap_index;
  delays[index] = delay;
}

////////////////////////////////////////////////////////////////

bool
Graph::arcDelayAnnotated(const Edge *edge,
			 const TimingArc *arc,
			 DcalcAPIndex ap_index) const
{
  return edge->arcDelayAnnotated(arc, ap_index, ap_count_);
}

void
Graph::setArcDelayAnnotated(Edge *edge,
			    const TimingArc *arc,
			    DcalcAPIndex ap_index,
			    bool annotated)
{
  return edge->setArcDelayAnnotated(arc, ap_index, ap_count_, annotated);
}

bool
Graph::wireDelayAnnotated(const Edge *edge,
			  const RiseFall *rf,
			  DcalcAPIndex ap_index) const
{
  int arc_index = TimingArcSet::wireArcIndex(rf);
  TimingArc *arc = TimingArcSet::wireTimingArcSet()->findTimingArc(arc_index);
  return edge->arcDelayAnnotated(arc, ap_index, ap_count_);
}

void
Graph::setWireDelayAnnotated(Edge *edge,
			     const RiseFall *rf,
			     DcalcAPIndex ap_index,
			     bool annotated)
{
  int arc_index = TimingArcSet::wireArcIndex(rf);
  TimingArc *arc = TimingArcSet::wireTimingArcSet()->findTimingArc(arc_index);
  return edge->setArcDelayAnnotated(arc, ap_index, ap_count_, annotated);
}

void
Graph::removeDelayAnnotated(Edge *edge)
{
  edge->removeDelayAnnotated();
}

////////////////////////////////////////////////////////////////

// This only gets called if the analysis type changes from single
// to bc_wc/ocv or visa versa.
void
Graph::setDelayCount(DcalcAPIndex ap_count)
{
  if (ap_count != ap_count_) {
    // Discard any existing delays.
    removePeriodCheckAnnotations();
    ap_count_ = ap_count;
    initSlews();
  }
}

void
Graph::initSlews()
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    initSlews(vertex);

    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      initArcDelays(edge);
    }
  }
}

void
Graph::initSlews(Vertex *vertex)
{
  size_t slew_count = slewCount();
  Slew *slews = new Slew[slew_count];
  vertex->setSlews(slews);
  for (size_t i = 0; i < slew_count; i++)
    slews[i] = 0.0;
}

size_t
Graph::slewCount()
{
  return slew_rf_count_ * ap_count_;
}

void
Graph::initArcDelays(Edge *edge)
{
  size_t arc_count = edge->timingArcSet()->arcCount();
  size_t delay_count = arc_count * ap_count_;
  ArcDelay *arc_delays = new ArcDelay[delay_count];
  edge->setArcDelays(arc_delays);
  for (size_t i = 0; i < delay_count; i++)
    arc_delays[i] = 0.0;
}

bool
Graph::delayAnnotated(Edge *edge)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    for (DcalcAPIndex ap_index = 0; ap_index < ap_count_; ap_index++) {
      if (!arcDelayAnnotated(edge, arc, ap_index))
	return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////

void
Graph::minPulseWidthArc(Vertex *vertex,
                        // high = rise, low = fall
                        const RiseFall *hi_low,
                        // Return values.
                        Edge *&edge,
                        TimingArc *&arc)
{
  VertexOutEdgeIterator edge_iter(vertex, this);
  while (edge_iter.hasNext()) {
    edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set->role() == TimingRole::width()) {
      for (TimingArc *arc1 : arc_set->arcs()) {
        if (arc1->fromEdge()->asRiseFall() == hi_low) {
          arc = arc1;
          return;
        }
      }
    }
  }
  edge = nullptr;
  arc = nullptr;
}

void
Graph::minPeriodArc(Vertex *vertex,
		    const RiseFall *rf,
		    // Return values.
		    Edge *&edge,
		    TimingArc *&arc)
{
  VertexOutEdgeIterator edge_iter(vertex, this);
  while (edge_iter.hasNext()) {
    edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set->role() == TimingRole::period()) {
      for (TimingArc *arc1 : arc_set->arcs()) {
        if (arc1->fromEdge()->asRiseFall() == rf) {
          arc = arc1;
          return;
        }
      }
    }
  }
  edge = nullptr;
  arc = nullptr;
}

////////////////////////////////////////////////////////////////

void
Graph::periodCheckAnnotation(const Pin *pin,
			     DcalcAPIndex ap_index,
			     // Return values.
			     float &period,
			     bool &exists)
{
  exists = false;
  if (period_check_annotations_) {
    float *periods = period_check_annotations_->findKey(pin);
    if (periods) {
      period = periods[ap_index];
      if (period >= 0.0)
	exists = true;
    }
  }
}

void
Graph::setPeriodCheckAnnotation(const Pin *pin,
				DcalcAPIndex ap_index,
				float period)
{
  if (period_check_annotations_ == nullptr)
    period_check_annotations_ = new PeriodCheckAnnotations(network_);
  float *periods = period_check_annotations_->findKey(pin);
  if (periods == nullptr) {
    periods = new float[ap_count_];
    // Use negative (illegal) period values to indicate unannotated checks.
    for (int i = 0; i < ap_count_; i++)
      periods[i] = -1;
    (*period_check_annotations_)[pin] = periods;
  }
  periods[ap_index] = period;
}

void
Graph::removePeriodCheckAnnotations()
{
  if (period_check_annotations_) {
    for (const auto [pin, periods] : *period_check_annotations_)
      delete [] periods;
    delete period_check_annotations_;
    period_check_annotations_ = nullptr;
  }
}

void
Graph::removeDelaySlewAnnotations()
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      removeDelayAnnotated(edge);
    }
    vertex->removeSlewAnnotated();
  }
  removePeriodCheckAnnotations();
}

////////////////////////////////////////////////////////////////
//
// Vertex
//
////////////////////////////////////////////////////////////////

Vertex::Vertex()
{
  init(nullptr, false, false);
  object_idx_ = object_idx_null;
}

void
Vertex::init(Pin *pin,
	     bool is_bidirect_drvr,
	     bool is_reg_clk)
{
  pin_ = pin;
  is_reg_clk_ = is_reg_clk;
  is_bidirect_drvr_ = is_bidirect_drvr;
  in_edges_ = edge_id_null;
  out_edges_ = edge_id_null;
  slews_ = nullptr;
  paths_ = nullptr;
  tag_group_index_ = tag_group_index_max;
  slew_annotated_ = false;
  sim_value_ = unsigned(LogicValue::unknown);
  is_disabled_constraint_ = false;
  is_gated_clk_enable_ = false;
  has_checks_ = false;
  is_check_clk_ = false;
  is_constrained_ = false;
  has_downstream_clk_pin_ = false;
  level_ = 0;
  visited1_ = false;
  visited2_ = false;
  bfs_in_queue_ = 0;
}

Vertex::~Vertex()
{
  clear();
}

void
Vertex::clear()
{
  delete [] slews_;
  slews_ = nullptr;
  delete [] paths_;
  paths_ = nullptr;
}

void
Vertex::setObjectIdx(ObjectIdx idx)
{
  object_idx_ = idx;
}

string
Vertex::to_string(const StaState *sta) const
{
  const Network *network = sta->sdcNetwork();
  if (network->direction(pin_)->isBidirect()) {
    string str = network->pathName(pin_);
    str += ' ';
    str += is_bidirect_drvr_ ? "driver" : "load";
    return str;
  }
  else
    return network->pathName(pin_);
}

const char *
Vertex::name(const Network *network) const
{
  string name = to_string(network);
  return makeTmpString(name);  
}

bool
Vertex::isDriver(const Network *network) const
{
  PortDirection *dir = network->direction(pin_);
  bool top_level_port = network->isTopLevelPort(pin_);
  return ((top_level_port
	   && (dir->isInput()
	       || (dir->isBidirect()
		   && is_bidirect_drvr_)))
	  || (!top_level_port
	      && (dir->isOutput()
		  || dir->isTristate()
		  || (dir->isBidirect()
		      && is_bidirect_drvr_)
		  || dir->isInternal())));
}

void
Vertex::setLevel(Level level)
{
  level_ = level;
}

void
Vertex::setVisited(bool visited)
{
  visited1_ = visited;
}

void
Vertex::setVisited2(bool visited)
{
  visited2_ = visited;
}

void
Vertex::setSlews(Slew *slews)
{
  delete [] slews_;
  slews_ = slews;
}

bool
Vertex::slewAnnotated(const RiseFall *rf,
		      const MinMax *min_max) const
{
  int index = min_max->index() * transitionCount() + rf->index();
  return ((1 << index) & slew_annotated_) != 0;
}

bool
Vertex::slewAnnotated() const
{
  return slew_annotated_ != 0;
}

void
Vertex::setSlewAnnotated(bool annotated,
			 const RiseFall *rf,
			 DcalcAPIndex ap_index)
{
  // Track rise/fall/min/max annotations separately, but after that
  // only rise/fall.
  if (ap_index > 1)
    ap_index = 0;
  int index = ap_index * transitionCount() + rf->index();
  if (annotated)
    slew_annotated_ |= (1 << index);
  else
    slew_annotated_ &= ~(1 << index);
}

void
Vertex::removeSlewAnnotated()
{
  slew_annotated_ = 0;
}

TagGroupIndex
Vertex::tagGroupIndex() const
{
  return tag_group_index_;
}

void
Vertex::setTagGroupIndex(TagGroupIndex tag_index)
{
  tag_group_index_ = tag_index;
}

void
Vertex::setPaths(Path *paths)
{
  delete [] paths_;
  paths_ = paths;
}

LogicValue
Vertex::simValue() const
{
  return static_cast<LogicValue>(sim_value_);
}

void
Vertex::setSimValue(LogicValue value)
{
  sim_value_ = unsigned(value);
}

bool
Vertex::isConstant() const
{
  LogicValue value = static_cast<LogicValue>(sim_value_);
  return value == LogicValue::zero
    || value == LogicValue::one;
}

void
Vertex::setIsDisabledConstraint(bool disabled)
{
  is_disabled_constraint_ = disabled;
}

bool
Vertex::hasFanin() const
{
  return in_edges_ != edge_id_null;
}

bool
Vertex::hasFanout() const
{
  return out_edges_ != edge_id_null;
}

void
Vertex::setHasChecks(bool has_checks)
{
  has_checks_ = has_checks;
}

void
Vertex::setIsCheckClk(bool is_check_clk)
{
  is_check_clk_ = is_check_clk;
}

void
Vertex::setIsGatedClkEnable(bool enable)
{
  is_gated_clk_enable_ = enable;
}

void
Vertex::setIsConstrained(bool constrained)
{
  is_constrained_ = constrained;
}

void
Vertex::setHasDownstreamClkPin(bool has_clk_pin)
{
  has_downstream_clk_pin_ = has_clk_pin;
}

bool
Vertex::bfsInQueue(BfsIndex index) const
{
  return (bfs_in_queue_ >> unsigned(index)) & 1;
}

void
Vertex::setBfsInQueue(BfsIndex index,
		      bool value)
{
  if (value)
    bfs_in_queue_ |= 1 << int(index);
  else
    bfs_in_queue_ &= ~(1 << int(index));
}

////////////////////////////////////////////////////////////////
//
// Edge
//
////////////////////////////////////////////////////////////////

Edge::Edge()
{
  init(0, 0, nullptr);
  object_idx_ = object_idx_null;
}

void
Edge::init(VertexId from,
	   VertexId to,
	   TimingArcSet *arc_set)
{
  from_ = from;
  to_ = to;
  arc_set_ = arc_set;
  vertex_in_link_ = edge_id_null;
  vertex_out_next_ = edge_id_null;
  vertex_out_prev_ = edge_id_null;
  is_bidirect_inst_path_ = false;
  is_bidirect_net_path_ = false;

  arc_delays_ = nullptr;
  arc_delay_annotated_is_bits_ = true;
  arc_delay_annotated_.bits_ = 0;
  delay_annotation_is_incremental_ = false;
  sim_timing_sense_ = unsigned(TimingSense::unknown);
  is_disabled_constraint_ = false;
  is_disabled_cond_ = false;
  is_disabled_loop_ = false;
}

Edge::~Edge()
{
  clear();
}

void
Edge::clear()
{
  delete [] arc_delays_;
  arc_delays_ = nullptr;
  if (!arc_delay_annotated_is_bits_)
    delete arc_delay_annotated_.seq_;
  arc_delay_annotated_is_bits_ = true;
  arc_delay_annotated_.seq_ = nullptr;
}

void
Edge::setObjectIdx(ObjectIdx idx)
{
  object_idx_ = idx;
}

string
Edge::to_string(const StaState *sta) const
{
  const Graph *graph = sta->graph();
  string str = from(graph)->to_string(sta);
  str += " -> ";
  str += to(graph)->to_string(sta);
  FuncExpr *when = arc_set_->cond();
  if (when) {
    str += " ";
    str += when->to_string();
  }
  return str;
}

void
Edge::setTimingArcSet(TimingArcSet *set)
{
  arc_set_ = set;
}

void
Edge::setArcDelays(ArcDelay *arc_delays)
{
  delete [] arc_delays_;
  arc_delays_ = arc_delays;
}

bool
Edge::arcDelayAnnotated(const TimingArc *arc,
                        DcalcAPIndex ap_index,
                        DcalcAPIndex ap_count) const
{
  size_t index = arc->index() * ap_count + ap_index;
  if (arc_delay_annotated_is_bits_)
    return arc_delay_annotated_.bits_ & arcDelayAnnotateBit(index);
  else
    return (*arc_delay_annotated_.seq_)[index];
}

void
Edge::setArcDelayAnnotated(const TimingArc *arc,
                           DcalcAPIndex ap_index,
                           DcalcAPIndex ap_count,
                           bool annotated)
{
  size_t index = arc->index() * ap_count + ap_index;
  if (index > sizeof(uintptr_t) * 8
      && arc_delay_annotated_is_bits_) {
    arc_delay_annotated_is_bits_ = false;
    size_t bit_count = ap_count * RiseFall::index_count * 2;
    arc_delay_annotated_.seq_ = new std::vector<bool>(bit_count);
  }
  if (arc_delay_annotated_is_bits_) {
    if (annotated)
      arc_delay_annotated_.bits_ |= arcDelayAnnotateBit(index);
    else
      arc_delay_annotated_.bits_ &= ~arcDelayAnnotateBit(index);
  }
  else
    (*arc_delay_annotated_.seq_)[index] = annotated;
}

void
Edge::removeDelayAnnotated()
{
  delay_annotation_is_incremental_ = false;
  if (arc_delay_annotated_is_bits_)
    arc_delay_annotated_.bits_ = 0;
  else {
    delete arc_delay_annotated_.seq_;
    arc_delay_annotated_.seq_ = nullptr;
  }
}

void
Edge::setDelayAnnotationIsIncremental(bool is_incr)
{
  delay_annotation_is_incremental_ = is_incr;
}

uintptr_t
Edge::arcDelayAnnotateBit(size_t index)
{
  return static_cast<uintptr_t>(1) << index;
}

const TimingRole *
Edge::role() const
{
  return arc_set_->role();
}

bool
Edge::isWire() const
{
  return arc_set_->role()->isWire();
}

TimingSense
Edge::sense() const
{
  return arc_set_->sense();
}


TimingSense
Edge::simTimingSense() const
{
  return  static_cast<TimingSense>(sim_timing_sense_);
}

void
Edge::setSimTimingSense(TimingSense sense)
{
  sim_timing_sense_ = unsigned(sense);
}

bool
Edge::isDisabledConstraint() const
{
  const TimingRole *role = arc_set_->role();
  bool is_wire = role->isWire();
  return is_disabled_constraint_
    || arc_set_->isDisabledConstraint()
    // set_disable_timing cell does not disable timing checks.
    || (!(role->isTimingCheck() || is_wire)
	&& arc_set_->libertyCell()->isDisabledConstraint())
    || (!is_wire 
	&& arc_set_->from()->isDisabledConstraint())
    || (!is_wire 
	&& arc_set_->to()->isDisabledConstraint());
}


void
Edge::setIsDisabledConstraint(bool disabled)
{
  is_disabled_constraint_ = disabled;
}

void
Edge::setIsDisabledCond(bool disabled)
{
  is_disabled_cond_ = disabled;
}

void
Edge::setIsDisabledLoop(bool disabled)
{
  is_disabled_loop_ = disabled;
}

void
Edge::setIsBidirectInstPath(bool is_bidir)
{
  is_bidirect_inst_path_ = is_bidir;
}

void
Edge::setIsBidirectNetPath(bool is_bidir)
{
  is_bidirect_net_path_ = is_bidir;
}

////////////////////////////////////////////////////////////////

VertexIterator::VertexIterator(Graph *graph) :
  graph_(graph),
  network_(graph->network()),
  top_inst_(network_->topInstance()),
  inst_iter_(network_->leafInstanceIterator()),
  pin_iter_(nullptr),
  vertex_(nullptr),
  bidir_vertex_(nullptr)
{
  if (inst_iter_)
    findNext();
}

Vertex *
VertexIterator::next()
{
  Vertex *next = nullptr;
  if (vertex_) {
    next = vertex_;
    vertex_ = nullptr;
  }
  else if (bidir_vertex_) {
    next = bidir_vertex_;
    bidir_vertex_ = nullptr;
  }
  if (bidir_vertex_ == nullptr)
    findNext();
  return next;
}

bool
VertexIterator::findNextPin()
{
  while (pin_iter_->hasNext()) {
    Pin *pin = pin_iter_->next();
    vertex_ = graph_->vertex(network_->vertexId(pin));
    bidir_vertex_ = network_->direction(pin)->isBidirect() 
      ? graph_->pin_bidirect_drvr_vertex_map_.findKey(pin)
      : nullptr;
    if (vertex_ || bidir_vertex_)
      return true;
  }
  delete pin_iter_;
  pin_iter_ = nullptr;
  return false;
}

void 
VertexIterator::findNext()
{
  while (inst_iter_) {
    if (pin_iter_
        && findNextPin())
      return;
    
    if (inst_iter_->hasNext()) {
      Instance *inst = inst_iter_->next();
      pin_iter_ = network_->pinIterator(inst);
    } else {
      delete inst_iter_;
      inst_iter_ = nullptr;
      if (top_inst_) {
        pin_iter_ = network_->pinIterator(top_inst_);
        top_inst_ = nullptr;
      }
    }
  }
  if (pin_iter_)
    findNextPin();
}

VertexInEdgeIterator::VertexInEdgeIterator(Vertex *vertex,
					   const Graph *graph) :
  next_(graph->edge(vertex->in_edges_)),
  graph_(graph)
{
}

VertexInEdgeIterator::VertexInEdgeIterator(VertexId vertex_id,
					   const Graph *graph) :
  next_(graph->edge(graph->vertex(vertex_id)->in_edges_)),
  graph_(graph)
{
}

Edge *
VertexInEdgeIterator::next()
{
  Edge *next = next_;
  if (next_)
    next_ = graph_->edge(next_->vertex_in_link_);
  return next;
}

VertexOutEdgeIterator::VertexOutEdgeIterator(Vertex *vertex,
					     const Graph *graph) :
  next_(graph->edge(vertex->out_edges_)),
  graph_(graph)
{
}

Edge *
VertexOutEdgeIterator::next()
{
  Edge *next = next_;
  if (next_)
    next_ = graph_->edge(next_->vertex_out_next_);
  return next;
}

////////////////////////////////////////////////////////////////

class FindEdgesThruHierPinVisitor : public HierPinThruVisitor
{
public:
  FindEdgesThruHierPinVisitor(EdgeSet &edges,
			      Graph *graph);
  virtual void visit(const Pin *drvr,
		     const Pin *load);
  
protected:
  EdgeSet &edges_;
  Graph *graph_;
};

FindEdgesThruHierPinVisitor::FindEdgesThruHierPinVisitor(EdgeSet &edges,
							 Graph *graph) :
  HierPinThruVisitor(),
  edges_(edges),
  graph_(graph)
{
}

void
FindEdgesThruHierPinVisitor::visit(const Pin *drvr,
				   const Pin *load)
{
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr);
  Vertex *load_vertex = graph_->pinLoadVertex(load);
  // Iterate over load drivers to avoid driver fanout^2.
  VertexInEdgeIterator edge_iter(load_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->from(graph_) == drvr_vertex)
      edges_.insert(edge);
  }
}

EdgesThruHierPinIterator::EdgesThruHierPinIterator(const Pin *hpin,
						   Network *network,
						   Graph *graph)
{
  FindEdgesThruHierPinVisitor visitor(edges_, graph);
  visitDrvrLoadsThruHierPin(hpin, network, &visitor);
  edge_iter_.init(edges_);
}

////////////////////////////////////////////////////////////////

VertexIdLess::VertexIdLess(Graph *&graph) :
  graph_(graph)
{
}

bool
VertexIdLess::operator()(const Vertex *vertex1,
                         const Vertex *vertex2) const
{
  return graph_->id(vertex1) < graph_->id(vertex2);
}

VertexSet::VertexSet(Graph *&graph) :
  Set<Vertex*, VertexIdLess>(VertexIdLess(graph))
{
}

} // namespace
