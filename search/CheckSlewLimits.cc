// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "CheckSlewLimits.hh"

#include "Fuzzy.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "InputDrive.hh"
#include "Graph.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "Path.hh"
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
  bool operator()(const Pin *pin1,
		  const Pin *pin2) const;

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
PinSlewLimitSlackLess::operator()(const Pin *pin1,
				  const Pin *pin2) const
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
CheckSlewLimits::checkSlewLimits(const Pin *pin,
                                 bool violators,
                                 const Corner *corner,
                                 const MinMax *min_max,
                                 PinSeq &slew_pins,
                                 float &min_slack)
{
  const Corner *corner1;
  const RiseFall *rf;
  Slew slew;
  float limit, slack;
  checkSlew(pin, corner, min_max, true, corner1, rf, slew, limit, slack);
  if (!fuzzyInf(slack)) {
    if (violators) {
      if (slack < 0.0)
        slew_pins.push_back(pin);
    }
    else {
      if (slew_pins.empty()
          || slack < min_slack) {
        slew_pins.push_back(pin);
        min_slack = slack;
      }
    }
  }
}

void
CheckSlewLimits::checkSlew(const Pin *pin,
			   const Corner *corner,
			   const MinMax *min_max,
			   bool check_clks,
			   // Return values.
			   const Corner *&corner1,
			   const RiseFall *&rf1,
			   Slew &slew1,
			   float &limit1,
			   float &slack1) const
{
  corner1 = nullptr;
  rf1 = nullptr;
  slew1 = 0.0;
  limit1 = 0.0;
  slack1 = MinMax::min()->initValue();

  Vertex *vertex, *bidirect_drvr_vertex;
  sta_->graph()->pinVertices(pin, vertex, bidirect_drvr_vertex);
  if (vertex)
    checkSlew1(pin, vertex, corner, min_max, check_clks,
               corner1, rf1, slew1, limit1, slack1);
  if (bidirect_drvr_vertex)
    checkSlew1(pin, bidirect_drvr_vertex, corner, min_max, check_clks,
               corner1, rf1, slew1, limit1, slack1);
}

void
CheckSlewLimits::checkSlew1(const Pin *pin,
                            const Vertex *vertex,
                            const Corner *corner,
                            const MinMax *min_max,
                            bool check_clks,
                            // Return values.
                            const Corner *&corner1,
                            const RiseFall *&rf1,
                            Slew &slew1,
                            float &limit1,
                            float &slack1) const
{
  if (!vertex->isDisabledConstraint()
      && !vertex->isConstant()
      && !sta_->clkNetwork()->isIdealClock(pin)) {
    ClockSet clks;
    if (check_clks)
      clks = clockDomains(vertex);
    if (corner)
      checkSlew2(pin, vertex, corner, min_max, clks,
                 corner1, rf1, slew1, limit1, slack1);
    else {
      for (auto corner : *sta_->corners()) {
        checkSlew2(pin, vertex, corner, min_max, clks,
                   corner1, rf1, slew1, limit1, slack1);
      }
    }
  }
}

void
CheckSlewLimits::checkSlew2(const Pin *pin,
                            const Vertex *vertex,
                            const Corner *corner,
                            const MinMax *min_max,
                            const ClockSet &clks,
                            // Return values.
                            const Corner *&corner1,
                            const RiseFall *&rf1,
                            Slew &slew1,
                            float &limit1,
                            float &slack1) const
{
  for (const RiseFall *rf : RiseFall::range()) {
    float limit;
    bool exists;
    findLimit(pin, corner, rf, min_max, clks,
              limit, exists);
    if (exists) {
      checkSlew3(vertex, corner, rf, min_max, limit,
                 corner1, rf1, slew1, slack1, limit1);
    }
  }
}

void
CheckSlewLimits::checkSlew3(const Vertex *vertex,
                            const Corner *corner,
                            const RiseFall *rf,
                            const MinMax *min_max,
                            float limit,
                            // Return values.
                            const Corner *&corner1,
                            const RiseFall *&rf1,
                            Slew &slew1,
                            float &slack1,
                            float &limit1) const
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
  Slew slew = sta_->graph()->slew(vertex, rf, dcalc_ap->index());
  float slew2 = delayAsFloat(slew);
  float slack = (min_max == MinMax::max())
    ? limit - slew2 : slew2 - limit;
  if (corner1 == nullptr
      || (slack < slack1
	  // Break ties for the sake of regression stability.
	  || (fuzzyEqual(slack, slack1)
	      && rf->index() < rf1->index()))) {
    corner1 = corner;
    rf1 = rf;
    slew1 = slew;
    slack1 = slack;
    limit1 = limit;
  }
}

