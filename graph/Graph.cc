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

#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "Stats.hh"
#include "Error.hh"
#include "Debug.hh"
#include "Pool.hh"
#include "MinMax.hh"
#include "PortDirection.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "DcalcAnalysisPt.hh"
#include "Graph.hh"

namespace sta {

////////////////////////////////////////////////////////////////
//
// Graph
//
////////////////////////////////////////////////////////////////

Graph::Graph(StaState *sta,
	     int slew_tr_count,
	     bool have_arc_delays,
	     DcalcAPIndex ap_count) :
  StaState(sta),
  vertices_(nullptr),
  edges_(nullptr),
  vertex_count_(0),
  edge_count_(0),
  arc_count_(0),
  slew_tr_count_(slew_tr_count),
  have_arc_delays_(have_arc_delays),
  ap_count_(ap_count),
  float_pool_(nullptr),
  width_check_annotations_(nullptr),
  period_check_annotations_(nullptr)
{
}

Graph::~Graph()
{
  delete vertices_;
  delete edges_;
  deleteSlewPools();
  deleteArcDelayPools();
  delete width_check_annotations_;
  delete period_check_annotations_;
  delete float_pool_;
}

void
Graph::makeGraph()
{
  Stats stats(debug_);
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
  VertexIndex vertex_count;
  EdgeIndex edge_count;
  ArcIndex arc_count;
  vertexAndEdgeCounts(vertex_count, edge_count, arc_count);
  vertices_ = new VertexPool(vertex_count);
  edges_ = new EdgePool(edge_count);
  makeSlewPools(vertex_count, ap_count_);
  makeArcDelayPools(arc_count, ap_count_);

  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    makePinVertices(inst);
    makeInstanceEdges(inst);
  }
  delete leaf_iter;
  makePinVertices(network_->topInstance());
}

void
Graph::vertexAndEdgeCounts(// Return values.
			   VertexIndex &vertex_count,
			   EdgeIndex &edge_count,
			   ArcIndex &arc_count)
{
  vertex_count = edge_count = arc_count = 0;
  PinSet visited_drvrs;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    vertexAndEdgeCounts(inst, visited_drvrs,
			vertex_count, edge_count, arc_count);
  }
  delete leaf_iter;
  vertexAndEdgeCounts(network_->topInstance(), visited_drvrs,
		      vertex_count, edge_count, arc_count);
}

void
Graph::vertexAndEdgeCounts(const Instance *inst,
			   PinSet &visited_drvrs,
			   // Return values.
			   VertexIndex &vertex_count,
			   EdgeIndex &edge_count,
			   ArcIndex &arc_count)
{
  LibertyCell *cell = network_->libertyCell(inst);
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    PortDirection *dir = network_->direction(pin);
    if (!dir->isPowerGround()) {
      if (dir->isBidirect())
	vertex_count += 2;
      else
	vertex_count++;
      if (cell) {
	LibertyPort *port = network_->libertyPort(pin);
	LibertyCellTimingArcSetIterator set_iter(cell, port, nullptr);
	while (set_iter.hasNext()) {
	  TimingArcSet *arc_set = set_iter.next();
	  LibertyPort *to_port = arc_set->to();
	  if (network_->findPin(inst, to_port)) {
	    if (dir->isBidirect()) {
	      // Internal path from bidirect output back into the instance.
	      edge_count += 2;
	      arc_count += arc_set->arcCount() * 2;
	    }
	    else {
	      edge_count++;
	      arc_count += arc_set->arcCount();
	    }
	  }
	}
      }
      // Count wire edges from driver pins.
      if (network_->isDriver(pin))
	drvrPinEdgeCounts(pin, visited_drvrs,
			  edge_count, arc_count);
    }
  }
  delete pin_iter;
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
  virtual void operator()(Pin *pin);

