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

#include <limits>
#include <algorithm> // max
#include "Machine.hh"
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
#include "ReduceParasitics.hh"
#include "MakeConcreteParasitics.hh"
#include "ConcreteParasitics.hh"
#include "Parasitics.hh"
#include "Corner.hh"
#include "ConcreteParasiticsPvt.hh"

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

ParasiticDeviceIterator *
ConcreteParasitic::deviceIterator()
{
  return nullptr;
}

ParasiticNodeIterator *
ConcreteParasitic::nodeIterator()
{
  return nullptr;
}

////////////////////////////////////////////////////////////////

ConcreteElmore::ConcreteElmore() :
  loads_(nullptr)
{
}

ConcreteElmore::~ConcreteElmore()
{
  delete loads_;
}

void
ConcreteElmore::findElmore(const Pin *load_pin,
			   float &elmore,
			   bool &exists) const
{
  if (loads_)
    loads_->findKey(load_pin, elmore, exists);
  else
    exists = false;
}

void
ConcreteElmore::deleteLoad(const Pin *load_pin)
{
  loads_->erase(load_pin);
}

void
ConcreteElmore::setElmore(const Pin *load_pin,
			  float elmore)
{
  if (loads_ == nullptr)
    loads_ = new ConcreteElmoreLoadMap;
  (*loads_)[load_pin] = elmore;
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
  ConcretePi(c2, rpi, c1),
  ConcreteElmore()
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
  ConcreteElmore::findElmore(load_pin, elmore, exists);
}

void
ConcretePiElmore::setElmore(const Pin *load_pin,
			    float elmore)
{
  ConcreteElmore::setElmore(load_pin, elmore);
}

////////////////////////////////////////////////////////////////

ConcretePiElmoreEstimated::ConcretePiElmoreEstimated(float c2,
						     float rpi,
						     float c1,
						     float elmore_res,
						     float elmore_cap,
						     bool elmore_use_load_cap,
						     const RiseFall *rf,
						     const OperatingConditions *op,
						     const Corner *corner,
						     const MinMax *min_max,
						     Sdc *sdc):
  ConcretePi(c2, rpi, c1),
  elmore_res_(elmore_res),
  elmore_cap_(elmore_cap),
  elmore_use_load_cap_(elmore_use_load_cap),
  rf_(rf),
  op_cond_(op),
  corner_(corner),
  min_max_(min_max),
  sdc_(sdc)
{
}

float
ConcretePiElmoreEstimated::capacitance() const
{
  return ConcretePi::capacitance();
}

void
ConcretePiElmoreEstimated::piModel(float &c2,
				   float &rpi,
				   float &c1) const
{
  ConcretePi::piModel(c2, rpi, c1);
}

void
ConcretePiElmoreEstimated::findElmore(const Pin *load_pin,
				      float &elmore,
				      bool &exists) const
{
  float load_cap = 0.0;
  if (elmore_use_load_cap_)
    load_cap = sdc_->pinCapacitance(load_pin, rf_, op_cond_,
				    corner_, min_max_);
  elmore = elmore_res_ * (elmore_cap_ + load_cap);
  exists = true;
}

void
ConcretePiElmoreEstimated::setElmore(const Pin *,
				     float)
{
  // Cannot set elmore on estimated parasitic.
}

////////////////////////////////////////////////////////////////

ConcretePoleResidue::
ConcretePoleResidue(ComplexFloatSeq *poles,
		    ComplexFloatSeq *residues) :
  poles_(poles),
  residues_(residues)
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

////////////////////////////////////////////////////////////////

ConcretePiPoleResidue::ConcretePiPoleResidue(float c2,
					     float rpi,
					     float c1) :
  ConcretePi(c2, rpi, c1),
  load_pole_residue_(nullptr)
{
}

ConcretePiPoleResidue::~ConcretePiPoleResidue()
{
  if (load_pole_residue_) {
    load_pole_residue_->deleteContents();
    delete load_pole_residue_;
  }
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
  if (load_pole_residue_)
    return load_pole_residue_->findKey(load_pin);
  else
    return nullptr;
}

