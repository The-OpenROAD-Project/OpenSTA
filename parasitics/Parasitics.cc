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

#include "Parasitics.hh"

#include "Error.hh"
#include "Debug.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "Wireload.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "ReduceParasitics.hh"
#include "EstimateParasitics.hh"

namespace sta {

Parasitics::Parasitics(StaState *sta) :
  StaState(sta)
{
}

void
Parasitics::report(const Parasitic *parasitic) const
{
  if (isParasiticNetwork(parasitic)) {
    const Unit *cap_unit = units_->capacitanceUnit();
    report_->reportLine("Net %s %s",
                        network_->pathName(net(parasitic)),
                        cap_unit->asString(capacitance(parasitic)));
    report_->reportLine("Nodes:");
    for (ParasiticNode *node : nodes(parasitic))
      report_->reportLine("%s%s %s",
                          name(node),
                          isExternal(node) ? " (ext)" : "",
                          cap_unit->asString(nodeGndCap(node)));
    report_->reportLine("Resistors:");
    for (ParasiticResistor *res : resistors(parasitic)) {
      ParasiticNode *node1 = this->node1(res);
      ParasiticNode *node2 = this->node2(res);
      report_->reportLine("%zu %s%s %s%s %s",
                          id(res),
                          name(node1),
                          isExternal(node1) ? " (ext)" : "",
                          name(node2),
                          isExternal(node2) ? " (ext)" : "",
                          units_->resistanceUnit()->asString(value(res)));
    }
    report_->reportLine("Coupling Capacitors:");
    for (ParasiticCapacitor *cap : capacitors(parasitic)) {
      ParasiticNode *node1 = this->node1(cap);
      ParasiticNode *node2 = this->node2(cap);
      report_->reportLine("%zu %s%s %s%s %s",
                          id(cap),
                          name(node1),
                          isExternal(node1) ? " (ext)" : "",
                          name(node2),
                          isExternal(node2) ? " (ext)" : "",
                          cap_unit->asString(value(cap)));
    }
  }
}

const Net *
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

ParasiticNode *
Parasitics::findNode(const Parasitic *parasitic,
                     const Pin *pin) const
{
  return findParasiticNode(parasitic, pin);
}

PinSet
Parasitics::loads(const Pin *drvr_pin) const
{
  PinSet loads(network_);
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isLoad(pin) && !network_->isHierarchical(pin))
      loads.insert(pin);
  }
  delete pin_iter;
  return loads;
}

ParasiticNodeResistorMap
Parasitics::parasiticNodeResistorMap(const Parasitic *parasitic) const
{
  ParasiticNodeResistorMap resistor_map;
  for (ParasiticResistor *resistor : resistors(parasitic)) {
    ParasiticNode *n1 = node1(resistor);
    ParasiticNode *n2 = node2(resistor);
    resistor_map[n1].push_back(resistor);
    resistor_map[n2].push_back(resistor);
  }
  return resistor_map;
}

ParasiticNodeCapacitorMap 
Parasitics::parasiticNodeCapacitorMap(const Parasitic *parasitic) const
{
  ParasiticNodeCapacitorMap  capacitor_map;
  for (ParasiticCapacitor *capacitor : capacitors(parasitic)) {
    ParasiticNode *n1 = node1(capacitor);
    ParasiticNode *n2 = node2(capacitor);
    capacitor_map[n1].push_back(capacitor);
    capacitor_map[n2].push_back(capacitor);
  }
  return capacitor_map;
}

ParasiticNode *
Parasitics::otherNode(const ParasiticResistor *resistor,
                      ParasiticNode *node) const
{
  ParasiticNode *n1 = node1(resistor);
  if (node == n1)
    return node2(resistor);
  else if (node == node2(resistor))
    return n1;
  else
    return nullptr;
}

