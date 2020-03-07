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
#include "MinMax.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "Parasitics.hh"
#include "ReduceParasitics.hh"

namespace sta {

typedef Map<ParasiticNode*, double> ParasiticNodeValueMap;
typedef Map<ParasiticDevice*, double> ParasiticDeviceValueMap;
typedef Set<ParasiticNode*> ParasiticNodeSet;
typedef Set<ParasiticDevice*> ParasiticDeviceSet;

class ReduceToPi : public StaState
{
public:
  ReduceToPi(StaState *sta);
  void reduceToPi(const Pin *drvr_pin,
		  ParasiticNode *drvr_node,
		  bool includes_pin_caps,
		  float coupling_cap_factor,
		  const RiseFall *rf,
		  const OperatingConditions *op_cond,
		  const Corner *corner,
		  const MinMax *cnst_min_max,
		  const ParasiticAnalysisPt *ap,
		  float &c2,
		  float &rpi,
		  float &c1);
  bool pinCapsOneValue() { return pin_caps_one_value_; }

protected:
  void reducePiDfs(const Pin *drvr_pin,
		   ParasiticNode *node,
		   ParasiticDevice *from_res,
		   const ParasiticAnalysisPt *ap,
		   double &y1,
		   double &y2,
		   double &y3,
		   double &dwn_cap);
  void visit(ParasiticNode *node);
  bool isVisited(ParasiticNode *node);
  void leave(ParasiticNode *node);
  void setDownstreamCap(ParasiticNode *node,
			float cap);
  float downstreamCap(ParasiticNode *node);
  float pinCapacitance(ParasiticNode *node);
  bool isLoopResistor(ParasiticDevice *device);
  void markLoopResistor(ParasiticDevice *device);