protected:
  Pin *drvr_pin_;
  PinSet &visited_drvrs_;
  int &drvr_count_;
  int &bidirect_count_;
  int &load_count_;
  const Network *network_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindNetDrvrLoadCounts);
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
FindNetDrvrLoadCounts::operator()(Pin *pin)
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
Graph::drvrPinEdgeCounts(Pin *drvr_pin,
			 PinSet &visited_drvrs,
			 // Return values.
			 EdgeIndex &edge_count,
			 ArcIndex &arc_count)
{
  if (!visited_drvrs.findKey(drvr_pin)) {
    int drvr_count = 0;
    int bidirect_count = 0;
    int load_count = 0;
    FindNetDrvrLoadCounts visitor(drvr_pin, visited_drvrs, drvr_count,
				  bidirect_count, load_count, network_);
    network_->visitConnectedPins(drvr_pin, visitor);
    int net_edge_count = drvr_count * load_count
      + (bidirect_count * (load_count - 1));
    edge_count += net_edge_count;
    arc_count += net_edge_count * TimingArcSet::wireArcCount();
  }
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
Graph::makePinInstanceEdges(Pin *pin)
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
  LibertyCellTimingArcSetIterator timing_iter(cell);
  while (timing_iter.hasNext()) {
    TimingArcSet *arc_set = timing_iter.next();
    LibertyPort *from_port = arc_set->from();
    LibertyPort *to_port = arc_set->to();
    if (from_to_port == nullptr
	|| from_port == from_to_port
	|| to_port == from_to_port) {
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
  	  bool is_check = arc_set->role()->isTimingCheck();
	  if (to_bidirect_drvr_vertex &&
	      !is_check)
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
  PinSet visited_drvrs;
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    makeInstDrvrWireEdges(inst, visited_drvrs);
  }
  delete inst_iter;
  makeInstDrvrWireEdges(network_->topInstance(), visited_drvrs);
}

void
Graph::makeInstDrvrWireEdges(Instance *inst,
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
Graph::makeWireEdgesFromPin(Pin *drvr_pin)
{
  PinSeq loads, drvrs;
  PinSet visited_drvrs;
  FindNetDrvrLoads visitor(drvr_pin, visited_drvrs, loads, drvrs, network_);
  network_->visitConnectedPins(drvr_pin, visitor);

  for (auto load_pin : loads) {
    if (drvr_pin != load_pin)
      makeWireEdge(drvr_pin, load_pin);
  }
}

void
Graph::makeWireEdgesFromPin(Pin *drvr_pin,
			    PinSet &visited_drvrs)
{
  // Find all drivers and loads on the net to avoid N*M run time
  // for large fanin/fanout nets.
  PinSeq loads, drvrs;
  FindNetDrvrLoads visitor(drvr_pin, visited_drvrs, loads, drvrs, network_);
  network_->visitConnectedPins(drvr_pin, visitor);

  for (auto drvr_pin : drvrs) {
    for (auto load_pin : loads) {
      if (drvr_pin != load_pin)
	makeWireEdge(drvr_pin, load_pin);
    }
  }
}

void
Graph::makeWireEdgesToPin(Pin *to_pin)
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
  DISALLOW_COPY_AND_ASSIGN(MakeEdgesThruHierPin);
  virtual void visit(Pin *drvr,
		     Pin *load);

  Graph *graph_;
};

MakeEdgesThruHierPin::MakeEdgesThruHierPin(Graph *graph) :
  HierPinThruVisitor(),
  graph_(graph)
{
}

void
MakeEdgesThruHierPin::visit(Pin *drvr,
			    Pin *load)
{
  graph_->makeWireEdge(drvr, load);
}

void
Graph::makeWireEdgesThruPin(Pin *hpin)
{
  MakeEdgesThruHierPin visitor(this);
  visitDrvrLoadsThruHierPin(hpin, network_, &visitor);
}

void
Graph::makeWireEdge(Pin *from_pin,
		    Pin *to_pin)
{
  TimingArcSet *arc_set = TimingArcSet::wireTimingArcSet();
  Vertex *from_vertex, *from_bidirect_drvr_vertex;
  pinVertices(from_pin, from_vertex, from_bidirect_drvr_vertex);
  Vertex *to_vertex = pinLoadVertex(to_pin);
  // From and/or to can be bidirect, but edge is always from driver to load.
  if (from_bidirect_drvr_vertex)
    makeEdge(from_bidirect_drvr_vertex, to_vertex, arc_set);
  else
    makeEdge(from_vertex, to_vertex, arc_set);
}

////////////////////////////////////////////////////////////////

Vertex *
Graph::vertex(VertexIndex vertex_index) const
{
  return vertices_->find(vertex_index);
}

VertexIndex
Graph::index(const Vertex *vertex) const
{
  return vertices_->index(vertex);
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
    network_->setVertexIndex(pin, index(vertex));
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
  Vertex *vertex = vertices_->makeObject();
  vertex->init(pin, is_bidirect_drvr, is_reg_clk);
  vertex_count_++;
  makeVertexSlews();
  if (is_reg_clk)
    reg_clk_vertices_.insert(vertex);
  return vertex;
}

void
Graph::pinVertices(const Pin *pin,
		   // Return values.
		   Vertex *&vertex,
		   Vertex *&bidirect_drvr_vertex)  const
{
  vertex = Graph::vertex(network_->vertexIndex(pin));
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
    return Graph::vertex(network_->vertexIndex(pin));
}

Vertex *
Graph::pinLoadVertex(const Pin *pin) const
{
  return vertex(network_->vertexIndex(pin));
}

void
Graph::deleteVertex(Vertex *vertex)
{
  if (vertex->isRegClk())
    reg_clk_vertices_.eraseKey(vertex);
  Pin *pin = vertex->pin_;
  if (vertex->isBidirectDriver())
    pin_bidirect_drvr_vertex_map_.erase(pin_bidirect_drvr_vertex_map_
					.find(pin));
  else
    network_->setVertexIndex(pin, 0);
  // Delete edges to vertex.
  EdgeIndex edge_index, next_index;
  for (edge_index = vertex->in_edges_; edge_index; edge_index = next_index) {
    Edge *edge = Graph::edge(edge_index);
    next_index = edge->vertex_in_link_;
    deleteOutEdge(edge->from(this), edge);
    deleteEdgeArcDelays(edge);
    edge_count_--;
    arc_count_ -= edge->timingArcSet()->arcCount();
    edges_->deleteObject(edge);
  }
  // Delete edges from vertex.
  for (edge_index = vertex->out_edges_; edge_index; edge_index = next_index) {
    Edge *edge = Graph::edge(edge_index);
    next_index = edge->vertex_out_next_;
    deleteInEdge(edge->to(this), edge);
    deleteEdgeArcDelays(edge);
    edge_count_--;
    arc_count_ -= edge->timingArcSet()->arcCount();
    edges_->deleteObject(edge);
  }
  deleteVertexSlews(vertex);
  vertices_->deleteObject(vertex);
  vertex_count_--;
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
  EdgeIndex edge_index = index(edge);
  EdgeIndex prev = 0;
  for (EdgeIndex i = vertex->in_edges_;
       i && i != edge_index;
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
  EdgeIndex next = edge->vertex_out_next_;
  EdgeIndex prev = edge->vertex_out_prev_;
  if (prev)
    Graph::edge(prev)->vertex_out_next_ = next;
  else
    vertex->out_edges_ = next;
  if (next)
    Graph::edge(next)->vertex_out_prev_ = prev;
}

const Slew &
Graph::slew(const Vertex *vertex,
	    const TransRiseFall *tr,
	    DcalcAPIndex ap_index)
{
  if (slew_tr_count_) {
    int pool_index =
      (slew_tr_count_ == 1) ? ap_index : ap_index*slew_tr_count_+tr->index();
    DelayPool *pool = slew_pools_[pool_index];
    VertexIndex vertex_index = index(vertex);
    return *pool->find(vertex_index);
  }
  else {
    static Slew slew(0.0);
    return slew;
  }
}

void
Graph::setSlew(Vertex *vertex,
	       const TransRiseFall *tr,
	       DcalcAPIndex ap_index,
	       const Slew &slew)
{
  if (slew_tr_count_) {
    int pool_index =
      (slew_tr_count_ == 1) ? ap_index : ap_index*slew_tr_count_+tr->index();
    DelayPool *pool = slew_pools_[pool_index];
    VertexIndex vertex_index = index(vertex);
    Slew &vertex_slew = *pool->find(vertex_index);
    vertex_slew = slew;
  }
}

////////////////////////////////////////////////////////////////

Edge *
Graph::edge(EdgeIndex edge_index) const
{
  return edges_->find(edge_index);
}

EdgeIndex
Graph::index(const Edge *edge) const
{
  return edges_->index(edge);
}

Edge *
Graph::makeEdge(Vertex *from,
		Vertex *to,
		TimingArcSet *arc_set)
{
  Edge *edge = edges_->makeObject();
  EdgeIndex edge_index = edges_->index(edge);
  edge->init(vertices_->index(from), vertices_->index(to), arc_set);
  makeEdgeArcDelays(edge);
  edge_count_++;
  arc_count_ += arc_set->arcCount();
  // Add out edge to from vertex.
  EdgeIndex next = from->out_edges_;
  edge->vertex_out_next_ = next;
  edge->vertex_out_prev_ = 0;
  if (next)
    Graph::edge(next)->vertex_out_prev_ = edge_index;
  from->out_edges_ = edge_index;

  // Add in edge to to vertex.
  edge->vertex_in_link_ = to->in_edges_;
  to->in_edges_ = edge_index;

  return edge;
}

void
Graph::deleteEdge(Edge *edge)
{
  Vertex *from = edge->from(this);
  Vertex *to = edge->to(this);
  deleteOutEdge(from, edge);
  deleteInEdge(to, edge);
  deleteEdgeArcDelays(edge);
  arc_count_ -= edge->timingArcSet()->arcCount();
  edge_count_--;
  edges_->deleteObject(edge);
}

void
Graph::makeArcDelayPools(ArcIndex arc_count,
			 DcalcAPIndex ap_count)
{
  if (have_arc_delays_) {
    arc_delays_.resize(ap_count);
    for (DcalcAPIndex i = 0; i < ap_count; i++) {
      DelayPool *pool = new DelayPool(arc_count);
      arc_delays_[i] = pool;
    }

    // Leave some room for edits.
    unsigned annot_size = arc_count * 1.2;
    arc_delay_annotated_.resize(annot_size * ap_count);
  }
}

void
Graph::deleteArcDelayPools()
{
  if (have_arc_delays_) {
    for (DcalcAPIndex i = 0; i < ap_count_; i++) {
      DelayPool *pool = arc_delays_[i];
      delete pool;
    }
    arc_delays_.clear();
  }
}

void
Graph::makeEdgeArcDelays(Edge *edge)
{
  if (have_arc_delays_) {
    int arc_count = edge->timingArcSet()->arcCount();
    ArcIndex arc_index = 0;
    for (DcalcAPIndex i = 0; i < ap_count_; i++) {
      DelayPool *pool = arc_delays_[i];
      ArcDelay *arc_delays = pool->makeObjects(arc_count);
      arc_index = pool->index(arc_delays);
      for (int j = 0; j < arc_count; j++)
	*arc_delays++ = 0.0;
    }
    edge->setArcDelays(arc_index);
    // Make sure there is room for delay_annotated flags.
    unsigned max_annot_index = (arc_index + arc_count) * ap_count_;
    if (max_annot_index >= arc_delay_annotated_.size()) {
      unsigned size = max_annot_index * 1.2;
      arc_delay_annotated_.resize(size);
    }
    removeDelayAnnotated(edge);
  }
}

void
Graph::deleteEdgeArcDelays(Edge *edge)
{
  if (have_arc_delays_) {
    ArcIndex arc_count = edge->timingArcSet()->arcCount();
    ArcIndex arc_index = edge->arcDelays();
    for (DcalcAPIndex i = 0; i < ap_count_; i++) {
      DelayPool *pool = arc_delays_[i];
      pool->deleteObjects(arc_index, arc_count);
    }
  }
}

ArcDelay
Graph::arcDelay(const Edge *edge,
		const TimingArc *arc,
		DcalcAPIndex ap_index) const
{
  if (have_arc_delays_) {
    DelayPool *pool = arc_delays_[ap_index];
    ArcIndex arc_index = edge->arcDelays() + arc->index();
    ArcDelay &arc_delay = *pool->find(arc_index);
    return arc_delay;
  }
  else
    return delay_zero;
}

void
Graph::setArcDelay(Edge *edge,
		   const TimingArc *arc,
		   DcalcAPIndex ap_index,
		   ArcDelay delay)
{
  if (have_arc_delays_) {
    DelayPool *pool = arc_delays_[ap_index];
    ArcIndex arc_index = edge->arcDelays() + arc->index();
    ArcDelay &arc_delay = *pool->find(arc_index);
    arc_delay = delay;
  }
}

const ArcDelay &
Graph::wireArcDelay(const Edge *edge,
		    const TransRiseFall *tr,
		    DcalcAPIndex ap_index)
{
  if (have_arc_delays_) {
    DelayPool *pool = arc_delays_[ap_index];
    ArcIndex arc_index = edge->arcDelays() + tr->index();
    const ArcDelay &arc_delay = *pool->find(arc_index);
    return arc_delay;
  }
  else
    return delay_zero;
}

void
Graph::setWireArcDelay(Edge *edge,
		       const TransRiseFall *tr,
		       DcalcAPIndex ap_index,
		       const ArcDelay &delay)
{
  if (have_arc_delays_) {
    DelayPool *pool = arc_delays_[ap_index];
    ArcIndex arc_index = edge->arcDelays() + tr->index();
    ArcDelay &arc_delay = *pool->find(arc_index);
    arc_delay = delay;
  }
}

bool
Graph::arcDelayAnnotated(Edge *edge,
			 TimingArc *arc,
			 DcalcAPIndex ap_index) const
{
  if (arc_delay_annotated_.size()) {
    unsigned index = (edge->arcDelays() + arc->index()) * ap_count_ + ap_index;
    if (index >= arc_delay_annotated_.size())
      internalError("arc_delay_annotated array bounds exceeded");
    return arc_delay_annotated_[index];
  }
  else
    return false;
}

void
Graph::setArcDelayAnnotated(Edge *edge,
			    TimingArc *arc,
			    DcalcAPIndex ap_index,
			    bool annotated)
{
  unsigned index = (edge->arcDelays() + arc->index()) * ap_count_ + ap_index;
  if (index >= arc_delay_annotated_.size())
    internalError("arc_delay_annotated array bounds exceeded");
  arc_delay_annotated_[index] = annotated;
}

bool
Graph::wireDelayAnnotated(Edge *edge,
			  const TransRiseFall *tr,
			  DcalcAPIndex ap_index) const
{
  unsigned index = (edge->arcDelays() + TimingArcSet::wireArcIndex(tr)) * ap_count_
    + ap_index;
  if (index >= arc_delay_annotated_.size())
    internalError("arc_delay_annotated array bounds exceeded");
  return arc_delay_annotated_[index];
}

void
Graph::setWireDelayAnnotated(Edge *edge,
			     const TransRiseFall *tr,
			     DcalcAPIndex ap_index,
			     bool annotated)
{
  unsigned index = (edge->arcDelays() + TimingArcSet::wireArcIndex(tr)) * ap_count_
    + ap_index;
  if (index >= arc_delay_annotated_.size())
    internalError("arc_delay_annotated array bounds exceeded");
  arc_delay_annotated_[index] = annotated;
}

// This only gets called if the analysis type changes from single
// to bc_wc/ocv or visa versa.
void
Graph::setDelayCount(DcalcAPIndex ap_count)
{
  if (ap_count != ap_count_) {
    // Discard any existing delays.
    deleteSlewPools();
    deleteArcDelayPools();
    removeWidthCheckAnnotations();
    removePeriodCheckAnnotations();
    makeSlewPools(vertex_count_, ap_count);
    makeArcDelayPools(arc_count_, ap_count);
    ap_count_ = ap_count;
    removeDelays();
  }
}

void
Graph::removeDelays()
{
  VertexIterator vertex_iter(this);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    makeVertexSlews();
    VertexOutEdgeIterator edge_iter(vertex, this);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      makeEdgeArcDelays(edge);
      removeDelayAnnotated(edge);
    }
  }
}

