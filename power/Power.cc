// OpenSTA, Static Timing Analyzer
// Copyright (c) 2022, Parallax Software, Inc.
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

#include "Power.hh"

#include <algorithm> // max
#include <cmath>     // aps

#include "Debug.hh"
#include "EnumNameMap.hh"
#include "Hash.hh"
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
#include "search/Levelize.hh"
#include "search/Sim.hh"
#include "Search.hh"
#include "Bfs.hh"
#include "ClkNetwork.hh"

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

using std::abs;
using std::isnormal;

static bool
isPositiveUnate(const LibertyCell *cell,
		const LibertyPort *from,
		const LibertyPort *to);

Power::Power(StaState *sta) :
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
    user_activity_map_[pin] = {activity, duty, PwrActivityOrigin::user};
    activities_valid_ = false;
  }
}

void
Power::setUserActivity(const Pin *pin,
		      float activity,
		      float duty,
		      PwrActivityOrigin origin)
{
  user_activity_map_[pin] = {activity, duty, origin};
  activities_valid_ = false;
}

PwrActivity &
Power::userActivity(const Pin *pin)
{
  return user_activity_map_[pin];
}

bool
Power::hasUserActivity(const Pin *pin)
{
  return user_activity_map_.hasKey(pin);
}

void
Power::setActivity(const Pin *pin,
		   PwrActivity &activity)
{
  activity_map_[pin] = activity;
}

PwrActivity &
Power::activity(const Pin *pin)
{
  return activity_map_[pin];
}

bool
Power::hasActivity(const Pin *pin)
{
  return activity_map_.hasKey(pin);
}

// Sequential internal pins may not be in the netlist so their
// activities are stored by instance/liberty_port pairs.
void
Power::setSeqActivity(const Instance *reg,
		      LibertyPort *output,
		      PwrActivity &activity)
{
  seq_activity_map_[SeqPin(reg, output)] = activity;
  activities_valid_ = false;
}

bool
Power::hasSeqActivity(const Instance *reg,
		      LibertyPort *output)
{
  return seq_activity_map_.hasKey(SeqPin(reg, output));
}

PwrActivity
Power::seqActivity(const Instance *reg,
		   LibertyPort *output)
{
  return seq_activity_map_[SeqPin(reg, output)];
}

size_t
SeqPinHash::operator()(const SeqPin &pin) const
{
  return hashSum(hashPtr(pin.first), hashPtr(pin.second));
}

bool
SeqPinEqual::operator()(const SeqPin &pin1,
			const SeqPin &pin2) const
{
  return pin1.first == pin2.first
    && pin1.second == pin2.second;
}

////////////////////////////////////////////////////////////////

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

  ensureActivities();
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
    ensureActivities();
    power(inst, cell, corner, result);
  }
}

////////////////////////////////////////////////////////////////

class ActivitySrchPred : public SearchPredNonLatch2
{
public:
  explicit ActivitySrchPred(const StaState *sta);
  virtual bool searchThru(Edge *edge);
};

ActivitySrchPred::ActivitySrchPred(const StaState *sta) :
  SearchPredNonLatch2(sta)
{
}

bool
ActivitySrchPred::searchThru(Edge *edge)
{
  TimingRole *role = edge->role();
  return SearchPredNonLatch2::searchThru(edge)
    && role != TimingRole::regClkToQ();
}

////////////////////////////////////////////////////////////////

