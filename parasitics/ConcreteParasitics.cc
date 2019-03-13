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
ConcreteParasitic::isLumpedElmore() const
{
  return false;
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
  loads_->eraseKey(load_pin);
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

ConcreteLumpedElmore::ConcreteLumpedElmore(float cap) :
  ConcreteElmore(),
  cap_(cap)
{
}

void
ConcreteLumpedElmore::setCapacitance(float cap)
{
  cap_ = cap;
}

void
ConcreteLumpedElmore::findElmore(const Pin *load_pin,
				 float &elmore,
				 bool &exists) const
{
  ConcreteElmore::findElmore(load_pin, elmore, exists);
}

void
ConcreteLumpedElmore::setElmore(const Pin *load_pin,
				float elmore)
{
  ConcreteElmore::setElmore(load_pin, elmore);
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
						     const TransRiseFall *tr,
						     const OperatingConditions *op,
						     const Corner *corner,
						     const MinMax *min_max,
						     Sdc *sdc):
  ConcretePi(c2, rpi, c1),
  elmore_res_(elmore_res),
  elmore_cap_(elmore_cap),
  elmore_use_load_cap_(elmore_use_load_cap),
  tr_(tr),
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
    load_cap = sdc_->pinCapacitance(load_pin, tr_, op_cond_,
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
    load_pole_residue_->eraseKey(load_pin);
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

ConcreteParasiticSubNode::ConcreteParasiticSubNode(Net *net,
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
ConcreteParasiticNetwork::ensureParasiticNode(Net *net,
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

    pin_nodes_.eraseKey(pin);
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
  Net *net1 = net_id1->first;
  Net *net2 = net_id2->first;
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
  Parasitics(sta),
  lumped_elmore_maps_(nullptr),
  pi_elmore_maps_(nullptr),
  pi_pole_residue_maps_(nullptr),
  parasitic_network_maps_(nullptr)
{
}

ConcreteParasitics::~ConcreteParasitics()
{
  clear();
}

bool
ConcreteParasitics::haveParasitics()
{
  return lumped_elmore_maps_ != nullptr
    || pi_elmore_maps_ != nullptr
    || pi_pole_residue_maps_ != nullptr
    || parasitic_network_maps_ != nullptr;
}

void
ConcreteParasitics::clear()
{
  deleteParasitics();

  if (lumped_elmore_maps_) {
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	int ap_index = parasiticAnalysisPtIndex(ap, tr);
	delete (*lumped_elmore_maps_)[ap_index];
      }
    }
    delete lumped_elmore_maps_;
    lumped_elmore_maps_ = nullptr;
  }

  if (pi_elmore_maps_) {
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	int ap_index = parasiticAnalysisPtIndex(ap, tr);
	delete (*pi_elmore_maps_)[ap_index];
      }
    }
    delete pi_elmore_maps_;
    pi_elmore_maps_ = nullptr;
  }

  if (pi_pole_residue_maps_) {
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	int ap_index = parasiticAnalysisPtIndex(ap, tr);
	delete (*pi_pole_residue_maps_)[ap_index];
      }
    }
    delete pi_pole_residue_maps_;
    pi_pole_residue_maps_ = nullptr;
  }

  if (parasitic_network_maps_) {
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	int ap_index = parasiticAnalysisPtIndex(ap, tr);
	delete (*parasitic_network_maps_)[ap_index];
      }
    }
    delete parasitic_network_maps_;
    parasitic_network_maps_ = nullptr;
  }
}

int
ConcreteParasitics::parasiticAnalysisPtIndex(const ParasiticAnalysisPt *ap,
					     const TransRiseFall *tr) const
{
  return ap->index() * TransRiseFall::index_count + tr->index();
}

void
ConcreteParasitics::deleteParasitics()
{
  ParasiticAnalysisPtIterator ap_iter(corners_);
  while (ap_iter.hasNext()) {
    ParasiticAnalysisPt *ap = ap_iter.next();
    TransRiseFallIterator tr_iter;
    while (tr_iter.hasNext()) {
      TransRiseFall *tr = tr_iter.next();
      int ap_index = parasiticAnalysisPtIndex(ap, tr);
      deleteParasitics(ap_index);
    }
  }
}