void
Graph::removeDelayAnnotated(Edge *edge)
{
  edge->setDelayAnnotationIsIncremental(false);
  TimingArcSet *arc_set = edge->timingArcSet();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    for (DcalcAPIndex ap_index = 0; ap_index < ap_count_; ap_index++) {
      setArcDelayAnnotated(edge, arc, ap_index, false);
    }
  }
}

bool
Graph::delayAnnotated(Edge *edge)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    for (DcalcAPIndex ap_index = 0; ap_index < ap_count_; ap_index++) {
      if (arcDelayAnnotated(edge, arc, ap_index))
	return true;
    }
  }
  return false;
}

void
Graph::makeSlewPools(VertexIndex vertex_count,
		     DcalcAPIndex ap_count)
{
  DcalcAPIndex tr_ap_count = slew_tr_count_ * ap_count;
  slew_pools_.resize(tr_ap_count);
  for (DcalcAPIndex i = 0; i < tr_ap_count; i++) {
    DelayPool *pool = new DelayPool(vertex_count);
    slew_pools_[i] = pool;
  }
}

void
Graph::deleteSlewPools()
{
  if (slew_tr_count_) {
    DcalcAPIndex tr_ap_count = slew_tr_count_ * ap_count_;
    for (DcalcAPIndex i = 0; i < tr_ap_count; i++) {
      DelayPool *pool = slew_pools_[i];
      delete pool;
    }
    slew_pools_.clear();
  }
}

