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

#include "CheckFanoutLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "InputDrive.hh"
#include "Sim.hh"
#include "PortDirection.hh"
#include "Graph.hh"
#include "Search.hh"

namespace sta {

class PinFanoutLimitSlackLess
{
public:
  PinFanoutLimitSlackLess(const MinMax *min_max,
			  CheckFanoutLimits *check_fanout_limit,
			  const StaState *sta);
  bool operator()(const Pin *pin1,
		  const Pin *pin2) const;

private:
  const MinMax *min_max_;
  CheckFanoutLimits *check_fanout_limit_;
  const StaState *sta_;

};

PinFanoutLimitSlackLess::PinFanoutLimitSlackLess(const MinMax *min_max,
						 CheckFanoutLimits *check_fanout_limit,
						 const StaState *sta) :
  min_max_(min_max),
  check_fanout_limit_(check_fanout_limit),
  sta_(sta)
{
}

bool
PinFanoutLimitSlackLess::operator()(const Pin *pin1,
				    const Pin *pin2) const
{
  float fanout1, fanout2;
  float limit1, limit2, slack1, slack2;
  check_fanout_limit_->checkFanout(pin1, min_max_,
				   fanout1, limit1, slack1);
  check_fanout_limit_->checkFanout(pin2, min_max_,
				   fanout2, limit2, slack2);
  return fuzzyLess(slack1, slack2)
    || (fuzzyEqual(slack1, slack2)
	// Break ties for the sake of regression stability.
	&& sta_->network()->pinLess(pin1, pin2));
}

////////////////////////////////////////////////////////////////

CheckFanoutLimits::CheckFanoutLimits(const Sta *sta) :
  sta_(sta)
{
}

void
CheckFanoutLimits::checkFanout(const Pin *pin,
			       const MinMax *min_max,
			       // Return values.
			       float &fanout,
			       float &limit,
			       float &slack) const
{
  fanout = 0.0;
  limit = min_max->initValue();
  slack = MinMax::min()->initValue();

  float limit1;
  bool limit1_exists;
  findLimit(pin, min_max, limit1, limit1_exists);
  if (limit1_exists)
    checkFanout(pin, min_max, limit1,
		fanout, limit, slack);
}
  
// return the tightest limit.
void
CheckFanoutLimits::findLimit(const Pin *pin,
			     const MinMax *min_max,
			     // Return values.
			     float &limit,
			     bool &exists) const
{
  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();

  limit = min_max->initValue();
  exists = false;

  // Default to top ("design") limit.
  // Applies to input ports as well as instance outputs.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->fanoutLimit(top_cell, min_max,
                   limit, exists);

  float limit1;
  bool exists1;
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->fanoutLimit(port, min_max, limit1, exists1);
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
          to_port->fanoutLimit(min_max, limit1, exists1);
          if (!exists1
              && min_max == MinMax::max()
              && to_port->direction()->isAnyOutput())
            to_port->libertyLibrary()->defaultMaxFanout(limit1, exists1);
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
    sdc->fanoutLimit(cell, min_max,
		     limit1, exists1);
    if (exists1
	&& (!exists
	    || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
    LibertyPort *port = network->libertyPort(pin);
    if (port) {
      port->fanoutLimit(min_max, limit1, exists1);
      if (!exists1
	  && min_max == MinMax::max()
	  && port->direction()->isAnyOutput())
	port->libertyLibrary()->defaultMaxFanout(limit1, exists1);
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
CheckFanoutLimits::checkFanout(const Pin *pin,
			       const MinMax *min_max,
			       float limit1,
			       // Return values.
			       float &fanout,
			       float &limit,
			       float &slack) const
{
  float fanout1 = fanoutLoad(pin);
  float slack1 = (min_max == MinMax::max())
    ? limit1 - fanout1
    : fanout1 - limit1;
  if (fuzzyLessEqual(slack1, slack)) {
    fanout = fanout1;
    slack = slack1;
    limit = limit1;
  }
}

float
CheckFanoutLimits::fanoutLoad(const Pin *pin) const
{
  float fanout = 0;
  const Network *network = sta_->network();
  NetConnectedPinIterator *pin_iter = network->connectedPinIterator(pin);
  while (pin_iter->hasNext()) {
    const Pin *fanout_pin = pin_iter->next();
    if (network->isLoad(fanout_pin)
        && !network->isTopLevelPort(fanout_pin)) {
      LibertyPort *port = network->libertyPort(fanout_pin);
      if (port) {
        float fanout_load;
        bool exists;
        port->fanoutLoad(fanout_load, exists);
        if (!exists) {
          LibertyLibrary *lib = port->libertyLibrary();
          lib->defaultFanoutLoad(fanout_load, exists);
        }
        if (exists)
          fanout += fanout_load;
      }
      else
        fanout += 1;
    }
  }
  delete pin_iter;
  return fanout;
}

////////////////////////////////////////////////////////////////

PinSeq
CheckFanoutLimits::checkFanoutLimits(const Net *net,
                                     bool violators,
                                     const MinMax *min_max)
{
  const Network *network = sta_->network();
  PinSeq fanout_pins;
  float min_slack = MinMax::min()->initValue();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      checkFanoutLimits(pin, violators, min_max, fanout_pins, min_slack);
    }
    delete pin_iter;
  }
  else {
    LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      const Instance *inst = inst_iter->next();
      checkFanoutLimits(inst, violators, min_max, fanout_pins, min_slack);
    }
    delete inst_iter;
    // Check top level ports.
    checkFanoutLimits(network->topInstance(), violators, min_max,
                    fanout_pins, min_slack);
  }
  sort(fanout_pins, PinFanoutLimitSlackLess(min_max, this, sta_));
  // Keep the min slack pin unless all violators or net pins.
  if (!fanout_pins.empty() && !violators && net == nullptr)
    fanout_pins.resize(1);
  return fanout_pins;
}

void
CheckFanoutLimits::checkFanoutLimits(const Instance *inst,
                                     bool violators,
                                     const MinMax *min_max,
                                     PinSeq &fanout_pins,
                                     float &min_slack)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    checkFanoutLimits(pin, violators, min_max, fanout_pins, min_slack);
  }
  delete pin_iter;
}

void
CheckFanoutLimits::checkFanoutLimits(const Pin *pin,
                                     bool violators,
                                     const MinMax *min_max,
                                     PinSeq &fanout_pins,
                                     float &min_slack)
{
  if (checkPin(pin)) {
    float fanout;
    float limit, slack;
    checkFanout(pin, min_max, fanout, limit, slack);
    if (!fuzzyInf(slack)) {
      if (violators) {
        if (slack < 0.0)
          fanout_pins.push_back(pin);
      }
      else {
        if (fanout_pins.empty()
            || slack < min_slack) {
          fanout_pins.push_back(pin);
          min_slack = slack;
        }
      }
    }
  }
}

bool
CheckFanoutLimits::checkPin(const Pin *pin)
{
  const Network *network = sta_->network();
  const Sim *sim = sta_->sim();
  const Sdc *sdc = sta_->sdc();
  const Graph *graph = sta_->graph();
  Vertex *vertex = graph->pinDrvrVertex(pin);
  return network->isDriver(pin)
    && !sim->logicZeroOne(pin)
    && !sdc->isDisabled(pin)
    && !(vertex && sta_->isIdealClock(pin));
}

} // namespace