  bool includes_pin_caps_;
  float coupling_cap_multiplier_;
  const RiseFall *rf_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *cnst_min_max_;
  ParasiticNodeSet visited_nodes_;
  ParasiticNodeValueMap node_values_;
  ParasiticDeviceSet loop_resistors_;
  bool pin_caps_one_value_;
};

ReduceToPi::ReduceToPi(StaState *sta) :
  StaState(sta),
  coupling_cap_multiplier_(1.0),
  rf_(nullptr),
  op_cond_(nullptr),
  corner_(nullptr),
  cnst_min_max_(nullptr),
  pin_caps_one_value_(true)
{
}

// "Modeling the Driving-Point Characteristic of Resistive
// Interconnect for Accurate Delay Estimation", Peter O'Brien and
// Thomas Savarino, Proceedings of the 1989 Design Automation
// Conference.
void
ReduceToPi::reduceToPi(const Pin *drvr_pin,
		       ParasiticNode *drvr_node,
		       bool includes_pin_caps,
		       float coupling_cap_factor,
		       const RiseFall *rf,
		       const OperatingConditions *op_cond,
		       const Corner *corner,
		       const MinMax *cnst_min_max,
		       const ParasiticAnalysisPt *ap,
		       float &c2,
		       float &rpi,
		       float &c1)
{
  includes_pin_caps_ = includes_pin_caps;
  coupling_cap_multiplier_ = coupling_cap_factor;
  rf_ = rf;
  op_cond_ = op_cond;
  corner_ = corner;
  cnst_min_max_ = cnst_min_max;

  double y1, y2, y3, dcap;
  reducePiDfs(drvr_pin, drvr_node, 0, ap, y1, y2, y3, dcap);

  if (y2 == 0.0 && y3 == 0.0) {
    // Capacitive load.
    c1 = y1;
    c2 = 0.0;
    rpi = 0.0;
  }
  else {
    c1 = y2 * y2 / y3;
    c2 = y1 - y2 * y2 / y3;
    rpi = -y3 * y3 / (y2 * y2 * y2);
  }
  debugPrint3(debug_, "parasitic_reduce", 2,
	      " Pi model c2=%.3g rpi=%.3g c1=%.3g\n",
	      c2, rpi, c1);
}

// Find admittance moments.
void
ReduceToPi::reducePiDfs(const Pin *drvr_pin,
			ParasiticNode *node,
			ParasiticDevice *from_res,
			const ParasiticAnalysisPt *ap,
			double &y1,
			double &y2,
			double &y3,
			double &dwn_cap)
{
  double coupling_cap = 0.0;
  ParasiticDeviceIterator *device_iter1 = parasitics_->deviceIterator(node);
  while (device_iter1->hasNext()) {
    ParasiticDevice *device = device_iter1->next();
    if (parasitics_->isCouplingCap(device))
      coupling_cap += parasitics_->value(device, ap);
  }
  delete device_iter1;

  y1 = dwn_cap = parasitics_->nodeGndCap(node, ap)
    + coupling_cap * coupling_cap_multiplier_
    + pinCapacitance(node);
  y2 = y3 = 0.0;

  visit(node);
  ParasiticDeviceIterator *device_iter2 = parasitics_->deviceIterator(node);
  while (device_iter2->hasNext()) {
    ParasiticDevice *device = device_iter2->next();
    if (parasitics_->isResistor(device)
	&& !isLoopResistor(device)) {
      ParasiticNode *onode = parasitics_->otherNode(device, node);
      // Cadence Fire&Ice likes to create resistors with identical
      // from/to nodes.
      if (onode != node
	  && device != from_res) {
	if (isVisited(onode)) {
	  // Resistor loop.
	  debugPrint1(debug_, "parasitic_reduce", 2,
		      " loop detected thru resistor %s\n",
		      parasitics_->name(device));
	  markLoopResistor(device);
	}	
	else {
	  double yd1, yd2, yd3, dcap;
	  reducePiDfs(drvr_pin, onode, device, ap, yd1, yd2, yd3,dcap);
	  // Rule 3.  Upstream traversal of a series resistor.
	  // Rule 4.  Parallel admittances add.
	  double r = parasitics_->value(device, ap);
	  y1 += yd1;
	  y2 += yd2 - r * yd1 * yd1;
	  y3 += yd3 - 2 * r * yd1 * yd2 + r * r * yd1 * yd1 * yd1;
	  dwn_cap += dcap;
	}
      }
    }
  }
  delete device_iter2;

  setDownstreamCap(node, dwn_cap);
  leave(node);
  debugPrint5(debug_, "parasitic_reduce", 3,
	      " node %s y1=%.3g y2=%.3g y3=%.3g cap=%.3g\n",
	      parasitics_->name(node), y1, y2, y3, dwn_cap);
}

float
ReduceToPi::pinCapacitance(ParasiticNode *node)
{
  const Pin *pin = parasitics_->connectionPin(node);
  float pin_cap = 0.0;
  if (pin) {
    Port *port = network_->port(pin);
    LibertyPort *lib_port = network_->libertyPort(port);
    if (lib_port) {
      if (!includes_pin_caps_) {
	pin_cap = sdc_->pinCapacitance(pin, rf_, op_cond_, corner_,
				       cnst_min_max_);
	pin_caps_one_value_ &= lib_port->capacitanceIsOneValue();
      }
    }
    else if (network_->isTopLevelPort(pin))
      pin_cap = sdc_->portExtCap(port, rf_, cnst_min_max_);
  }
  return pin_cap;
}

void
ReduceToPi::visit(ParasiticNode *node)
{
  visited_nodes_.insert(node);
}

bool
ReduceToPi::isVisited(ParasiticNode *node)
{
  return visited_nodes_.hasKey(node);
}

void
ReduceToPi::leave(ParasiticNode *node)
{
  visited_nodes_.erase(node);
}

bool
ReduceToPi::isLoopResistor(ParasiticDevice *device)
{
  return loop_resistors_.hasKey(device);
}

void
ReduceToPi::markLoopResistor(ParasiticDevice *device)
{
  loop_resistors_.insert(device);
}

void
ReduceToPi::setDownstreamCap(ParasiticNode *node,
			     float cap)
{
  node_values_[node] = cap;
}

float
ReduceToPi::downstreamCap(ParasiticNode *node)
{
  return node_values_[node];
}

////////////////////////////////////////////////////////////////

class ReduceToPiElmore : public ReduceToPi
{
public:
  ReduceToPiElmore(StaState *sta);
  void makePiElmore(Parasitic *parasitic_network,
		    const Pin *drvr_pin,
		    ParasiticNode *drvr_node,
		    float coupling_cap_factor,
		    const RiseFall *rf,
		    const OperatingConditions *op_cond,
		    const Corner *corner,
		    const MinMax *cnst_min_max,
		    const ParasiticAnalysisPt *ap);
  void reduceElmoreDfs(const Pin *drvr_pin,
		       ParasiticNode *node,
		       ParasiticDevice *from_res,
		       double elmore,
		       Parasitic *pi_elmore,
		       const ParasiticAnalysisPt *ap);
};

void
reduceToPiElmore(Parasitic *parasitic_network,
		 const Pin *drvr_pin,
		 float coupling_cap_factor,
		 const OperatingConditions *op_cond,
		 const Corner *corner,
		 const MinMax *cnst_min_max,
		 const ParasiticAnalysisPt *ap,
		 StaState *sta)
{
  Parasitics *parasitics = sta->parasitics();
  ParasiticNode *drvr_node = parasitics->findNode(parasitic_network,
						  drvr_pin);
  if (drvr_node) {
    debugPrint1(sta->debug(), "parasitic_reduce", 1,
		"Reduce driver %s\n",
		sta->network()->pathName(drvr_pin));
    ReduceToPiElmore reducer(sta);
    reducer.makePiElmore(parasitic_network, drvr_pin, drvr_node,
			 coupling_cap_factor, RiseFall::rise(),
			 op_cond, corner, cnst_min_max, ap);
    if (!reducer.pinCapsOneValue())
      reducer.makePiElmore(parasitic_network, drvr_pin, drvr_node,
			   coupling_cap_factor, RiseFall::fall(),
			   op_cond, corner, cnst_min_max, ap);
  }
}

ReduceToPiElmore::ReduceToPiElmore(StaState *sta) :
  ReduceToPi(sta)
{
}

void
ReduceToPiElmore::makePiElmore(Parasitic *parasitic_network,
			       const Pin *drvr_pin,
			       ParasiticNode *drvr_node,
			       float coupling_cap_factor,
			       const RiseFall *rf,
			       const OperatingConditions *op_cond,
			       const Corner *corner,
			       const MinMax *cnst_min_max,
			       const ParasiticAnalysisPt *ap)
{
  float c2, rpi, c1;
  reduceToPi(drvr_pin, drvr_node,
	     parasitics_->includesPinCaps(parasitic_network),
	     coupling_cap_factor,
	     rf, op_cond, corner, cnst_min_max, ap,
	     c2, rpi, c1);
  Parasitic *pi_elmore = parasitics_->makePiElmore(drvr_pin, rf, ap,
						   c2, rpi, c1);
  parasitics_->setIsReducedParasiticNetwork(pi_elmore, true);
  reduceElmoreDfs(drvr_pin, drvr_node, 0, 0.0, pi_elmore, ap);
}

// Find elmore delays on 2nd DFS search using downstream capacitances
// set by reducePiDfs.
void
ReduceToPiElmore::reduceElmoreDfs(const Pin *drvr_pin,
				  ParasiticNode *node,
				  ParasiticDevice *from_res,
				  double elmore,
				  Parasitic *pi_elmore,
				  const ParasiticAnalysisPt *ap)
{
  const Pin *pin = parasitics_->connectionPin(node);
  if (from_res && pin) {
    if (network_->isLoad(pin)) {
      debugPrint2(debug_, "parasitic_reduce", 2,
		  " Load %s elmore=%.3g\n",
		  network_->pathName(pin),
		  elmore);
      parasitics_->setElmore(pi_elmore, pin, elmore);
    }
  }
  visit(node);
  ParasiticDeviceIterator *device_iter = parasitics_->deviceIterator(node);
  while (device_iter->hasNext()) {
    ParasiticDevice *device = device_iter->next();
    if (parasitics_->isResistor(device)) {
      ParasiticNode *onode = parasitics_->otherNode(device, node);
      if (device != from_res
	  && !isVisited(onode)
	  && !isLoopResistor(device)) {
	float r = parasitics_->value(device, ap);
	double onode_elmore = elmore + r * downstreamCap(onode);
	reduceElmoreDfs(drvr_pin, onode, device, onode_elmore,
			pi_elmore, ap);
      }
    }
  }
  delete device_iter;
  leave(node);
}

////////////////////////////////////////////////////////////////

class ReduceToPiPoleResidue2 : public ReduceToPi
{
public:
  ReduceToPiPoleResidue2(StaState *sta);
  ~ReduceToPiPoleResidue2();
  void findPolesResidues(Parasitic *parasitic_network,
			 Parasitic *pi_pole_residue,
			 const Pin *drvr_pin,
			 ParasiticNode *drvr_node,
			 const ParasiticAnalysisPt *ap);
  void makePiPoleResidue2(Parasitic *parasitic_network,
			  const Pin *drvr_pin,
			  ParasiticNode *drvr_node,
			  float coupling_cap_factor,
			  const RiseFall *rf,
			  const OperatingConditions *op_cond,
			  const Corner *corner,
			  const MinMax *cnst_min_max,
			  const ParasiticAnalysisPt *ap);

private:
  void findMoments(const Pin *drvr_pin,
		   ParasiticNode *drvr_node,
		   int moment_count,
		   const ParasiticAnalysisPt *ap);
  void findMoments(const Pin *drvr_pin,
		   ParasiticNode *node,
		   double from_volt,
		   ParasiticDevice *from_res,
		   int moment_index,
		   const ParasiticAnalysisPt *ap);
  double findBranchCurrents(const Pin *drvr_pin,
			    ParasiticNode *node,
			    ParasiticDevice *from_res,
			    int moment_index,
			    const ParasiticAnalysisPt *ap);
  double moment(ParasiticNode *node,
		int moment_index);
  void setMoment(ParasiticNode *node,
		 double moment,
		 int moment_index);
  double current(ParasiticDevice *res);
  void setCurrent(ParasiticDevice *res,
		  double i);
  void findPolesResidues(Parasitic *pi_pole_residue,
			 const Pin *drvr_pin,
			 const Pin *load_pin,
			 ParasiticNode *load_node);