void
ConcretePiPoleResidue::setPoleResidue(const Pin *load_pin,
				      ComplexFloatSeq *poles,
				      ComplexFloatSeq *residues)
{
  if (load_pole_residue_ == nullptr)
    load_pole_residue_ = new ConcretePoleResidueMap;
  ConcretePoleResidue *pole_residue = load_pole_residue_->findKey(load_pin);
  if (pole_residue == nullptr) {
    pole_residue = new ConcretePoleResidue(poles, residues);
    (*load_pole_residue_)[load_pin] = pole_residue;
  }
  else
    pole_residue->setPoleResidue(poles, residues);
}

void
ConcretePiPoleResidue::deleteLoad(const Pin *load_pin)
{
  ConcretePoleResidue *pole_residue = load_pole_residue_->findKey(load_pin);
  if (pole_residue) {
    load_pole_residue_->erase(load_pin);
    delete pole_residue;
  }
}

////////////////////////////////////////////////////////////////

ConcreteParasiticNode::ConcreteParasiticNode() :
  cap_(0.0)
{
}

void
ConcreteParasiticNode::incrCapacitance(float cap)
{
  cap_ += cap;
}

float
ConcreteParasiticNode::capacitance() const
{
  return cap_;
}

void
ConcreteParasiticNode::addDevice(ConcreteParasiticDevice *device)
{
  devices_.push_back(device);
}

////////////////////////////////////////////////////////////////

ConcreteParasiticSubNode::ConcreteParasiticSubNode(const Net *net,
						   int id) :
  ConcreteParasiticNode(),
  net_(net),
  id_(id)
{
}

const char *
ConcreteParasiticSubNode::name(const Network *network) const
{
  const char *net_name = network->pathName(net_);
  return stringPrintTmp("%s:%d", net_name, id_);
}

////////////////////////////////////////////////////////////////

ConcreteParasiticPinNode::ConcreteParasiticPinNode(const Pin *pin) :
  ConcreteParasiticNode(),
  pin_(pin)
{
}

const char *
ConcreteParasiticPinNode::name(const Network *network) const
{
  return network->pathName(pin_);
}

////////////////////////////////////////////////////////////////

ConcreteParasiticDevice::ConcreteParasiticDevice(const char *name,
						 ConcreteParasiticNode *node,
						 float value) :
  name_(name),
  node_(node),
  value_(value)
{
}

ConcreteParasiticDevice::~ConcreteParasiticDevice()
{
  if (name_)
    stringDelete(name_);
}

ConcreteParasiticResistor::
ConcreteParasiticResistor(const char *name,
			  ConcreteParasiticNode *node,
			  ConcreteParasiticNode *other_node,
			  float res) :
  ConcreteParasiticDevice(name, node, res),
  other_node_(other_node)
{
}

ParasiticNode *
ConcreteParasiticResistor::otherNode(ParasiticNode *node) const
{
  if (node == node_)
    return other_node_;
  else if (node == other_node_)
    return node_;
  else
    return nullptr;
}

void
ConcreteParasiticResistor::replaceNode(ConcreteParasiticNode *from_node,
				       ConcreteParasiticNode *to_node)
{
  if (from_node == node_)
    node_ = to_node;
  else
    other_node_ = to_node;
}

////////////////////////////////////////////////////////////////

ConcreteCouplingCap::ConcreteCouplingCap(const char *name,
					 ConcreteParasiticNode *node,
					 float cap) :
  ConcreteParasiticDevice(name, node, cap)
{
}

void
ConcreteCouplingCap::replaceNode(ConcreteParasiticNode *from_node,
				 ConcreteParasiticNode *to_node)
{
  if (from_node == node_)
    node_ = to_node;
}

////////////////////////////////////////////////////////////////

ConcreteCouplingCapInt::
ConcreteCouplingCapInt(const char *name,
		       ConcreteParasiticNode *node,
		       ConcreteParasiticNode *other_node,
		       float cap) :
  ConcreteCouplingCap(name, node, cap),
  other_node_(other_node)
{
}

ParasiticNode *
ConcreteCouplingCapInt::otherNode(ParasiticNode *node) const
{
  if (node == node_)
    return other_node_;
  else if (node == other_node_)
    return node_;
  else
    return nullptr;
}

void
ConcreteCouplingCapInt::replaceNode(ConcreteParasiticNode *from_node,
				    ConcreteParasiticNode *to_node)
{
  if (from_node == node_)
    node_ = to_node;
  else
    other_node_ = to_node;
}

////////////////////////////////////////////////////////////////

