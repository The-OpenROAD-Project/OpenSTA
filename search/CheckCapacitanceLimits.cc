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

#include "CheckCapacitanceLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "PortDirection.hh"
#include "Sim.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"

namespace sta {

class PinCapacitanceLimitSlackLess
{
public:
  PinCapacitanceLimitSlackLess(const Corner *corner,
			       const MinMax *min_max,
			       CheckCapacitanceLimits *check_capacitance_limit,
			       const StaState *sta);
  bool operator()(Pin *pin1,
		  Pin *pin2) const;

private:
  const Corner *corner_;
  const MinMax *min_max_;
  CheckCapacitanceLimits *check_capacitance_limit_;
  const StaState *sta_;

};

PinCapacitanceLimitSlackLess::PinCapacitanceLimitSlackLess(const Corner *corner,
							   const MinMax *min_max,
							   CheckCapacitanceLimits *check_capacitance_limit,
							   const StaState *sta) :
  corner_(corner),
  min_max_(min_max),
  check_capacitance_limit_(check_capacitance_limit),
  sta_(sta)
{
}

bool
PinCapacitanceLimitSlackLess::operator()(Pin *pin1,
					 Pin *pin2) const
{
  const Corner *corner1, *corner2;
  const RiseFall *rf1, *rf2;
  float capacitance1, capacitance2;
  float limit1, limit2, slack1, slack2;
  check_capacitance_limit_->checkCapacitance(pin1, corner_, min_max_,
					     corner1, rf1, capacitance1,
					     limit1, slack1);
  check_capacitance_limit_->checkCapacitance(pin2, corner_, min_max_,
					     corner2, rf2, capacitance2,
					     limit2, slack2);
  return fuzzyLess(slack1, slack2)
    || (fuzzyEqual(slack1, slack2)
	// Break ties for the sake of regression stability.
	&& sta_->network()->pinLess(pin1, pin2));
}

////////////////////////////////////////////////////////////////

CheckCapacitanceLimits::CheckCapacitanceLimits(const Sta *sta) :
  sta_(sta)
{
}

void
CheckCapacitanceLimits::checkCapacitance(const Pin *pin,
					 const Corner *corner1,
					 const MinMax *min_max,
					 // Return values.
					 const Corner *&corner,
					 const RiseFall *&rf,
					 float &capacitance,
					 float &limit,
					 float &slack) const
{
  corner = nullptr;
  rf = nullptr;
  capacitance = 0.0;
  limit = 0.0;
  slack = MinMax::min()->initValue();
  if (corner1)
    checkCapacitance1(pin, corner1, min_max,
		      corner, rf, capacitance, limit, slack);
  else {
    for (auto corner1 : *sta_->corners()) {
      checkCapacitance1(pin, corner1, min_max,
			corner, rf, capacitance, limit, slack);
    }
  }
}

void
CheckCapacitanceLimits::checkCapacitance1(const Pin *pin,
					  const Corner *corner1,
					  const MinMax *min_max,
					  // Return values.
					  const Corner *&corner,
					  const RiseFall *&rf,
					  float &capacitance,
					  float &limit,
					  float &slack) const
{
  float limit1;
  bool limit1_exists;
  findLimit(pin, min_max, limit1, limit1_exists);
  if (limit1_exists) {
    for (auto rf1 : RiseFall::range()) {
      checkCapacitance(pin, corner1, min_max, rf1, limit1,
		       corner, rf, capacitance, slack, limit);
    }
  }
}

// return the tightest limit.
void
CheckCapacitanceLimits::findLimit(const Pin *pin,
				  const MinMax *min_max,
				  // Return values.
				  float &limit,
				  bool &exists) const
{
  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();

  // Default to top ("design") limit.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->capacitanceLimit(top_cell, min_max,
			limit, exists);

