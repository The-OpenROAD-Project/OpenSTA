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
#include "Error.hh"
#include "Debug.hh"
#include "Wireload.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "ReduceParasitics.hh"
#include "Parasitics.hh"

namespace sta {

Parasitics::Parasitics(StaState *sta) :
  StaState(sta)
{
}

Net *
Parasitics::findParasiticNet(const Pin *pin) const
{
  Net *net = network_->net(pin);
  // Pins on the top level instance may not have nets.
  // Use the net connected to the pin's terminal.
  if (net == nullptr && network_->isTopLevelPort(pin)) {
    Term *term = network_->term(pin);
    if (term)
      return network_->net(term);
    else
      return nullptr;
  }
  if (net)
    return network_->highestConnectedNet(net);
  else
    return nullptr;
}

void
Parasitics::check(Parasitic *) const
{
#if 0
  ConcreteParasiticSubNodeMap::Iterator sub_node_iter(sub_nodes_);
  while (sub_node_iter.hasNext()) {
    ConcreteParasiticSubNode *node = sub_node_iter.next();
    ConcreteParasiticDeviceSeq::Iterator device_iter(node->devices());
    int res_count = 0;
    while (device_iter.hasNext()) {
      ConcreteParasiticDevice *device = device_iter.next();
      if (device->isResistor())
	res_count++;
    }
    if (res_count == 0)
      report->warn("sub node %s has no resistor connections\n",
		   node->name(network));
    else if (res_count == 1)
      report->warn("sub node %s has one resistor connection\n",
		   node->name(network));
  }
#endif
}

////////////////////////////////////////////////////////////////

Parasitic *
Parasitics::makeWireloadNetwork(const Pin *drvr_pin,
				const Wireload *wireload,
				float fanout,
				const OperatingConditions *op_cond,
				const ParasiticAnalysisPt *ap)
{
  Net *net = network_->net(drvr_pin);
  Parasitic *parasitic = makeParasiticNetwork(net, false, ap);
  float wireload_cap, wireload_res;
  wireload->findWireload(fanout, op_cond, wireload_cap, wireload_res);

  WireloadTree tree = WireloadTree::balanced;
  if (op_cond)
    tree = op_cond->wireloadTree();
  switch (tree) {
  case WireloadTree::worst_case:
    makeWireloadNetworkWorst(parasitic, drvr_pin, wireload_cap, 
			     wireload_res, fanout, ap);
    break;
  case WireloadTree::balanced:
    makeWireloadNetworkBalanced(parasitic, drvr_pin, wireload_cap,
				wireload_res, fanout, ap);
    break;
  case WireloadTree::best_case:
  case WireloadTree::unknown:
    makeWireloadNetworkBest(parasitic, drvr_pin, wireload_cap, 
			    wireload_res, fanout, ap);
    break;
  }
  return parasitic;
}

// All load capacitance (except driver pin cap) is on the far side of
// the resistor.
void
Parasitics::makeWireloadNetworkWorst(Parasitic *parasitic,
				     const Pin *drvr_pin,
				     float wireload_cap,
				     float wireload_res,
				     float /* fanout */,
				     const ParasiticAnalysisPt *ap)
{
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin);
  Net *net = network_->net(drvr_pin);
  ParasiticNode *load_node = ensureParasiticNode(parasitic, net, 0);
  makeResistor(nullptr, drvr_node, load_node, wireload_res, ap);
  parasitics_->incrCap(load_node, wireload_cap, ap);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin);
      makeResistor(nullptr, load_node, load_node1, 0.0, ap);    
    }
  }
}

// No wire resistance, so load is lumped capacitance.
void
Parasitics::makeWireloadNetworkBest(Parasitic *parasitic,
				    const Pin *drvr_pin,
				    float wireload_cap,
				    float /* wireload_res */,
				    float /* fanout */,
				    const ParasiticAnalysisPt *ap)
{
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin);
  parasitics_->incrCap(drvr_node, wireload_cap, ap);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin);
      makeResistor(nullptr, drvr_node, load_node1, 0.0, ap);    
    }
  }
}

// Each load capacitance and wireload cap/fanout has resistance/fanout
// connecting it to the driver.
void
Parasitics::makeWireloadNetworkBalanced(Parasitic *parasitic,
					const Pin *drvr_pin,
					float wireload_cap,
					float wireload_res,
					float fanout,
					const ParasiticAnalysisPt *ap)
{
  float fanout_cap = wireload_cap / fanout;
  float fanout_res = wireload_res / fanout;
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin);
      makeResistor(nullptr, drvr_node, load_node1, fanout_res, ap);    
      parasitics_->incrCap(load_node1, fanout_cap, ap);
    }
  }
}

////////////////////////////////////////////////////////////////

ParasiticAnalysisPt::ParasiticAnalysisPt(const char *name,
					 int index,
					 const MinMax *min_max) :
  name_(stringCopy(name)),
  index_(index),
  min_max_(min_max),
  coupling_cap_factor_(1.0)
{
}

ParasiticAnalysisPt::~ParasiticAnalysisPt()
{
  stringDelete(name_);
}

void
ParasiticAnalysisPt::setCouplingCapFactor(float factor)
{
  coupling_cap_factor_ = factor;
}

} // namespace
