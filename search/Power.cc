// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include <algorithm> // max
#include "Machine.hh"
#include "Debug.hh"
#include "Units.hh"
#include "Transition.hh"
#include "MinMax.hh"
#include "Liberty.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "TimingArc.hh"
#include "FuncExpr.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sim.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "PathVertex.hh"
#include "Clock.hh"
#include "Power.hh"

// Related liberty not supported:
// library
//  default_cell_leakage_power : 0;
//  output_voltage (default_VDD_VSS_output) {
// leakage_power
//  related_pg_pin : VDD;
// internal_power
//  input_voltage : default_VDD_VSS_input;
// pin
//  output_voltage : default_VDD_VSS_output;


namespace sta {

typedef std::pair<float, int> SumCount;
typedef Map<const char*, SumCount, CharPtrLess> SupplySumCounts;

Power::Power(Sta *sta) :
  StaState(sta),
  sta_(sta),
  default_signal_toggle_rate_(.1)
{
}

float
Power::defaultSignalToggleRate()
{
  return default_signal_toggle_rate_;
}

void
Power::setDefaultSignalToggleRate(float rate)
{
  default_signal_toggle_rate_ = rate;
}


void
Power::power(const Corner *corner,
	     // Return values.
	     PowerResult &total,
	     PowerResult &sequential,
  	     PowerResult &combinational,
	     PowerResult &macro,
	     PowerResult &pad)
{
  total.clear();
  sequential.clear();
  combinational.clear();
  macro.clear();
  pad.clear();
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      PowerResult inst_power;
      power(inst, corner, inst_power);
      if (cell->isMacro())
	macro.incr(inst_power);
      else if (cell->isPad())
	pad.incr(inst_power);
      else if (cell->hasSequentials())
	sequential.incr(inst_power);
      else
	combinational.incr(inst_power);
      total.incr(inst_power);
    }
  }
  delete inst_iter;
}

////////////////////////////////////////////////////////////////

void
Power::power(const Instance *inst,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result)
{
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell)
    power(inst, cell, corner, result);
}

void
Power::power(const Instance *inst,
	     LibertyCell *cell,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result)
{
  MinMax *mm = MinMax::max();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(mm);
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    const LibertyPort *to_port = network_->libertyPort(to_pin);
    float load_cap = to_port->direction()->isAnyOutput()
      ? loadCap(to_pin, dcalc_ap)
      : 0.0;
    float activity1;
    bool is_clk;
    activity(to_pin, activity1, is_clk);
    if (to_port->direction()->isAnyOutput()) {
      findSwitchingPower(cell, to_port, activity1, load_cap,
			 dcalc_ap, result);
    }
    findInternalPower(inst, cell, to_port, activity1,
		      load_cap, dcalc_ap, result);
  }
  delete pin_iter;
  findLeakagePower(inst, cell, result);
}

void
Power::findInternalPower(const Instance *inst,
			 LibertyCell *cell,
			 const LibertyPort *to_port,
			 float activity,
			 float load_cap,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 PowerResult &result)
{
  debugPrint3(debug_, "power", 2, "internal %s/%s (%s)\n",
	      network_->pathName(inst),
	      to_port->name(),
	      cell->name());
  SupplySumCounts supply_sum_counts;
  const Pvt *pvt = dcalc_ap->operatingConditions();
  float duty = 1.0;
  debugPrint2(debug_, "power", 2, " cap = %s duty = %.1f\n",
	      units_->capacitanceUnit()->asString(load_cap),
	      duty);
  LibertyCellInternalPowerIterator pwr_iter(cell);
  while (pwr_iter.hasNext()) {
    InternalPower *pwr = pwr_iter.next();
    const char *related_pg_pin = pwr->relatedPgPin();
    const LibertyPort *from_port = pwr->relatedPort();
    if (pwr->port() == to_port) {
      if (from_port == nullptr)
	from_port = to_port;
      const Pin *from_pin = network_->findPin(inst, from_port);
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      float port_internal = 0.0;
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *to_tr = tr_iter.next();
	// Should use unateness to find from_tr.
	float slew = delayAsFloat(sta_->vertexSlew(from_vertex,
						   to_tr, dcalc_ap));
	float energy;
	if (from_port)
	  energy = pwr->power(to_tr, pvt, slew, load_cap);
	else
	  energy = pwr->power(to_tr, pvt, 0.0, 0.0);
	float tr_internal = energy * activity * duty;
	port_internal += tr_internal;
	debugPrint5(debug_, "power", 2, " %s -> %s %s %s (%s)\n",
		    from_port->name(),
		    to_tr->shortName(),
		    to_port->name(),
		    pwr->when() ? pwr->when()->asString() : "",
		    related_pg_pin ? related_pg_pin : "");
	debugPrint4(debug_, "power", 2, "  slew = %s activity = %.2f/ns energy = %.5g pwr = %.5g\n",
		    units_->timeUnit()->asString(slew),
		    activity * 1e-9,
		    energy,
		    tr_internal);
      }
      // Sum/count internal power arcs by supply to average across conditions.
      SumCount &supply_sum_count = supply_sum_counts[related_pg_pin];
      // Average rise/fall internal power.
      supply_sum_count.first += port_internal / 2.0;
      supply_sum_count.second++;
    }
  }
  float internal = 0.0;
  SupplySumCounts::Iterator supply_iter(supply_sum_counts);
  while (supply_iter.hasNext()) {
    const SumCount &supply_cum_count = supply_iter.next();
    float supply_internal = supply_cum_count.first;
    int supply_count = supply_cum_count.second;
    internal += supply_internal / (supply_count > 0 ? supply_count : 1);
  }

  debugPrint1(debug_, "power", 2, " internal = %.5g\n", internal);
  result.setInternal(result.internal() + internal);
}

