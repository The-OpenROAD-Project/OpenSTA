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

#include "ReduceParasitics.hh"

#include "Error.hh"
#include "Debug.hh"
#include "MinMax.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "Parasitics.hh"

namespace sta {

using std::max;

typedef Map<ParasiticNode*, double> ParasiticNodeValueMap;
typedef Map<ParasiticResistor*, double> ResistorCurrentMap;
typedef Set<ParasiticResistor*> ParasiticResistorSet;
typedef Set<ParasiticNode*> ParasiticNodeSet;

class ReduceToPi : public StaState
{
public:
  ReduceToPi(StaState *sta);
  void reduceToPi(const Parasitic *parasitic_network,
                  const Pin *drvr_pin,
		  ParasiticNode *drvr_node,
		  float coupling_cap_factor,
		  const RiseFall *rf,
		  const Corner *corner,
		  const MinMax *min_max,
		  const ParasiticAnalysisPt *ap,
		  float &c2,
		  float &rpi,
		  float &c1);
  bool pinCapsOneValue() { return pin_caps_one_value_; }

protected:
  void reducePiDfs(const Pin *drvr_pin,
		   ParasiticNode *node,
		   ParasiticResistor *from_res,
                   double src_resistance,
		   double &y1,
		   double &y2,
		   double &y3,
		   double &dwn_cap,
                   double &max_resistance);
  void visit(ParasiticNode *node);
  bool isVisited(ParasiticNode *node);
  void leave(ParasiticNode *node);
  void setDownstreamCap(ParasiticNode *node,
			float cap);
  float downstreamCap(ParasiticNode *node);
  float pinCapacitance(ParasiticNode *node);
  bool isLoopResistor(ParasiticResistor *resistor);
  void markLoopResistor(ParasiticResistor *resistor);

  bool includes_pin_caps_;
  float coupling_cap_multiplier_;
  const RiseFall *rf_;
  const Corner *corner_;
  const MinMax *min_max_;
  const ParasiticAnalysisPt *ap_;
  ParasiticNodeResistorMap resistor_map_;
  ParasiticNodeCapacitorMap capacitor_map_;

