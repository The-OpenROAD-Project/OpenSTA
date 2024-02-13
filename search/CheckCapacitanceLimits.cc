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

#include "CheckCapacitanceLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "InputDrive.hh"
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
  bool operator()(const Pin *pin1,
		  const Pin *pin2) const;

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
PinCapacitanceLimitSlackLess::operator()(const Pin *pin1,
					 const Pin *pin2) const
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
					 const Corner *corner,
					 const MinMax *min_max,
					 // Return values.
					 const Corner *&corner1,
					 const RiseFall *&rf1,
					 float &capacitance1,
					 float &limit1,
					 float &slack1) const
{
  corner1 = nullptr;
  rf1 = nullptr;
  capacitance1 = 0.0;
  limit1 = 0.0;
  slack1 = MinMax::min()->initValue();
  if (corner)
    checkCapacitance1(pin, corner, min_max,
		      corner1, rf1, capacitance1, limit1, slack1);
  else {
    for (auto corner : *sta_->corners()) {
      checkCapacitance1(pin, corner, min_max,
                        corner1, rf1, capacitance1, limit1, slack1);
    }
  }
}

void
CheckCapacitanceLimits::checkCapacitance1(const Pin *pin,
					  const Corner *corner,
					  const MinMax *min_max,
					  // Return values.
					  const Corner *&corner1,
					  const RiseFall *&rf1,
					  float &capacitance1,
					  float &limit1,
					  float &slack1) const
{
  float limit;
  bool limit_exists;
  findLimit(pin, corner, min_max, limit, limit_exists);
  if (limit_exists) {
    for (auto rf : RiseFall::range()) {
      checkCapacitance(pin, corner, min_max, rf, limit,
		       corner1, rf1, capacitance1, slack1, limit1);
    }
  }
}

// Return the tightest limit.
void
CheckCapacitanceLimits::findLimit(const Pin *pin,
                                  const Corner *corner,
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
    InputDrive *drive = sdc->findInputDrive(port);
    if (drive) {
      for (auto rf : RiseFall::range()) {
        const LibertyCell *cell;
        const LibertyPort *from_port;
        float *from_slews;
        const LibertyPort *to_port;
        drive->driveCell(rf, min_max, cell, from_port, from_slews, to_port);
        if (to_port) {
          const LibertyPort *corner_port = to_port->cornerPort(corner, min_max);
          corner_port->capacitanceLimit(min_max, limit1, exists1);
          if (!exists1
              && corner_port->direction()->isAnyOutput()
              && min_max == MinMax::max())
            corner_port->libertyLibrary()->defaultMaxCapacitance(limit1, exists1);
          if (exists1
              && (!exists
                  || min_max->compare(limit, limit1))) {
            limit = limit1;
            exists = true;
          }
        }
      }
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
      LibertyPort *corner_port = port->cornerPort(corner, min_max);
      corner_port->capacitanceLimit(min_max, limit1, exists1);
      if (!exists1
	  && port->direction()->isAnyOutput())
	corner_port->libertyLibrary()->defaultMaxCapacitance(limit1, exists1);
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
					 const RiseFall *rf,
					 float limit,
					 // Return values.
					 const Corner *&corner1,
					 const RiseFall *&rf1,
					 float &capacitance1,
					 float &slack1,
					 float &limit1) const
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  GraphDelayCalc *dcalc = sta_->graphDelayCalc();
  float cap = dcalc->loadCap(pin, dcalc_ap);

  float slack = (min_max == MinMax::max())
    ? limit - cap : cap - limit;
  if (slack < slack1
      // Break ties for the sake of regression stability.
      || (fuzzyEqual(slack, slack1)
          && rf->index() < rf1->index())) {
    corner1 = corner;
    rf1 = rf;
    capacitance1 = cap;
    slack1 = slack;
    limit1 = limit;
  }
}

////////////////////////////////////////////////////////////////

PinSeq
CheckCapacitanceLimits::checkCapacitanceLimits(const Net *net,
                                               bool violators,
                                               const Corner *corner,
                                               const MinMax *min_max)
{
  const Network *network = sta_->network();
  PinSeq cap_pins;
  float min_slack = MinMax::min()->initValue();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
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
  if (!cap_pins.empty() && !violators && net == nullptr)
    cap_pins.resize(1);
  return cap_pins;
}

void
CheckCapacitanceLimits::checkCapLimits(const Instance *inst,
                                       bool violators,
                                       const Corner *corner,
                                       const MinMax *min_max,
                                       PinSeq &cap_pins,
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
CheckCapacitanceLimits::checkCapLimits(const Pin *pin,
                                       bool violators,
                                       const Corner *corner,
                                       const MinMax *min_max,
                                       PinSeq &cap_pins,
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
          cap_pins.push_back(pin);
      }
      else {
        if (cap_pins.empty()
            || slack < min_slack) {
          cap_pins.push_back(pin);
          min_slack = slack;
        }
      }
    }
  }
}

bool
CheckCapacitanceLimits::checkPin(const Pin *pin)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  const Sdc *sdc = sta_->sdc();
  const Graph *graph = sta_->graph();
  Vertex *vertex = graph->pinLoadVertex(pin);
  return network->isDriver(pin)
    && !sim->logicZeroOne(pin)
    && !sdc->isDisabled(pin)
    && !(vertex && sta_->isIdealClock(pin));
}

} // namespace