ConcreteCouplingCapExtNode::
ConcreteCouplingCapExtNode(const char *name,
			   ConcreteParasiticNode *node,
			   Net *,
			   int,
			   float cap) :
  ConcreteCouplingCap(name, node, cap)
{
}

ParasiticNode *
ConcreteCouplingCapExtNode::otherNode(ParasiticNode *) const
{
  return nullptr;
}

void
ConcreteCouplingCapExtNode::replaceNode(ConcreteParasiticNode *from_node,
					ConcreteParasiticNode *to_node)
{
  if (from_node == node_)
    node_ = to_node;
}

////////////////////////////////////////////////////////////////

ConcreteCouplingCapExtPin::
ConcreteCouplingCapExtPin(const char *name,
			  ConcreteParasiticNode *node,
			  Pin *,
			  float cap) :
  ConcreteCouplingCap(name, node, cap)
{
}

ParasiticNode *
ConcreteCouplingCapExtPin::otherNode(ParasiticNode *) const
{
  return nullptr;
}

void
ConcreteCouplingCapExtPin::replaceNode(ConcreteParasiticNode *from_node,
				       ConcreteParasiticNode *to_node)
{
  if (from_node == node_)
    node_ = to_node;
}

////////////////////////////////////////////////////////////////

ConcreteParasiticNetwork::ConcreteParasiticNetwork(bool includes_pin_caps) :
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
  ConcreteParasiticSubNodeMap::Iterator node_iter1(sub_nodes_);
  while (node_iter1.hasNext()) {
    NetId *net_id;
    ConcreteParasiticSubNode *node;
    node_iter1.next(net_id, node);
    delete net_id;
    delete node;
  }
  pin_nodes_.deleteContents();
}

void
ConcreteParasiticNetwork::deleteDevices()
{
  ConcreteParasiticDeviceSet devices1;
  devices(&devices1);
  devices1.deleteContents();
}

ParasiticNodeIterator *
ConcreteParasiticNetwork::nodeIterator()
{
  ConcreteParasiticNodeSeq *nodes = new ConcreteParasiticNodeSeq();
  ConcreteParasiticPinNodeMap::Iterator node_iter2(pin_nodes_);
  while (node_iter2.hasNext()) {
    ConcreteParasiticPinNode *node = node_iter2.next();
    nodes->push_back(node);
  }
  ConcreteParasiticSubNodeMap::Iterator node_iter1(sub_nodes_);
  while (node_iter1.hasNext()) {
    ConcreteParasiticSubNode *node = node_iter1.next();
    nodes->push_back(node);
  }
  return new ConcreteParasiticNodeSeqIterator(nodes);
}

ParasiticDeviceIterator *
ConcreteParasiticNetwork::deviceIterator()
{
  ConcreteParasiticDeviceSet *devices1 = new ConcreteParasiticDeviceSet();
  devices(devices1);
  return new ConcreteParasiticDeviceSetIterator(devices1);
}

void
ConcreteParasiticNetwork::devices(ConcreteParasiticDeviceSet *devices)
{
  // Collect devices into a set so they are only deleted once
  // because multiple sub-nodes or pin nodes can refer to them.
  ConcreteParasiticSubNodeMap::Iterator node_iter1(sub_nodes_);
  while (node_iter1.hasNext()) {
    ConcreteParasiticSubNode *node = node_iter1.next();
    ConcreteParasiticDeviceSeq::Iterator device_iter(node->devices());
    while (device_iter.hasNext()) {
      ConcreteParasiticDevice *device = device_iter.next();
      devices->insert(device);
    }
  }

  ConcreteParasiticPinNodeMap::Iterator node_iter2(pin_nodes_);
  while (node_iter2.hasNext()) {
    ConcreteParasiticPinNode *node = node_iter2.next();
    ConcreteParasiticDeviceSeq::Iterator device_iter(node->devices());
    while (device_iter.hasNext()) {
      ConcreteParasiticDevice *device = device_iter.next();
      devices->insert(device);
    }
  }
}