float
Power::loadCap(const Pin *to_pin,
	       const DcalcAnalysisPt *dcalc_ap)
{
  float ceff_sum = 0.0;
  int ceff_count = 0;
  Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
  VertexInEdgeIterator edge_iter(to_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    TimingArcSetArcIterator arc_iter(arc_set);
    while (arc_iter.hasNext()) {
      TimingArc *arc = arc_iter.next();
      ceff_sum += graph_delay_calc_->ceff(edge, arc, dcalc_ap);
      ceff_count++;
    }
  }
  if (ceff_count == 0)
    return 0.0;
  else
    return ceff_sum / ceff_count;
}

void
Power::findLeakagePower(const Instance *inst,
			LibertyCell *cell,
			// Return values.
			PowerResult &result)
{
  float leakage = cell->leakagePower();
  LibertyCellLeakagePowerIterator pwr_iter(cell);
  while (pwr_iter.hasNext()) {
    LeakagePower *leak = pwr_iter.next();
    FuncExpr *when = leak->when();
    if (when) {
      LogicValue when_value = sim_->evalExpr(when, inst);
      switch (when_value) {
      case LogicValue::zero:
      case LogicValue::one:
	leakage = max(leakage, leak->power());
	break;
      case LogicValue::unknown:
      default:
	break;
      }
    }
  }
  result.setLeakage(leakage);
}

void
Power::findSwitchingPower(LibertyCell *cell,
			  const LibertyPort *to_port,
			  float activity,
			  float load_cap,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  PowerResult &result)
{
  float volt = voltage(cell, to_port, dcalc_ap);
  float switching = load_cap * volt * volt * activity / 2.0;
  result.setSwitching(switching);
}

void
Power::activity(const Pin *pin,
		// Return values.
		float &activity,
		bool &is_clk)
{
  const Clock *clk;
  findClk(pin, clk, is_clk);
  activity = 0.0;
  if (clk) {
    float period = clk->period();
    if (period > 0.0) {
      if (is_clk)
	activity = 2.0 / period;
      else
	activity = default_signal_toggle_rate_ * 2.0 / period;
    }
  }
}

float
Power::voltage(LibertyCell *cell,
	       const LibertyPort *,
	       const DcalcAnalysisPt *dcalc_ap)
{
  // Should use cell pg_pin voltage name to voltage.
  const Pvt *pvt = dcalc_ap->operatingConditions();
  if (pvt == nullptr)
    pvt = cell->libertyLibrary()->defaultOperatingConditions();
  if (pvt)
    return pvt->voltage();
  else
    return 0.0;
}

void
Power::findClk(const Pin *to_pin,
	       // Return values.
	       const Clock *&clk,
	       bool &is_clk)
{
  clk = nullptr;
  is_clk = false;
  Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
  VertexPathIterator path_iter(to_vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    const Clock *path_clk = path->clock(this);
    if (path_clk
	&& (clk == nullptr
	    || path_clk->period() < clk->period()))
      clk = path_clk;
    if (path->isClock(this))
      is_clk = true;
  }
}

////////////////////////////////////////////////////////////////

PowerResult::PowerResult() :
  internal_(0.0),
  switching_(0.0),
  leakage_(0.0)
{
}

void
PowerResult::clear() 
{
  internal_ = 0.0;
  switching_ = 0.0;
  leakage_ = 0.0;
}

float
PowerResult::total() const
{
  return   internal_ + switching_ + leakage_;
}

void
PowerResult::setInternal(float internal)
{
  internal_ = internal;
}

void
PowerResult::setLeakage(float leakage)
{
  leakage_ = leakage;
}

void
PowerResult::setSwitching(float switching)
{
  switching_ = switching;
}

void
PowerResult::incr(PowerResult &result)
{
  internal_ += result.internal_;
  switching_ += result.switching_;
  leakage_ += result.leakage_;
}

} // namespace