  // Resistor/capacitor currents.
  ParasiticDeviceValueMap currents_;
  ParasiticNodeValueMap *moments_;
};

ReduceToPiPoleResidue2::ReduceToPiPoleResidue2(StaState *sta) :
  ReduceToPi(sta),
  moments_(nullptr)
{
}

// The interconnect moments are found using RICE.
// "RICE: Rapid Interconnect Circuit Evaluation Using AWE",
// Curtis Ratzlaff and Lawrence Pillage, IEEE Transactions on
// Computer-Aided Design of Integrated Circuits and Systems,
// Vol 13, No 6, June 1994, pg 763-776.
//
// The poles and residues are found using these algorithms.
// "An Explicit RC-Circuit Delay Approximation Based on the First
// Three Moments of the Impulse Response", Proceedings of the 33rd
// Design Automation Conference, 1996, pg 611-616.
void
reduceToPiPoleResidue2(Parasitic *parasitic_network,
		       const Pin *drvr_pin,
		       float coupling_cap_factor,
		       const OperatingConditions *op_cond,
		       const Corner *corner,
		       const MinMax *cnst_min_max,
		       const ParasiticAnalysisPt *ap,
		       StaState *sta)
{
  Parasitics *parasitics = sta->parasitics();
  ParasiticNode *drvr_node = parasitics->findNode(parasitic_network, drvr_pin);
  if (drvr_node) {
    debugPrint1(sta->debug(), "parasitic_reduce", 1,
		"Reduce driver %s\n",
		sta->network()->pathName(drvr_pin));
    ReduceToPiPoleResidue2 reducer(sta);
    reducer.makePiPoleResidue2(parasitic_network, drvr_pin, drvr_node,
			       coupling_cap_factor, RiseFall::rise(),
			       op_cond, corner, cnst_min_max, ap);
    if (!reducer.pinCapsOneValue())
      reducer.makePiPoleResidue2(parasitic_network, drvr_pin, drvr_node,
				 coupling_cap_factor, RiseFall::fall(),
				 op_cond, corner, cnst_min_max, ap);
  }
}

void
ReduceToPiPoleResidue2::makePiPoleResidue2(Parasitic *parasitic_network,
					   const Pin *drvr_pin,
					   ParasiticNode *drvr_node,
					   float coupling_cap_factor,
					   const RiseFall *rf,
					   const OperatingConditions *op_cond,
					   const Corner *corner,
					   const MinMax *cnst_min_max,
					   const ParasiticAnalysisPt *ap)
{
  float c2, rpi, c1;
  reduceToPi(drvr_pin, drvr_node,
	     parasitics_->includesPinCaps(parasitic_network),
	     coupling_cap_factor,
	     rf, op_cond, corner, cnst_min_max, ap,
	     c2, rpi, c1);
  Parasitic *pi_pole_residue = parasitics_->makePiPoleResidue(drvr_pin,
							      rf, ap,
							      c2, rpi, c1);
  parasitics_->setIsReducedParasiticNetwork(pi_pole_residue, true);
  findPolesResidues(parasitic_network, pi_pole_residue,
		    drvr_pin, drvr_node, ap);
}

ReduceToPiPoleResidue2::~ReduceToPiPoleResidue2()
{
  delete [] moments_;
}

void
ReduceToPiPoleResidue2::findPolesResidues(Parasitic *parasitic_network,
					  Parasitic *pi_pole_residue,
					  const Pin *drvr_pin,
					  ParasiticNode *drvr_node,
					  const ParasiticAnalysisPt *ap)
{
  moments_ = new ParasiticNodeValueMap[4];
  findMoments(drvr_pin, drvr_node, 4, ap);

  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->isLoad(pin)) {
      ParasiticNode *load_node = parasitics_->findNode(parasitic_network, pin);
      if (load_node) {
	findPolesResidues(pi_pole_residue, drvr_pin, pin, load_node);
      }
    }
  }
  delete pin_iter;
}