void
Graph::makeVertexSlews()
{
  DcalcAPIndex tr_ap_count = slew_tr_count_ * ap_count_;
  slew_pools_.resize(tr_ap_count);
  for (DcalcAPIndex i = 0; i < tr_ap_count; i++) {
    DelayPool *pool = slew_pools_[i];
    Slew *slew = pool->makeObject();
    *slew = 0.0;
  }
}

void
Graph::deleteVertexSlews(Vertex *vertex)
{
  VertexIndex vertex_index = index(vertex);
  DcalcAPIndex tr_ap_count = slew_tr_count_ * ap_count_;
  slew_pools_.resize(tr_ap_count);
  for (DcalcAPIndex i = 0; i < tr_ap_count; i++) {
    DelayPool *pool = slew_pools_[i];
    pool->deleteObject(vertex_index);
  }
}

////////////////////////////////////////////////////////////////

void
Graph::widthCheckAnnotation(const Pin *pin,
			    const TransRiseFall *tr,
			    DcalcAPIndex ap_index,
			    // Return values.
			    float &width,
			    bool &exists)
{
  exists = false;
  if (width_check_annotations_) {
    float *widths = width_check_annotations_->findKey(pin);
    if (widths) {
      int index = ap_index * TransRiseFall::index_count + tr->index();
      width = widths[index];
      if (width >= 0.0)
	exists = true;
    }
  }
}