  ParasiticNodeSet visited_nodes_;
  ParasiticNodeValueMap node_values_;
  ParasiticResistorSet loop_resistors_;
  bool pin_caps_one_value_;
};

ReduceToPi::ReduceToPi(StaState *sta) :
  StaState(sta),
  coupling_cap_multiplier_(1.0),
  rf_(nullptr),
  corner_(nullptr),
  min_max_(nullptr),
  pin_caps_one_value_(true)
{
}

// "Modeling the Driving-Point Characteristic of Resistive
// Interconnect for Accurate Delay Estimation", Peter O'Brien and
// Thomas Savarino, Proceedings of the 1989 Design Automation
// Conference.
void
ReduceToPi::reduceToPi(const Parasitic *parasitic_network,
                       const Pin *drvr_pin,
		       ParasiticNode *drvr_node,
		       float coupling_cap_factor,
		       const RiseFall *rf,
		       const Corner *corner,
		       const MinMax *min_max,
		       const ParasiticAnalysisPt *ap,
		       float &c2,
		       float &rpi,
		       float &c1)
{
  includes_pin_caps_ = parasitics_->includesPinCaps(parasitic_network),
  coupling_cap_multiplier_ = coupling_cap_factor;
  rf_ = rf;
  corner_ = corner;
  min_max_ = min_max;
  ap_ = ap;
  resistor_map_ = parasitics_->parasiticNodeResistorMap(parasitic_network);
  capacitor_map_ = parasitics_->parasiticNodeCapacitorMap(parasitic_network);

  double y1, y2, y3, dcap;
  double max_resistance = 0.0;
  reducePiDfs(drvr_pin, drvr_node, nullptr, 0.0,
              y1, y2, y3, dcap, max_resistance);

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
  debugPrint(debug_, "parasitic_reduce", 2,
             " Pi model c2=%.3g rpi=%.3g c1=%.3g max_r=%.3g",
             c2, rpi, c1, max_resistance);
}

// Find admittance moments.
void
ReduceToPi::reducePiDfs(const Pin *drvr_pin,
			ParasiticNode *node,
			ParasiticResistor *from_res,
			double src_resistance,
			double &y1,
			double &y2,
			double &y3,
			double &dwn_cap,
                        double &max_resistance)
{
  double coupling_cap = 0.0;
  ParasiticCapacitorSeq &capacitors = capacitor_map_[node];
  for (ParasiticCapacitor *capacitor : capacitors)
    coupling_cap += parasitics_->value(capacitor);

  dwn_cap = parasitics_->nodeGndCap(node)
    + coupling_cap * coupling_cap_multiplier_
    + pinCapacitance(node);
  y1 = dwn_cap;
  y2 = y3 = 0.0;
  max_resistance = max(max_resistance, src_resistance);

  visit(node);
  ParasiticResistorSeq &resistors = resistor_map_[node];
  for (ParasiticResistor *resistor : resistors) {
    if (!isLoopResistor(resistor)) {
      ParasiticNode *onode = parasitics_->otherNode(resistor, node);
      // One commercial extractor creates resistors with identical from/to nodes.
      if (onode != node
          && resistor != from_res) {
        if (isVisited(onode)) {
          // Resistor loop.
          debugPrint(debug_, "parasitic_reduce", 2, " loop detected thru resistor %zu",
                     parasitics_->id(resistor));
          markLoopResistor(resistor);
        }
        else {
          double r = parasitics_->value(resistor);
          double yd1, yd2, yd3, dcap;
          reducePiDfs(drvr_pin, onode, resistor, src_resistance + r,
                      yd1, yd2, yd3, dcap, max_resistance);
          // Rule 3.  Upstream traversal of a series resistor.
          // Rule 4.  Parallel admittances add.
          y1 += yd1;
          y2 += yd2 - r * yd1 * yd1;
          y3 += yd3 - 2 * r * yd1 * yd2 + r * r * yd1 * yd1 * yd1;
          dwn_cap += dcap;
        }
      }
    }
  }

  setDownstreamCap(node, dwn_cap);
  leave(node);
  debugPrint(debug_, "parasitic_reduce", 3,
             " node %s y1=%.3g y2=%.3g y3=%.3g cap=%.3g",
             parasitics_->name(node), y1, y2, y3, dwn_cap);
}

float
ReduceToPi::pinCapacitance(ParasiticNode *node)
{
  const Pin *pin = parasitics_->pin(node);
  float pin_cap = 0.0;
  if (pin) {
    Port *port = network_->port(pin);
    LibertyPort *lib_port = network_->libertyPort(port);
    if (lib_port) {
      if (!includes_pin_caps_) {
	pin_cap = sdc_->pinCapacitance(pin, rf_, corner_, min_max_);
	pin_caps_one_value_ &= lib_port->capacitanceIsOneValue();
      }
    }
    else if (network_->isTopLevelPort(pin))
      pin_cap = sdc_->portExtCap(port, rf_, corner_, min_max_);
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
ReduceToPi::isLoopResistor(ParasiticResistor *resistor)
{
  return loop_resistors_.hasKey(resistor);
}

void
ReduceToPi::markLoopResistor(ParasiticResistor *resistor)
{
  loop_resistors_.insert(resistor);
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
  Parasitic *makePiElmore(const Parasitic *parasitic_network,
                          const Pin *drvr_pin,
                          ParasiticNode *drvr_node,
                          float coupling_cap_factor,
                          const RiseFall *rf,
                          const Corner *corner,
                          const MinMax *min_max,
                          const ParasiticAnalysisPt *ap);
  void reduceElmoreDfs(const Pin *drvr_pin,
		       ParasiticNode *node,
		       ParasiticResistor *from_res,
		       double elmore,
		       Parasitic *pi_elmore);
};

Parasitic *
reduceToPiElmore(const Parasitic *parasitic_network,
		 const Pin *drvr_pin,
                 const RiseFall *rf,
                 float coupling_cap_factor,
		 const Corner *corner,
		 const MinMax *min_max,
		 const ParasiticAnalysisPt *ap,
		 StaState *sta)
{
  Parasitics *parasitics = sta->parasitics();
  ParasiticNode *drvr_node =
    parasitics->findParasiticNode(parasitic_network, drvr_pin);
  if (drvr_node) {
    debugPrint(sta->debug(), "parasitic_reduce", 1, "Reduce driver %s %s %s",
               sta->network()->pathName(drvr_pin),
               rf->asString(),
               min_max->asString());
    ReduceToPiElmore reducer(sta);
    return reducer.makePiElmore(parasitic_network, drvr_pin, drvr_node,
                                coupling_cap_factor, rf, corner,
                                min_max, ap);
  }
  return nullptr;
}

ReduceToPiElmore::ReduceToPiElmore(StaState *sta) :
  ReduceToPi(sta)
{
}

Parasitic *
ReduceToPiElmore::makePiElmore(const Parasitic *parasitic_network,
			       const Pin *drvr_pin,
			       ParasiticNode *drvr_node,
			       float coupling_cap_factor,
			       const RiseFall *rf,
			       const Corner *corner,
			       const MinMax *min_max,
			       const ParasiticAnalysisPt *ap)
{
  float c2, rpi, c1;
  reduceToPi(parasitic_network, drvr_pin, drvr_node, coupling_cap_factor,
             rf, corner, min_max, ap, c2, rpi, c1);
  Parasitic *pi_elmore = parasitics_->makePiElmore(drvr_pin, rf, ap,
						   c2, rpi, c1);
  parasitics_->setIsReducedParasiticNetwork(pi_elmore, true);
  reduceElmoreDfs(drvr_pin, drvr_node, 0, 0.0, pi_elmore);
  return pi_elmore;
}

// Find elmore delays on 2nd DFS search using downstream capacitances
// set by reducePiDfs.
void
ReduceToPiElmore::reduceElmoreDfs(const Pin *drvr_pin,
				  ParasiticNode *node,
				  ParasiticResistor *from_res,
				  double elmore,
				  Parasitic *pi_elmore)
{
  const Pin *pin = parasitics_->pin(node);
  if (from_res && pin) {
    if (network_->isLoad(pin)) {
      debugPrint(debug_, "parasitic_reduce", 2, " Load %s elmore=%.3g",
                 network_->pathName(pin),
                 elmore);
      parasitics_->setElmore(pi_elmore, pin, elmore);
    }
  }
  visit(node);
  ParasiticResistorSeq &resistors = resistor_map_[node];
  for (ParasiticResistor *resistor : resistors) {
    ParasiticNode *onode = parasitics_->otherNode(resistor, node);
    if (resistor != from_res
        && !isVisited(onode)
        && !isLoopResistor(resistor)) {
      float r = parasitics_->value(resistor);
      double onode_elmore = elmore + r * downstreamCap(onode);
      reduceElmoreDfs(drvr_pin, onode, resistor, onode_elmore, pi_elmore);
    }
  }
  leave(node);
}

////////////////////////////////////////////////////////////////

class ReduceToPiPoleResidue2 : public ReduceToPi
{
public:
  ReduceToPiPoleResidue2(StaState *sta);
  ~ReduceToPiPoleResidue2();
  void findPolesResidues(const Parasitic *parasitic_network,
                         Parasitic *pi_pole_residue,
			 const Pin *drvr_pin,
			 ParasiticNode *drvr_node);
  Parasitic *makePiPoleResidue2(const Parasitic *parasitic_network,
                                const Pin *drvr_pin,
                                ParasiticNode *drvr_node,
                                float coupling_cap_factor,
                                const RiseFall *rf,
                                const Corner *corner,
                                const MinMax *min_max,
                                const ParasiticAnalysisPt *ap);

private:
  void findMoments(const Pin *drvr_pin,
		   ParasiticNode *drvr_node,
		   int moment_count);
  void findMoments(const Pin *drvr_pin,
		   ParasiticNode *node,
		   double from_volt,
		   ParasiticResistor *from_res,
		   int moment_index);
  double findBranchCurrents(const Pin *drvr_pin,
			    ParasiticNode *node,
			    ParasiticResistor *from_res,
			    int moment_index);
  double moment(ParasiticNode *node,
		int moment_index);
  void setMoment(ParasiticNode *node,
		 double moment,
		 int moment_index);
  double current(ParasiticResistor *res);
  void setCurrent(ParasiticResistor *res,
		  double i);
  void findPolesResidues(Parasitic *pi_pole_residue,
			 const Pin *drvr_pin,
			 const Pin *load_pin,
			 ParasiticNode *load_node);

  // Resistor/capacitor currents.
  ResistorCurrentMap currents_;
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
Parasitic *
reduceToPiPoleResidue2(const Parasitic *parasitic_network,
		       const Pin *drvr_pin,
                       const RiseFall *rf,
		       float coupling_cap_factor,
		       const Corner *corner,
		       const MinMax *min_max,
		       const ParasiticAnalysisPt *ap,
		       StaState *sta)
{
  Parasitics *parasitics = sta->parasitics();
  ParasiticNode *drvr_node =
    parasitics->findParasiticNode(parasitic_network, drvr_pin);
  if (drvr_node) {
    debugPrint(sta->debug(), "parasitic_reduce", 1, "Reduce driver %s",
               sta->network()->pathName(drvr_pin));
    ReduceToPiPoleResidue2 reducer(sta);
    return reducer.makePiPoleResidue2(parasitic_network, drvr_pin, drvr_node,
                                      coupling_cap_factor, rf,
                                      corner, min_max, ap);
  }
  return nullptr;
}

Parasitic *
ReduceToPiPoleResidue2::makePiPoleResidue2(const Parasitic *parasitic_network,
					   const Pin *drvr_pin,
					   ParasiticNode *drvr_node,
					   float coupling_cap_factor,
					   const RiseFall *rf,
					   const Corner *corner,
					   const MinMax *min_max,
					   const ParasiticAnalysisPt *ap)
{
  float c2, rpi, c1;
  reduceToPi(parasitic_network, drvr_pin, drvr_node,
	     coupling_cap_factor, rf, corner, min_max, ap,
	     c2, rpi, c1);
  Parasitic *pi_pole_residue = parasitics_->makePiPoleResidue(drvr_pin,
							      rf, ap,
							      c2, rpi, c1);
  parasitics_->setIsReducedParasiticNetwork(pi_pole_residue, true);
  findPolesResidues(parasitic_network, pi_pole_residue, drvr_pin, drvr_node);
  return pi_pole_residue;
}

ReduceToPiPoleResidue2::~ReduceToPiPoleResidue2()
{
  delete [] moments_;
}

void
ReduceToPiPoleResidue2::findPolesResidues(const Parasitic *parasitic_network,
                                          Parasitic *pi_pole_residue,
					  const Pin *drvr_pin,
					  ParasiticNode *drvr_node)
{
  moments_ = new ParasiticNodeValueMap[4];
  findMoments(drvr_pin, drvr_node, 4);

  PinConnectedPinIterator *pin_iter = network_->connectedPinIterator(drvr_pin);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isLoad(pin)) {
      ParasiticNode *load_node =
        parasitics_->findParasiticNode(parasitic_network, pin);
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
				    int moment_count)
{
  // Driver model thevenin resistance.
  double rd = 0.0;
  // Zero'th moments are all 1 because Vin(0)=1 and there is no
  // current thru the resistors.  Thus, there is no point in doing a
  // pass to find the zero'th moments.
  for (int moment_index = 1; moment_index < moment_count; moment_index++) {
    double rd_i = findBranchCurrents(drvr_pin, drvr_node, 0, moment_index);
    double rd_volt = rd_i * rd;
    setMoment(drvr_node, 0.0, moment_index);
    findMoments(drvr_pin, drvr_node, -rd_volt, 0, moment_index);
  }
}

double
ReduceToPiPoleResidue2::findBranchCurrents(const Pin *drvr_pin,
					   ParasiticNode *node,
					   ParasiticResistor *from_res,
					   int moment_index)
{
  visit(node);
  double branch_i = 0.0;
  double coupling_cap = 0.0;
  ParasiticResistorSeq &resistors = resistor_map_[node];
  for (ParasiticResistor *resistor : resistors) {
    ParasiticNode *onode = parasitics_->otherNode(resistor, node);
    // One commercial extractor creates resistors with identical from/to nodes.
    if (onode != node
        && resistor != from_res
        && !isVisited(onode)
        && !isLoopResistor(resistor)) {
      branch_i += findBranchCurrents(drvr_pin, onode, resistor, moment_index);
    }
  }
  ParasiticCapacitorSeq &capacitors = capacitor_map_[node];
  for (ParasiticCapacitor *capacitor : capacitors)
    coupling_cap += parasitics_->value(capacitor);

  double cap = parasitics_->nodeGndCap(node)
    + coupling_cap * coupling_cap_multiplier_
    + pinCapacitance(node);
  branch_i += cap * moment(node, moment_index - 1);
  leave(node);
  if (from_res) {
    setCurrent(from_res, branch_i);
    debugPrint(debug_, "parasitic_reduce", 3, " res i=%.3g", branch_i);
  }
  return branch_i;
}

void
ReduceToPiPoleResidue2::findMoments(const Pin *drvr_pin,
				    ParasiticNode *node,
				    double from_volt,
				    ParasiticResistor *from_res,
				    int moment_index)
{
  visit(node);
  ParasiticResistorSeq &resistors = resistor_map_[node];
  for (ParasiticResistor *resistor : resistors) {
    ParasiticNode *onode = parasitics_->otherNode(resistor, node);
    // One commercial extractor creates resistors with identical from/to nodes.
    if (onode != node
        && resistor != from_res
        && !isVisited(onode)
        && !isLoopResistor(resistor)) {
      double r = parasitics_->value(resistor);
      double r_volt = r * current(resistor);
      double onode_volt = from_volt - r_volt;
      setMoment(onode, onode_volt, moment_index);
      debugPrint(debug_, "parasitic_reduce", 3, " moment %s %d %.3g",
                 parasitics_->name(onode),
                 moment_index,
                 onode_volt);
      findMoments(drvr_pin, onode, onode_volt, resistor, moment_index);
    }
  }
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
ReduceToPiPoleResidue2::current(ParasiticResistor *res)
{
  return currents_[res];
}

void
ReduceToPiPoleResidue2::setCurrent(ParasiticResistor *res,
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
    debugPrint(debug_, "parasitic_reduce", 3, " load %s p1=%.3g k1=%.3g",
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
    debugPrint(debug_, "parasitic_reduce", 3,
               " load %s p1=%.3g p2=%.3g k1=%.3g k2=%.3g",
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
