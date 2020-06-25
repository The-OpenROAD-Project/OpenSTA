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
					 const Corner *corner,
					 const MinMax *min_max,
					 const RiseFall *rf1,
					 float limit1,
					 // Return values.
					 const Corner *&corner1,
					 const RiseFall *&rf,
					 float &capacitance,
					 float &slack,
					 float &limit) const
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
  GraphDelayCalc *dcalc = sta_->graphDelayCalc();
  float cap = dcalc->loadCap(pin, dcalc_ap);

  float slack1 = (min_max == MinMax::max())
    ? limit1 - cap : cap - limit1;
  if (corner == nullptr
      || (slack1 < slack
	  // Break ties for the sake of regression stability.
	  || (fuzzyEqual(slack1, slack)
	      && rf1->index() < rf->index()))) {
    corner1 = corner;
    rf = rf1;
    capacitance = cap;
    slack = slack1;
    limit = limit1;
  }
}

PinSeq *
CheckCapacitanceLimits::pinCapacitanceLimitViolations(const Corner *corner,
						      const MinMax *min_max)
{
  const Network *network = sta_->network();
  PinSeq *violators = new PinSeq;
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinCapacitanceLimitViolations(inst, corner, min_max, violators);
  }
  delete inst_iter;
  // Check top level ports.
  pinCapacitanceLimitViolations(network->topInstance(), corner, min_max, violators);
  sort(violators, PinCapacitanceLimitSlackLess(corner, min_max, this, sta_));
  return violators;
}

void
CheckCapacitanceLimits::pinCapacitanceLimitViolations(Instance *inst,
						      const Corner *corner,
						      const MinMax *min_max,
						      PinSeq *violators)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (checkPin(pin)) {
      const Corner *corner1;
      const RiseFall *rf;
      float capacitance, limit, slack;
      checkCapacitance(pin, corner, min_max, corner1, rf, capacitance, limit, slack );
      if (rf && slack < 0.0 && !fuzzyInf(slack))
	violators->push_back(pin);
    }
  }
  delete pin_iter;
}

Pin *
CheckCapacitanceLimits::pinMinCapacitanceLimitSlack(const Corner *corner,
						    const MinMax *min_max)
{
  const Network *network = sta_->network();
  Pin *min_slack_pin = nullptr;
  float min_slack = MinMax::min()->initValue();
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinMinCapacitanceLimitSlack(inst, corner, min_max, min_slack_pin, min_slack);
  }
  delete inst_iter;
  // Check top level ports.
  pinMinCapacitanceLimitSlack(network->topInstance(), corner, min_max,
		       min_slack_pin, min_slack);
  return min_slack_pin;
}

void
CheckCapacitanceLimits::pinMinCapacitanceLimitSlack(Instance *inst,
						    const Corner *corner,
						    const MinMax *min_max,
						    // Return values.
						    Pin *&min_slack_pin,
						    float &min_slack)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (checkPin(pin)) {
      const Corner *corner1;
      const RiseFall *rf;
      float capacitance, limit, slack;
      checkCapacitance(pin, corner, min_max, corner1, rf, capacitance, limit, slack);
      if (rf
	  && !fuzzyInf(slack)
	  && (min_slack_pin == nullptr
	      || slack < min_slack)) {
	min_slack_pin = pin;
	min_slack = slack;
      }
    }
  }
  delete pin_iter;
}

bool
CheckCapacitanceLimits::checkPin(Pin *pin)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  const Sdc *sdc = sta_->sdc();
  const Graph *graph = sta_->graph();
  Search *search = sta_->search();
  Vertex *vertex = graph->pinLoadVertex(pin);
  return network->direction(pin)->isAnyOutput()
    && !sim->logicZeroOne(pin)
    && !sdc->isDisabled(pin)
    && !(vertex && sta_->isIdealClock(pin));
}

} // namespace
