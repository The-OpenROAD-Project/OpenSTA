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
#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "StaState.hh"
#include "DcalcAnalysisPt.hh"
#include "Corner.hh"
#include "GraphDelayCalc.hh"
#include "PathVertex.hh"
#include "Search.hh"
#include "CheckSlewLimits.hh"

namespace sta {

class PinSlewLimitSlackLess
{
public:
  PinSlewLimitSlackLess(const Corner *corner,
			const MinMax *min_max,
			CheckSlewLimits *check_slew_limit,
			const StaState *sta);
  bool operator()(Pin *pin1,
		  Pin *pin2) const;

private:
  const Corner *corner_;
  const MinMax *min_max_;
  CheckSlewLimits *check_slew_limit_;
  const StaState *sta_;

};

PinSlewLimitSlackLess::PinSlewLimitSlackLess(const Corner *corner,
					     const MinMax *min_max,
					     CheckSlewLimits *check_slew_limit,
					     const StaState *sta) :
  corner_(corner),
  min_max_(min_max),
  check_slew_limit_(check_slew_limit),
  sta_(sta)
{
}

bool
PinSlewLimitSlackLess::operator()(Pin *pin1,
				  Pin *pin2) const
{
  const Corner *corner1, *corner2;
  const RiseFall *rf1, *rf2;
  Slew slew1, slew2;
  float limit1, limit2, slack1, slack2;
  check_slew_limit_->checkSlews(pin1, corner_, min_max_,
				corner1, rf1, slew1, limit1, slack1);
  check_slew_limit_->checkSlews(pin2, corner_, min_max_,
				corner2, rf2, slew2, limit2, slack2);
  return slack1 < slack2
    || (fuzzyEqual(slack1, slack2)
	// Break ties for the sake of regression stability.
	&& sta_->network()->pinLess(pin1, pin2));
}

////////////////////////////////////////////////////////////////

CheckSlewLimits::CheckSlewLimits(const StaState *sta) :
  sta_(sta)
{
}

void
CheckSlewLimits::init(const MinMax *min_max)
{
  const Network *network = sta_->network();
  Cell *top_cell = network->cell(network->topInstance());
  float top_limit;
  bool top_limit_exists;
  sta_->sdc()->slewLimit(top_cell, min_max,
			 top_limit, top_limit_exists);
  top_limit_= top_limit;
  top_limit_exists_ = top_limit_exists;
}

void
CheckSlewLimits::checkSlews(const Pin *pin,
			    const Corner *corner,
			    const MinMax *min_max,
			    // Return values.
			    const Corner *&corner1,
			    const RiseFall *&rf,
			    Slew &slew,
			    float &limit,
			    float &slack) const
{
  corner1 = nullptr;
  rf = nullptr;
  slew = 0.0;
  limit = 0.0;
  slack = MinMax::min()->initValue();
  if (corner)
    checkSlews1(pin, corner, min_max,
		corner1, rf, slew, limit, slack);
  else {
    for (auto corner : *sta_->corners()) {
      checkSlews1(pin, corner, min_max,
		  corner1, rf, slew, limit, slack);
    }
  }
}

void
CheckSlewLimits::checkSlews1(const Pin *pin,
			     const Corner *corner,
			     const MinMax *min_max,
			     // Return values.
			     const Corner *&corner1,
			     const RiseFall *&rf,
			     Slew &slew,
			     float &limit,
			     float &slack) const
{
  Vertex *vertex, *bidirect_drvr_vertex;
  sta_->graph()->pinVertices(pin, vertex, bidirect_drvr_vertex);
  checkSlews1(vertex, corner, min_max,
	      corner1, rf, slew, limit, slack);
  if (bidirect_drvr_vertex)
    checkSlews1(bidirect_drvr_vertex, corner, min_max,
		corner1, rf, slew, limit, slack);
}
  
void
CheckSlewLimits::checkSlews1(Vertex *vertex,
			     const Corner *corner1,
			     const MinMax *min_max,
			     // Return values.
			     const Corner *&corner,
			     const RiseFall *&rf,
			     Slew &slew,
			     float &limit,
			     float &slack) const
{
  for (auto rf1 : RiseFall::range()) {
    float limit1;
    bool limit1_exists;
    findLimit(vertex->pin(), vertex, rf1, min_max, limit1, limit1_exists);
    if (limit1_exists) {
      checkSlew(vertex, corner1, min_max, rf1, limit1,
		corner, rf, slew, slack, limit);
    }
  }
}

void
CheckSlewLimits::findLimit(const Pin *pin,
			   const Vertex *vertex,
			   const RiseFall *rf,
			   const MinMax *min_max,
			   // Return values.
			   float &limit,
			   bool &exists) const
{
  exists = false;
  if (!sta_->graphDelayCalc()->isIdealClk(vertex)) {
    const Network *network = sta_->network();
    Sdc *sdc = sta_->sdc();
    bool is_clk = sta_->search()->isClock(vertex);
    // Look for clock slew limits.
    ClockSet clks;
    clockDomains(vertex, clks);
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      PathClkOrData clk_data = is_clk ? PathClkOrData::clk : PathClkOrData::data;
      float clk_limit;
      bool clk_limit_exists;
      sdc->slewLimit(clk, rf, clk_data, min_max,
		     clk_limit, clk_limit_exists);
      if (clk_limit_exists
	  && (!exists
	      || min_max->compare(limit, clk_limit))) {
	// Use the tightest clock limit.
	limit = clk_limit;
	exists = true;
      }
    }
    if (!exists) {
      // Default to top ("design") limit.
      exists = top_limit_exists_;
      limit = top_limit_;
      if (network->isTopLevelPort(pin)) {
	Port *port = network->port(pin);
	float port_limit;
	bool port_limit_exists;
	sdc->slewLimit(port, min_max, port_limit, port_limit_exists);
	// Use the tightest limit.
	if (port_limit_exists
	    && (!exists
		|| min_max->compare(limit, port_limit))) {
	  limit = port_limit;
	  exists = true;
	}
      }
      else {
	float pin_limit;
	bool pin_limit_exists;
	sdc->slewLimit(pin, min_max,
		       pin_limit, pin_limit_exists);
	// Use the tightest limit.
	if (pin_limit_exists
	    && (!exists
		|| min_max->compare(limit, pin_limit))) {
	  limit = pin_limit;
	  exists = true;
	}

	float port_limit;
	bool port_limit_exists;
	LibertyPort *port = network->libertyPort(pin);
	if (port) {
	  port->slewLimit(min_max, port_limit, port_limit_exists);
	  // Use the tightest limit.
	  if (port_limit_exists
	      && (!exists
		  || min_max->compare(limit, port_limit))) {
	    limit = port_limit;
	    exists = true;
	  }
	}
      }
    }
  }
}