void
ReduceToPiPoleResidue2::findMoments(const Pin *drvr_pin,
				    ParasiticNode *drvr_node,
				    int moment_count,
				    const ParasiticAnalysisPt *ap)
{
  // Driver model thevenin resistance.
  double rd = 0.0;
  // Zero'th moments are all 1 because Vin(0)=1 and there is no
  // current thru the resistors.  Thus, there is no point in doing a
  // pass to find the zero'th moments.
  for (int moment_index = 1; moment_index < moment_count; moment_index++) {
    double rd_i = findBranchCurrents(drvr_pin, drvr_node, 0,
				    moment_index, ap);
    double rd_volt = rd_i * rd;
    setMoment(drvr_node, 0.0, moment_index);
    findMoments(drvr_pin, drvr_node, -rd_volt, 0, moment_index, ap);
  }
}

double
ReduceToPiPoleResidue2::findBranchCurrents(const Pin *drvr_pin,
					   ParasiticNode *node,
					   ParasiticDevice *from_res,
					   int moment_index,
					   const ParasiticAnalysisPt *ap)
{
  visit(node);
  double branch_i = 0.0;
  double coupling_cap = 0.0;
  ParasiticDeviceIterator *device_iter = parasitics_->deviceIterator(node);
  while (device_iter->hasNext()) {
    ParasiticDevice *device = device_iter->next();
    if (parasitics_->isResistor(device)) {
      ParasiticNode *onode = parasitics_->otherNode(device, node);
      // Cadence Fire&Ice likes to create resistors with identical
      // from/to nodes.
      if (onode != node
	  && device != from_res
	  && !isVisited(onode)
	  && !isLoopResistor(device)) {
	branch_i += findBranchCurrents(drvr_pin, onode, device,
				       moment_index, ap);
      }
    }
    else if (parasitics_->isCouplingCap(device))
      coupling_cap += parasitics_->value(device, ap);
  }
  delete device_iter;
  double cap = parasitics_->nodeGndCap(node, ap)
    + coupling_cap * coupling_cap_multiplier_
    + pinCapacitance(node);
  branch_i += cap * moment(node, moment_index - 1);
  leave(node);
  if (from_res) {
    setCurrent(from_res, branch_i);
    debugPrint1(debug_, "parasitic_reduce", 3,
		" res i=%.3g\n", branch_i);
  }
  return branch_i;
}

