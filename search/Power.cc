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

static int
funcExprPortCount(FuncExpr *expr);

Power::Power(Sta *sta) :
  StaState(sta),
  global_activity_{0.0, 0.0, PwrActivityOrigin::unknown},
  input_activity_{0.1, 0.5, PwrActivityOrigin::input},
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

bool
Power::hasPinActivity(const Pin *pin)
{
  return activity_map_.hasKey(pin);
}

void
Power::setPinActivity(const Pin *pin,
		      PwrActivity &activity)
{
  activity_map_[pin] = activity;
  activities_valid_ = false;
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
  ~PropActivityVisitor();
  virtual VertexVisitor *copy();
  virtual void visit(Vertex *vertex);
  void init();
  bool foundRegWithoutActivity() const;
  InstanceSet *stealVisitedRegs();

private:
  InstanceSet *visited_regs_;
  bool found_reg_without_activity_;
  Power *power_;
  BfsFwdIterator *bfs_;
};

PropActivityVisitor::PropActivityVisitor(Power *power,
					 BfsFwdIterator *bfs) :
  StaState(power),
  visited_regs_(nullptr),
  power_(power),
  bfs_(bfs)
{
}

PropActivityVisitor::~PropActivityVisitor()
{
  delete visited_regs_;
}

VertexVisitor *
PropActivityVisitor::copy()
{
  return new PropActivityVisitor(power_, bfs_);
}

void
PropActivityVisitor::init()
{
  visited_regs_ = new InstanceSet;
  found_reg_without_activity_ = false;
}

InstanceSet *
PropActivityVisitor::stealVisitedRegs()
{
  InstanceSet *visited_regs = visited_regs_;
  visited_regs_ = nullptr;
  return visited_regs;
}

bool
PropActivityVisitor::foundRegWithoutActivity() const
{
  return found_reg_without_activity_;
}

void
PropActivityVisitor::visit(Vertex *vertex)
{
  auto pin = vertex->pin();
  debugPrint1(debug_, "power_activity", 3, "visit %s\n",
	      vertex->name(network_));
  bool input_without_activity = false;
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
	if (!power_->hasPinActivity(pin))
	  input_without_activity = true;
	power_->setPinActivity(pin, to_activity);
      }
    }
    Instance *inst = network_->instance(pin);
    auto cell = network_->libertyCell(inst);
    if (cell && cell->hasSequentials()) {
      debugPrint1(debug_, "power_activity", 3, "pending reg %s\n",
		  network_->pathName(inst));
      visited_regs_->insert(inst);
      found_reg_without_activity_ = input_without_activity;
    }
  }
  if (network_->isDriver(pin)) {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      FuncExpr *func = port->function();
      Instance *inst = network_->instance(pin);
      PwrActivity activity = power_->evalActivity(func, inst);
      power_->setPinActivity(pin, activity);
      debugPrint3(debug_, "power_activity", 3, "set %s %.2e %.2f\n",
		  vertex->name(network_),
		  activity.activity(),
		  activity.duty());
    }
  }
  bfs_->enqueueAdjacentVertices(vertex);
}

PwrActivity
Power::evalActivity(FuncExpr *expr,
		    const Instance *inst)
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    Pin *pin = network_->findPin(inst, expr->port()->name());
    if (pin)
      return findActivity(pin);
    else
      return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
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
      visitor.init();
      bfs.visit(levelize_->maxLevel(), &visitor);
      while (visitor.foundRegWithoutActivity()) {
	InstanceSet *regs = visitor.stealVisitedRegs();
	InstanceSet::Iterator reg_iter(regs);
	while (reg_iter.hasNext()) {
	  auto reg = reg_iter.next();
	  // Propagate activiities across register D->Q.
	  seedRegOutputActivities(reg, bfs);
	}
	delete regs;
	visitor.init();
	bfs.visit(levelize_->maxLevel(), &visitor);
      }
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
    if (!sdc_->isLeafPinClock(pin)
	&& network_->direction(pin) != PortDirection::internal()) {
      debugPrint1(debug_, "power_activity", 3, "seed %s\n",
		  vertex->name(network_));
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
Power::seedRegOutputActivities(const Instance *inst,
			       BfsFwdIterator &bfs)
{
  auto cell = network_->libertyCell(inst);
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
	    || func->port() == seq->outputInv()) {
	  debugPrint1(debug_, "power_activity", 3, "enqueue reg output %s\n",
		      vertex->name(network_));
	  bfs.enqueue(vertex);
	}
      }
    }
    delete pin_iter;
  }
}