void
ConcreteParasitics::deleteParasitics(int ap_index)
{
  if (lumped_elmore_maps_)
    (*lumped_elmore_maps_)[ap_index]->deleteContentsClear();
  if (pi_elmore_maps_)
    (*pi_elmore_maps_)[ap_index]->deleteContentsClear();
  if (pi_pole_residue_maps_)
    (*pi_pole_residue_maps_)[ap_index]->deleteContentsClear();
  if (parasitic_network_maps_)
    (*parasitic_network_maps_)[ap_index]->deleteContentsClear();
}

void
ConcreteParasitics::deleteParasitics(const Pin *drvr_pin,
				     const ParasiticAnalysisPt *ap)
{
  TransRiseFallIterator tr_iter;
  while (tr_iter.hasNext()) {
    TransRiseFall *tr = tr_iter.next();
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    if (lumped_elmore_maps_) {
      ConcreteLumpedElmore *lumped_elmore =
	(*lumped_elmore_maps_)[ap_index]->findKey(drvr_pin);
      if (lumped_elmore) {
	delete lumped_elmore;
	(*lumped_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      }
    }

    if (pi_elmore_maps_) {
      ConcretePiElmore *pi_elmore =
	(*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
      if (pi_elmore) {
	delete pi_elmore;
	(*pi_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      }
    }

    if (pi_pole_residue_maps_) {
      ConcretePiPoleResidue *pi_pole_residue =
	(*pi_pole_residue_maps_)[ap_index]->findKey(drvr_pin);
      if (pi_pole_residue) {
	(*pi_pole_residue_maps_)[ap_index]->eraseKey(drvr_pin);
	delete pi_pole_residue;
      }
    }
  }
}

void
ConcreteParasitics::deleteParasitics(const Net *net,
				     const ParasiticAnalysisPt *ap)
{
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->isDriver(pin))
      deleteParasitics(pin, ap);
  }
  delete pin_iter;

  if (parasitic_network_maps_) {
    TransRiseFallIterator tr_iter;
    while (tr_iter.hasNext()) {
      TransRiseFall *tr = tr_iter.next();
      int ap_index = parasiticAnalysisPtIndex(ap, tr);
      ConcreteParasiticNetwork *parasitic_network =
	(*parasitic_network_maps_)[ap_index]->findKey(net);
      if (parasitic_network) {
	(*parasitic_network_maps_)[ap_index]->eraseKey(net);
	delete parasitic_network;
      }
    }
  }
}

void
ConcreteParasitics::deleteParasitic(const Pin *drvr_pin,
				    const TransRiseFall *tr,
				    const ParasiticAnalysisPt *ap,
				    Parasitic *parasitic)
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  // Estimated parasitics are not recorded in a parasitic map and do
  // not require an analysis pt.
  if (ap) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    if (cparasitic->isLumpedElmore())
      (*lumped_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
    else if (cparasitic->isPiElmore() && pi_elmore_maps_)
      (*pi_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
    else if (cparasitic->isPiPoleResidue())
      (*pi_pole_residue_maps_)[ap_index]->eraseKey(drvr_pin);
    else if (cparasitic->isParasiticNetwork()) {
      const Net *net = network_->net(drvr_pin);
      (*parasitic_network_maps_)[ap_index]->eraseKey(net);
    }
  }
  delete cparasitic;
}

void
ConcreteParasitics::makeParasiticAnalysisPtAfter()
{
  // Make room in the parasitic maps.
  size_t map_size = mapSize();
  if (lumped_elmore_maps_)
    lumped_elmore_maps_->resize(map_size);
  if (pi_elmore_maps_)
    pi_elmore_maps_->resize(map_size);
  if (pi_pole_residue_maps_)
    pi_pole_residue_maps_->resize(map_size);
  if (parasitic_network_maps_)
    parasitic_network_maps_->resize(map_size);

  ParasiticAnalysisPtIterator ap_iter(corners_);
  while (ap_iter.hasNext()) {
    ParasiticAnalysisPt *ap = ap_iter.next();
    TransRiseFallIterator tr_iter;
    while (tr_iter.hasNext()) {
      TransRiseFall *tr = tr_iter.next();
      size_t ap_index = parasiticAnalysisPtIndex(ap, tr);
      if (lumped_elmore_maps_)
	(*lumped_elmore_maps_)[ap_index] = new ConcreteLumpedElmoreMap;
      if (pi_elmore_maps_)
	(*pi_elmore_maps_)[ap_index] = new ConcretePiElmoreMap;
      if (pi_pole_residue_maps_)
	(*pi_pole_residue_maps_)[ap_index] = new ConcretePiPoleResidueMap;
      if (parasitic_network_maps_)
	(*parasitic_network_maps_)[ap_index] = new ConcreteParasiticNetworkMap;
    }
  }
}

int
ConcreteParasitics::mapSize()
{
  return corners_->parasiticAnalysisPtCount() * TransRiseFall::index_count;
}

void
ConcreteParasitics::finish(Parasitic *)
{
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
    Net *net = findParasiticNet(pin);
    if (net) {
      if (lumped_elmore_maps_ || pi_elmore_maps_ || pi_pole_residue_maps_) {
	NetConnectedPinIterator *pin_iter=network_->connectedPinIterator(net);
	while (pin_iter->hasNext()) {
	  Pin *net_pin = pin_iter->next();
	  if (network_->isDriver(net_pin)) {
	    ParasiticAnalysisPtIterator ap_iter(corners_);
	    while (ap_iter.hasNext()) {
	      ParasiticAnalysisPt *ap = ap_iter.next();
	      TransRiseFallIterator tr_iter;
	      while (tr_iter.hasNext()) {
		TransRiseFall *tr = tr_iter.next();
		int ap_index = parasiticAnalysisPtIndex(ap, tr);
		disconnectPinBefore(net_pin, pin, ap_index);
	      }
	    }
	  }
	}
	delete pin_iter;
      }

      if (parasitic_network_maps_) {
	ParasiticAnalysisPtIterator ap_iter(corners_);
	while (ap_iter.hasNext()) {
	  ParasiticAnalysisPt *ap = ap_iter.next();
	  TransRiseFallIterator tr_iter;
	  while (tr_iter.hasNext()) {
	    TransRiseFall *tr = tr_iter.next();
	    int ap_index = parasiticAnalysisPtIndex(ap, tr);
	    ConcreteParasiticNetwork *parasitic_network = 
	      (*parasitic_network_maps_)[ap_index]->findKey(net);
	    if (parasitic_network)
	      parasitic_network->disconnectPin(pin, net);
	  }
	}
      }
    }
  }
}

