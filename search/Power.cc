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

#include "Power.hh"

#include <algorithm> // max

#include "Debug.hh"
#include "EnumNameMap.hh"
#include "MinMax.hh"
#include "Units.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Sequential.hh"
#include "TimingArc.hh"
#include "FuncExpr.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Clock.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "Corner.hh"
#include "PathVertex.hh"
#include "Levelize.hh"
#include "Sim.hh"
#include "Search.hh"
#include "Bfs.hh"

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
//
// transition_density = activity / clock_period

namespace sta {

static bool
isPositiveUnate(const LibertyCell *cell,
		const LibertyPort *from,
		const LibertyPort *to);

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
      power(inst, cell, corner, inst_power);
      if (cell->isMacro()
	  || cell->isMemory())
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
      if (func) {
	Instance *inst = network_->instance(pin);
	PwrActivity activity = power_->evalActivity(func, inst);
	power_->setPinActivity(pin, activity);
	debugPrint3(debug_, "power_activity", 3, "set %s %.2e %.2f\n",
		    vertex->name(network_),
		    activity.activity(),
		    activity.duty());
      }
    }
  }
  bfs_->enqueueAdjacentVertices(vertex);
}

PwrActivity
Power::evalActivity(FuncExpr *expr,
		    const Instance *inst)
{
  return evalActivity(expr, inst, nullptr, true);
}

