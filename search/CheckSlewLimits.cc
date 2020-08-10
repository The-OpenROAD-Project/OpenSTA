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

#include "CheckSlewLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "PathVertex.hh"
#include "PortDirection.hh"
#include "Search.hh"
#include "ClkNetwork.hh"

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
  check_slew_limit_->checkSlew(pin1, corner_, min_max_, true,
			       corner1, rf1, slew1, limit1, slack1);
  check_slew_limit_->checkSlew(pin2, corner_, min_max_, true,
			       corner2, rf2, slew2, limit2, slack2);
  return fuzzyLess(slack1, slack2)
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
CheckSlewLimits::checkSlew(const Pin *pin,
			   const Corner *corner,
			   const MinMax *min_max,
			   bool check_clks,
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
    checkSlews1(pin, corner, min_max, check_clks,
		corner1, rf, slew, limit, slack);
  else {
    for (auto corner : *sta_->corners()) {
      checkSlews1(pin, corner, min_max, check_clks,
		  corner1, rf, slew, limit, slack);
    }
  }
}

void
CheckSlewLimits::checkSlews1(const Pin *pin,
			     const Corner *corner,
			     const MinMax *min_max,
			     bool check_clks,
			     // Return values.
			     const Corner *&corner1,
			     const RiseFall *&rf,
			     Slew &slew,
			     float &limit,
			     float &slack) const
{
  Vertex *vertex, *bidirect_drvr_vertex;
  sta_->graph()->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    checkSlews1(vertex, corner, min_max, check_clks,
		corner1, rf, slew, limit, slack);
  if (bidirect_drvr_vertex)
    checkSlews1(bidirect_drvr_vertex, corner, min_max, check_clks,
		corner1, rf, slew, limit, slack);
}
  
void
CheckSlewLimits::checkSlews1(Vertex *vertex,
			     const Corner *corner1,
			     const MinMax *min_max,
			     bool check_clks,
			     // Return values.
			     const Corner *&corner,
			     const RiseFall *&rf,
			     Slew &slew,
			     float &limit,
			     float &slack) const
{
  const Pin *pin = vertex->pin();
  if (!vertex->isDisabledConstraint()
      && !vertex->isConstant()
      && !sta_->clkNetwork()->isIdealClock(pin)) {
    for (auto rf1 : RiseFall::range()) {
      float limit1;
      bool limit1_exists;
      findLimit(pin, vertex, rf1, min_max, check_clks,
		limit1, limit1_exists);
      if (limit1_exists) {
	checkSlew(vertex, corner1, rf1, min_max, limit1,
		  corner, rf, slew, slack, limit);
      }
    }
  }
}

// return the tightest limit.
void
CheckSlewLimits::findLimit(const Pin *pin,
			   const Vertex *vertex,
			   const RiseFall *rf,
			   const MinMax *min_max,
			   bool check_clks,
			   // Return values.
			   float &limit,
			   bool &exists) const
{
  exists = false;
  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();

  // Default to top ("design") limit.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->slewLimit(top_cell, min_max,
		 limit, exists);
  float limit1;
  bool exists1;
  if (check_clks) {
    // Look for clock slew limits.
    bool is_clk = sta_->clkNetwork()->isIdealClock(pin);
    ClockSet clks;
    clockDomains(vertex, clks);
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      PathClkOrData clk_data = is_clk ? PathClkOrData::clk : PathClkOrData::data;
      sdc->slewLimit(clk, rf, clk_data, min_max,
		     limit1, exists1);
      if (exists1
	  && (!exists
	      || min_max->compare(limit, limit1))) {
	limit = limit1;
	exists = true;
      }
    }
  }
  if (network->isTopLevelPort(pin)) {
    Port *port = network->port(pin);
    sdc->slewLimit(port, min_max, limit1, exists1);
    if (exists1
	&& (!exists
	    || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
  }
  else {
    LibertyPort *port = network->libertyPort(pin);
    if (port) {
      port->slewLimit(min_max, limit1, exists1);
      if (!exists1
	  && port->direction()->isAnyOutput()
	  && min_max == MinMax::max())
	port->libertyLibrary()->defaultMaxSlew(limit1, exists1);
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
			   const RiseFall *rf1,
			   const MinMax *min_max,
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
    checkSlew(pin, corner, min_max, true, corner1, rf, slew, limit, slack);
    if (rf && slack < 0.0 && !fuzzyInf(slack))
      violators->push_back(pin);
  }
  delete pin_iter;
}

Pin *
CheckSlewLimits::pinMinSlewLimitSlack(const Corner *corner,
				      const MinMax *min_max)
{
  const Network *network = sta_->network();
  Pin *min_slack_pin = nullptr;
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
    checkSlew(pin, corner, min_max, true, corner1, rf, slew, limit, slack);
    if (rf
	&& (min_slack_pin == nullptr
	    || slack < min_slack)) {
      min_slack_pin = pin;
      min_slack = slack;
    }
  }
  delete pin_iter;
}

} // namespace
