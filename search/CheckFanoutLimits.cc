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

#include "CheckFanoutLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "PortDirection.hh"

namespace sta {

class PinFanoutLimitSlackLess
{
public:
  PinFanoutLimitSlackLess(const MinMax *min_max,
			  CheckFanoutLimits *check_fanout_limit,
			  const StaState *sta);
  bool operator()(Pin *pin1,
		  Pin *pin2) const;

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
PinFanoutLimitSlackLess::operator()(Pin *pin1,
				    Pin *pin2) const
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

CheckFanoutLimits::CheckFanoutLimits(const StaState *sta) :
  sta_(sta)
{
}

void
CheckFanoutLimits::init(const MinMax *min_max)
{
  const Network *network = sta_->network();
  Cell *top_cell = network->cell(network->topInstance());
  float top_limit;
  bool top_limit_exists;
  sta_->sdc()->fanoutLimit(top_cell, min_max,
			   top_limit, top_limit_exists);
  top_limit_= top_limit;
  top_limit_exists_ = top_limit_exists;
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
  limit = 0.0;
  slack = MinMax::min()->initValue();

  float limit1;
  bool limit1_exists;
  findLimit(pin, min_max, limit1, limit1_exists);
  if (limit1_exists) {
    checkFanout(pin, min_max, limit1,
		fanout, slack, limit);
  }
}
  
void
CheckFanoutLimits::findLimit(const Pin *pin,
			     const MinMax *min_max,
			     // Return values.
			     float &limit,
			     bool &exists) const
{
  exists = false;
  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->fanoutLimit(port, min_max, limit, exists);
    if (!exists) {
      limit = top_limit_;
      exists = top_limit_exists_;
    }
  }
  else {
    Cell *cell = network->cell(network->instance(pin));
    sdc->fanoutLimit(cell, min_max,
		     limit, exists);
    if (!exists) {
      LibertyPort *port = network->libertyPort(pin);
      if (port) {
	port->fanoutLimit(min_max, limit, exists);
	if (!exists
	    && port->direction()->isAnyOutput())
	  port->libertyLibrary()->defaultMaxFanout(limit, exists);
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
			       float &slack,
			       float &limit) const
{
  float fanout1 = this->fanout(pin);
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
CheckFanoutLimits::fanout(const Pin *pin) const
{
  float fanout = 0;
  const Network *network = sta_->network();
  Net *net = network->net(pin);
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      if (network->isLoad(pin))
	fanout++;
    }
    delete pin_iter;
  }
  return fanout;
}

PinSeq *
CheckFanoutLimits::pinFanoutLimitViolations(const MinMax *min_max)
{
  init(min_max);
  const Network *network = sta_->network();
  PinSeq *violators = new PinSeq;
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinFanoutLimitViolations(inst, min_max, violators);
  }
  delete inst_iter;

  // Check top level ports.
  pinFanoutLimitViolations(network->topInstance(), min_max, violators);
  sort(violators, PinFanoutLimitSlackLess(min_max, this, sta_));
  return violators;
}

void
CheckFanoutLimits::pinFanoutLimitViolations(Instance *inst,
					    const MinMax *min_max,
					    PinSeq *violators)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    float fanout;
    float limit, slack;
    checkFanout(pin, min_max, fanout, limit, slack );
    if (slack < 0.0)
      violators->push_back(pin);
  }
  delete pin_iter;
}

Pin *
CheckFanoutLimits::pinMinFanoutLimitSlack(const MinMax *min_max)
{
  init(min_max);
  const Network *network = sta_->network();
  Pin *min_slack_pin = 0;
  float min_slack = MinMax::min()->initValue();
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinMinFanoutLimitSlack(inst, min_max, min_slack_pin, min_slack);
  }
  delete inst_iter;
  // Check top level ports.
  pinMinFanoutLimitSlack(network->topInstance(), min_max,
		       min_slack_pin, min_slack);
  return min_slack_pin;
}

void
CheckFanoutLimits::pinMinFanoutLimitSlack(Instance *inst,
					  const MinMax *min_max,
					  // Return values.
					  Pin *&min_slack_pin,
					  float &min_slack)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    float fanout;
    float limit, slack;
    checkFanout(pin, min_max, fanout, limit, slack);
    if (min_slack_pin == 0
	|| slack < min_slack) {
      min_slack_pin = pin;
      min_slack = slack;
    }
  }
  delete pin_iter;
}

} // namespace