float
ConcreteParasiticNetwork::capacitance() const
{
  float cap = 0.0;
  ConcreteParasiticSubNodeMap::ConstIterator sub_node_iter(sub_nodes_);
  while (sub_node_iter.hasNext()) {
    ConcreteParasiticSubNode *node = sub_node_iter.next();
    cap += node->capacitance();
  }

  ConcreteParasiticPinNodeMap::ConstIterator pin_node_iter(pin_nodes_);
  while (pin_node_iter.hasNext()) {
    ConcreteParasiticPinNode *node = pin_node_iter.next();
    cap += node->capacitance();
  }
  return cap;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::ensureParasiticNode(const Net *net,
					      int id)
{
  NetId net_id(net, id);
  ConcreteParasiticSubNode *node = sub_nodes_.findKey(&net_id);
  if (node == nullptr) {
    node = new ConcreteParasiticSubNode(net, id);
    sub_nodes_[new NetId(net, id)] = node;
    max_node_id_ = max((int) max_node_id_, id);
  }
  return node;
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::findNode(const Pin *pin)
{
  return pin_nodes_.findKey(pin);
}

void
ConcreteParasiticNetwork::disconnectPin(const Pin *pin,
					Net *net)
{
  ConcreteParasiticNode *node = pin_nodes_.findKey(pin);
  if (node) {
    // Make a subnode to replace the pin node.
    ConcreteParasiticNode *subnode = ensureParasiticNode(net,max_node_id_+1);
    // Hand over the devices.
    ConcreteParasiticDeviceSeq::Iterator device_iter(node->devices());
    while (device_iter.hasNext()) {
      ConcreteParasiticDevice *device = device_iter.next();
      subnode->addDevice(device);
      device->replaceNode(node, subnode);
    }

    pin_nodes_.erase(pin);
    delete node;
  }
}

ConcreteParasiticNode *
ConcreteParasiticNetwork::ensureParasiticNode(const Pin *pin)
{
  ConcreteParasiticPinNode *node = pin_nodes_.findKey(pin);
  if (node == nullptr) {
    node = new ConcreteParasiticPinNode(pin);
    pin_nodes_[pin] = node;
  }
  return node;
}

bool
NetIdLess::operator()(const NetId *net_id1,
		      const NetId *net_id2) const
{
  const Net *net1 = net_id1->first;
  const Net *net2 = net_id2->first;
  int id1 = net_id1->second;
  int id2 = net_id2->second;
  return net1 < net2
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
  for (auto drvr_parasitics : drvr_parasitic_map_) {
    ConcreteParasitic **parasitics = drvr_parasitics.second;
    if (parasitics) {
      for (int i = 0; i < ap_rf_count; i++)
	delete parasitics[i];
      delete [] parasitics;
    }
  }
  drvr_parasitic_map_.clear();

  for (auto net_parasitics : parasitic_network_map_) {
    ConcreteParasiticNetwork **parasitics = net_parasitics.second;
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
    for (auto tr : RiseFall::range()) {
      int ap_rf_index = parasiticAnalysisPtIndex(ap, tr);
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

void
ConcreteParasitics::deleteUnsavedParasitic(Parasitic *parasitic)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  delete cparasitic;
}

void
ConcreteParasitics::save()
{
  // No database to save to.
}

float
ConcreteParasitics::capacitance(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic->capacitance();
}

bool
ConcreteParasitics::isReducedParasiticNetwork(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
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
ConcreteParasitics::disconnectPinBefore(const Pin *pin)
{
  if (haveParasitics()) {
    deleteReducedParasitics(pin);

    Net *net = findParasiticNet(pin);
    if (net) {
      ConcreteParasiticNetwork **parasitics = parasitic_network_map_[net];
      if (parasitics) {
        int ap_count = corners_->parasiticAnalysisPtCount();
	for (int i = 0; i < ap_count; i++) {
	  ConcreteParasiticNetwork *parasitic = parasitics[i];
	  if (parasitic)
	    parasitic->disconnectPin(pin, net);
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

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPiElmore(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiElmore();
}

Parasitic *
ConcreteParasitics::findPiElmore(const Pin *drvr_pin,
				 const RiseFall *rf,
				 const ParasiticAnalysisPt *ap) const
{
  if (!drvr_parasitic_map_.empty()) {
    int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
    UniqueLock lock(lock_);
    ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
    if (parasitics) {
      ConcreteParasitic *parasitic = parasitics[ap_rf_index];
      if (parasitic == nullptr && rf == RiseFall::fall()) {
	ap_rf_index = parasiticAnalysisPtIndex(ap, RiseFall::rise());
	parasitic = parasitics[ap_rf_index];
      }
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
  UniqueLock lock(lock_);
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
ConcreteParasitics::isPiModel(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiModel();
}

void
ConcreteParasitics::piModel(Parasitic *parasitic,
			    float &c2,
			    float &rpi,
			    float &c1) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
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
ConcreteParasitics::findElmore(Parasitic *parasitic,
			       const Pin *load_pin,
			       float &elmore,
			       bool &exists) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
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
ConcreteParasitics::isPiPoleResidue(Parasitic* parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiPoleResidue();
}

Parasitic *
ConcreteParasitics::findPiPoleResidue(const Pin *drvr_pin,
				      const RiseFall *rf,
				      const ParasiticAnalysisPt *ap) const
{
  if (!drvr_parasitic_map_.empty()) {
    int ap_rf_index = parasiticAnalysisPtIndex(ap, rf);
    UniqueLock lock(lock_);
    ConcreteParasitic **parasitics = drvr_parasitic_map_.findKey(drvr_pin);
    if (parasitics) {
      ConcreteParasitic *parasitic = parasitics[ap_rf_index];
      if (parasitic == nullptr && rf == RiseFall::fall()) {
	ap_rf_index = parasiticAnalysisPtIndex(ap, RiseFall::rise());
	parasitic = parasitics[ap_rf_index];
      }
      if (parasitic->isPiPoleResidue())
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
  UniqueLock lock(lock_);
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
ConcreteParasitics::isParasiticNetwork(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isParasiticNetwork();
}

Parasitic *
ConcreteParasitics::findParasiticNetwork(const Net *net,
					 const ParasiticAnalysisPt *ap) const
{
  if (!parasitic_network_map_.empty()) {
    UniqueLock lock(lock_);
    if (!parasitic_network_map_.empty()) {
      ConcreteParasiticNetwork **parasitics=parasitic_network_map_.findKey(net);
      if (parasitics)
	return parasitics[ap->index()];
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::findParasiticNetwork(const Pin *pin,
					 const ParasiticAnalysisPt *ap) const
{
  if (!parasitic_network_map_.empty()) {
    UniqueLock lock(lock_);
    if (!parasitic_network_map_.empty()) {
      // Only call findParasiticNet if parasitics exist.
      Net *net = findParasiticNet(pin);
      ConcreteParasiticNetwork **parasitics=parasitic_network_map_.findKey(net);
      if (parasitics)
	return parasitics[ap->index()];
    }
  }
  return nullptr;
}

Parasitic *
ConcreteParasitics::makeParasiticNetwork(const Net *net,
					 bool includes_pin_caps,
					 const ParasiticAnalysisPt *ap)
{
  UniqueLock lock(lock_);
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
  if (parasitic)
    delete parasitic;
  parasitic = new ConcreteParasiticNetwork(includes_pin_caps);
  parasitics[ap_index] = parasitic;
  return parasitic;
}

void
ConcreteParasitics::deleteParasiticNetwork(const Net *net,
					   const ParasiticAnalysisPt *ap)
{
  if (!parasitic_network_map_.empty()) {
    UniqueLock lock(lock_);
    ConcreteParasiticNetwork **parasitics = parasitic_network_map_.findKey(net);
    if (parasitics) {
      int ap_index = ap->index();
      delete parasitics[ap_index];
      parasitics[ap_index] = nullptr;
    }
  }
}

bool
ConcreteParasitics::includesPinCaps(Parasitic *parasitic) const
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->includesPinCaps();
}

ParasiticNode *
ConcreteParasitics::ensureParasiticNode(Parasitic *parasitic,
					const Net *net,
					int id)
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->ensureParasiticNode(net, id);
}

ParasiticNode *
ConcreteParasitics::ensureParasiticNode(Parasitic *parasitic,
					const Pin *pin)
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->ensureParasiticNode(pin);
}

void
ConcreteParasitics::incrCap(ParasiticNode *node,
			    float cap,
			    const ParasiticAnalysisPt *)
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  cnode->incrCapacitance(cap);
}

void
ConcreteParasitics::makeCouplingCap(const char *name,
				    ParasiticNode *node,
				    ParasiticNode *other_node,
				    float cap,
				    const ParasiticAnalysisPt *)
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  ConcreteParasiticNode *other_cnode =
    static_cast<ConcreteParasiticNode*>(other_node);
  ConcreteCouplingCap *coupling_cap =
    new ConcreteCouplingCapInt(name, cnode, other_cnode, cap);
  cnode->addDevice(coupling_cap);
  other_cnode->addDevice(coupling_cap);
}

void
ConcreteParasitics::makeCouplingCap(const char *name,
				    ParasiticNode *node,
				    Net *other_node_net,
				    int other_node_id,
				    float cap,
				    const ParasiticAnalysisPt *)
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  ConcreteCouplingCap *coupling_cap =
    new ConcreteCouplingCapExtNode(name, cnode, other_node_net,
				   other_node_id, cap);
  cnode->addDevice(coupling_cap);
}

void
ConcreteParasitics::makeCouplingCap(const char *name,
				    ParasiticNode *node,
				    Pin *other_node_pin,
				    float cap,
				    const ParasiticAnalysisPt *)
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  ConcreteCouplingCap *coupling_cap =
    new ConcreteCouplingCapExtPin(name, cnode, other_node_pin, cap);
  cnode->addDevice(coupling_cap);
}

void
ConcreteParasitics::makeResistor(const char *name,
				 ParasiticNode *node1,
				 ParasiticNode *node2,
				 float res,
				 const ParasiticAnalysisPt *)
{
  ConcreteParasiticNode *cnode1 = static_cast<ConcreteParasiticNode*>(node1);
  ConcreteParasiticNode *cnode2 = static_cast<ConcreteParasiticNode*>(node2);
  ConcreteParasiticDevice *resistor =
    new ConcreteParasiticResistor(name, cnode1, cnode2, res);
  cnode1->addDevice(resistor);
  cnode2->addDevice(resistor);
}

ParasiticDeviceIterator *
ConcreteParasitics::deviceIterator(Parasitic *parasitic)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic->deviceIterator();
}

ParasiticNodeIterator *
ConcreteParasitics::nodeIterator(Parasitic *parasitic)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic->nodeIterator();
}

float
ConcreteParasitics::nodeGndCap(const ParasiticNode *node,
			       const ParasiticAnalysisPt *) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->capacitance();
}

const char *
ConcreteParasitics::name(const ParasiticNode *node)
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  return cnode->name(network_);
}

const Pin *
ConcreteParasitics::connectionPin(const ParasiticNode *node) const
{
  const ConcreteParasiticNode *cnode =
    static_cast<const ConcreteParasiticNode*>(node);
  if (cnode->isPinNode()) {
    const ConcreteParasiticPinNode *pin_node =
      dynamic_cast<const ConcreteParasiticPinNode*>(cnode);
    return pin_node->pin();
  }
  else
    return nullptr;
}

ParasiticNode *
ConcreteParasitics::findNode(Parasitic *parasitic,
			     const Pin *pin) const
{
  ConcreteParasiticNetwork *cparasitic =
    static_cast<ConcreteParasiticNetwork*>(parasitic);
  return cparasitic->findNode(pin);
}

ParasiticDeviceIterator *
ConcreteParasitics::deviceIterator(ParasiticNode *node) const
{
  ConcreteParasiticNode *cnode = static_cast<ConcreteParasiticNode*>(node);
  return new ConcreteParasiticDeviceSeqIterator(cnode->devices());
}

const char *
ConcreteParasitics::name(const ParasiticDevice *device) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->name();
}

bool
ConcreteParasitics::isResistor(const ParasiticDevice *device) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->isResistor();
}

bool
ConcreteParasitics::isCouplingCap(const ParasiticDevice *device) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->isCouplingCap();
}

float
ConcreteParasitics::value(const ParasiticDevice *device,
			  const ParasiticAnalysisPt *) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->value();
}

ParasiticNode *
ConcreteParasitics::node1(const ParasiticDevice *device) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->node1();
}

ParasiticNode *
ConcreteParasitics::node2(const ParasiticDevice *device) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->node2();
}

ParasiticNode *
ConcreteParasitics::otherNode(const ParasiticDevice *device,
			      ParasiticNode *node) const
{
  const ConcreteParasiticDevice *cdevice =
    static_cast<const ConcreteParasiticDevice*>(device);
  return cdevice->otherNode(node);
}

////////////////////////////////////////////////////////////////

void
ConcreteParasitics::reduceTo(Parasitic *parasitic,
			     const Net *net,
			     ReduceParasiticsTo reduce_to,
			     const OperatingConditions *op_cond,
			     const Corner *corner,
			     const MinMax *cnst_min_max,
			     const ParasiticAnalysisPt *ap)
{
  switch (reduce_to) {
  case ReduceParasiticsTo::pi_elmore:
    reduceToPiElmore(parasitic, net, op_cond, corner, cnst_min_max, ap);
    break;
  case ReduceParasiticsTo::pi_pole_residue2:
    reduceToPiPoleResidue2(parasitic, net, op_cond, corner,
			   cnst_min_max, ap);
    break;
  case ReduceParasiticsTo::none:
    break;
  }
}

void
ConcreteParasitics::reduceToPiElmore(Parasitic *parasitic,
				     const Net *net,
				     const OperatingConditions *op_cond,
				     const Corner *corner,
				     const MinMax *cnst_min_max,
				     const ParasiticAnalysisPt *ap)
{
  debugPrint1(debug_, "parasitic_reduce", 1, "Reduce net %s\n",
	      network_->pathName(net));
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)) {
      sta::reduceToPiElmore(parasitic, pin, ap->couplingCapFactor(),
			    op_cond, corner, cnst_min_max, ap, this);
    }
  }
  delete pin_iter;
}