  float limit1;
  bool exists1;
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->capacitanceLimit(port, min_max, limit1, exists1);
    if (exists1
	&& (!exists
	    || min_max->compare(limit, limit1))) {
	limit = limit1;
	exists = true;
    }
  }
  else {
    Cell *cell = network->cell(network->instance(pin));
    sdc->capacitanceLimit(cell, min_max,
			  limit1, exists1);
    if (exists1
	&& (!exists
	    || min_max->compare(limit, limit1))) {
	limit = limit1;
	exists = true;
    }
    LibertyPort *port = network->libertyPort(pin);
    if (port) {
      port->capacitanceLimit(min_max, limit1, exists1);
      if (!exists1
	  && port->direction()->isAnyOutput())
	port->libertyLibrary()->defaultMaxCapacitance(limit1, exists1);
      if (exists1
	  && (!exists
	      || min_max->compare(limit, limit1))) {
	limit = limit1;
	exists = true;
      }
    }
  }
}

void
CheckCapacitanceLimits::checkCapacitance(const Pin *pin,
					 const Corner *corner1,
					 const MinMax *min_max,
					 const RiseFall *rf1,
					 float limit1,
					 // Return values.
					 const Corner *&corner,
					 const RiseFall *&rf,
					 float &capacitance,
					 float &slack,
					 float &limit) const
{
  const DcalcAnalysisPt *dcalc_ap = corner1->findDcalcAnalysisPt(min_max);
  GraphDelayCalc *dcalc = sta_->graphDelayCalc();
  float cap = dcalc->loadCap(pin, dcalc_ap);

  float slack1 = (min_max == MinMax::max())
    ? limit1 - cap : cap - limit1;
  if (slack1 < slack
      // Break ties for the sake of regression stability.
      || (fuzzyEqual(slack1, slack)
          && rf1->index() < rf->index())) {
    corner = corner1;
    rf = rf1;
    capacitance = cap;
    slack = slack1;
    limit = limit1;
  }
}

////////////////////////////////////////////////////////////////

PinSeq *
CheckCapacitanceLimits::checkCapacitanceLimits(Net *net,
                                               bool violators,
                                               const Corner *corner,
                                               const MinMax *min_max)
{
  const Network *network = sta_->network();
  PinSeq *cap_pins = new PinSeq;
  Slack min_slack = MinMax::min()->initValue();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      checkCapLimits(pin, violators, corner, min_max, cap_pins, min_slack);
    }
    delete pin_iter;
  }
  else {
    LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      checkCapLimits(inst, violators, corner, min_max, cap_pins, min_slack);
    }
    delete inst_iter;
    // Check top level ports.
    checkCapLimits(network->topInstance(), violators, corner, min_max,
                   cap_pins, min_slack);
  }
  sort(cap_pins, PinCapacitanceLimitSlackLess(corner, min_max, this, sta_));
  // Keep the min slack pin unless all violators or net pins.
  if (!cap_pins->empty() && !violators && net == nullptr)
    cap_pins->resize(1);
  return cap_pins;
}

void
CheckCapacitanceLimits::checkCapLimits(Instance *inst,
                                       bool violators,
                                       const Corner *corner,
                                       const MinMax *min_max,
                                       PinSeq *cap_pins,
                                       float &min_slack)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    checkCapLimits(pin, violators, corner, min_max, cap_pins, min_slack);
  }
  delete pin_iter;
}

void
CheckCapacitanceLimits::checkCapLimits(Pin *pin,
                                       bool violators,
                                       const Corner *corner,
                                       const MinMax *min_max,
                                       PinSeq *cap_pins,
                                       float &min_slack)
{
  if (checkPin(pin)) {
    const Corner *corner1;
    const RiseFall *rf;
    float capacitance, limit, slack;
    checkCapacitance(pin, corner, min_max, corner1, rf, capacitance, limit, slack);
    if (!fuzzyInf(slack)) {
      if (violators) {
        if (slack < 0.0)
          cap_pins->push_back(pin);
      }
      else {
        if (cap_pins->empty()
            || slack < min_slack) {
          cap_pins->push_back(pin);
          min_slack = slack;
        }
      }
    }
  }
}

bool
CheckCapacitanceLimits::checkPin(Pin *pin)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  const Sdc *sdc = sta_->sdc();
  const Graph *graph = sta_->graph();
  Vertex *vertex = graph->pinLoadVertex(pin);
  return network->direction(pin)->isAnyOutput()
    && !sim->logicZeroOne(pin)
    && !sdc->isDisabled(pin)
    && !(vertex && sta_->isIdealClock(pin));
}

} // namespace
