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
#include "EnumNameMap.hh"
#include "Units.hh"
#include "Transition.hh"
#include "MinMax.hh"
#include "Clock.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Sequential.hh"
#include "TimingArc.hh"
#include "FuncExpr.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "PathVertex.hh"
#include "Levelize.hh"
#include "Sim.hh"
#include "Search.hh"
#include "Bfs.hh"
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
  global_activity_{0.0, 0.0, PwrActivityOrigin::unknown},
  input_activity_{0.1, 0.5, PwrActivityOrigin::input},
  default_activity_(.1),
  activities_valid_(false)
{
}

void
Power::setGlobalActivity(float activity,
			 float duty)
{
  global_activity_.set(activity, duty, PwrActivityOrigin::global);
  activities_valid_ = false;
}
  
void
Power::setInputActivity(float activity,
			float duty)
{
  input_activity_.set(activity, duty, PwrActivityOrigin::input);
  activities_valid_ = false;
}

void
Power::setInputPortActivity(const Port *input_port,
			    float activity,
		 	    float duty)
{
  Instance *top_inst = network_->topInstance();
  const Pin *pin = network_->findPin(top_inst, input_port);
  if (pin) {
    activity_map_[pin] = {activity, duty, PwrActivityOrigin::user};
    activities_valid_ = false;
  }
}

PwrActivity &
Power::pinActivity(const Pin *pin)
{
  return activity_map_[pin];
}

void
Power::setPinActivity(const Pin *pin,
		      float activity,
		      float duty,
		      PwrActivityOrigin origin)
{
  activity_map_[pin] = {activity, duty, origin};
  activities_valid_ = false;
}

void
Power::setPinActivity(const Pin *pin,
		      PwrActivity &activity)
{
  activity_map_[pin] = activity;
  activities_valid_ = false;
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

  preamble();
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

void
Power::power(const Instance *inst,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result)
{
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    preamble();
    power(inst, cell, corner, result);
  }
}

////////////////////////////////////////////////////////////////

class ActivitySrchPred : public SearchPred
{
public:
  explicit ActivitySrchPred(const StaState *sta);
  virtual bool searchFrom(const Vertex *from_vertex);
  virtual bool searchThru(Edge *edge);
  virtual bool searchTo(const Vertex *);

protected:
  const StaState *sta_;
};

ActivitySrchPred::ActivitySrchPred(const StaState *sta) :
  sta_(sta)
{
}

bool
ActivitySrchPred::searchFrom(const Vertex *)
{
  return true;
}

bool
ActivitySrchPred::searchThru(Edge *edge)
{
  auto role = edge->role();
  return !(edge->isDisabledLoop()
	   || role->isTimingCheck()
	   || role == TimingRole::regClkToQ());
}

bool
ActivitySrchPred::searchTo(const Vertex *)
{
  return true;
}

////////////////////////////////////////////////////////////////

class PropActivityVisitor : public VertexVisitor, StaState
{
public:
  PropActivityVisitor(Power *power,
		      BfsFwdIterator *bfs);
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);

private:
  Power *power_;
  BfsFwdIterator *bfs_;
};

PropActivityVisitor::PropActivityVisitor(Power *power,
					 BfsFwdIterator *bfs) :
  StaState(power),
  power_(power),
  bfs_(bfs)
{
}

VertexVisitor *
PropActivityVisitor::copy()
{
  return new PropActivityVisitor(power_, bfs_);
}