void
CheckSlewLimits::clockDomains(const Vertex *vertex,
			      // Return value.
			      ClockSet &clks) const
{
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), sta_);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    Clock *clk = path->clock(sta_);
    if (clk)
      clks.insert(clk);
  }
}

void
CheckSlewLimits::checkSlew(Vertex *vertex,
			   const Corner *corner1,
			   const MinMax *min_max,
			   const RiseFall *rf1,
			   float limit1,
			   // Return values.
			   const Corner *&corner,
			   const RiseFall *&rf,
			   Slew &slew,
			   float &slack,
			   float &limit) const
{
  const DcalcAnalysisPt *dcalc_ap = corner1->findDcalcAnalysisPt(min_max);
  Slew slew1 = sta_->graph()->slew(vertex, rf1, dcalc_ap->index());
  float slew2 = delayAsFloat(slew1);
  float slack1 = (min_max == MinMax::max())
    ? limit1 - slew2 : slew2 - limit1;
  if (corner == nullptr
      || (slack1 < slack
	  // Break ties for the sake of regression stability.
	  || (fuzzyEqual(slack1, slack)
	      && rf1->index() < rf->index()))) {
    corner = corner1;
    rf = rf1;
    slew = slew1;
    slack = slack1;
    limit = limit1;
  }
}

PinSeq *
CheckSlewLimits::pinSlewLimitViolations(const Corner *corner,
					const MinMax *min_max)
{
  init(min_max);
  const Network *network = sta_->network();
  PinSeq *violators = new PinSeq;
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinSlewLimitViolations(inst, corner, min_max, violators);
  }
  delete inst_iter;
  // Check top level ports.
  pinSlewLimitViolations(network->topInstance(), corner, min_max, violators);
  sort(violators, PinSlewLimitSlackLess(corner, min_max, this, sta_));
  return violators;
}

void
CheckSlewLimits::pinSlewLimitViolations(Instance *inst,
					const Corner *corner,
					const MinMax *min_max,
					PinSeq *violators)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    const Corner *corner1;
    const RiseFall *rf;
    Slew slew;
    float limit, slack;
    checkSlews(pin, corner, min_max, corner1, rf, slew, limit, slack );
    if (rf && slack < 0.0)
      violators->push_back(pin);
  }
  delete pin_iter;
}

Pin *
CheckSlewLimits::pinMinSlewLimitSlack(const Corner *corner,
				      const MinMax *min_max)
{
  init(min_max);
  const Network *network = sta_->network();
  Pin *min_slack_pin = 0;
  float min_slack = MinMax::min()->initValue();
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    pinMinSlewLimitSlack(inst, corner, min_max, min_slack_pin, min_slack);
  }
  delete inst_iter;
  // Check top level ports.
  pinMinSlewLimitSlack(network->topInstance(), corner, min_max,
		       min_slack_pin, min_slack);
  return min_slack_pin;
}

void
CheckSlewLimits::pinMinSlewLimitSlack(Instance *inst,
				      const Corner *corner,
				      const MinMax *min_max,
				      // Return values.
				      Pin *&min_slack_pin,
				      float &min_slack)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    const Corner *corner1;
    const RiseFall *rf;
    Slew slew;
    float limit, slack;
    checkSlews(pin, corner, min_max, corner1, rf, slew, limit, slack);
    if (rf
	&& (min_slack_pin == 0
	    || slack < min_slack)) {
      min_slack_pin = pin;
      min_slack = slack;
    }
  }
  delete pin_iter;
}

} // namespace