class PropActivityVisitor : public VertexVisitor, StaState
{
public:
  PropActivityVisitor(Power *power,
		      BfsFwdIterator *bfs);
  ~PropActivityVisitor();
  virtual VertexVisitor *copy() const;
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
PropActivityVisitor::copy() const
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
  Pin *pin = vertex->pin();
  Instance *inst = network_->instance(pin);
  debugPrint(debug_, "power_activity", 3, "visit %s",
             vertex->name(network_));
  bool input_without_activity = false;
  if (power_->hasUserActivity(pin)) {
    PwrActivity &activity = power_->userActivity(pin);
    debugPrint(debug_, "power_activity", 3, "set %s %.2e %.2f",
               vertex->name(network_),
               activity.activity(),
               activity.duty());
    if (!power_->hasActivity(pin))
      input_without_activity = true;
    power_->setActivity(pin, activity);
  }
  else {
    if (network_->isLoad(pin)) {
      VertexInEdgeIterator edge_iter(vertex, graph_);
      if (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (edge->isWire()) {
	  Vertex *from_vertex = edge->from(graph_);
	  const Pin *from_pin = from_vertex->pin();
	  PwrActivity &from_activity = power_->activity(from_pin);
	  PwrActivity to_activity(from_activity.activity(),
				  from_activity.duty(),
				  PwrActivityOrigin::propagated);
	  if (!power_->hasActivity(pin))
	    input_without_activity = true;
	  power_->setActivity(pin, to_activity);
	}
      }
    }
    if (network_->isDriver(pin)) {
      LibertyPort *port = network_->libertyPort(pin);
      if (port) {
	FuncExpr *func = port->function();
	if (func) {
	  Instance *inst = network_->instance(pin);
	  PwrActivity activity = power_->evalActivity(func, inst);
	  power_->setActivity(pin, activity);
	  debugPrint(debug_, "power_activity", 3, "set %s %.2e %.2f",
                     vertex->name(network_),
                     activity.activity(),
                     activity.duty());
	}
        if (port->isClockGateOut()) {
          const Pin *enable, *clk, *gclk;
          power_->clockGatePins(inst, enable, clk, gclk);
          if (enable && clk && gclk) {
            PwrActivity activity1 = power_->findActivity(clk);
            PwrActivity activity2 = power_->findActivity(enable);
            float p1 = activity1.duty();
            float p2 = activity2.duty();
            PwrActivity activity(activity1.activity() * p2 + activity2.activity() * p1,
                                 p1 * p2,
                                 PwrActivityOrigin::propagated);
            power_->setActivity(gclk, activity);
            debugPrint(debug_, "power_activity", 3, "gated_clk %s %.2e %.2f",
                       network_->pathName(gclk),
                       activity.activity(),
                       activity.duty());
          }
        }
      }
    }
  }
  LibertyCell *cell = network_->libertyCell(inst);
  if (network_->isLoad(pin) && cell) {
    if (cell->hasSequentials()) {
      debugPrint(debug_, "power_activity", 3, "pending seq %s",
                 network_->pathName(inst));
      visited_regs_->insert(inst);
      found_reg_without_activity_ |= input_without_activity;
    }
    // Gated clock cells latch the enable so there is no EN->GCLK timing arc.
    if (cell->isClockGate()) {
      const Pin *enable, *clk, *gclk;
      power_->clockGatePins(inst, enable, clk, gclk);
      if (gclk) {
        Vertex *gclk_vertex = graph_->pinDrvrVertex(gclk);
        bfs_->enqueue(gclk_vertex);
      }
    }
  }
  bfs_->enqueueAdjacentVertices(vertex);
}

