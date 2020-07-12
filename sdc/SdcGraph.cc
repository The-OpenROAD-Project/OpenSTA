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

#include "Stats.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "DisabledPorts.hh"
#include "PortDelay.hh"
#include "ClockLatency.hh"
#include "HpinDrvrLoad.hh"
#include "Sdc.hh"

namespace sta {

static void
annotateGraphDisabledWireEdge(Pin *from_pin,
			      Pin *to_pin,
			      bool annotate,
			      Graph *graph);

// Annotate constraints to the timing graph.
void
Sdc::annotateGraph(bool annotate)
{
  Stats stats(debug_);
  // All output pins are considered constrained because
  // they may be downstream from a set_min/max_delay -from that
  // does not have a set_output_delay.
  annotateGraphConstrainOutputs();
  annotateDisables(annotate);
  annotateGraphOutputDelays(annotate);
  annotateGraphDataChecks(annotate);
  annotateHierClkLatency(annotate);
  stats.report("Annotate constraints to graph");
}

void
Sdc::annotateGraphConstrainOutputs()
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyOutput())
      annotateGraphConstrained(pin, true);
  }
  delete pin_iter;
}

void
Sdc::annotateDisables(bool annotate)
{
  PinSet::Iterator pin_iter(disabled_pins_);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    annotateGraphDisabled(pin, annotate);
  }

  if (!disabled_lib_ports_.empty()) {
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      Pin *pin = vertex->pin();
      LibertyPort *port = network_->libertyPort(pin);
      if (disabled_lib_ports_.hasKey(port))
	annotateGraphDisabled(pin, annotate);
    }
  }

  Instance *top_inst = network_->topInstance();
  PortSet::Iterator port_iter(disabled_ports_);
  while (port_iter.hasNext()) {
    Port *port = port_iter.next();
    Pin *pin = network_->findPin(top_inst, port);
    annotateGraphDisabled(pin, annotate);
  }

  PinPairSet::Iterator pair_iter(disabled_wire_edges_);
  while (pair_iter.hasNext()) {
    PinPair *pair = pair_iter.next();
    annotateGraphDisabledWireEdge(pair->first, pair->second, annotate, graph_);
  }

  EdgeSet::Iterator edge_iter(disabled_edges_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge->setIsDisabledConstraint(annotate);
  }

  DisabledInstancePortsMap::Iterator disable_inst_iter(disabled_inst_ports_);
  while (disable_inst_iter.hasNext()) {
    DisabledInstancePorts *disabled_inst = disable_inst_iter.next();
    setEdgeDisabledInstPorts(disabled_inst, annotate);
  }
}

class DisableHpinEdgeVisitor : public HierPinThruVisitor
{
public:
  DisableHpinEdgeVisitor(bool annotate, Graph *graph);
  virtual void visit(Pin *from_pin,
		     Pin *to_pin);

protected:
  bool annotate_;
  Graph *graph_;

private:
  DISALLOW_COPY_AND_ASSIGN(DisableHpinEdgeVisitor);
};

DisableHpinEdgeVisitor::DisableHpinEdgeVisitor(bool annotate,
					       Graph *graph) :
  HierPinThruVisitor(),
  annotate_(annotate),
  graph_(graph)
{
}

void
DisableHpinEdgeVisitor::visit(Pin *from_pin,
			      Pin *to_pin)
{
  annotateGraphDisabledWireEdge(from_pin, to_pin, annotate_, graph_);
}

static void
annotateGraphDisabledWireEdge(Pin *from_pin,
			      Pin *to_pin,
			      bool annotate,
			      Graph *graph)
{
  Vertex *from_vertex = graph->pinDrvrVertex(from_pin);
  Vertex *to_vertex = graph->pinLoadVertex(to_pin);
  if (from_vertex && to_vertex) {
    VertexOutEdgeIterator edge_iter(from_vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()
	  && edge->to(graph) == to_vertex)
	edge->setIsDisabledConstraint(annotate);
    }
  }
}

void
Sdc::annotateGraphDisabled(const Pin *pin,
			   bool annotate)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  vertex->setIsDisabledConstraint(annotate);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsDisabledConstraint(annotate);
}

void
Sdc::setEdgeDisabledInstPorts(DisabledInstancePorts *disabled_inst,
			      bool annotate)
{
  setEdgeDisabledInstPorts(disabled_inst, disabled_inst->instance(), annotate);
}