void
Graph::setWidthCheckAnnotation(const Pin *pin,
			       const TransRiseFall *tr,
			       DcalcAPIndex ap_index,
			       float width)
{
  if (width_check_annotations_ == nullptr)
    width_check_annotations_ = new WidthCheckAnnotations;
  float *widths = width_check_annotations_->findKey(pin);
  if (widths == nullptr) {
    int width_count = TransRiseFall::index_count * ap_count_;
    widths = makeFloats(width_count);
    // Use negative (illegal) width values to indicate unannotated checks.
    for (int i = 0; i < width_count; i++)
      widths[i] = -1;
    (*width_check_annotations_)[pin] = widths;
  }
  int index = ap_index * TransRiseFall::index_count + tr->index();
  widths[index] = width;
}

void
Graph::removeWidthCheckAnnotations()
{
  if (width_check_annotations_) {
    int width_count = TransRiseFall::index_count * ap_count_;
    WidthCheckAnnotations::Iterator check_iter(width_check_annotations_);
    while (check_iter.hasNext()) {
      float *widths = check_iter.next();
      deleteFloats(widths, width_count);
    }
    delete width_check_annotations_;
    width_check_annotations_ = nullptr;
  }
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
    period_check_annotations_ = new PeriodCheckAnnotations;
  float *periods = period_check_annotations_->findKey(pin);
  if (periods == nullptr) {
    periods = makeFloats(ap_count_);
    // Use negative (illegal) period values to indicate unannotated checks.
    for (DcalcAPIndex i = 0; i < ap_count_; i++)
      periods[i] = -1;
    (*period_check_annotations_)[pin] = periods;
  }
  periods[ap_index] = period;
}

