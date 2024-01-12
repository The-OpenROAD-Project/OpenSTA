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

#include "Stats.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "DisabledPorts.hh"
#include "PortDelay.hh"
#include "ClockLatency.hh"
#include "Sdc.hh"

namespace sta {

static void
annotateGraphDisabledWireEdge(const Pin *from_pin,
			      const Pin *to_pin,
			      Graph *graph);

// Annotate constraints to the timing graph.
void
Sdc::annotateGraph()
{
  Stats stats(debug_, report_);
  // All output pins are considered constrained because
  // they may be downstream from a set_min/max_delay -from that
  // does not have a set_output_delay.
  annotateGraphConstrainOutputs();
  annotateDisables();
  annotateGraphOutputDelays();
  annotateGraphDataChecks();
  annotateHierClkLatency();
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
      annotateGraphConstrained(pin);
  }
  delete pin_iter;
}

void
Sdc::annotateDisables()
{
  PinSet::Iterator pin_iter(disabled_pins_);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    annotateGraphDisabled(pin);
  }

  if (!disabled_lib_ports_.empty()) {
    VertexIterator vertex_iter(graph_);
    while (vertex_iter.hasNext()) {
      Vertex *vertex = vertex_iter.next();
      Pin *pin = vertex->pin();
      LibertyPort *port = network_->libertyPort(pin);
      if (disabled_lib_ports_.hasKey(port))
	annotateGraphDisabled(pin);
    }
  }

  Instance *top_inst = network_->topInstance();
  PortSet::Iterator port_iter(disabled_ports_);
  while (port_iter.hasNext()) {
    const Port *port = port_iter.next();
    Pin *pin = network_->findPin(top_inst, port);
    annotateGraphDisabled(pin);
  }

  for (const PinPair &pair : disabled_wire_edges_)
    annotateGraphDisabledWireEdge(pair.first, pair.second, graph_);

  for (Edge *edge : disabled_edges_)
    edge->setIsDisabledConstraint(true);

  DisabledInstancePortsMap::Iterator disable_inst_iter(disabled_inst_ports_);
  while (disable_inst_iter.hasNext()) {
    DisabledInstancePorts *disabled_inst = disable_inst_iter.next();
    setEdgeDisabledInstPorts(disabled_inst);
  }
}

class DisableHpinEdgeVisitor : public HierPinThruVisitor
{
public:
  DisableHpinEdgeVisitor(Graph *graph);
  virtual void visit(const Pin *from_pin,
		     const Pin *to_pin);

protected:
  bool annotate_;
  Graph *graph_;
};

DisableHpinEdgeVisitor::DisableHpinEdgeVisitor(Graph *graph) :
  HierPinThruVisitor(),
  graph_(graph)
{
}

void
DisableHpinEdgeVisitor::visit(const Pin *from_pin,
			      const Pin *to_pin)
{
  annotateGraphDisabledWireEdge(from_pin, to_pin, graph_);
}

static void
annotateGraphDisabledWireEdge(const Pin *from_pin,
			      const Pin *to_pin,
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
	edge->setIsDisabledConstraint(true);
    }
  }
}

void
Sdc::annotateGraphDisabled(const Pin *pin)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  vertex->setIsDisabledConstraint(true);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsDisabledConstraint(true);
}

void
Sdc::setEdgeDisabledInstPorts(DisabledInstancePorts *disabled_inst)
{
  setEdgeDisabledInstPorts(disabled_inst, disabled_inst->instance());
}

void
Sdc::setEdgeDisabledInstPorts(DisabledPorts *disabled_port,
			      Instance *inst)
{
  if (disabled_port->all()) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      // set_disable_timing instance does not disable timing checks.
      setEdgeDisabledInstFrom(pin, false);
    }
    delete pin_iter;
  }

  // Disable from pins.
  LibertyPortSet::Iterator from_iter(disabled_port->from());
  while (from_iter.hasNext()) {
    LibertyPort *from_port = from_iter.next();
    Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin)
      setEdgeDisabledInstFrom(from_pin, true);
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
	    edge->setIsDisabledConstraint(true);
	  }
	}
      }
    }
  }

  // Disable from/to pins.
  if (disabled_port->fromTo()) {
    for (const LibertyPortPair &from_to : *disabled_port->fromTo()) {
      const LibertyPort *from_port = from_to.first;
      const LibertyPort *to_port = from_to.second;
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
              edge->setIsDisabledConstraint(true);
          }
        }
      }
    }
  }
}

void
Sdc::setEdgeDisabledInstFrom(Pin *from_pin,
			     bool disable_checks)
{
  if (network_->direction(from_pin)->isAnyInput()) {
    Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
    if (from_vertex) {
      VertexOutEdgeIterator edge_iter(from_vertex, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (disable_checks
	    || !edge->role()->isTimingCheck())
	  edge->setIsDisabledConstraint(true);
      }
    }
  }
}

void
Sdc::annotateGraphOutputDelays()
{
  for (OutputDelay *output_delay : output_delays_) {
    for (const Pin *lpin : output_delay->leafPins())
      annotateGraphConstrained(lpin);
  }
}

void
Sdc::annotateGraphDataChecks()
{
  DataChecksMap::Iterator data_checks_iter(data_checks_to_map_);
  while (data_checks_iter.hasNext()) {
    DataCheckSet *checks = data_checks_iter.next();
    DataCheckSet::Iterator check_iter(checks);
    // There may be multiple data checks on a single pin,
    // but we only need to mark it as constrained once.
    if (check_iter.hasNext()) {
      DataCheck *check = check_iter.next();
      annotateGraphConstrained(check->to());
    }
  }
}

void
Sdc::annotateGraphConstrained(const PinSet *pins)
{
  PinSet::ConstIterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    annotateGraphConstrained(pin);
  }
}

void
Sdc::annotateGraphConstrained(const InstanceSet *insts)
{
  InstanceSet::ConstIterator inst_iter(insts);
  while (inst_iter.hasNext()) {
    const Instance *inst = inst_iter.next();
    annotateGraphConstrained(inst);
  }
}

void
Sdc::annotateGraphConstrained(const Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyInput())
      annotateGraphConstrained(pin);
  }
  delete pin_iter;
}

void
Sdc::annotateGraphConstrained(const Pin *pin)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  // Pin may be hierarchical and have no vertex.
  if (vertex)
    vertex->setIsConstrained(true);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsConstrained(true);
}

void
Sdc::annotateHierClkLatency()
{
  ClockLatencies::Iterator latency_iter(clk_latencies_);
  while (latency_iter.hasNext()) {
    ClockLatency *latency = latency_iter.next();
    const Pin *pin = latency->pin();
    if (pin && network_->isHierarchical(pin))
      annotateHierClkLatency(pin, latency);
  }
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

////////////////////////////////////////////////////////////////

void
Sdc::removeGraphAnnotations()
{
  VertexIterator vertex_iter(graph_);
  while (vertex_iter.hasNext()) {
    Vertex *vertex = vertex_iter.next();
    vertex->setIsDisabledConstraint(false);
    vertex->setIsConstrained(false);

    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      edge->setIsDisabledConstraint(false);
    }
  }
  edge_clk_latency_.clear();
}

void
Sdc::searchPreamble()
{
  ensureClkHpinDisables();
  ensureClkGroupExclusions();
}

} // namespace