void
Sdc::setEdgeDisabledInstPorts(DisabledPorts *disabled_port,
			      Instance *inst,
			      bool annotate)
{
  if (disabled_port->all()) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      // set_disable_timing instance does not disable timing checks.
      setEdgeDisabledInstFrom(pin, false, annotate);
    }
    delete pin_iter;
  }

  // Disable from pins.
  LibertyPortSet::Iterator from_iter(disabled_port->from());
  while (from_iter.hasNext()) {
    LibertyPort *from_port = from_iter.next();
    Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin)
      setEdgeDisabledInstFrom(from_pin, true, annotate);
  }

  // Disable to pins.
  LibertyPortSet::Iterator to_iter(disabled_port->to());
  while (to_iter.hasNext()) {
    LibertyPort *to_port = to_iter.next();
    Pin *to_pin = network_->findPin(inst, to_port);
    if (to_pin) {
      if (network_->direction(to_pin)->isAnyOutput()) {
	Vertex *vertex = graph_->pinDrvrVertex(to_pin);
	if (vertex) {
	  VertexInEdgeIterator edge_iter(vertex, graph_);
	  while (edge_iter.hasNext()) {
	    Edge *edge = edge_iter.next();
	    edge->setIsDisabledConstraint(annotate);
	  }
	}
      }
    }
  }

  // Disable from/to pins.
  LibertyPortPairSet::Iterator from_to_iter(disabled_port->fromTo());
  while (from_to_iter.hasNext()) {
    LibertyPortPair *pair = from_to_iter.next();
    const LibertyPort *from_port = pair->first;
    const LibertyPort *to_port = pair->second;
    Pin *from_pin = network_->findPin(inst, from_port);
    Pin *to_pin = network_->findPin(inst, to_port);
    if (from_pin && network_->direction(from_pin)->isAnyInput()
	&& to_pin) {
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
      if (from_vertex && to_vertex) {
	VertexOutEdgeIterator edge_iter(from_vertex, graph_);
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  if (edge->to(graph_) == to_vertex)
	    edge->setIsDisabledConstraint(annotate);
	}
      }
    }
  }
}

void
Sdc::setEdgeDisabledInstFrom(Pin *from_pin,
			     bool disable_checks,
			     bool annotate)
{
  if (network_->direction(from_pin)->isAnyInput()) {
    Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
    if (from_vertex) {
      VertexOutEdgeIterator edge_iter(from_vertex, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (disable_checks
	    || !edge->role()->isTimingCheck())
	  edge->setIsDisabledConstraint(annotate);
      }
    }
  }
}

void
Sdc::annotateGraphOutputDelays(bool annotate)
{
  for (OutputDelay *output_delay : output_delays_) {
    for (Pin *lpin : output_delay->leafPins())
      annotateGraphConstrained(lpin, annotate);
  }
}

void
Sdc::annotateGraphDataChecks(bool annotate)
{
  DataChecksMap::Iterator data_checks_iter(data_checks_to_map_);
  while (data_checks_iter.hasNext()) {
    DataCheckSet *checks = data_checks_iter.next();
    DataCheckSet::Iterator check_iter(checks);
    // There may be multiple data checks on a single pin,
    // but we only need to mark it as constrained once.
    if (check_iter.hasNext()) {
      DataCheck *check = check_iter.next();
      annotateGraphConstrained(check->to(), annotate);
    }
  }
}

void
Sdc::annotateGraphConstrained(const PinSet *pins,
			      bool annotate)
{
  PinSet::ConstIterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    annotateGraphConstrained(pin, annotate);
  }
}

void
Sdc::annotateGraphConstrained(const InstanceSet *insts,
			      bool annotate)
{
  InstanceSet::ConstIterator inst_iter(insts);
  while (inst_iter.hasNext()) {
    const Instance *inst = inst_iter.next();
    annotateGraphConstrained(inst, annotate);
  }
}

void
Sdc::annotateGraphConstrained(const Instance *inst,
			      bool annotate)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyInput())
      annotateGraphConstrained(pin, annotate);
  }
  delete pin_iter;
}

void
Sdc::annotateGraphConstrained(const Pin *pin,
			      bool annotate)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  // Pin may be hierarchical and have no vertex.
  if (vertex)
    vertex->setIsConstrained(annotate);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsConstrained(annotate);
}

void
Sdc::annotateHierClkLatency(bool annotate)
{
  if (annotate) {
    ClockLatencies::Iterator latency_iter(clk_latencies_);
    while (latency_iter.hasNext()) {
      ClockLatency *latency = latency_iter.next();
      const Pin *pin = latency->pin();
      if (pin && network_->isHierarchical(pin))
	annotateHierClkLatency(pin, latency);
    }
  }
  else
    edge_clk_latency_.clear();
}