void
ConcreteParasitics::disconnectPinBefore(const Pin *drvr_pin,
					const Pin *pin,
					int ap_index)
{
  if (lumped_elmore_maps_) {
    ConcreteLumpedElmore *lumped_elmore =
      (*lumped_elmore_maps_)[ap_index]->findKey(drvr_pin);
    if (lumped_elmore) {
      if (pin == drvr_pin) {
	delete lumped_elmore;
	(*lumped_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      }
      else
	lumped_elmore->deleteLoad(pin);
    }
  }

  if (pi_elmore_maps_) {
    ConcretePiElmore *pi_elmore =
      (*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
    if (pi_elmore) {
      if (pin == drvr_pin) {
	delete pi_elmore;
	(*pi_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      }
      else
	pi_elmore->deleteLoad(pin);
    }
  }

  if (pi_pole_residue_maps_) {
    ConcretePiPoleResidue *pi_pole_residue =
      (*pi_pole_residue_maps_)[ap_index]->findKey(drvr_pin);
    if (pi_pole_residue) {
      if (pin == drvr_pin) {
	delete pi_pole_residue;
	(*pi_pole_residue_maps_)[ap_index]->eraseKey(drvr_pin);
      }
      else
	pi_pole_residue->deleteLoad(pin);
    }
  }
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isLumpedElmore(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isLumpedElmore();
}

bool
ConcreteParasitics::hasLumpedElmore(const Pin *drvr_pin,
				    const TransRiseFall *tr,
				    const ParasiticAnalysisPt *ap) const
{
  if (lumped_elmore_maps_ && ap) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    bool exists = (*lumped_elmore_maps_)[ap_index]->hasKey(drvr_pin);
    return exists;
  }
  else
    return false;
}

Parasitic *
ConcreteParasitics::findLumpedElmore(const Pin *drvr_pin,
				     const TransRiseFall *tr,
				     const ParasiticAnalysisPt *ap) const
{
  if (ap && lumped_elmore_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    Parasitic *parasitic = (*lumped_elmore_maps_)[ap_index]->findKey(drvr_pin);
    return parasitic;
  }
  else
    return nullptr;
}

Parasitic *
ConcreteParasitics::makeLumpedElmore(const Pin *drvr_pin,
				     float cap,
				     const TransRiseFall *tr,
				     const ParasiticAnalysisPt *ap)
{
  int ap_index = parasiticAnalysisPtIndex(ap, tr);
  UniqueLock lock(lock_);
  if (lumped_elmore_maps_ == nullptr) {
    lumped_elmore_maps_ = new ConcreteLumpedElmoreMapSeq(mapSize());
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	size_t ap_index = parasiticAnalysisPtIndex(ap, tr);
	(*lumped_elmore_maps_)[ap_index] = new ConcreteLumpedElmoreMap;
      }
    }
  }

  ConcreteLumpedElmore *lumped_elmore =
    (*lumped_elmore_maps_)[ap_index]->findKey(drvr_pin);
  if (lumped_elmore)
    lumped_elmore->setCapacitance(cap);
  else {
    lumped_elmore = new ConcreteLumpedElmore(cap);
    (*(*lumped_elmore_maps_)[ap_index])[drvr_pin] = lumped_elmore;
  }
  return lumped_elmore;
}

void
ConcreteParasitics::deleteLumpedElmore(const Pin *drvr_pin,
				       const TransRiseFall *tr,
				       const ParasiticAnalysisPt *ap)
{
  if (ap && lumped_elmore_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    ConcreteLumpedElmore *lumped_elmore =
      (*lumped_elmore_maps_)[ap_index]->findKey(drvr_pin);
    if (lumped_elmore) {
      (*lumped_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      delete lumped_elmore;
    }
  }
}

////////////////////////////////////////////////////////////////

bool
ConcreteParasitics::isPiElmore(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isPiElmore();
}

bool
ConcreteParasitics::hasPiElmore(const Pin *drvr_pin,
				const TransRiseFall *tr,
				const ParasiticAnalysisPt *ap) const
{
  if (ap && pi_elmore_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    bool exists = (*pi_elmore_maps_)[ap_index]->hasKey(drvr_pin);
    if (!exists && tr == TransRiseFall::fall()) {
      ap_index = parasiticAnalysisPtIndex(ap, TransRiseFall::rise());
      exists = (*pi_elmore_maps_)[ap_index]->hasKey(drvr_pin);
    }
    return exists;
  }
  else
    return false;
}

Parasitic *
ConcreteParasitics::findPiElmore(const Pin *drvr_pin,
				 const TransRiseFall *tr,
				 const ParasiticAnalysisPt *ap) const
{
  if (ap && pi_elmore_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    Parasitic *parasitic = (*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
    if (parasitic == nullptr && tr == TransRiseFall::fall()) {
      ap_index = parasiticAnalysisPtIndex(ap, TransRiseFall::rise());
      parasitic = (*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
    }
    return parasitic;
  }
  else
    return nullptr;
}

Parasitic *
ConcreteParasitics::makePiElmore(const Pin *drvr_pin,
				 const TransRiseFall *tr,
				 const ParasiticAnalysisPt *ap,
				 float c2,
				 float rpi,
				 float c1)
{
  int ap_index = parasiticAnalysisPtIndex(ap, tr);
  UniqueLock lock(lock_);
  if (pi_elmore_maps_ == nullptr) {
    pi_elmore_maps_ = new ConcretePiElmoreMapSeq(mapSize());
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	size_t ap_index = parasiticAnalysisPtIndex(ap, tr);
	(*pi_elmore_maps_)[ap_index] = new ConcretePiElmoreMap;
      }
    }
  }

  ConcretePiElmore *pi_elmore = (*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
  if (pi_elmore)
    pi_elmore->setPiModel(c2, rpi, c1);
  else {
    pi_elmore = new ConcretePiElmore(c2, rpi, c1);
    (*(*pi_elmore_maps_)[ap_index])[drvr_pin] = pi_elmore;
  }
  return pi_elmore;
}

void
ConcreteParasitics::deletePiElmore(const Pin *drvr_pin,
				   const TransRiseFall *tr,
				   const ParasiticAnalysisPt *ap)
{
  if (ap && pi_elmore_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    ConcretePiElmore *pi_elmore =
      (*pi_elmore_maps_)[ap_index]->findKey(drvr_pin);
    if (pi_elmore) {
      (*pi_elmore_maps_)[ap_index]->eraseKey(drvr_pin);
      delete pi_elmore;
    }
  }
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

bool
ConcreteParasitics::hasPiPoleResidue(const Pin *drvr_pin,
				     const TransRiseFall *tr,
				     const ParasiticAnalysisPt *ap) const
{
  if (ap && pi_pole_residue_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    bool exists = (*pi_pole_residue_maps_)[ap_index]->hasKey(drvr_pin);
    if (!exists && tr == TransRiseFall::fall()) {
      ap_index = parasiticAnalysisPtIndex(ap, TransRiseFall::rise());
      exists = (*pi_pole_residue_maps_)[ap_index]->hasKey(drvr_pin);
    }
    return exists;
  }
  else
    return false;
}

Parasitic *
ConcreteParasitics::findPiPoleResidue(const Pin *drvr_pin,
				      const TransRiseFall *tr,
				      const ParasiticAnalysisPt *ap) const
{
  if (ap && pi_pole_residue_maps_) {
    int ap_index = parasiticAnalysisPtIndex(ap, tr);
    UniqueLock lock(lock_);
    Parasitic *parasitic = (*pi_pole_residue_maps_)[ap_index]->findKey(drvr_pin);
    if (parasitic == nullptr && tr == TransRiseFall::fall()) {
      ap_index = parasiticAnalysisPtIndex(ap, TransRiseFall::rise());
      parasitic = (*pi_pole_residue_maps_)[ap_index]->findKey(drvr_pin);
    }
    return parasitic;
  }
  else
    return nullptr;
}

Parasitic *
ConcreteParasitics::makePiPoleResidue(const Pin *drvr_pin,
				      const TransRiseFall *tr,
				      const ParasiticAnalysisPt *ap,
				      float c2,
				      float rpi,
				      float c1)
{
  int ap_index = parasiticAnalysisPtIndex(ap, tr);
  UniqueLock lock(lock_);
  if (pi_pole_residue_maps_ == nullptr) {
    pi_pole_residue_maps_ = new ConcretePiPoleResidueMapSeq(mapSize());
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	size_t ap_index = parasiticAnalysisPtIndex(ap, tr);
	(*pi_pole_residue_maps_)[ap_index] = new ConcretePiPoleResidueMap;
      }
    }
  }

  ConcretePiPoleResidue *pi_pole_residue =
    (*pi_pole_residue_maps_)[ap_index]->findKey(drvr_pin);
  if (pi_pole_residue)
    pi_pole_residue->setPiModel(c2, rpi, c1);
  else {
    pi_pole_residue = new ConcretePiPoleResidue(c2, rpi, c1);
    (*(*pi_pole_residue_maps_)[ap_index])[drvr_pin] = pi_pole_residue;
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

// Parasitic networks do not have separate rise/fall values.
int
ConcreteParasitics::parasiticNetworkAnalysisPtIndex(const ParasiticAnalysisPt *ap) const
{
  return parasiticAnalysisPtIndex(ap, TransRiseFall::rise());
}

bool
ConcreteParasitics::isParasiticNetwork(Parasitic *parasitic) const
{
  ConcreteParasitic *cparasitic = static_cast<ConcreteParasitic*>(parasitic);
  return cparasitic && cparasitic->isParasiticNetwork();
}

bool
ConcreteParasitics::hasParasiticNetwork(const Net *net,
					const ParasiticAnalysisPt *ap) const
{
  if (ap && parasitic_network_maps_) {
    int ap_index = parasiticNetworkAnalysisPtIndex(ap);
    UniqueLock lock(lock_);
    bool exists = (*parasitic_network_maps_)[ap_index]->hasKey(net);
    return exists;
  }
  else
    return false;
}

Parasitic *
ConcreteParasitics::findParasiticNetwork(const Pin *pin,
					 const ParasiticAnalysisPt *ap) const
{
  if (ap && parasitic_network_maps_) {
    int ap_index = parasiticNetworkAnalysisPtIndex(ap);
    ConcreteParasiticNetworkMap *parasitics = (*parasitic_network_maps_)[ap_index];
    UniqueLock lock(lock_);
    Parasitic *parasitic = nullptr;
    if (parasitics->size()) {
      // Only call findParasiticNet if parasitics exist.
      Net *net = findParasiticNet(pin);
      parasitic = parasitics->findKey(net);
    }
    return parasitic;
  }
  else
    return nullptr;
}

Parasitic *
ConcreteParasitics::makeParasiticNetwork(Net *net,
					 bool includes_pin_caps,
					 const ParasiticAnalysisPt *ap)
{
  int ap_index = parasiticNetworkAnalysisPtIndex(ap);
  UniqueLock lock(lock_);
  if (parasitic_network_maps_ == nullptr) {
    parasitic_network_maps_ = new ConcreteParasiticNetworkMapSeq(mapSize());
    ParasiticAnalysisPtIterator ap_iter(corners_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *tr = tr_iter.next();
	size_t ap_index = parasiticAnalysisPtIndex(ap, tr);
	(*parasitic_network_maps_)[ap_index] = new ConcreteParasiticNetworkMap;
      }
    }
  }

  ConcreteParasiticNetwork *parasitic =
    (*parasitic_network_maps_)[ap_index]->findKey(net);
  if (parasitic == nullptr) {
    parasitic = new ConcreteParasiticNetwork(includes_pin_caps);
    (*(*parasitic_network_maps_)[ap_index])[net] = parasitic;
  }
  return parasitic;
}

void
ConcreteParasitics::deleteParasiticNetwork(const Net *net,
					   const ParasiticAnalysisPt *ap)
{
  if (ap && parasitic_network_maps_) {
    int ap_index = parasiticNetworkAnalysisPtIndex(ap);
    UniqueLock lock(lock_);
    ConcreteParasiticNetwork *parasitic =
      (*parasitic_network_maps_)[ap_index]->findKey(net);
    if (parasitic) {
      (*parasitic_network_maps_)[ap_index]->eraseKey(net);
      delete parasitic;
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
					Net *net,
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
			     const TransRiseFall *tr,
			     const OperatingConditions *op_cond,
			     const Corner *corner,
			     const MinMax *cnst_min_max,
			     const ParasiticAnalysisPt *ap)
{
  switch (reduce_to) {
  case ReduceParasiticsTo::pi_elmore:
    reduceToPiElmore(parasitic, net, tr, op_cond, corner, cnst_min_max, ap);
    break;
  case ReduceParasiticsTo::pi_pole_residue2:
    reduceToPiPoleResidue2(parasitic, net, tr, op_cond, corner,
			   cnst_min_max, ap);
    break;
  case ReduceParasiticsTo::none:
    break;
  }
}

void
ConcreteParasitics::reduceToPiElmore(Parasitic *parasitic,
				     const Net *net,
				     const TransRiseFall *tr,
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
			    tr, op_cond, corner, cnst_min_max, ap, this);
    }
  }
  delete pin_iter;
}

Parasitic *
ConcreteParasitics::reduceToPiElmore(Parasitic *parasitic,
				     const Pin *drvr_pin,
				     const TransRiseFall *tr,
				     const OperatingConditions *op_cond,
				     const Corner *corner,
				     const MinMax *cnst_min_max,
				     const ParasiticAnalysisPt *ap)
{
  return sta::reduceToPiElmore(parasitic, drvr_pin, ap->couplingCapFactor(),
			       tr, op_cond, corner, cnst_min_max, ap, this);
}

void
ConcreteParasitics::reduceToPiPoleResidue2(Parasitic *parasitic,
					   const Net *net,
					   const TransRiseFall *tr,
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
				  tr, op_cond, corner, cnst_min_max, ap, this);
  }
  delete pin_iter;
}

Parasitic *
ConcreteParasitics::reduceToPiPoleResidue2(Parasitic *parasitic,
					   const Pin *drvr_pin,
					   const TransRiseFall *tr,
					   const OperatingConditions *op_cond,
					   const Corner *corner,
					   const MinMax *cnst_min_max,
					   const ParasiticAnalysisPt *ap)
{
  return sta::reduceToPiPoleResidue2(parasitic, drvr_pin,
				     ap->couplingCapFactor(),
				     tr, op_cond, corner, cnst_min_max,
				     ap, this);
}

////////////////////////////////////////////////////////////////

Parasitic *
ConcreteParasitics::estimatePiElmore(const Pin *drvr_pin,
				     const TransRiseFall *tr,
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
  estimatePiElmore(drvr_pin, tr, wireload, fanout, net_pin_cap,
		   op_cond, corner, min_max, this,
		   c2, rpi, c1,
		   elmore_res, elmore_cap, elmore_use_load_cap);

  return new ConcretePiElmoreEstimated(c2, rpi, c1, elmore_res, elmore_cap,
				       elmore_use_load_cap,
				       tr, op_cond, corner, min_max,
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