void
ReduceToPiPoleResidue2::findMoments(const Pin *drvr_pin,
				    ParasiticNode *node,
				    double from_volt,
				    ParasiticDevice *from_res,
				    int moment_index,
				    const ParasiticAnalysisPt *ap)
{
  visit(node);
  ParasiticDeviceIterator *device_iter = parasitics_->deviceIterator(node);
  while (device_iter->hasNext()) {
    ParasiticDevice *device = device_iter->next();
    if (parasitics_->isResistor(device)) {
      ParasiticNode *onode = parasitics_->otherNode(device, node);
      // Cadence Fire&Ice likes to create resistors with identical
      // from/to nodes.
      if (onode != node
	  && device != from_res
	  && !isVisited(onode)
	  && !isLoopResistor(device)) {
	double r = parasitics_->value(device, ap);
	double r_volt = r * current(device);
	double onode_volt = from_volt - r_volt;
	setMoment(onode, onode_volt, moment_index);
	debugPrint3(debug_, "parasitic_reduce", 3,
		    " moment %s %d %.3g\n",
		    parasitics_->name(onode),
		    moment_index,
		    onode_volt);
	findMoments(drvr_pin, onode, onode_volt, device, moment_index, ap);
      }
    }
  }
  delete device_iter;
  leave(node);
}

