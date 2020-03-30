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
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "GraphDelayCalc.hh"

namespace sta {

GraphDelayCalc::GraphDelayCalc(StaState *sta) :
  StaState(sta)
{
}

void
GraphDelayCalc::copyState(const StaState *sta)
{
  StaState::copyState(sta);
}

void
GraphDelayCalc::setObserver(DelayCalcObserver *observer)
{
  // Observer not needed by GraphDelayCalc.
  delete observer;
}

string *
GraphDelayCalc::reportDelayCalc(Edge *,
				TimingArc *,
				const Corner *,
				const MinMax *,
				int)
{
  return new string;
}

float
GraphDelayCalc::incrementalDelayTolerance()
{
  return 0.0;
}

void
GraphDelayCalc::loadCap(const Pin *,
			Parasitic *,
			const RiseFall *,
			const DcalcAnalysisPt *,
			// Return values.
			float &pin_cap,
			float &wire_cap) const
{
  pin_cap = wire_cap = 0.0F;
}

float
GraphDelayCalc::loadCap(const Pin *,
			const RiseFall *,
			const DcalcAnalysisPt *) const
{
  return 0.0F;
}

float
GraphDelayCalc::loadCap(const Pin *,
			Parasitic *,
			const RiseFall *,
			const DcalcAnalysisPt *) const
{
  return 0.0F;
}

float
GraphDelayCalc::loadCap(const Pin *,
			const DcalcAnalysisPt *) const
{
  return 0.0F;
}

void
GraphDelayCalc::netCaps(const Pin *,
			const RiseFall *,
			const DcalcAnalysisPt *,
			// Return values.
			float &pin_cap,
			float &wire_cap,
			float &fanout,
			bool &has_set_load) const
{
  pin_cap = wire_cap = fanout = 0.0F;
  has_set_load = false;
}

float
GraphDelayCalc::ceff(Edge *,
		     TimingArc *,
		     const DcalcAnalysisPt *)
{
  return 0.0;
}

bool
GraphDelayCalc::isIdealClk(const Vertex *vertex)
{
  return false;
}

void
GraphDelayCalc::minPulseWidth(const Pin *pin,
			      const RiseFall *hi_low,
			      DcalcAPIndex ap_index,
			      const MinMax *min_max,
			      // Return values.
			      float &min_width,
			      bool &exists)
{
  // Sdf annotation.
  graph_->widthCheckAnnotation(pin, hi_low, ap_index,
			       min_width, exists);
  if (!exists) {
    // Liberty library.
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      Instance *inst = network_->instance(pin);
      Pvt *pvt = inst ? sdc_->pvt(inst, min_max) : nullptr;
      OperatingConditions *op_cond=sdc_->operatingConditions(min_max);
      port->minPulseWidth(hi_low, op_cond, pvt, min_width, exists);
    }
  }
}

void
GraphDelayCalc::minPeriod(const Pin *pin,
			  // Return values.
			  float &min_period,
			  bool &exists)
{
  exists = false;
  const MinMax *min_max = MinMax::max();
  for (auto dcalc_ap : corners_->dcalcAnalysisPts()) {
    // Sdf annotation.
    float min_period1 = 0.0;
    bool exists1 = false;
    graph_->periodCheckAnnotation(pin, dcalc_ap->index(),
				  min_period, exists);
    if (exists1 
	&& (!exists || min_period1 < min_period)) {
      min_period = min_period1;
      exists = true;
    }
  }
  if (!exists) {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      // Liberty library.
      Instance *inst = network_->instance(pin);
      OperatingConditions *op_cond = sdc_->operatingConditions(min_max);
      Pvt *pvt = inst ? sdc_->pvt(inst, min_max) : nullptr;
      port->minPeriod(op_cond, pvt, min_period, exists);
    }
  }
}

} // namespace