void
Graph::removePeriodCheckAnnotations()
{
  if (period_check_annotations_) {
    for (auto pin_floats : *period_check_annotations_) {
      float *periods = pin_floats.second;
      deleteFloats(periods, ap_count_);
    }
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
  removeWidthCheckAnnotations();
  removePeriodCheckAnnotations();
}

float *
Graph::makeFloats(ObjectIndex count)
{
  if (float_pool_ == nullptr)
    float_pool_ = new Pool<float>(100);
  return float_pool_->makeObjects(count);
}

void
Graph::deleteFloats(float *floats, ObjectIndex count)
{
  float_pool_->deleteObjects(floats, count);
}

////////////////////////////////////////////////////////////////
//
// Vertex
//
////////////////////////////////////////////////////////////////

Vertex::Vertex()
{
  init(nullptr, false, false);
}

void
Vertex::init(Pin *pin,
	     bool is_bidirect_drvr,
	     bool is_reg_clk)
{
  pin_ = pin;
  is_reg_clk_ = is_reg_clk;
  is_bidirect_drvr_ = is_bidirect_drvr;
  in_edges_ = 0;
  out_edges_ = 0;
  arrivals_ = nullptr;
  prev_paths_ = nullptr;
  has_requireds_ = false;
  tag_group_index_ = tag_group_index_max;
  slew_annotated_ = false;
  sim_value_ = unsigned(LogicValue::unknown);
  is_disabled_constraint_ = false;
  is_gated_clk_enable_ = false;
  has_checks_ = false;
  is_check_clk_ = false;
  is_constrained_ = false;
  has_downstream_clk_pin_ = false;
  color_ = unsigned(LevelColor::white);
  level_ = 0;
  bfs_in_queue_ = 0;
  crpr_path_pruning_disabled_ = false;
  requireds_pruned_ = false;
}

const char *
Vertex::name(const Network *network) const
{
  if (network->direction(pin_)->isBidirect()) {
    const char *pin_name = network->pathName(pin_);
    return stringPrintTmp("%s %s",
			  pin_name,
			  is_bidirect_drvr_ ? "driver" : "load");
  }
  else
    return network->pathName(pin_);
}

void
Vertex::setLevel(Level level)
{
  level_ = level;
}

void
Vertex::setColor(LevelColor color)
{
  color_ = unsigned(color);
}

bool
Vertex::slewAnnotated(const TransRiseFall *tr,
		      const MinMax *min_max) const
{
  int index = min_max->index() * transitionCount() + tr->index();
  return ((1 << index) & slew_annotated_) != 0;
}

bool
Vertex::slewAnnotated() const
{
  return slew_annotated_ != 0;
}

void
Vertex::setSlewAnnotated(bool annotated,
			 const TransRiseFall *tr,
			 DcalcAPIndex ap_index)
{
  // Track rise/fall/min/max annotations separately, but after that
  // only rise/fall.
  if (ap_index > 1)
    ap_index = 0;
  int index = ap_index * transitionCount() + tr->index();
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

void
Vertex::setCrprPathPruningDisabled(bool disabled)
{
  crpr_path_pruning_disabled_ = disabled;
}

void
Vertex::setRequiredsPruned(bool pruned)
{
  requireds_pruned_ = pruned;
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
Vertex::setArrivals(Arrival *arrivals)
{
  arrivals_ = arrivals;
}

void
Vertex::setPrevPaths(PathVertexRep *prev_paths)
{
  prev_paths_ = prev_paths;
}

void
Vertex::setHasRequireds(bool has_req)
{
  has_requireds_ = has_req;
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
}

void
Edge::init(VertexIndex from,
	   VertexIndex to,
	   TimingArcSet *arc_set)
{
  from_ = from;
  to_ = to;
  arc_set_ = arc_set;
  arc_delays_ = 0;

  vertex_in_link_ = 0;
  vertex_out_next_ = 0;
  vertex_out_prev_ = 0;
  is_bidirect_inst_path_ = false;
  is_bidirect_net_path_ = false;
  delay_annotation_is_incremental_ = false;
  sim_timing_sense_ = unsigned(TimingSense::unknown);
  is_disabled_constraint_ = false;
  is_disabled_cond_ = false;
  is_disabled_loop_ = false;
}

void
Edge::setTimingArcSet(TimingArcSet *set)
{
  arc_set_ = set;
}

void
Edge::setArcDelays(ArcIndex arc_delays)
{
  arc_delays_ = arc_delays;
}

bool
Edge::delayAnnotationIsIncremental() const
{
  return delay_annotation_is_incremental_;
}

void
Edge::setDelayAnnotationIsIncremental(bool is_incr)
{
  delay_annotation_is_incremental_ = is_incr;
}

TimingRole *
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
  TimingRole *role = arc_set_->role();
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
    vertex_ = graph_->vertex(network_->vertexIndex(pin));
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

VertexInEdgeIterator::VertexInEdgeIterator(VertexIndex vertex_index,
					   const Graph *graph) :
  next_(graph->edge(graph->vertex(vertex_index)->in_edges_)),
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
  virtual void visit(Pin *drvr,
		     Pin *load);
  
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
FindEdgesThruHierPinVisitor::visit(Pin *drvr,
				   Pin *load)
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

} // namespace