// Return the tightest limit.
void
CheckSlewLimits::findLimit(const Pin *pin,
                           const Corner *corner,
			   const RiseFall *rf,
			   const MinMax *min_max,
			   const ClockSet &clks,
			   // Return values.
			   float &limit,
			   bool &exists) const
{
  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();
  LibertyPort *port = network->libertyPort(pin);
  findLimit(port, corner, min_max,
            limit, exists);

  float limit1;
  bool exists1;
  if (!clks.empty()) {
    // Look for clock slew limits.
    bool is_clk = sta_->clkNetwork()->isIdealClock(pin);
    for (Clock *clk : clks) {
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
          corner_port->slewLimit(min_max, limit1, exists1);
          if (!exists1
              && corner_port->direction()->isAnyOutput()
              && min_max == MinMax::max())
            corner_port->libertyLibrary()->defaultMaxSlew(limit1, exists1);
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
}

void
CheckSlewLimits::findLimit(const LibertyPort *port,
                           const Corner *corner,
			   const MinMax *min_max,
			   // Return values.
			   float &limit,
			   bool &exists) const
{
  limit = INF;
  exists = false;

  const Network *network = sta_->network();
  Sdc *sdc = sta_->sdc();
  float limit1;
  bool exists1;

  // Default to top ("design") limit.
  Cell *top_cell = network->cell(network->topInstance());
  sdc->slewLimit(top_cell, min_max,
		 limit1, exists1);
  if (exists1) {
    limit = limit1;
    exists = true;
  }

  if (port) {
    const LibertyPort *corner_port = port->cornerPort(corner, min_max);
    corner_port->slewLimit(min_max, limit1, exists1);
    if (!exists1
        // default_max_transition only applies to outputs.
        && corner_port->direction()->isAnyOutput()
        && min_max == MinMax::max())
      corner_port->libertyLibrary()->defaultMaxSlew(limit1, exists1);
    if (exists1
        && (!exists
            || min_max->compare(limit, limit1))) {
      limit = limit1;
      exists = true;
    }
  }
}

ClockSet
CheckSlewLimits::clockDomains(const Vertex *vertex) const
{
  ClockSet clks;
  VertexPathIterator path_iter(const_cast<Vertex*>(vertex), sta_);
  while (path_iter.hasNext()) {
    Path *path = path_iter.next();
    const Clock *clk = path->clock(sta_);
    if (clk)
      clks.insert(const_cast<Clock*>(clk));
  }
  return clks;
}

////////////////////////////////////////////////////////////////

PinSeq
CheckSlewLimits::checkSlewLimits(const Net *net,
                                 bool violators,
                                 const Corner *corner,
                                 const MinMax *min_max)
{
  const Network *network = sta_->network();
  PinSeq slew_pins;
  float min_slack = MinMax::min()->initValue();
  if (net) {
    NetPinIterator *pin_iter = network->pinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      checkSlewLimits(pin, violators, corner, min_max, slew_pins, min_slack);
    }
    delete pin_iter;
  }
  else {
    LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      const Instance *inst = inst_iter->next();
      checkSlewLimits(inst, violators,corner, min_max, slew_pins, min_slack);
    }
    delete inst_iter;
    // Check top level ports.
    checkSlewLimits(network->topInstance(), violators, corner, min_max,
                    slew_pins, min_slack);
  }
  sort(slew_pins, PinSlewLimitSlackLess(corner, min_max, this, sta_));
  // Keep the min slack pin unless all violators or net pins.
  if (!slew_pins.empty() && !violators && net == nullptr)
    slew_pins.resize(1);
  return slew_pins;
}

void
CheckSlewLimits::checkSlewLimits(const Instance *inst,
                                 bool violators,
                                 const Corner *corner,
                                 const MinMax *min_max,
                                 PinSeq &slew_pins,
                                 float &min_slack)
{
  const Network *network = sta_->network();
  InstancePinIterator *pin_iter = network->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    checkSlewLimits(pin, violators, corner, min_max, slew_pins, min_slack);
  }
  delete pin_iter;
}

} // namespace