double
ReduceToPiPoleResidue2::moment(ParasiticNode *node,
			       int moment_index)
{
  // Zero'th moments are all 1.
  if (moment_index == 0)
    return 1.0;
  else {
    ParasiticNodeValueMap &map = moments_[moment_index];
    return map[node];
  }
}

void
ReduceToPiPoleResidue2::setMoment(ParasiticNode *node,
				  double moment,
				  int moment_index)
{
  // Zero'th moments are all 1.
  if (moment_index > 0) {
    ParasiticNodeValueMap &map = moments_[moment_index];
    map[node] = moment;
  }
}

double
ReduceToPiPoleResidue2::current(ParasiticDevice *res)
{
  return currents_[res];
}

void
ReduceToPiPoleResidue2::setCurrent(ParasiticDevice *res,
				   double i)
{
  currents_[res] = i;
}

void
ReduceToPiPoleResidue2::findPolesResidues(Parasitic *pi_pole_residue,
					  const Pin *,
					  const Pin *load_pin,
					  ParasiticNode *load_node)
{
  double m1 = moment(load_node, 1);
  double m2 = moment(load_node, 2);
  double m3 = moment(load_node, 3);
  double p1 = -m2 / m3;
  double p2 = p1 * (1.0 /  m1 - m1 / m2) / (m1 / m2 - m2 / m3);
  if (p1 <= 0.0
      || p2 <= 0.0
      // Coincident poles.  Not handled by delay calculator.
      || p1 == p2
      || m1 / m2 == m2 / m3) {
    double p1 = -1.0 / m1;
    double k1 = 1.0;
    debugPrint3(debug_, "parasitic_reduce", 3,
		" load %s p1=%.3g k1=%.3g\n",
		network_->pathName(load_pin), p1, k1);
    ComplexFloatSeq *poles = new ComplexFloatSeq(1);
    ComplexFloatSeq *residues = new ComplexFloatSeq(1);
    (*poles)[0] = ComplexFloat(p1, 0.0);
    (*residues)[0] = ComplexFloat(k1, 0.0);
    parasitics_->setPoleResidue(pi_pole_residue, load_pin, poles, residues);
  }
  else {
    double k1 = p1 * p1 * (1.0 + m1 * p2) / (p1 - p2);
    double k2 = -p2 * p2 * (1.0 + m1 * p1) / (p1 - p2);
    if (k1 < 0.0 && k2 > 0.0) {
      // Swap p1 and p2.
      double p = p2, k = k2;
      p2 = p1;
      k2 = k1;
      p1 = p;
      k1 = k;
    }
    debugPrint5(debug_, "parasitic_reduce", 3,
		" load %s p1=%.3g p2=%.3g k1=%.3g k2=%.3g\n",
		network_->pathName(load_pin), p1, p2, k1, k2);

    ComplexFloatSeq *poles = new ComplexFloatSeq(2);
    ComplexFloatSeq *residues = new ComplexFloatSeq(2);
    (*poles)[0] = ComplexFloat(p1, 0.0);
    (*residues)[0] = ComplexFloat(k1, 0.0);
    (*poles)[1] = ComplexFloat(p2, 0.0);
    (*residues)[1] = ComplexFloat(k2, 0.0);
    parasitics_->setPoleResidue(pi_pole_residue, load_pin, poles, residues);
  }
}

} // namespace