void
ConcreteParasitics::reduceToPiElmore(Parasitic *parasitic,
				     const Pin *drvr_pin,
				     const OperatingConditions *op_cond,
				     const Corner *corner,
				     const MinMax *cnst_min_max,
				     const ParasiticAnalysisPt *ap)
{
  sta::reduceToPiElmore(parasitic, drvr_pin, ap->couplingCapFactor(),
			op_cond, corner, cnst_min_max, ap, this);
}

void
ConcreteParasitics::reduceToPiPoleResidue2(Parasitic *parasitic,
					   const Net *net,
					   const OperatingConditions *op_cond,
					   const Corner *corner,
					   const MinMax *cnst_min_max,
					   const ParasiticAnalysisPt *ap)
{
  debugPrint1(debug_, "parasitic_reduce", 1, "Reduce net %s\n",
	      network_->pathName(net));
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin))
      sta::reduceToPiPoleResidue2(parasitic, pin, ap->couplingCapFactor(),
				  op_cond, corner, cnst_min_max, ap, this);
  }
  delete pin_iter;
}

void
ConcreteParasitics::reduceToPiPoleResidue2(Parasitic *parasitic,
					   const Pin *drvr_pin,
					   const OperatingConditions *op_cond,
					   const Corner *corner,
					   const MinMax *cnst_min_max,
					   const ParasiticAnalysisPt *ap)
{
  sta::reduceToPiPoleResidue2(parasitic, drvr_pin,
			      ap->couplingCapFactor(),
			      op_cond, corner, cnst_min_max,
			      ap, this);
}