void
Power::clockGatePins(const Instance *inst,
                     // Return values.
                     const Pin *&enable,
                     const Pin *&clk,
                     const Pin *&gclk) const
{
  enable = nullptr;
  clk = nullptr;
  gclk = nullptr;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const LibertyPort *port = network_->libertyPort(pin);
    if (port->isClockGateEnable())
      enable = pin;
    if (port->isClockGateClock())
      clk = pin;
    if (port->isClockGateOut())
      gclk = pin;
  }
  delete pin_iter;
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
    if (port->direction()->isInternal())
      return findSeqActivity(inst, port);
    else {
      Pin *pin = findLinkPin(inst, port);
      if (pin)
	return findActivity(pin);
    }
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
    return PwrActivity(activity1.activity() * p2 + activity2.activity() * p1,
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
    float d1 = activity1.duty();
    float d2 = activity2.duty();
    float p1 = d1 * (1.0 - d2);
    float p2 = (1.0 - d1) * d2;
    return PwrActivity(activity1.activity() + activity2.activity(),
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
Power::ensureActivities()
{
  // No need to propagate activites if global activity is set.
  if (!global_activity_.isSet()) {
    if (!activities_valid_) {
      // Clear existing activities.
      activity_map_.clear();
      seq_activity_map_.clear();

      ActivitySrchPred activity_srch_pred(this);
      BfsFwdIterator bfs(BfsIndex::other, &activity_srch_pred, this);
      seedActivities(bfs);
      PropActivityVisitor visitor(this, &bfs);
      visitor.init();
      // Propagate activities through combinational logic.
      bfs.visit(levelize_->maxLevel(), &visitor);
      // Propagate activiities through registers.
      while (visitor.foundRegWithoutActivity()) {
	InstanceSet *regs = visitor.stealVisitedRegs();
	InstanceSet::Iterator reg_iter(regs);
	while (reg_iter.hasNext()) {
	  Instance *reg = reg_iter.next();
	  // Propagate activiities across register D->Q.
	  seedRegOutputActivities(reg, bfs);
	}
	delete regs;

	visitor.init();
	// Propagate register output activities through
	// combinational logic.
	bfs.visit(levelize_->maxLevel(), &visitor);
      }
      activities_valid_ = true;
    }
  }
}

void
Power::seedActivities(BfsFwdIterator &bfs)
{
  for (Vertex *vertex : *levelize_->roots()) {
    const Pin *pin = vertex->pin();
    // Clock activities are baked in.
    if (!sdc_->isLeafPinClock(pin)
	&& !network_->direction(pin)->isInternal()) {
      debugPrint(debug_, "power_activity", 3, "seed %s",
                 vertex->name(network_));
      if (hasUserActivity(pin))
	setActivity(pin, userActivity(pin));
      else
	// Default inputs without explicit activities to the input default.
	setActivity(pin, input_activity_);
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      bfs.enqueueAdjacentVertices(vertex);
    }
  }
}

void
Power::seedRegOutputActivities(const Instance *inst,
			       BfsFwdIterator &bfs)
{
  LibertyCell *cell = network_->libertyCell(inst);
  for (Sequential *seq : cell->sequentials()) {
    seedRegOutputActivities(inst, seq, seq->output(), false);
    seedRegOutputActivities(inst, seq, seq->outputInv(), true);
    // Enqueue register output pins with functions that reference
    // the sequential internal pins (IQ, IQN).
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      LibertyPort *port = network_->libertyPort(pin);
      if (port) {
        FuncExpr *func = port->function();
        Vertex *vertex = graph_->pinDrvrVertex(pin);
        if (vertex
            && func
            && (func->port() == seq->output()
                || func->port() == seq->outputInv())) {
          debugPrint(debug_, "power_activity", 3, "enqueue reg output %s",
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
  const Pin *out_pin = network_->findPin(reg, output);
  if (!hasUserActivity(out_pin)) {
    PwrActivity activity = evalActivity(seq->data(), reg);
    // Register output activity cannnot exceed one transition per clock cycle,
    // but latch output can.
    if (seq->isRegister()
        && activity.activity() > 1.0)
      activity.setActivity(1.0);
    if (invert)
      activity.setDuty(1.0 - activity.duty());
    setSeqActivity(reg, output, activity);
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
    if (to_port) {
      float load_cap = to_port->direction()->isAnyOutput()
        ? graph_delay_calc_->loadCap(to_pin, dcalc_ap)
        : 0.0;
      PwrActivity activity = findClkedActivity(to_pin, inst_clk);
      if (to_port->direction()->isAnyOutput()) {
        findSwitchingPower(cell, to_port, activity, load_cap, corner, result);
        findOutputInternalPower(to_pin, to_port, inst, cell, activity,
                                load_cap, corner, result);
      }
      if (to_port->direction()->isAnyInput())
        findInputInternalPower(to_pin, to_port, inst, cell, activity,
                               load_cap, corner, result);
    }
  }
  delete pin_iter;
  findLeakagePower(inst, cell, corner, result);
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
			      const Corner *corner,
			      // Return values.
			      PowerResult &result)
{
  const MinMax *min_max = MinMax::max();
  LibertyCell *corner_cell = cell->cornerCell(corner, min_max);
  const LibertyPort *corner_port = port->cornerPort(corner, min_max);
  if (corner_cell && corner_port) {
    const InternalPowerSeq &internal_pwrs = corner_cell->internalPowers(corner_port);
    if (!internal_pwrs.empty()) {
      debugPrint(debug_, "power", 2, "internal input %s/%s (%s)",
                 network_->pathName(inst),
                 port->name(),
                 corner_cell->name());
      const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
      const Pvt *pvt = dcalc_ap->operatingConditions();
      Vertex *vertex = graph_->pinLoadVertex(pin);
      debugPrint(debug_, "power", 2, " cap = %s",
                 units_->capacitanceUnit()->asString(load_cap));
      debugPrint(debug_, "power", 2, "       when  act/ns duty  energy    power");
      float internal = 0.0;
      for (InternalPower *pwr : internal_pwrs) {
        const char *related_pg_pin = pwr->relatedPgPin();
        float energy = 0.0;
        int rf_count = 0;
        for (RiseFall *rf : RiseFall::range()) {
          float slew = getSlew(vertex, rf, corner);
          if (!delayInf(slew)) {
            float table_energy = pwr->power(rf, pvt, slew, load_cap);
            energy += table_energy;
            rf_count++;
          }
        }
        if (rf_count)
          energy /= rf_count; // average non-inf energies
        float duty = 1.0; // fallback default
        FuncExpr *when = pwr->when();
        if (when) {
          LibertyPort *out_corner_port = findExprOutPort(when);
          if (out_corner_port) {
            const LibertyPort *out_port = findLinkPort(cell, out_corner_port);
            if (out_port) {
              FuncExpr *func = out_port->function();
              if (func && func->hasPort(port))
                duty = evalActivityDifference(func, inst, port).duty();
              else
                duty = evalActivity(when, inst).duty();
            }
          }
          else
            duty = evalActivity(when, inst).duty();
        }
        float port_internal = energy * duty * activity.activity();
        debugPrint(debug_, "power", 2,  " %3s %6s  %.2f  %.2f %9.2e %9.2e %s",
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
}

float
Power::getSlew(Vertex *vertex,
               const RiseFall *rf,
               const Corner *corner)
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  const Pin *pin = vertex->pin();
  if (clk_network_->isIdealClock(pin))
    return clk_network_->idealClkSlew(pin, rf, MinMax::max());
  else
    return delayAsFloat(graph_->slew(vertex, rf, dcalc_ap->index()));
}

LibertyPort *
Power::findExprOutPort(FuncExpr *expr)
{
  LibertyPort *port;
  switch (expr->op()) {
  case FuncExpr::op_port:
    port = expr->port();
    if (port && port->direction()->isAnyOutput())
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
			       const Corner *corner,
			       // Return values.
			       PowerResult &result)
{
  debugPrint(debug_, "power", 2, "internal output %s/%s (%s)",
             network_->pathName(inst),
             to_port->name(),
             cell->name());
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  const Pvt *pvt = dcalc_ap->operatingConditions();
  LibertyCell *corner_cell = cell->cornerCell(dcalc_ap);
  const LibertyPort *to_corner_port = to_port->cornerPort(dcalc_ap);
  debugPrint(debug_, "power", 2, " cap = %s",
             units_->capacitanceUnit()->asString(load_cap));
  FuncExpr *func = to_port->function();

  map<const char*, float, StringLessIf> pg_duty_sum;
  for (InternalPower *pwr : corner_cell->internalPowers(to_corner_port)) {
    float duty = findInputDuty(to_pin, inst, func, pwr);
    const char *related_pg_pin = pwr->relatedPgPin();
    // Note related_pg_pin may be null.
    pg_duty_sum[related_pg_pin] += duty;
  }

  float internal = 0.0;
  for (InternalPower *pwr : corner_cell->internalPowers(to_corner_port)) {
    FuncExpr *when = pwr->when();
    const char *related_pg_pin = pwr->relatedPgPin();
    float duty = findInputDuty(to_pin, inst, func, pwr);
    Vertex *from_vertex = nullptr;
    bool positive_unate = true;
    const LibertyPort *from_corner_port = pwr->relatedPort();
    if (from_corner_port) {
      positive_unate = isPositiveUnate(corner_cell, from_corner_port, to_corner_port);
      const Pin *from_pin = findLinkPin(inst, from_corner_port);
      if (from_pin) {
	from_vertex = graph_->pinLoadVertex(from_pin);
      }
    }
    float energy = 0.0;
    int rf_count = 0;
    debugPrint(debug_, "power", 2,
                "             when act/ns duty  wgt   energy    power");
    for (RiseFall *to_rf : RiseFall::range()) {
      // Use unateness to find from_rf.
      RiseFall *from_rf = positive_unate ? to_rf : to_rf->opposite();
      float slew = from_vertex
	? getSlew(from_vertex, from_rf, corner)
	: 0.0;
      if (!delayInf(slew)) {
	float table_energy = pwr->power(to_rf, pvt, slew, load_cap);
	energy += table_energy;
	rf_count++;
      }
    }
    if (rf_count)
      energy /= rf_count; // average non-inf energies
    auto duty_sum_iter = pg_duty_sum.find(related_pg_pin);
    float weight = 0.0;
    if (duty_sum_iter != pg_duty_sum.end()) {
      float duty_sum = duty_sum_iter->second;
      if (duty_sum != 0.0)
	weight = duty / duty_sum;
    }
    float port_internal = weight * energy * to_activity.activity();
    debugPrint(debug_, "power", 2,  "%3s -> %-3s %6s  %.2f %.2f %.2f %9.2e %9.2e %s",
               from_corner_port ? from_corner_port->name() : "-" ,
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
  const LibertyPort *from_corner_port = pwr->relatedPort();
  if (from_corner_port) {
    const LibertyPort *from_port = findLinkPort(network_->libertyCell(inst),
						from_corner_port);
    const Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin) {
      FuncExpr *when = pwr->when();
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      if (func && func->hasPort(from_port)) {
	float from_activity = findActivity(from_pin).activity();
	float to_activity = findActivity(to_pin).activity();
	float duty1 = evalActivityDifference(func, inst, from_port).duty();
	float duty = 0.0;
	if (to_activity != 0.0F) {
	  duty = from_activity * duty1 / to_activity;
	  // Activities can get very small from multiplying probabilities
	  // through deep chains of logic. Dividing by very close to zero values
	  // can result in NaN/Inf depending on numerator.
	  if (!isnormal(duty))
	    duty = 0.0;
	}
	return duty;
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

// Hack to find cell port that corresponds to corner_port.
LibertyPort *
Power::findLinkPort(const LibertyCell *cell,
		    const LibertyPort *corner_port)
{
  return cell->findLibertyPort(corner_port->name());
}

Pin *
Power::findLinkPin(const Instance *inst,
		   const LibertyPort *corner_port)
{
  const LibertyCell *cell = network_->libertyCell(inst);
  LibertyPort *port = findLinkPort(cell, corner_port);
  return network_->findPin(inst, port);
}

static bool
isPositiveUnate(const LibertyCell *cell,
		const LibertyPort *from,
		const LibertyPort *to)
{
  const TimingArcSetSeq &arc_sets = cell->timingArcSets(from, to);
  if (!arc_sets.empty()) {
    TimingSense sense = arc_sets[0]->sense();
    return sense == TimingSense::positive_unate
      || sense == TimingSense::non_unate;
  }
  // default
  return true;
}

////////////////////////////////////////////////////////////////

void
Power::findLeakagePower(const Instance *,
			LibertyCell *cell,
			const Corner *corner,
			// Return values.
			PowerResult &result)
{
  LibertyCell *corner_cell = cell->cornerCell(corner, MinMax::max());
  float cond_leakage = 0.0;
  bool found_cond = false;
  float uncond_leakage = 0.0;
  bool found_uncond = false;
  float cond_duty_sum = 0.0;
  for (LeakagePower *leak : *corner_cell->leakagePowers()) {
    FuncExpr *when = leak->when();
    if (when) {
      FuncExprPortIterator port_iter(when);
      float duty = 1.0;
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	if (port->direction()->isAnyInput())
	  duty *= port->isClock() ? 0.25 : 0.5;
      }
      debugPrint(debug_, "power", 2, "leakage %s %s %.3e * %.2f",
                 cell->name(),
                 when->asString(),
                 leak->power(),
                 duty);
      cond_leakage += leak->power() * duty;
      if (leak->power() > 0.0)
        cond_duty_sum += duty;
      found_cond = true;
    }
    else {
      debugPrint(debug_, "power", 2, "leakage -- %s %.3e",
                 cell->name(),
                 leak->power());
      uncond_leakage += leak->power();
      found_uncond = true;
    }
  }
  float leakage = 0.0;
  float cell_leakage;
  bool cell_leakage_exists;
  cell->leakagePower(cell_leakage, cell_leakage_exists);
  if (cell_leakage_exists) {
    float duty = 1.0 - cond_duty_sum;
    debugPrint(debug_, "power", 2, "leakage cell %s %.3e * %.2f",
               cell->name(),
               cell_leakage,
               duty);
    cell_leakage *= duty;
  }
  // Ignore unconditional leakage unless there are no conditional leakage groups.
  if (found_cond)
    leakage = cond_leakage;
  else if (found_uncond)
    leakage = uncond_leakage;
  if (cell_leakage_exists)
    leakage += cell_leakage;
  debugPrint(debug_, "power", 2, "leakage %s %.3e",
             cell->name(),
             leakage);
  result.leakage() += leakage;
}

void
Power::findSwitchingPower(LibertyCell *cell,
			  const LibertyPort *to_port,
			  PwrActivity &activity,
			  float load_cap,
			  const Corner *corner,
			  // Return values.
			  PowerResult &result)
{
  MinMax *mm = MinMax::max();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(mm);
  LibertyCell *corner_cell = cell->cornerCell(dcalc_ap);
  float volt = portVoltage(corner_cell, to_port, dcalc_ap);
  float switching = .5 * load_cap * volt * volt * activity.activity();
  debugPrint(debug_, "power", 2, "switching %s/%s activity = %.2e volt = %.2f %.3e",
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
  ensureActivities();
  return findClkedActivity(pin, inst_clk);
}

PwrActivity
Power::findClkedActivity(const Pin *pin,
			 const Clock *inst_clk)
{
  PwrActivity activity = findActivity(pin);
  const Clock *clk = findClk(pin);
  if (clk == nullptr)
    clk = inst_clk;
  if (clk) {
    float period = clk->period();
    if (period > 0.0)
      return PwrActivity(activity.activity() / period,
			 activity.duty(),
			 activity.origin());
  }
  return activity;
}

PwrActivity
Power::findActivity(const Pin *pin)
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex && vertex->isConstant())
    return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
  else if (vertex && search_->isClock(vertex)) {
    if (activity_map_.hasKey(pin)) {
      PwrActivity &activity = activity_map_[pin];
      if (activity.origin() != PwrActivityOrigin::unknown)
        return activity;
    }
    return PwrActivity(2.0, 0.5, PwrActivityOrigin::clock);
  }
  else if (global_activity_.isSet())
    return global_activity_;
  else if (activity_map_.hasKey(pin)) {
    PwrActivity &activity = activity_map_[pin];
    if (activity.origin() != PwrActivityOrigin::unknown)
      return activity;
  }
  return input_activity_;
}

PwrActivity
Power::findSeqActivity(const Instance *inst,
		       LibertyPort *port)
{
  if (global_activity_.isSet())
    return global_activity_;
  else if (hasSeqActivity(inst, port)) {
    PwrActivity activity = seqActivity(inst, port);
    if (activity.origin() != PwrActivityOrigin::unknown)
      return activity;
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
    LibertyPgPort *pg_port = cell->findPgPort(pg_port_name);
    if (pg_port) {
      const char *volt_name = pg_port->voltageName();
      LibertyLibrary *library = cell->libertyLibrary();
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
  if (to_vertex) {
    VertexPathIterator path_iter(to_vertex, this);
    while (path_iter.hasNext()) {
      PathVertex *path = path_iter.next();
      const Clock *path_clk = path->clock(this);
      if (path_clk
	  && (clk == nullptr
	      || path_clk->period() < clk->period()))
	clk = path_clk;
    }
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
  check();
}

void
PwrActivity::setActivity(float activity)
{
  activity_ = activity;
}

void
PwrActivity::setDuty(float duty)
{
  duty_ = duty;
}

void
PwrActivity::set(float activity,
		 float duty,
		 PwrActivityOrigin origin)
{
  activity_ = activity;
  duty_ = duty;
  origin_ = origin;
  check();
}

void
PwrActivity::check()
{
  // Activities can get very small from multiplying probabilities
  // through deep chains of logic. Clip them to prevent floating
  // point anomalies.
  if (abs(activity_) < min_activity)
    activity_ = 0.0;
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
