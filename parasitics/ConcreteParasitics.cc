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

#include "ConcreteParasitics.hh"

#include <algorithm> // max

#include "Report.hh"
#include "Debug.hh"
#include "Error.hh"
#include "Mutex.hh"
#include "Set.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Wireload.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "MakeConcreteParasitics.hh"
#include "ConcreteParasiticsPvt.hh"
#include "Corner.hh"

// Multiple inheritance is used to share elmore and pi model base
// classes, but care is taken to make sure there are no loops in the
// inheritance graph (ConcreteParasitic only included once).

namespace sta {

using std::max;

ConcreteParasitic::~ConcreteParasitic()
{
}

bool
ConcreteParasitic::isPiElmore() const
{
  return false;
}

bool
ConcreteParasitic::isPiModel() const
{
  return false;
}

bool
ConcreteParasitic::isPiPoleResidue() const
{
  return false;
}

bool
ConcreteParasitic::isPoleResidue() const
{
  return false;
}

bool
ConcreteParasitic::isParasiticNetwork() const
{
  return false;
}

void
ConcreteParasitic::piModel(float &,
			   float &,
			   float &) const
{
}

void
ConcreteParasitic::setPiModel(float,
			      float,
			      float)
{
}

bool
ConcreteParasitic::isReducedParasiticNetwork() const
{
  return false;
}

void
ConcreteParasitic::setIsReduced(bool)
{
}

void
ConcreteParasitic::findElmore(const Pin *,
			      float &,
			      bool &exists) const
{
  exists = false;
}

void
ConcreteParasitic::setElmore(const Pin *,
			     float)
{
}

Parasitic *
ConcreteParasitic::findPoleResidue(const Pin *) const
{
  return nullptr;
}

void
ConcreteParasitic::setPoleResidue(const Pin *,
				  ComplexFloatSeq *,
				  ComplexFloatSeq *)
{
}

////////////////////////////////////////////////////////////////

ConcretePi::ConcretePi(float c2,
		       float rpi,
		       float c1) :
  c2_(c2),
  rpi_(rpi),
  c1_(c1),
  is_reduced_(false)
{
}

float
ConcretePi::capacitance() const
{
  return c1_ + c2_;
}

void
ConcretePi::setPiModel(float c2,
		       float rpi,
		       float c1)
{
  c2_ = c2;
  rpi_ = rpi;
  c1_ = c1;
}

void
ConcretePi::piModel(float &c2,
		    float &rpi,
		    float &c1) const
{
  c2 = c2_;
  rpi = rpi_;
  c1 = c1_;
}

void
ConcretePi::setIsReduced(bool reduced)
{
  is_reduced_ = reduced;
}

////////////////////////////////////////////////////////////////

ConcretePiElmore::ConcretePiElmore(float c2,
				   float rpi,
				   float c1) :
  ConcretePi(c2, rpi, c1)
{
}

float
ConcretePiElmore::capacitance() const
{
  return ConcretePi::capacitance();
}

void
ConcretePiElmore::piModel(float &c2,
			  float &rpi,
			  float &c1) const
{
  ConcretePi::piModel(c2, rpi, c1);
}

void
ConcretePiElmore::setPiModel(float c2,
			     float rpi,
			     float c1)
{
  ConcretePi::setPiModel(c2, rpi, c1);
}

bool
ConcretePiElmore::isReducedParasiticNetwork() const
{
  return ConcretePi::isReducedParasiticNetwork();
}

void
ConcretePiElmore::setIsReduced(bool reduced)
{
  ConcretePi::setIsReduced(reduced);
}

void
ConcretePiElmore::findElmore(const Pin *load_pin,
			     float &elmore,
			     bool &exists) const
{
  auto itr = loads_.find(load_pin);
  if (itr == loads_.end())
    exists = false;
  else {
    elmore = itr->second;
    exists = true;
  }
}

void
ConcretePiElmore::setElmore(const Pin *load_pin,
			    float elmore)
{
  loads_[load_pin] = elmore;
}

void
ConcretePiElmore::deleteLoad(const Pin *load_pin)
{
  loads_.erase(load_pin);
}

PinSet
ConcretePiElmore::unannotatedLoads(const Pin *drvr_pin,
                                   const Parasitics *parasitics) const
{
  PinSet loads = parasitics->loads(drvr_pin);
  for (const auto [load, elmore] : loads_)
    loads.erase(load);
  return loads;
}

////////////////////////////////////////////////////////////////

ConcretePoleResidue::
ConcretePoleResidue() :
  poles_(nullptr),
  residues_(nullptr)
{
}

ConcretePoleResidue::~ConcretePoleResidue()
{
  delete poles_;
  delete residues_;
}

size_t
ConcretePoleResidue::poleResidueCount() const
{
  return poles_->size();
}

void
ConcretePoleResidue::poleResidue(int index,
				 ComplexFloat &pole,
				 ComplexFloat &residue) const
{
  pole = (*poles_)[index];
  residue = (*residues_)[index];
}

void
ConcretePoleResidue::setPoleResidue(ComplexFloatSeq *poles,
				    ComplexFloatSeq *residues)
{
  poles_ = poles;
  residues_ = residues;
}

PinSet
ConcretePoleResidue::unannotatedLoads(const Pin *,
                                      const Parasitics *) const
{
  return PinSet();
}

////////////////////////////////////////////////////////////////

ConcretePiPoleResidue::ConcretePiPoleResidue(float c2,
					     float rpi,
					     float c1) :
  ConcretePi(c2, rpi, c1)
{
}

float
ConcretePiPoleResidue::capacitance() const
{
  return ConcretePi::capacitance();
}

void
ConcretePiPoleResidue::piModel(float &c2,
			       float &rpi,
			       float &c1) const
{
  ConcretePi::piModel(c2, rpi, c1);
}

void
ConcretePiPoleResidue::setPiModel(float c2,
				  float rpi,
				  float c1)
{
  ConcretePi::setPiModel(c2, rpi, c1);
}

bool
ConcretePiPoleResidue::isReducedParasiticNetwork() const
{
  return ConcretePi::isReducedParasiticNetwork();
}

void
ConcretePiPoleResidue::setIsReduced(bool reduced)
{
  ConcretePi::setIsReduced(reduced);
}

Parasitic *
ConcretePiPoleResidue::findPoleResidue(const Pin *load_pin) const
{
  auto itr = load_pole_residue_.find(load_pin);
  if (itr == load_pole_residue_.end())
    return nullptr;
  else
    return &const_cast<ConcretePoleResidue&>(itr->second);
}

void
ConcretePiPoleResidue::setPoleResidue(const Pin *load_pin,
				      ComplexFloatSeq *poles,
				      ComplexFloatSeq *residues)
{
  ConcretePoleResidue &pole_residue = load_pole_residue_[load_pin];
  pole_residue.setPoleResidue(poles, residues);
}

void
ConcretePiPoleResidue::deleteLoad(const Pin *load_pin)
{
  load_pole_residue_.erase(load_pin);
}

PinSet
ConcretePiPoleResidue::unannotatedLoads(const Pin *drvr_pin,
                                        const Parasitics *parasitics) const
{
  PinSet loads = parasitics->loads(drvr_pin);
  for (const auto& [load, pole_residue] : load_pole_residue_)
    loads.erase(load);
  return loads;
}

////////////////////////////////////////////////////////////////

ConcreteParasiticNode::ConcreteParasiticNode(const Net *net,
                                             int id,
                                             bool is_external) :
  is_net_(true),
  is_external_(is_external),
  id_(id),
  cap_(0.0)
{
  net_pin_.net_ = net;
}

ConcreteParasiticNode::ConcreteParasiticNode(const Pin *pin,
                                             bool is_external) :
  is_net_(false),
  is_external_(is_external),
  id_(0),
  cap_(0.0)
{
  net_pin_.pin_ = pin;
}

void
ConcreteParasiticNode::incrCapacitance(float cap)
{
  cap_ += cap;
}

const char *
ConcreteParasiticNode::name(const Network *network) const
{
  if (is_net_) {
    const char *net_name = network->pathName(net_pin_.net_);
    return stringPrintTmp("%s:%d", net_name, id_);
  }
  else
    return network->pathName(net_pin_.pin_);
}

const Pin *
ConcreteParasiticNode::pin() const
{
  if (is_net_)
    return nullptr;
  else
    return net_pin_.pin_;
}

const Net *
ConcreteParasiticNode::net(const Network *network) const
{
  if (is_net_)
    return net_pin_.net_;
  else {
    Net *net = network->net(net_pin_.pin_);
    // Pins on the top level instance may not have nets.
    // Use the net connected to the pin's terminal.
    if (net == nullptr && network->isTopLevelPort(net_pin_.pin_)) {
      Term *term = network->term(net_pin_.pin_);
      if (term)
        return network->net(term);
    }
    return net;
  }
}

////////////////////////////////////////////////////////////////

                       
ConcreteParasiticDevice::ConcreteParasiticDevice(size_t id,
						 float value,
						 ConcreteParasiticNode *node1,
						 ConcreteParasiticNode *node2) :
  id_(id),
  value_(value),
  node1_(node1),
  node2_(node2)
{
}

void
ConcreteParasiticDevice::replaceNode(ConcreteParasiticNode *from_node,
                                     ConcreteParasiticNode *to_node)
{
  if (from_node == node1_)
    node1_ = to_node;
  else if (from_node == node2_)
    node2_ = to_node;
}

ConcreteParasiticResistor::ConcreteParasiticResistor(size_t id,
                                                     float value,
                                                     ConcreteParasiticNode *node1,
                                                     ConcreteParasiticNode *node2) :
  ConcreteParasiticDevice(id, value, node1, node2)
{
}

ConcreteParasiticCapacitor::ConcreteParasiticCapacitor(size_t id,
                                                       float value,
                                                       ConcreteParasiticNode *node1,
                                                       ConcreteParasiticNode *node2) :
  ConcreteParasiticDevice(id, value, node1, node2)
{
}

////////////////////////////////////////////////////////////////

ConcreteParasiticNetwork::ConcreteParasiticNetwork(const Net *net,
                                                   bool includes_pin_caps,
                                                   const Network *network) :
  net_(net),
  sub_nodes_(network),
  pin_nodes_(network),
  max_node_id_(0),
  includes_pin_caps_(includes_pin_caps)
{
}

ConcreteParasiticNetwork::~ConcreteParasiticNetwork()
{
  deleteDevices();
  deleteNodes();
}

void
ConcreteParasiticNetwork::deleteNodes()
{
  for (const auto& [id, node] : sub_nodes_)
    delete node;
  for (const auto& [pin, node] : pin_nodes_)
    delete node;
}

void
ConcreteParasiticNetwork::deleteDevices()
{
  for (ParasiticResistor *resistor : resistors_) {
    ConcreteParasiticResistor *cresistor =
      reinterpret_cast<ConcreteParasiticResistor*>(resistor);
    delete cresistor;
  }
  for (ParasiticCapacitor *capacitor : capacitors_) {
    ConcreteParasiticCapacitor *ccapacitor =
      reinterpret_cast<ConcreteParasiticCapacitor*>(capacitor);
    delete ccapacitor;
  }
}

void
ConcreteParasiticNetwork::addResistor(ParasiticResistor *resistor)
{
  resistors_.push_back(resistor);
}

void
ConcreteParasiticNetwork::addCapacitor(ParasiticCapacitor *capacitor)
{
  capacitors_.push_back(capacitor);
}

ParasiticNodeSeq
ConcreteParasiticNetwork::nodes() const
{
  ParasiticNodeSeq nodes;
  for (const auto [pin, node] : pin_nodes_)
    nodes.push_back(node);
  for (const auto& [id, node] : sub_nodes_)
    nodes.push_back(node);
  return nodes;
}

float
ConcreteParasiticNetwork::capacitance() const
{
  float cap = 0.0;
  for (const auto& [id, node] : sub_nodes_) {
    if (!node->isExternal())
      cap += node->capacitance();
  }

  for (const auto [pin, node] : pin_nodes_) {
    if (!node->isExternal())
      cap += node->capacitance();
  }

  for (ParasiticCapacitor *capacitor : capacitors_) {
    ConcreteParasiticCapacitor *ccap =
      static_cast<ConcreteParasiticCapacitor*>(capacitor);
    cap += ccap->value();
  }

  return cap;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::findParasiticNode(const Net *net,
                                            int id,
                                            const Network *) const
{
  NetIdPair net_id(net, id);
  auto id_node = sub_nodes_.find(net_id);
  if (id_node == sub_nodes_.end()) 
    return nullptr;
  else
    return id_node->second;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::findParasiticNode(const Pin *pin) const
{
  auto pin_node = pin_nodes_.find(pin);
  if (pin_node == pin_nodes_.end())
    return nullptr;
  else
    return pin_node->second;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::ensureParasiticNode(const Net *net,
					      int id,
                                              const Network *network)
{
  ConcreteParasiticNode *node;
  NetIdPair net_id(net, id);
  auto id_node = sub_nodes_.find(net_id);
  if (id_node == sub_nodes_.end()) {
    Net *net1 = network->highestNetAbove(const_cast<Net*>(net));
    node = new ConcreteParasiticNode(net, id, network->highestNetAbove(net1) != net_);
    sub_nodes_[net_id] = node;
    if (net == net_)
      max_node_id_ = max((int) max_node_id_, id);
  }
  else
    node = id_node->second;
  return node;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::ensureParasiticNode(const Pin *pin,
                                              const Network *network)
{
  ConcreteParasiticNode *node;
  auto pin_node = pin_nodes_.find(pin);
  if (pin_node == pin_nodes_.end()) {
    Net *net = network->net(pin);
    // Pins on the top level instance may not have nets.
    // Use the net connected to the pin's terminal.
    if (net == nullptr && network->isTopLevelPort(pin)) {
      Term *term = network->term(pin);
      if (term)
        net = network->net(term);
    }
    else if (net)
      net = network->highestNetAbove(net);
    node = new ConcreteParasiticNode(pin, net != net_);
    pin_nodes_[pin] = node;
  }
  else
    node = pin_node->second;
  return node;
}

PinSet
ConcreteParasiticNetwork::unannotatedLoads(const Pin *drvr_pin,
                                           const Parasitics *parasitics) const
{
  PinSet loads = parasitics->loads(drvr_pin);
  ParasiticNode *drvr_node = findParasiticNode(drvr_pin);
  if (drvr_node) {
    ParasiticNodeResistorMap resistor_map =
      parasitics->parasiticNodeResistorMap(this);
    ParasiticNodeSet visited_nodes;
    ParasiticResistorSet loop_resistors;
    unannotatedLoads(drvr_node, nullptr, loads, visited_nodes,
                     loop_resistors, resistor_map, parasitics);
  }
  return loads;
}

void
ConcreteParasiticNetwork::unannotatedLoads(ParasiticNode *node,
                                           ParasiticResistor *from_res,
                                           PinSet &loads,
                                           ParasiticNodeSet &visited_nodes,
                                           ParasiticResistorSet &loop_resistors,
                                           ParasiticNodeResistorMap &resistor_map,
                                           const Parasitics *parasitics) const
{
  const Pin *pin = parasitics->pin(node);
  if (pin)
    loads.erase(const_cast<Pin*>(pin));

  visited_nodes.insert(node);
  ParasiticResistorSeq &resistors = resistor_map[node];
  for (ParasiticResistor *resistor : resistors) {
    if (loop_resistors.find(resistor) == loop_resistors.end()) {
      ParasiticNode *onode = parasitics->otherNode(resistor, node);
      // One commercial extractor creates resistors with identical from/to nodes.
      if (onode != node
	  && resistor != from_res) {
        if (visited_nodes.find(onode) == visited_nodes.end())
          unannotatedLoads(onode, resistor, loads, visited_nodes,
                           loop_resistors, resistor_map, parasitics);
        else
          // resistor loop
          loop_resistors.insert(resistor);
      }
    }
  }
  visited_nodes.erase(node);
}

////////////////////////////////////////////////////////////////

void
ConcreteParasiticNetwork::disconnectPin(const Pin *pin,
					const Net *net,
                                        const Network *network)
{
  auto pin_node = pin_nodes_.find(pin);
  if (pin_node != pin_nodes_.end()) {
    ConcreteParasiticNode *node = pin_node->second;
    // Make a subnode to replace the pin node.
    ConcreteParasiticNode *subnode = ensureParasiticNode(net,max_node_id_+1,
                                                         network);
    // Hand over the devices.
    for (ParasiticResistor *resistor : resistors_) {
      ConcreteParasiticResistor *cresistor =
        reinterpret_cast<ConcreteParasiticResistor*>(resistor);
      cresistor->replaceNode(node, subnode);
    }
    for (ParasiticCapacitor *capacitor : capacitors_) {
      ConcreteParasiticCapacitor *ccapacitor =
        reinterpret_cast<ConcreteParasiticCapacitor*>(capacitor);
      ccapacitor->replaceNode(node, subnode);
    }

    pin_nodes_.erase(pin);
    delete node;
  }
}

NetIdPairLess::NetIdPairLess(const Network *network) :
  net_less_(network)
{
}

bool
NetIdPairLess::operator()(const NetIdPair &net_id1,
                          const NetIdPair &net_id2) const
{
  const Net *net1 = net_id1.first;
  const Net *net2 = net_id2.first;
  int id1 = net_id1.second;
  int id2 = net_id2.second;
  return net_less_(net1, net2)
    || (net1 == net2
	&& id1 < id2);
}

////////////////////////////////////////////////////////////////

Parasitics *
makeConcreteParasitics(StaState *sta)
{
  return new ConcreteParasitics(sta);
}

ConcreteParasitics::ConcreteParasitics(StaState *sta) :
  Parasitics(sta)
{
}

ConcreteParasitics::~ConcreteParasitics()
{
  clear();
}

bool
ConcreteParasitics::haveParasitics()
{
  return !drvr_parasitic_map_.empty()
    || !parasitic_network_map_.empty();
}

void
ConcreteParasitics::clear()
{
  deleteParasitics();
}

int
ConcreteParasitics::parasiticAnalysisPtIndex(const ParasiticAnalysisPt *ap,
					     const RiseFall *rf) const
{
  return ap->index() * RiseFall::index_count + rf->index();
}

void
ConcreteParasitics::deleteParasitics()
{
  int ap_count = corners_->parasiticAnalysisPtCount();
  int ap_rf_count = ap_count * RiseFall::index_count;
  for (const auto [drvr, parasitics] : drvr_parasitic_map_) {
    if (parasitics) {
      for (int i = 0; i < ap_rf_count; i++)
	delete parasitics[i];
      delete [] parasitics;
    }
  }
  drvr_parasitic_map_.clear();

  for (const auto [net, parasitics] : parasitic_network_map_) {
    if (parasitics) {
      for (int i = 0; i < ap_count; i++)
	delete parasitics[i];
      delete [] parasitics;
    }
  }
  parasitic_network_map_.clear();
}

void
ConcreteParasitics::deleteParasitics(const Pin *drvr_pin,
				     const ParasiticAnalysisPt *ap)
{
  ConcreteParasitic **parasitics = drvr_parasitic_map_[drvr_pin];
  if (parasitics) {
    for (auto rf : RiseFall::range()) {
      int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
      delete parasitics[ap_rf_index];
      parasitics[ap_rf_index] = nullptr;
    }
  }
}

void
ConcreteParasitics::deleteParasitics(const Net *net,
				     const ParasiticAnalysisPt *ap)
{
  PinSet *drivers = network_->drivers(net);
  for (auto drvr_pin : *drivers)
    deleteParasitics(drvr_pin, ap);

  ConcreteParasiticNetwork **parasitics = parasitic_network_map_[net];
  if (parasitics) {
    delete parasitics[ap->index()];
    parasitics[ap->index()] = nullptr;
  }
}

float
ConcreteParasitics::capacitance(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic->capacitance();
}

bool
ConcreteParasitics::isReducedParasiticNetwork(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic->isReducedParasiticNetwork();
}

void
ConcreteParasitics::setIsReducedParasiticNetwork(Parasitic *parasitic,
						 bool is_reduced)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  cparasitic->setIsReduced(is_reduced);
}

void
ConcreteParasitics::disconnectPinBefore(const Pin *pin,
                                        const Network *network)
{
  if (haveParasitics()) {
    deleteReducedParasitics(pin);

    const Net *net = findParasiticNet(pin);
    if (net) {
      ConcreteParasiticNetwork **parasitics = parasitic_network_map_[net];
      if (parasitics) {
        int ap_count = corners_->parasiticAnalysisPtCount();
	for (int i = 0; i < ap_count; i++) {
	  ConcreteParasiticNetwork *parasitic = parasitics[i];
	  if (parasitic)
	    parasitic->disconnectPin(pin, net, network);
	}
      }
    }
  }
}

void
ConcreteParasitics::loadPinCapacitanceChanged(const Pin *pin)
{
  // Delete reduced models that depend on load pin capacitances.
  deleteReducedParasitics(pin);
}

void
ConcreteParasitics::deleteReducedParasitics(const Net *net,
                                            const ParasiticAnalysisPt *ap)
{
  if (!drvr_parasitic_map_.empty()) {
    PinSet *drivers = network_->drivers(net);
    if (drivers) {
      for (auto drvr_pin : *drivers)
        deleteDrvrReducedParasitics(drvr_pin, ap);
    }
  }
}

// Delete reduced models on pin's net.
void
ConcreteParasitics::deleteReducedParasitics(const Pin *pin)
{
  if (!drvr_parasitic_map_.empty()) {
    PinSet *drivers = network_->drivers(pin);
    if (drivers) {
      for (auto drvr_pin : *drivers)
	deleteDrvrReducedParasitics(drvr_pin);
    }
  }
}

void
ConcreteParasitics::deleteDrvrReducedParasitics(const Pin *drvr_pin)
{
  LockGuard lock(lock_);
  ConcreteParasitic **parasitics = drvr_parasitic_map_[drvr_pin];
  if (parasitics) {
    int ap_count = corners_->parasiticAnalysisPtCount();
    int ap_rf_count = ap_count * RiseFall::index_count;
    for (int i = 0; i < ap_rf_count; i++)
      delete parasitics[i];
    delete [] parasitics;
  }
  drvr_parasitic_map_[drvr_pin] = nullptr;
}

void
ConcreteParasitics::deleteDrvrReducedParasitics(const Pin *drvr_pin,
                                                const ParasiticAnalysisPt *ap)
{
  LockGuard lock(lock_);
  ConcreteParasitic **parasitics = drvr_parasitic_map_[drvr_pin];
  if (parasitics) {
    int ap_index = ap->index();
    delete parasitics[ap_index];
    parasitics[ap_index] = nullptr;
  }
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPiElmore(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiElmore();
}

Parasitic *
ConcreteParasitics::findPiElmore(const Pin *drvr_pin,
				 const RiseFall *rf,
				 const ParasiticAnalysisPt *ap) const
{
  if (!drvr_parasitic_map_.empty()) {
    int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
    LockGuard lock(lock_);
    ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
    if (parasitics) {
      ConcreteParasitic *parasitic = parasitics[ap_rf_index];
      if (parasitic && parasitic->isPiElmore())
	return parasitic;
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::makePiElmore(const Pin *drvr_pin,
				 const RiseFall *rf,
				 const ParasiticAnalysisPt *ap,
				 float c2,
				 float rpi,
				 float c1)
{
  LockGuard lock(lock_);
  ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
  if (parasitics == nullptr) {
    int ap_count = corners_->parasiticAnalysisPtCount();
    int ap_rf_count = ap_count * RiseFall::index_count;
    parasitics = new ConcreteParasitic*[ap_rf_count];
    for (int i = 0; i < ap_rf_count; i++)
      parasitics[i] = nullptr;
    drvr_parasitic_map_[drvr_pin] = parasitics;
  }
  int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
  ConcreteParasitic *parasitic = parasitics[ap_rf_index];
  ConcretePiElmore *pi_elmore = nullptr;
  if (parasitic) {
    if (parasitic->isPiElmore()) {
      pi_elmore = dynamic_cast<ConcretePiElmore*>(parasitic);
      pi_elmore->setPiModel(c2, rpi, c1);
    }
    else {
      delete parasitic;
      pi_elmore = new ConcretePiElmore(c2, rpi, c1);
      parasitics[ap_rf_index] = pi_elmore;
    }
  }
  else {
    pi_elmore = new ConcretePiElmore(c2, rpi, c1);
    parasitics[ap_rf_index] = pi_elmore;
  }
  return pi_elmore;
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPiModel(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiModel();
}

void
ConcreteParasitics::piModel(const Parasitic *parasitic,
			    float &c2,
			    float &rpi,
			    float &c1) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  cparasitic->piModel(c2, rpi, c1);
}

void
ConcreteParasitics::setPiModel(Parasitic *parasitic,
			       float c2,
			       float rpi,
			       float c1)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  cparasitic->setPiModel(c2, rpi, c1);
}

////////////////////////////////////////////////////////////////

void
ConcreteParasitics::findElmore(const Parasitic *parasitic,
			       const Pin *load_pin,
			       float &elmore,
			       bool &exists) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  cparasitic->findElmore(load_pin, elmore, exists);
}

void
ConcreteParasitics::setElmore(Parasitic *parasitic,
			      const Pin *load_pin,
			      float elmore)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  cparasitic->setElmore(load_pin, elmore);
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPiPoleResidue(const Parasitic* parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiPoleResidue();
}

Parasitic *
ConcreteParasitics::findPiPoleResidue(const Pin *drvr_pin,
				      const RiseFall *rf,
				      const ParasiticAnalysisPt *ap) const
{
  if (!drvr_parasitic_map_.empty()) {
    int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
    LockGuard lock(lock_);
    ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
    if (parasitics) {
      ConcreteParasitic *parasitic = parasitics[ap_rf_index];
      if (parasitic == nullptr && rf == RiseFall::fall()) {
	ap_rf_index = parasiticAnalysisPtIndex(ap, RiseFall::rise());
	parasitic = parasitics[ap_rf_index];
      }
      if (parasitic && parasitic->isPiPoleResidue())
	return parasitic;
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::makePiPoleResidue(const Pin *drvr_pin,
				      const RiseFall *rf,
				      const ParasiticAnalysisPt *ap,
				      float c2,
				      float rpi,
				      float c1)
{
  LockGuard lock(lock_);
  ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
  if (parasitics == nullptr) {
    int ap_count = corners_->parasiticAnalysisPtCount();
    int ap_rf_count = ap_count * RiseFall::index_count;
    parasitics = new ConcreteParasitic*[ap_rf_count];
    for (int i = 0; i < ap_rf_count; i++)
      parasitics[i] = nullptr;
    drvr_parasitic_map_[drvr_pin] = parasitics;
  }
  int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
  ConcreteParasitic *parasitic = parasitics[ap_rf_index];
  ConcretePiPoleResidue *pi_pole_residue = nullptr;
  if (parasitic) {
    if (parasitic->isPiElmore()) {
      pi_pole_residue = dynamic_cast<ConcretePiPoleResidue*>(parasitic);
      pi_pole_residue->setPiModel(c2, rpi, c1);
    }
    else {
      delete parasitic;
      pi_pole_residue = new ConcretePiPoleResidue(c2, rpi, c1);
      parasitics[ap_rf_index] = pi_pole_residue;
    }
  }
  else {
    pi_pole_residue = new ConcretePiPoleResidue(c2, rpi, c1);
    parasitics[ap_rf_index] = pi_pole_residue;
  }
  return pi_pole_residue;
}

Parasitic *
ConcreteParasitics::findPoleResidue(const Parasitic *parasitic,
				    const Pin *load_pin) const
{
  const ConcreteParasitic *cparasitic =
    static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic->findPoleResidue(load_pin);
}

void
ConcreteParasitics::setPoleResidue(Parasitic *parasitic,
				   const Pin *load_pin,
				   ComplexFloatSeq *poles,
				   ComplexFloatSeq *residues)
{
  ConcreteParasitic *cparasitic =
    static_cast<ConcreteParasitic*>(parasitic);
  cparasitic->setPoleResidue(load_pin, poles, residues);
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPoleResidue(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic =
    static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic->isPoleResidue();
}

size_t
ConcreteParasitics::poleResidueCount(const Parasitic *parasitic) const
{
  const ConcretePoleResidue *pr_parasitic =
    static_cast<const ConcretePoleResidue*>(parasitic);
  return pr_parasitic->poleResidueCount();
}

void
ConcreteParasitics::poleResidue(const Parasitic *parasitic,
				int pole_index,
				ComplexFloat &pole,
				ComplexFloat &residue) const
{
  const ConcretePoleResidue *pr_parasitic =
    static_cast<const ConcretePoleResidue*>(parasitic);
  pr_parasitic->poleResidue(pole_index, pole, residue);
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isParasiticNetwork(const Parasitic *parasitic) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isParasiticNetwork();
}

Parasitic *
ConcreteParasitics::findParasiticNetwork(const Net *net,
					 const ParasiticAnalysisPt *ap) const
{
  if (!parasitic_network_map_.empty()) {
    LockGuard lock(lock_);
    if (!parasitic_network_map_.empty()) {
      ConcreteParasiticNetwork **parasitics=parasitic_network_map_.findKey(net);
      if (parasitics) {
        ConcreteParasiticNetwork *parasitic = parasitics[ap->index()];
        if (parasitic == nullptr)
          parasitic = parasitics[ap->indexMax()];
	return parasitic;
      }
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::findParasiticNetwork(const Pin *pin,
					 const ParasiticAnalysisPt *ap) const
{
  if (!parasitic_network_map_.empty()) {
    LockGuard lock(lock_);
    if (!parasitic_network_map_.empty()) {
      // Only call findParasiticNet if parasitics exist.
      const Net *net = findParasiticNet(pin);
      ConcreteParasiticNetwork **parasitics=parasitic_network_map_.findKey(net);
      if (parasitics) {
        ConcreteParasiticNetwork *parasitic = parasitics[ap->index()];
        if (parasitic == nullptr)
          parasitic = parasitics[ap->indexMax()];
	return parasitic;
      }
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::makeParasiticNetwork(const Net *net,
					 bool includes_pin_caps,
					 const ParasiticAnalysisPt *ap)
{
  LockGuard lock(lock_);
  ConcreteParasiticNetwork **parasitics = parasitic_network_map_.findKey(net);
  if (parasitics == nullptr) {
    int ap_count = corners_->parasiticAnalysisPtCount();
    parasitics = new ConcreteParasiticNetwork*[ap_count];
    for (int i = 0; i < ap_count; i++)
      parasitics[i] = nullptr;
    parasitic_network_map_[net] = parasitics;
  }
  int ap_index = ap->index();
  ConcreteParasiticNetwork *parasitic = parasitics[ap_index];
  if (parasitic) {
    delete parasitic;
    if (net) {
      for (const Pin *drvr_pin : *network_->drivers(net))
        deleteParasitics(drvr_pin, ap);
    }
  }
  parasitic = new ConcreteParasiticNetwork(net, includes_pin_caps, network_);
  parasitics[ap_index] = parasitic;
  return parasitic;
}

void
ConcreteParasitics::deleteParasiticNetwork(const Net *net,
					   const ParasiticAnalysisPt *ap)
{
  if (!parasitic_network_map_.empty()) {
    LockGuard lock(lock_);
    ConcreteParasiticNetwork **parasitics = parasitic_network_map_.findKey(net);
    if (parasitics) {
      int ap_index = ap->index();
      delete parasitics[ap_index];
      parasitics[ap_index] = nullptr;

      int ap_count = corners_->parasiticAnalysisPtCount();
      bool have_parasitics = false;
      for (int i = 0; i < ap_count; i++) {
        if (parasitics[i]) {
          have_parasitics = true;
          break;
        }
      }
      if (!have_parasitics) {
        delete [] parasitics;
        parasitic_network_map_.erase(net);
      }
    }
  }
}

void
ConcreteParasitics::deleteParasiticNetworks(const Net *net)
{
  if (!parasitic_network_map_.empty()) {
    LockGuard lock(lock_);
    ConcreteParasiticNetwork **parasitics = parasitic_network_map_.findKey(net);
    if (parasitics) {
      int ap_count = corners_->parasiticAnalysisPtCount();
      for (int i = 0; i < ap_count; i++)
	delete parasitics[i];
      delete [] parasitics;
      parasitic_network_map_.erase(net);
    }
  }
}

const Net *
ConcreteParasitics::net(const Parasitic *parasitic) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  if (cparasitic->isParasiticNetwork())
    return cparasitic->net();
  else
    return nullptr;
}

bool
ConcreteParasitics::includesPinCaps(const Parasitic *parasitic) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->includesPinCaps();
}

ParasiticNode *
ConcreteParasitics::findParasiticNode(Parasitic *parasitic,
                                      const Net *net,
                                      int id,
                                      const Network *network) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->findParasiticNode(net, id, network);
}

ParasiticNode *
ConcreteParasitics::ensureParasiticNode(Parasitic *parasitic,
					const Net *net,
					int id,
                                        const Network *network)
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->ensureParasiticNode(net, id, network);
}

ParasiticNode *
ConcreteParasitics::findParasiticNode(const Parasitic *parasitic,
                                      const Pin *pin) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->findParasiticNode(pin);
}

ParasiticNode *
ConcreteParasitics::ensureParasiticNode(Parasitic *parasitic,
					const Pin *pin,
                                        const Network *network)
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->ensureParasiticNode(pin, network);
}

void
ConcreteParasitics::incrCap(ParasiticNode *node,
			    float cap)
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  cnode->incrCapacitance(cap);
}

void
ConcreteParasitics::makeCapacitor(Parasitic *parasitic,
                                  size_t index,
                                  float cap,
                                  ParasiticNode *node1,
                                  ParasiticNode *node2)
{
  ConcreteParasiticNode *cnode1 = static_cast<ConcreteParasiticNode*>(node1);
  ConcreteParasiticNode *cnode2 = static_cast<ConcreteParasiticNode*>(node2);
  ConcreteParasiticCapacitor *capacitor =
    new ConcreteParasiticCapacitor(index, cap, cnode1, cnode2);
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  cparasitic->addCapacitor(capacitor);
}

void
ConcreteParasitics::makeResistor(Parasitic *parasitic,
                                 size_t index,
                                 float res,
				 ParasiticNode *node1,
				 ParasiticNode *node2)
{
  ConcreteParasiticNode *cnode1 = static_cast<ConcreteParasiticNode*>(node1);
  ConcreteParasiticNode *cnode2 = static_cast<ConcreteParasiticNode*>(node2);
  ParasiticResistor *resistor = new ConcreteParasiticResistor(index, res,
                                                              cnode1, cnode2);
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  cparasitic->addResistor(resistor);
}

ParasiticNodeSeq
ConcreteParasitics::nodes(const Parasitic *parasitic) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->nodes();
}

ParasiticResistorSeq
ConcreteParasitics::resistors(const Parasitic *parasitic) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->resistors();
}

ParasiticCapacitorSeq
ConcreteParasitics::capacitors(const Parasitic *parasitic) const
{
  const ConcreteParasiticNetwork *cparasitic =
    static_cast<const ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->capacitors();
}


const char *
ConcreteParasitics::name(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->name(network_);
}

float
ConcreteParasitics::nodeGndCap(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->capacitance();
}

const Pin *
ConcreteParasitics::pin(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->pin();
}

const Net *
ConcreteParasitics::net(const ParasiticNode *node,
                        const Network *network) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->net(network);
}

unsigned
ConcreteParasitics::netId(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->id();
}

bool
ConcreteParasitics::isExternal(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->isExternal();
}

////////////////////////////////////////////////////////////////

size_t
ConcreteParasitics::id(const ParasiticResistor *resistor) const
{
  const ConcreteParasiticResistor *cresistor =
    static_cast<const ConcreteParasiticResistor*>(resistor);
  return cresistor->id();
}

float
ConcreteParasitics::value(const ParasiticResistor *resistor) const
{
  const ConcreteParasiticResistor *cresistor =
    static_cast<const ConcreteParasiticResistor*>(resistor);
  return cresistor->value();
}

ParasiticNode *
ConcreteParasitics::node1(const ParasiticResistor *resistor) const
{
  const ConcreteParasiticResistor *cresistor =
    static_cast<const ConcreteParasiticResistor*>(resistor);
  return cresistor->node1();
}

ParasiticNode *
ConcreteParasitics::node2(const ParasiticResistor *resistor) const
{
  const ConcreteParasiticResistor *cresistor =
    static_cast<const ConcreteParasiticResistor*>(resistor);
  return cresistor->node2();
}

////////////////////////////////////////////////////////////////

size_t
ConcreteParasitics::id(const ParasiticCapacitor *capacitor) const
{
  const ConcreteParasiticCapacitor *ccapacitor =
    static_cast<const ConcreteParasiticCapacitor*>(capacitor);
  return ccapacitor->id();
}

float
ConcreteParasitics::value(const ParasiticCapacitor *capacitor) const
{
  const ConcreteParasiticCapacitor *ccapacitor =
    static_cast<const ConcreteParasiticCapacitor*>(capacitor);
  return ccapacitor->value();
}

ParasiticNode *
ConcreteParasitics::node1(const ParasiticCapacitor *capacitor) const
{
  const ConcreteParasiticCapacitor *ccapacitor =
    static_cast<const ConcreteParasiticCapacitor*>(capacitor);
  return ccapacitor->node1();
}

ParasiticNode *
ConcreteParasitics::node2(const ParasiticCapacitor *capacitor) const
{
  const ConcreteParasiticCapacitor *ccapacitor =
    static_cast<const ConcreteParasiticCapacitor*>(capacitor);
  return ccapacitor->node2();
}

////////////////////////////////////////////////////////////////

PinSet
ConcreteParasitics::unannotatedLoads(const Parasitic *parasitic,
                                     const Pin *drvr_pin) const
{
  const ConcreteParasitic *cparasitic = static_cast<const ConcreteParasitic*>(parasitic);
  return cparasitic->unannotatedLoads(drvr_pin, this);
}

} // namespace