PwrActivity
Power::evalActivity(FuncExpr *expr,
		    const Instance *inst)
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    Pin *pin = network_->findPin(inst, expr->port()->name());
    if (pin)
      return pinActivity(pin);
  }
  case FuncExpr::op_not: {
    PwrActivity activity1 = evalActivity(expr->left(), inst);
    return PwrActivity(activity1.activity(),
		       1.0 - activity1.duty(),
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_or: {
    PwrActivity activity1 = evalActivity(expr->left(), inst);
    PwrActivity activity2 = evalActivity(expr->right(), inst);
    float p1 = 1.0 - activity1.duty();
    float p2 = 1.0 - activity2.duty();
    return PwrActivity(activity1.activity() * p2
		       + activity2.activity() * p1,
		       1.0 - p1 * p2,
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_and: {
    PwrActivity activity1 = evalActivity(expr->left(), inst);
    PwrActivity activity2 = evalActivity(expr->right(), inst);
    float p1 = activity1.duty();
    float p2 = activity2.duty();
    return PwrActivity(activity1.activity() * p2 + activity2.activity() * p1,
		       p1 * p2,
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_xor: {
    PwrActivity activity1 = evalActivity(expr->left(), inst);
    PwrActivity activity2 = evalActivity(expr->right(), inst);
    float p1 = activity1.duty() * (1.0 - activity2.duty());
    float p2 = activity2.duty() * (1.0 - activity1.duty());
    return PwrActivity(activity1.activity() * p1 + activity2.activity() * p2,
		       p1 + p2,
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_one:
    return PwrActivity(0.0, 1.0, PwrActivityOrigin::constant);
  case FuncExpr::op_zero:
    return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
  }
  return PwrActivity();
}

void
PropActivityVisitor::visit(Vertex *vertex)
{
  auto pin = vertex->pin();
  debugPrint1(debug_, "power", 3, "activity %s\n",
	      vertex->name(network_));
  if (network_->isLoad(pin)) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    if (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()) {
	Vertex *from_vertex = edge->from(graph_);
	const Pin *from_pin = from_vertex->pin();
	PwrActivity &from_activity = power_->pinActivity(from_pin);
	PwrActivity to_activity(from_activity.activity(),
				from_activity.duty(),
				PwrActivityOrigin::propagated);
	power_->setPinActivity(pin, to_activity);
      }
    }
  }
  if (network_->isDriver(pin)) {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      FuncExpr *func = port->function();
      Instance *inst = network_->instance(pin);
      PwrActivity activity = power_->evalActivity(func, inst);
      power_->setPinActivity(pin, activity);
    }
  }
  bfs_->enqueueAdjacentVertices(vertex);
}

////////////////////////////////////////////////////////////////

void
Power::preamble()
{
  ensureActivities();
}

void
Power::ensureActivities()
{
  if (!global_activity_.isSet()) {
    if (!activities_valid_) {
      ActivitySrchPred activity_srch_pred(this);
      BfsFwdIterator bfs(BfsIndex::other, &activity_srch_pred, this);
      seedActivities(bfs);
      PropActivityVisitor visitor(this, &bfs);
      bfs.visit(levelize_->maxLevel(), &visitor);
      // Propagate activiities across register D->Q.
      seedRegOutputActivities(bfs);
      bfs.visit(levelize_->maxLevel(), &visitor);
      activities_valid_ = true;
    }
  }
}

void
Power::seedActivities(BfsFwdIterator &bfs)
{
  for (auto vertex : levelize_->roots()) {
    const Pin *pin = vertex->pin();
    // Clock activities are baked in.
    if (!sdc_->isVertexPinClock(pin)) {
      PwrActivity &activity = pinActivity(pin);
      PwrActivityOrigin origin = activity.origin();
      // Default inputs without explicit activities to the input default.
      if (origin != PwrActivityOrigin::user)
	setPinActivity(pin, input_activity_);
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      bfs.enqueueAdjacentVertices(vertex);
    }
  }
}

void
Power::seedRegOutputActivities(BfsFwdIterator &bfs)
{
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    auto inst = leaf_iter->next();
    auto cell = network_->libertyCell(inst);
    if (cell) {
      LibertyCellSequentialIterator seq_iter(cell);
      while (seq_iter.hasNext()) {
	Sequential *seq = seq_iter.next();
	seedRegOutputActivities(inst, seq, seq->output(), false);
	seedRegOutputActivities(inst, seq, seq->outputInv(), true);
	// Enqueue register output pins with functions that reference
	// the sequential internal pins (IQ, IQN).
	InstancePinIterator *pin_iter = network_->pinIterator(inst);
	while (pin_iter->hasNext()) {
	  auto pin = pin_iter->next();
	  auto port = network_->libertyPort(pin);
	  auto func = port->function();
	  if (func) {
	    Vertex *vertex = graph_->pinDrvrVertex(pin);
	    if (func->port() == seq->output()
		|| func->port() == seq->outputInv())
	      bfs.enqueue(vertex);
	  }
	}
	delete pin_iter;
      }
    }
  }
  delete leaf_iter;
}

void
Power::seedRegOutputActivities(Instance *reg,
			       Sequential *seq,
			       LibertyPort *output,
			       bool invert)
{
  const Pin *pin = network_->findPin(reg, output);
  if (pin) {
    PwrActivity activity = evalActivity(seq->data(), reg);
    if (invert)
      activity.set(activity.activity(),
		   1.0 - activity.duty(),
		   activity.origin());
    setPinActivity(pin, activity);
  }
}

////////////////////////////////////////////////////////////////

void
Power::power(const Instance *inst,
	     LibertyCell *cell,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result)
{
  MinMax *mm = MinMax::max();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(mm);
  const Clock *inst_clk = findInstClk(inst);
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    const LibertyPort *to_port = network_->libertyPort(to_pin);
    float load_cap = to_port->direction()->isAnyOutput()
      ? graph_delay_calc_->loadCap(to_pin, TransRiseFall::rise(), dcalc_ap)
      : 0.0;
    PwrActivity activity = findActivity(to_pin, inst_clk);
    if (to_port->direction()->isAnyOutput())
      findSwitchingPower(cell, to_port, activity, load_cap,
			 dcalc_ap, result);
    findInternalPower(inst, cell, to_port, activity,
		      load_cap, dcalc_ap, result);
  }
  delete pin_iter;
  findLeakagePower(inst, cell, result);
}

const Clock *
Power::findInstClk(const Instance *inst)
{
  const Clock *inst_clk = nullptr;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const Clock *clk = findClk(pin);
    if (clk)
      inst_clk = clk;
  }
  delete pin_iter;
  return inst_clk;
}