// Eval activity thru expr.
// With cofactor_port eval the positive/negative cofactor of expr wrt cofactor_port.
PwrActivity
Power::evalActivity(FuncExpr *expr,
		    const Instance *inst,
		    const LibertyPort *cofactor_port,
		    bool cofactor_positive)
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    LibertyPort *port = expr->port();
    if (port == cofactor_port) {
      if (cofactor_positive)
	return PwrActivity(0.0, 1.0, PwrActivityOrigin::constant);
      else
	return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
    }
    Pin *pin = network_->findPin(inst, port);
    if (pin)
      return findActivity(pin);
    else
      return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
  }
  case FuncExpr::op_not: {
    PwrActivity activity1 = evalActivity(expr->left(), inst,
					 cofactor_port, cofactor_positive);
    return PwrActivity(activity1.activity(),
		       1.0 - activity1.duty(),
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_or: {
    PwrActivity activity1 = evalActivity(expr->left(), inst,
					 cofactor_port, cofactor_positive);
    PwrActivity activity2 = evalActivity(expr->right(), inst,
					 cofactor_port, cofactor_positive);
    float p1 = 1.0 - activity1.duty();
    float p2 = 1.0 - activity2.duty();
    return PwrActivity(activity1.activity() * p2
		       + activity2.activity() * p1,
		       1.0 - p1 * p2,
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_and: {
    PwrActivity activity1 = evalActivity(expr->left(), inst,
					 cofactor_port, cofactor_positive);
    PwrActivity activity2 = evalActivity(expr->right(), inst,
					 cofactor_port, cofactor_positive);
    float p1 = activity1.duty();
    float p2 = activity2.duty();
    return PwrActivity(activity1.activity() * p2 + activity2.activity() * p1,
		       p1 * p2,
		       PwrActivityOrigin::propagated);
  }
  case FuncExpr::op_xor: {
    PwrActivity activity1 = evalActivity(expr->left(), inst,
					 cofactor_port, cofactor_positive);
    PwrActivity activity2 = evalActivity(expr->right(), inst,
					 cofactor_port, cofactor_positive);
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
    if (to_port->direction()->isAnyOutput()) {
      findSwitchingPower(cell, to_port, activity, load_cap,
			 dcalc_ap, result);
      findOutputInternalPower(to_pin, to_port, inst, cell, activity,
			      load_cap, dcalc_ap, result);
    }
    if (to_port->direction()->isAnyInput())
      findInputInternalPower(to_pin, to_port, inst, cell, activity,
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
Power::findInputInternalPower(const Pin *pin,
			      const LibertyPort *port,
			      const Instance *inst,
			      LibertyCell *cell,
			      PwrActivity &activity,
			      float load_cap,
			      const DcalcAnalysisPt *dcalc_ap,
			      // Return values.
			      PowerResult &result)
{
  auto internal_pwrs = cell->internalPowers(port);
  if (!internal_pwrs->empty()) {
    debugPrint3(debug_, "power", 2, "internal input %s/%s (%s)\n",
		network_->pathName(inst),
		port->name(),
		cell->name());
    const Pvt *pvt = dcalc_ap->operatingConditions();
    Vertex *vertex = graph_->pinLoadVertex(pin);
    debugPrint1(debug_, "power", 2, " cap = %s\n",
		units_->capacitanceUnit()->asString(load_cap));
    debugPrint0(debug_, "power", 2, "       when act/ns duty  energy    power\n");
    float internal = 0.0;
    for (InternalPower *pwr : *internal_pwrs) {
      const char *related_pg_pin = pwr->relatedPgPin();
      float energy = 0.0;
      int tr_count = 0;
      for (auto rf : RiseFall::range()) {
	float slew = delayAsFloat(graph_->slew(vertex, rf,
					       dcalc_ap->index()));
	if (!delayInf(slew)) {
	  float table_energy = pwr->power(rf, pvt, slew, load_cap);
	  energy += table_energy;
	  tr_count++;
	}
      }
      if (tr_count)
	energy /= tr_count; // average non-inf energies
      float duty = .5; // fallback default
      FuncExpr *when = pwr->when();
      if (when) {
	LibertyPort *out_port = findExprOutPort(when);
	if (out_port) {
	  FuncExpr *func = out_port->function();
	  if (func && func->hasPort(port))
	    duty = evalActivityDifference(func, inst, port).duty();
	  else
	    duty = evalActivity(when, inst).duty();
	}
	else
	  duty = evalActivity(when, inst).duty();
      }
      float port_internal = energy * duty * activity.activity();
      debugPrint7(debug_, "power", 2,  " %3s %6s  %.2f  %.2f %9.2e %9.2e %s\n",
		  port->name(),
		  when ? when->asString() : "",
		  activity.activity() * 1e-9,
		  duty,
		  energy,
		  port_internal,
		  related_pg_pin ? related_pg_pin : "no pg_pin");
      internal += port_internal;
    }
    result.internal() += internal;
  }
}

LibertyPort *
Power::findExprOutPort(FuncExpr *expr)
{
  LibertyPort *port;
  switch (expr->op()) {
  case FuncExpr::op_port:
    port = expr->port();
    if (port->direction()->isAnyOutput())
      return expr->port();
    return nullptr;
  case FuncExpr::op_not:
    port = findExprOutPort(expr->left());
    if (port)
      return port;
    return nullptr;
  case FuncExpr::op_or:
  case FuncExpr::op_and:
  case FuncExpr::op_xor:
    port = findExprOutPort(expr->left());
    if (port)
      return port;
    port = findExprOutPort(expr->right());
    if (port)
      return port;
    return nullptr;
  case FuncExpr::op_one:
  case FuncExpr::op_zero:
    return nullptr;
  }
  return nullptr;
}

// Eval activity of differenc(expr) wrt cofactor port.
PwrActivity
Power::evalActivityDifference(FuncExpr *expr,
			      const Instance *inst,
			      const LibertyPort *cofactor_port)
{
  // Activity of positive/negative cofactors.
  PwrActivity pos = evalActivity(expr, inst, cofactor_port, true);
  PwrActivity neg = evalActivity(expr, inst, cofactor_port, false);
  // difference = xor(pos, neg).
  float p1 = pos.duty() * (1.0 - neg.duty());
  float p2 = neg.duty() * (1.0 - pos.duty());
  return PwrActivity(pos.activity() * p1 + neg.activity() * p2,
		     p1 + p2,
		     PwrActivityOrigin::propagated);
}

void
Power::findOutputInternalPower(const Pin *to_pin,
			       const LibertyPort *to_port,
			       const Instance *inst,
			       LibertyCell *cell,
			       PwrActivity &to_activity,
			       float load_cap,
			       const DcalcAnalysisPt *dcalc_ap,
			       // Return values.
			       PowerResult &result)
{
  debugPrint3(debug_, "power", 2, "internal output %s/%s (%s)\n",
	      network_->pathName(inst),
	      to_port->name(),
	      cell->name());
  const Pvt *pvt = dcalc_ap->operatingConditions();
  debugPrint1(debug_, "power", 2, " cap = %s\n",
	      units_->capacitanceUnit()->asString(load_cap));
  FuncExpr *func = to_port->function();

  map<const char*, float, StringLessIf> pg_duty_sum;
  for (InternalPower *pwr : *cell->internalPowers(to_port)) {
    float duty = findInputDuty(to_pin, inst, func, pwr);
    const char *related_pg_pin = pwr->relatedPgPin();
    // Note related_pg_pin may be null.
    pg_duty_sum[related_pg_pin] += duty;
  }

  float internal = 0.0;
  for (InternalPower *pwr : *cell->internalPowers(to_port)) {
    FuncExpr *when = pwr->when();
    const char *related_pg_pin = pwr->relatedPgPin();
    float duty = findInputDuty(to_pin, inst, func, pwr);
    const LibertyPort *from_port = pwr->relatedPort();
    Vertex *from_vertex = nullptr;
    if (from_port) {
      const Pin *from_pin = network_->findPin(inst, from_port);
      if (from_pin)
	from_vertex = graph_->pinLoadVertex(from_pin);
    }
    float energy = 0.0;
    int tr_count = 0;
    debugPrint0(debug_, "power", 2, "             when act/ns duty  wgt   energy    power\n");
    for (auto to_rf : RiseFall::range()) {
      // Use unateness to find from_rf.
      RiseFall *from_rf = isPositiveUnate(cell, from_port, to_port)
	? to_rf
	: to_rf->opposite();
      float slew = from_vertex
	? delayAsFloat(graph_->slew(from_vertex, from_rf,
				    dcalc_ap->index()))
	: 0.0;
      if (!delayInf(slew)) {
	float table_energy = pwr->power(to_rf, pvt, slew, load_cap);
	energy += table_energy;
	tr_count++;
      }
    }
    if (tr_count)
      energy /= tr_count; // average non-inf energies
    auto duty_sum_iter = pg_duty_sum.find(related_pg_pin);
    float duty_sum = duty_sum_iter == pg_duty_sum.end()
      ? 0.0
      : duty_sum_iter->second;
    float weight = duty_sum == 0.0 ? duty : duty / duty_sum;
    float port_internal = weight * energy * to_activity.activity();
    debugPrint9(debug_, "power", 2,  "%3s -> %-3s %6s  %.2f %.2f %.2f %9.2e %9.2e %s\n",
		from_port->name(),
		to_port->name(),
		when ? when->asString() : "",
		to_activity.activity() * 1e-9,
		duty,
		weight,
		energy,
		port_internal,
		related_pg_pin ? related_pg_pin : "no pg_pin");
    internal += port_internal;
  }
  result.internal() += internal;
}

float
Power::findInputDuty(const Pin *to_pin,
		     const Instance *inst,
		     FuncExpr *func,
		     InternalPower *pwr)

{
  const char *related_pg_pin = pwr->relatedPgPin();
  const LibertyPort *from_port = pwr->relatedPort();
  if (from_port) {
    const Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin) {
      FuncExpr *when = pwr->when();
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      if (func && func->hasPort(from_port)) {
	PwrActivity from_activity = findActivity(from_pin);
	PwrActivity to_activity = findActivity(to_pin);
	float duty1 = evalActivityDifference(func, inst, from_port).duty();
	if (to_activity.activity() == 0.0)
	  return 0.0;
	else
	  return from_activity.activity() / to_activity.activity() * duty1;
      }
      else if (when)
	return evalActivity(when, inst).duty();
      else if (search_->isClock(from_vertex))
	return 1.0;
      return 0.5;
    }
  }
  return 0.0;
}

static bool
isPositiveUnate(const LibertyCell *cell,
		const LibertyPort *from,
		const LibertyPort *to)
{
  TimingArcSetSeq *arc_sets = cell->timingArcSets(from, to);
  if (arc_sets && !arc_sets->empty()) {
    TimingSense sense = (*arc_sets)[0]->sense();
    return sense == TimingSense::positive_unate
      || sense == TimingSense::non_unate;
  }
  // default
  return true;
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

////////////////////////////////////////////////////////////////

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
  for (LeakagePower *leak : *cell->leakagePowers()) {
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
  result.leakage() += leakage;
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
  result.switching() += switching;
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