void
Power::seedRegOutputActivities(const Instance *reg,
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
      ? graph_delay_calc_->loadCap(to_pin, dcalc_ap)
      : 0.0;
    PwrActivity activity = findClkedActivity(to_pin, inst_clk);
    if (to_port->direction()->isAnyOutput())
      findSwitchingPower(cell, to_port, activity, load_cap,
			 dcalc_ap, result);
    findInternalPower(to_pin, to_port, inst, cell, activity,
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
Power::findInternalPower(const Pin *to_pin,
			 const LibertyPort *to_port,
			 const Instance *inst,
			 LibertyCell *cell,
			 PwrActivity &to_activity,
			 float load_cap,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 PowerResult &result)
{
  debugPrint3(debug_, "power", 2, "internal %s/%s (%s)\n",
	      network_->pathName(inst),
	      to_port->name(),
	      cell->name());
  const Pvt *pvt = dcalc_ap->operatingConditions();
  debugPrint1(debug_, "power", 2, " cap = %s\n",
	      units_->capacitanceUnit()->asString(load_cap));
  debugPrint0(debug_, "power", 2, "       when act/ns duty  energy    power\n");
  float internal = 0.0;
  LibertyCellInternalPowerIterator pwr_iter(cell);
  while (pwr_iter.hasNext()) {
    InternalPower *pwr = pwr_iter.next();
    if (pwr->port() == to_port) {
      const char *related_pg_pin = pwr->relatedPgPin();
      const LibertyPort *from_port = pwr->relatedPort();
      FuncExpr *when = pwr->when();
      FuncExpr *infered_when = nullptr;
      if (from_port) {
	if (when == nullptr) {
	  FuncExpr *func = to_port->function();
	  if (func)
	    infered_when = inferedWhen(func, from_port);
	}
      }
      else
	from_port = to_port;
      // If all the "when" clauses exist VSS internal power is ignored.
      const Pin *from_pin = network_->findPin(inst, from_port);
      if (from_pin
	  && ((when && internalPowerMissingWhen(cell, to_port, related_pg_pin))
	      || pgNameVoltage(cell, related_pg_pin, dcalc_ap) != 0.0)) {
	Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
	float duty;
	if (infered_when) {
	  PwrActivity from_activity = findActivity(from_pin);
	  PwrActivity to_activity = findActivity(to_pin);
	  float duty1 = evalActivity(infered_when, inst).duty();
	  if (to_activity.activity() == 0.0)
	    duty = 0.0;
	  else
	    duty = from_activity.activity() / to_activity.activity() * duty1;
	}
	else if (when)
	  duty = evalActivity(when, inst).duty();
	else if (search_->isClock(from_vertex))
	  duty = 1.0;
	else
	  duty = 0.5;
	float port_energy = 0.0;
	for (auto to_rf : RiseFall::range()) {
	  // Should use unateness to find from_rf.
	  RiseFall *from_rf = to_rf;
	  float slew = delayAsFloat(graph_->slew(from_vertex,
						 from_rf,
						 dcalc_ap->index()));
	  if (!fuzzyInf(slew)) {
	    float table_energy = pwr->power(to_rf, pvt, slew, load_cap);
	    float tr_energy = table_energy * duty;
	    debugPrint4(debug_, "power", 3,  " %s energy = %9.2e * %.2f = %9.2e\n",
			to_rf->shortName(),
			table_energy,
			duty,
			tr_energy);
	    port_energy += tr_energy;
	  }
	}
	float port_internal = port_energy * to_activity.activity();
	debugPrint8(debug_, "power", 2,  " %s -> %s %s  %.2f %.2f %9.2e %9.2e %s\n",
		    from_port->name(),
		    to_port->name(),
		    when ? when->asString() : (infered_when ? infered_when->asString() : "    "),
		    to_activity.activity() * 1e-9,
		    duty,
		    port_energy,
		    port_internal,
		    related_pg_pin ? related_pg_pin : "no pg_pin");
	internal += port_internal;
      }
      if (infered_when)
	infered_when->deleteSubexprs();
    }
  }
  result.setInternal(result.internal() + internal);
}

////////////////////////////////////////////////////////////////

static bool
isPortRef(FuncExpr *expr,
	  const LibertyPort *port)
{
  return (expr->op() == FuncExpr::op_port
	  && expr->port() == port)
    || (expr->op() == FuncExpr::op_not
	&& expr->left()->op() == FuncExpr::op_port
	&& expr->left()->port() == port);
}

static FuncExpr *
negate(FuncExpr *expr)
{
  if (expr->op() == FuncExpr::op_not)
    return expr->left()->copy();
  else
    return FuncExpr::makeNot(expr->copy());
}

FuncExpr *
Power::inferedWhen(FuncExpr *expr,
		   const LibertyPort *from_port)
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    if (expr->port() == from_port)
      return FuncExpr::makeOne();
    else
      return nullptr;
  }
  case FuncExpr::op_not:
    return inferedWhen(expr->left(), from_port);
  case FuncExpr::op_or:
  case FuncExpr::op_xor: {
    if (isPortRef(expr->left(), from_port))
      return negate(expr->right());
    if (isPortRef(expr->right(), from_port))
      return negate(expr->left());
    break;
  }
  case FuncExpr::op_and: {
    if (isPortRef(expr->left(), from_port))
      return expr->right()->copy();
    if (isPortRef(expr->right(), from_port))
      return expr->left()->copy();
    break;
  }
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
    break;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

// Return true if some a "when" clause for the internal power to_port
// is missing.
bool
Power::internalPowerMissingWhen(LibertyCell *cell,
				const LibertyPort *to_port,
				const char *related_pg_pin)
{
  int when_input_count = 0;
  int when_count = 0;
  LibertyCellInternalPowerIterator pwr_iter(cell);
  while (pwr_iter.hasNext()) {
    auto pwr = pwr_iter.next();
    auto when = pwr->when();
    if (pwr->port() == to_port
	&& pwr->relatedPort() == nullptr
	&& stringEqIf(pwr->relatedPgPin(), related_pg_pin)
	&& when) {
      when_count++;
      when_input_count = funcExprPortCount(when);
    }
  }
  return when_count != (1 << when_input_count);
}

static int
funcExprPortCount(FuncExpr *expr)
{
  int port_count = 0;
  FuncExprPortIterator port_iter(expr);
  while (port_iter.hasNext()) {
    port_iter.next();
    port_count++;
  }
  return port_count;
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
      float duty = 1.0;
      while (port_iter.hasNext()) {
	auto port = port_iter.next();
	if (port->direction()->isAnyInput())
	  duty *= port->isClock() ? 0.25 : 0.5;
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
  float cell_leakage;
  bool cell_leakage_exists;
  cell->leakagePower(cell_leakage, cell_leakage_exists);
  if (cell_leakage_exists)
    debugPrint2(debug_, "power", 2, "leakage cell %s %.3e\n",
		cell->name(),
		cell_leakage);
  // Ignore default leakages unless there are no conditional leakage groups.
  if (found_cond)
    leakage = cond_leakage;
  else if (found_default)
    leakage = default_leakage;
  else if (cell_leakage_exists)
    leakage = cell_leakage;
  debugPrint2(debug_, "power", 2, "leakage cell %s %.3e\n",
	      cell->name(),
	      leakage);
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
  float volt = portVoltage(cell, to_port, dcalc_ap);
  float switching = .5 * load_cap * volt * volt * activity.activity();
  debugPrint5(debug_, "power", 2, "switching %s/%s activity = %.2e volt = %.2f %.3e\n",
	      cell->name(),
	      to_port->name(),
	      activity.activity(),
	      volt,
	      switching);
  result.setSwitching(result.switching() + switching);
}

PwrActivity
Power::findClkedActivity(const Pin *pin)
{
  const Instance *inst = network_->instance(pin);
  const Clock *inst_clk = findInstClk(inst);
  return findClkedActivity(pin, inst_clk);
}

PwrActivity
Power::findClkedActivity(const Pin *pin,
			 const Clock *inst_clk)
{
  const Clock *clk = findClk(pin);
  if (clk == nullptr)
    clk = inst_clk;
  if (clk) {
    float period = clk->period();
    if (period > 0.0) {
      PwrActivity activity = findActivity(pin);
      return PwrActivity(activity.activity() / period,
			 activity.duty(),
			 activity.origin());
    }
  }
  // gotta find a clock someplace...
  return PwrActivity(input_activity_.activity(),
		     input_activity_.duty(),
		     PwrActivityOrigin::defaulted);
}

PwrActivity
Power::findActivity(const Pin *pin)
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (search_->isClock(vertex))
    return PwrActivity(2.0, 0.5, PwrActivityOrigin::clock);
  else if (global_activity_.isSet())
    return global_activity_;
  else {
    if (activity_map_.hasKey(pin)) {
      PwrActivity &activity = activity_map_[pin];
      if (activity.origin() != PwrActivityOrigin::unknown)
	return activity;
    }
  }
  return input_activity_;
}

float
Power::portVoltage(LibertyCell *cell,
		   const LibertyPort *port,
		   const DcalcAnalysisPt *dcalc_ap)
{
  return pgNameVoltage(cell, port->relatedPowerPin(), dcalc_ap);
}

float
Power::pgNameVoltage(LibertyCell *cell,
		     const char *pg_port_name,
		     const DcalcAnalysisPt *dcalc_ap)
{
  if (pg_port_name) {
    auto pg_port = cell->findPgPort(pg_port_name);
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