ParasiticNode *
Parasitics::otherNode(const ParasiticCapacitor *capacitor,
                      ParasiticNode *node) const
{
  ParasiticNode *n1 = node1(capacitor);
  if (node == n1)
    return node2(capacitor);
  else if (node == node2(capacitor))
    return n1;
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

Parasitic *
Parasitics::reduceToPiElmore(const Parasitic *parasitic,
                             const Pin *drvr_pin,
                             const RiseFall *rf,
                             const Corner *corner,
                             const MinMax *cnst_min_max,
                             const ParasiticAnalysisPt *ap)
{
  return sta::reduceToPiElmore(parasitic, drvr_pin, rf, ap->couplingCapFactor(),
                               corner, cnst_min_max, ap, this);
}

Parasitic *
Parasitics::reduceToPiPoleResidue2(const Parasitic *parasitic,
                                   const Pin *drvr_pin,
                                   const RiseFall *rf,
                                   const Corner *corner,
                                   const MinMax *cnst_min_max,
                                   const ParasiticAnalysisPt *ap)
{
  return sta::reduceToPiPoleResidue2(parasitic, drvr_pin, rf,
                                     ap->couplingCapFactor(),
                                     corner, cnst_min_max,
                                     ap, this);
}

////////////////////////////////////////////////////////////////

Parasitic *
Parasitics::estimatePiElmore(const Pin *drvr_pin,
                             const RiseFall *rf,
                             const Wireload *wireload,
                             float fanout,
                             float net_pin_cap,
                             const Corner *corner,
                             const MinMax *min_max)
{
  EstimateParasitics estimate(this);
  float c2, rpi, c1, elmore_res, elmore_cap;
  bool elmore_use_load_cap;
  estimate.estimatePiElmore(drvr_pin, rf, wireload, fanout, net_pin_cap,
                            corner, min_max,
                            c2, rpi, c1,
                            elmore_res, elmore_cap, elmore_use_load_cap);

  if (c1 > 0.0 || c2 > 0.0) {
    ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(min_max);
    Parasitic *parasitic = makePiElmore(drvr_pin, rf, ap, c2, rpi, c1);
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      if (network_->isLoad(pin)) {
        float load_cap = 0.0;
        if (elmore_use_load_cap)
          load_cap = sdc_->pinCapacitance(pin, rf, corner, min_max);
        float elmore = elmore_res * (elmore_cap + load_cap);
        setElmore(parasitic, pin, elmore);
      }
    }
    delete pin_iter;
    return parasitic;
  }
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

Parasitic *
Parasitics::makeWireloadNetwork(const Pin *drvr_pin,
				const Wireload *wireload,
				float fanout,
                                const MinMax *min_max,
				const ParasiticAnalysisPt *ap)
{
  Parasitic *parasitic = nullptr;
  const Net *net = findParasiticNet(drvr_pin);
  if (net) {
    parasitic = makeParasiticNetwork(net, false, ap);
    const OperatingConditions *op_cond = sdc_->operatingConditions(min_max);
    float wireload_cap, wireload_res;
    wireload->findWireload(fanout, op_cond, wireload_cap, wireload_res);

    WireloadTree tree = WireloadTree::balanced;
    if (op_cond)
      tree = op_cond->wireloadTree();
    switch (tree) {
    case WireloadTree::worst_case:
      makeWireloadNetworkWorst(parasitic, drvr_pin, net, wireload_cap, 
                               wireload_res, fanout);
      break;
    case WireloadTree::balanced:
      makeWireloadNetworkBalanced(parasitic, drvr_pin, wireload_cap,
                                  wireload_res, fanout);
      break;
    case WireloadTree::best_case:
    case WireloadTree::unknown:
      makeWireloadNetworkBest(parasitic, drvr_pin, wireload_cap, 
                              wireload_res, fanout);
      break;
    }
  }
  return parasitic;
}

// All load capacitance (except driver pin cap) is on the far side of
// the resistor.
void
Parasitics::makeWireloadNetworkWorst(Parasitic *parasitic,
				     const Pin *drvr_pin,
                                     const Net *net,
				     float wireload_cap,
				     float wireload_res,
				     float /* fanout */)
{
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin, network_);
  size_t resistor_index = 1;
  ParasiticNode *load_node = ensureParasiticNode(parasitic, net, 0, network_);
  makeResistor(parasitic, resistor_index++, wireload_res, drvr_node, load_node);
  parasitics_->incrCap(load_node, wireload_cap);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin, network_);
      makeResistor(parasitic, resistor_index++, 0.0, load_node, load_node1);
    }
  }
}

// No wire resistance, so load is lumped capacitance.
void
Parasitics::makeWireloadNetworkBest(Parasitic *parasitic,
				    const Pin *drvr_pin,
				    float wireload_cap,
				    float /* wireload_res */,
				    float /* fanout */)
{
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin, network_);
  parasitics_->incrCap(drvr_node, wireload_cap);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  size_t resistor_index = 1;
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin, network_);
      makeResistor(parasitic, resistor_index++, 0.0, drvr_node, load_node1);
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
					float fanout)
{
  float fanout_cap = wireload_cap / fanout;
  float fanout_res = wireload_res / fanout;
  ParasiticNode *drvr_node = ensureParasiticNode(parasitic, drvr_pin, network_);
  PinConnectedPinIterator *load_iter =
    network_->connectedPinIterator(drvr_pin);
  size_t resistor_index = 1;
  while (load_iter->hasNext()) {
    const Pin *load_pin = load_iter->next();
    if (load_pin != drvr_pin
	&& network_->isLoad(load_pin)) {
      ParasiticNode *load_node1 = ensureParasiticNode(parasitic, load_pin, network_);
      makeResistor(parasitic, resistor_index++, fanout_res, drvr_node, load_node1);    
      parasitics_->incrCap(load_node1, fanout_cap);
    }
  }
}

////////////////////////////////////////////////////////////////

ParasiticAnalysisPt::ParasiticAnalysisPt(const char *name,
					 int index,
                                         int index_max) :
  name_(name),
  index_(index),
  index_max_(index_max),
  coupling_cap_factor_(1.0)
{
}

void
ParasiticAnalysisPt::setCouplingCapFactor(float factor)
{
  coupling_cap_factor_ = factor;
}

////////////////////////////////////////////////////////////////

ParasiticNodeLess::ParasiticNodeLess(const Parasitics *parasitics,
                                     const Network *network) :
  parasitics_(parasitics),
  network_(network)
{
}

ParasiticNodeLess::ParasiticNodeLess(const ParasiticNodeLess &less) :
  parasitics_(less.parasitics_),
  network_(less.network_)
{
}

bool
ParasiticNodeLess::operator()(const ParasiticNode *node1,
                              const ParasiticNode *node2) const
{
  const Pin *pin1 = parasitics_->pin(node1);
  const Pin *pin2 = parasitics_->pin(node2);
  const Net *net1 = parasitics_->net(node1, network_);
  const Net *net2 = parasitics_->net(node2, network_);
  unsigned id1 = parasitics_->netId(node1);
  unsigned id2 = parasitics_->netId(node2);
  return (pin1 == nullptr && pin2)
    || (pin1 && pin2
        && network_->id(pin1) < network_->id(pin2))
    || (pin1 == nullptr && pin2 == nullptr
        && (network_->id(net1) < network_->id(net2)
            || (net1 == net2
                && id1 < id2)));
}

} // namespace