void
Power::findInternalPower(const Instance *inst,
			 LibertyCell *cell,
			 const LibertyPort *to_port,
			 PwrActivity &activity,
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
  debugPrint1(debug_, "power", 2, " cap = %s\n",
	      units_->capacitanceUnit()->asString(load_cap));
  debugPrint0(debug_, "power", 2, "       when act/ns duty  energy    power\n");
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
      FuncExpr *when = pwr->when();
      PwrActivity when_activity = when ? evalActivity(when, inst)
	: findActivity(from_pin);
      float duty = when_activity.duty();
      float port_energy = 0.0;
      float port_internal = 0.0;
      TransRiseFallIterator tr_iter;
      while (tr_iter.hasNext()) {
	TransRiseFall *to_tr = tr_iter.next();
	// Should use unateness to find from_tr.
	TransRiseFall *from_tr = to_tr;
	float slew = delayAsFloat(graph_->slew(from_vertex,
					       from_tr,
					       dcalc_ap->index()));
	float tr_energy = (from_port)
	  ? pwr->power(to_tr, pvt, slew, load_cap)
	  : pwr->power(to_tr, pvt, 0.0, 0.0);
	float tr_internal = tr_energy * activity.activity();
	port_energy += tr_energy;
	port_internal += tr_internal;
      }
      debugPrint8(debug_, "power", 2,  " %s -> %s %s  %.2f %.2f %9.2e %9.2e %s\n",
		  from_port->name(),
		  to_port->name(),
		  pwr->when() ? pwr->when()->asString() : "    ",
		  activity.activity() * 1e-9,
		  duty,
		  port_energy,
		  port_internal,
		  related_pg_pin ? related_pg_pin : "no pg_pin");

      // Sum/count internal power arcs by supply to average across conditions.
      SumCount &supply_sum_count = supply_sum_counts[related_pg_pin];
      // Average rise/fall internal power.
      supply_sum_count.first += port_internal / 2.0;
      supply_sum_count.second++;
    }
  }
  float internal = 0.0;
  for (auto supply_sum_count : supply_sum_counts) {
    SumCount sum_count = supply_sum_count.second;
    float supply_internal = sum_count.first;
    int supply_count = sum_count.second;
    internal += supply_internal / (supply_count > 0 ? supply_count : 1);
  }

  debugPrint2(debug_, "power", 2, " %s internal = %.3e\n",
	      to_port->name(),
	      internal);
  result.setInternal(result.internal() + internal);
}

void
Power::findLeakagePower(const Instance *,
			LibertyCell *cell,
			// Return values.
			PowerResult &result)
{
  float cond_leakage = 0.0;
  bool found_cond = false;
  float default_leakage = 0.0;
  bool found_default = false;
  LibertyCellLeakagePowerIterator pwr_iter(cell);
  while (pwr_iter.hasNext()) {
    LeakagePower *leak = pwr_iter.next();
    FuncExpr *when = leak->when();
    if (when) {
      FuncExprPortIterator port_iter(when);
      float duty = 2.0;
      while (port_iter.hasNext()) {
	auto port = port_iter.next();
	if (port->direction()->isAnyInput())
	  duty *= .5;
      }
      debugPrint4(debug_, "power", 2, "leakage %s %s %.3e * %.2f\n",
		  cell->name(),
		  when->asString(),
		  leak->power(),
		  duty);
      cond_leakage += leak->power() * duty;
      found_cond = true;
    }
    else {
      debugPrint2(debug_, "power", 2, "leakage default %s %.3e\n",
		  cell->name(),
		  leak->power());
      default_leakage += leak->power();
      found_default = true;
    }
  }
  float leakage = 0.0;
  float leak;
  bool exists;
  cell->leakagePower(leak, exists);
  if (exists) {
    // Prefer cell_leakage_power until propagated activities exist.
    debugPrint2(debug_, "power", 2, "leakage cell %s %.3e\n",
		  cell->name(),
		  leak);
      leakage = leak;
  }
  // Ignore default leakages unless there are no conditional leakage groups.
  else if (found_cond)
    leakage = cond_leakage;
  else if (found_default)
    leakage = default_leakage;
  result.setLeakage(result.leakage() + leakage);
}