void
Sdc::annotateHierClkLatency(const Pin *hpin,
			    ClockLatency *latency)
{
  EdgesThruHierPinIterator edge_iter(hpin, network_, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge_clk_latency_[edge] = latency;
  }
}

void
Sdc::deannotateHierClkLatency(const Pin *hpin)
{
  EdgesThruHierPinIterator edge_iter(hpin, network_, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge_clk_latency_.erase(edge);
  }
}

ClockLatency *
Sdc::clockLatency(Edge *edge) const
{
  return edge_clk_latency_.findKey(edge);
}

void
Sdc::clockLatency(Edge *edge,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  // Return values.
		  float &latency,
		  bool &exists) const
{
  ClockLatency *latencies = edge_clk_latency_.findKey(edge);
  if (latencies)
    latencies->delay(rf, min_max, latency, exists);
  else {
    latency = 0.0;
    exists = false;
  }
}

class FindClkHpinDisables : public HpinDrvrLoadVisitor
{
public:
  FindClkHpinDisables(Clock *clk,
		      const Network *network,
		      Sdc *sdc);
  ~FindClkHpinDisables();
  bool drvrLoadExists(Pin *drvr,
		      Pin *load);

protected:
  virtual void visit(HpinDrvrLoad *drvr_load);
  void makeClkHpinDisables(Pin *clk_src,
			   Pin *drvr,
			   Pin *load);

  Clock *clk_;
  PinPairSet drvr_loads_;
  const Network *network_;
  Sdc *sdc_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindClkHpinDisables);
};

FindClkHpinDisables::FindClkHpinDisables(Clock *clk,
					 const Network *network,
					 Sdc *sdc) :
  HpinDrvrLoadVisitor(),
  clk_(clk),
  network_(network),
  sdc_(sdc)
{
}

FindClkHpinDisables::~FindClkHpinDisables()
{
  drvr_loads_.deleteContents();
}

void
FindClkHpinDisables::visit(HpinDrvrLoad *drvr_load)
{
  Pin *drvr = drvr_load->drvr();
  Pin *load = drvr_load->load();

  makeClkHpinDisables(drvr, drvr, load);

  PinSet *hpins_from_drvr = drvr_load->hpinsFromDrvr();
  PinSet::Iterator hpin_iter(hpins_from_drvr);
  while (hpin_iter.hasNext()) {
    Pin *hpin = hpin_iter.next();
    makeClkHpinDisables(hpin, drvr, load);
  }
  drvr_loads_.insert(new PinPair(drvr, load));
}

void
FindClkHpinDisables::makeClkHpinDisables(Pin *clk_src,
					 Pin *drvr,
					 Pin *load)
{
  ClockSet *clks = sdc_->findClocks(clk_src);
  ClockSet::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    if (clk != clk_)
      // Do not propagate clock from source pin if another
      // clock is defined on a hierarchical pin between the
      // driver and load.
      sdc_->makeClkHpinDisable(clk, drvr, load);
  }
}

bool
FindClkHpinDisables::drvrLoadExists(Pin *drvr,
				    Pin *load)
{
  PinPair probe(drvr, load);
  return drvr_loads_.hasKey(&probe);
}

void
Sdc::ensureClkHpinDisables()
{
  if (!clk_hpin_disables_valid_) {
    clk_hpin_disables_.deleteContentsClear();
    for (auto clk : clocks_) {
      for (Pin *src : clk->pins()) {
	if (network_->isHierarchical(src)) {
	  FindClkHpinDisables visitor(clk, network_, this);
	  visitHpinDrvrLoads(src, network_, &visitor);
	  // Disable fanouts from the src driver pins that do
	  // not go thru the hierarchical src pin.
	  for (Pin *lpin : clk->leafPins()) {
	    Vertex *vertex, *bidirect_drvr_vertex;
	    graph_->pinVertices(lpin, vertex, bidirect_drvr_vertex);
	    makeVertexClkHpinDisables(clk, vertex, visitor);
	    if (bidirect_drvr_vertex)
	      makeVertexClkHpinDisables(clk, bidirect_drvr_vertex, visitor);
	  }
	}
      }
    }
    clk_hpin_disables_valid_ = true;
  }
}

void
Sdc::makeVertexClkHpinDisables(Clock *clk,
			       Vertex *vertex,
			       FindClkHpinDisables &visitor)
{
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->isWire()) {
      Pin *drvr = edge->from(graph_)->pin();
      Pin *load = edge->to(graph_)->pin();
      if (!visitor.drvrLoadExists(drvr, load))
	makeClkHpinDisable(clk, drvr, load);
    }
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::searchPreamble()
{
  ensureClkHpinDisables();
  ensureClkGroupExclusions();
}

} // namespace