////////////////////////////////////////////////////////////////

Parasitic *
ConcreteParasitics::estimatePiElmore(const Pin *drvr_pin,
				     const RiseFall *rf,
				     const Wireload *wireload,
				     float fanout,
				     float net_pin_cap,
				     const OperatingConditions *op_cond,
				     const Corner *corner,
				     const MinMax *min_max,
				     const ParasiticAnalysisPt *)
{
  float c2, rpi, c1, elmore_res, elmore_cap;
  bool elmore_use_load_cap;
  estimatePiElmore(drvr_pin, rf, wireload, fanout, net_pin_cap,
		   op_cond, corner, min_max, this,
		   c2, rpi, c1,
		   elmore_res, elmore_cap, elmore_use_load_cap);

  return new ConcretePiElmoreEstimated(c2, rpi, c1, elmore_res, elmore_cap,
				       elmore_use_load_cap,
				       rf, op_cond, corner, min_max,
				       sdc_);
}

////////////////////////////////////////////////////////////////

ConcreteParasiticDeviceSeqIterator::
ConcreteParasiticDeviceSeqIterator(ConcreteParasiticDeviceSeq *devices) :
  iter_(devices)
{
}

ConcreteParasiticDeviceSetIterator::
ConcreteParasiticDeviceSetIterator(ConcreteParasiticDeviceSet *devices) :
  iter_(devices)
{
}

ConcreteParasiticDeviceSetIterator::~ConcreteParasiticDeviceSetIterator()
{
  delete iter_.container();
}

ConcreteParasiticNodeSeqIterator::
ConcreteParasiticNodeSeqIterator(ConcreteParasiticNodeSeq *nodes) :
  iter_(nodes)
{
}

ConcreteParasiticNodeSeqIterator::~ConcreteParasiticNodeSeqIterator()
{
  delete iter_.container();
}

} // namespace