void
Power::findSwitchingPower(LibertyCell *cell,
			  const LibertyPort *to_port,
			  PwrActivity &activity,
			  float load_cap,
			  const DcalcAnalysisPt *dcalc_ap,
			  // Return values.
			  PowerResult &result)
{
  float volt = voltage(cell, to_port, dcalc_ap);
  float switching = .5 * load_cap * volt * volt * activity.activity();
  debugPrint5(debug_, "power", 2, "switching %s/%s activity = %.2e volt = %.2f %.3e\n",
	      cell->name(),
	      to_port->name(),
	      activity.activity(),
	      volt,
	      switching);
  volt = voltage(cell, to_port, dcalc_ap);
  result.setSwitching(result.switching() + switching);
}

PwrActivity
Power::findActivity(const Pin *pin)
{
  const Instance *inst = network_->instance(pin);
  const Clock *inst_clk = findInstClk(inst);
  return findActivity(pin, inst_clk);
}

PwrActivity
Power::findActivity(const Pin *pin,
		    const Clock *inst_clk)
{
  const Clock *clk = findClk(pin);
  if (clk == nullptr)
    clk = inst_clk;
  if (clk) {
    float period = clk->period();
    if (period > 0.0) {
      Vertex *vertex = graph_->pinLoadVertex(pin);
      if (search_->isClock(vertex))
	return PwrActivity(2.0 / period, 0.5, PwrActivityOrigin::clock);
      else if (global_activity_.isSet())
	return PwrActivity(global_activity_.activity() / period,
			   0.5, PwrActivityOrigin::global);
      else {
	if (activity_map_.hasKey(pin)) {
	  PwrActivity &activity = activity_map_[pin];
	  if (activity.origin() != PwrActivityOrigin::unknown)
	    return PwrActivity(activity.activity() / period,
			       activity.duty(),
			       activity.origin());
	}
	return PwrActivity(default_activity_ / period,
			   0.5, PwrActivityOrigin::defaulted);
      }
    }
  }
  return PwrActivity();
}

float
Power::voltage(LibertyCell *cell,
	       const LibertyPort *port,
	       const DcalcAnalysisPt *dcalc_ap)
{
  auto power_pin = port->relatedPowerPin();
  if (power_pin) {
    auto pg_port = cell->findPgPort(power_pin);
    if (pg_port) {
      auto volt_name = pg_port->voltageName();
      auto library = cell->libertyLibrary();
      float voltage;
      bool exists;
      library->supplyVoltage(volt_name, voltage, exists);
      if (exists)
	return voltage;
    }
  }

  const Pvt *pvt = dcalc_ap->operatingConditions();
  if (pvt == nullptr)
    pvt = cell->libertyLibrary()->defaultOperatingConditions();
  if (pvt)
    return pvt->voltage();
  else
    return 0.0;
}

const Clock *
Power::findClk(const Pin *to_pin)
{
  const Clock *clk = nullptr;
  Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
  VertexPathIterator path_iter(to_vertex, this);
  while (path_iter.hasNext()) {
    PathVertex *path = path_iter.next();
    const Clock *path_clk = path->clock(this);
    if (path_clk
	&& (clk == nullptr
	    || path_clk->period() < clk->period()))
      clk = path_clk;
  }
  return clk;
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
  return internal_ + switching_ + leakage_;
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

////////////////////////////////////////////////////////////////

static EnumNameMap<PwrActivityOrigin> pwr_activity_origin_map =
  {{PwrActivityOrigin::global, "global"},
   {PwrActivityOrigin::input, "input"},
   {PwrActivityOrigin::user, "user"},
   {PwrActivityOrigin::propagated, "propagated"},
   {PwrActivityOrigin::clock, "clock"},
   {PwrActivityOrigin::constant, "constant"},
   {PwrActivityOrigin::defaulted, "defaulted"},
   {PwrActivityOrigin::unknown, "unknown"}};

PwrActivity::PwrActivity(float activity,
			 float duty,
			 PwrActivityOrigin origin) :
  activity_(activity),
  duty_(duty),
  origin_(origin)
{
}

PwrActivity::PwrActivity() :
  activity_(0.0),
  duty_(0.0),
  origin_(PwrActivityOrigin::unknown)
{
}

void
PwrActivity::set(float activity,
		 float duty,
		 PwrActivityOrigin origin)
{
  activity_ = activity;
  duty_ = duty;
  origin_ = origin;
}

bool
PwrActivity::isSet() const
{
  return origin_ != PwrActivityOrigin::unknown;
}

const char *
PwrActivity::originName() const
{
  return pwr_activity_origin_map.find(origin_);
}

} // namespace
