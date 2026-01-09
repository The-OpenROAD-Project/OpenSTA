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

#include "Power.hh"

#include <algorithm> // max
#include <cmath>     // abs

#include "cudd.h"
#include "Stats.hh"
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
#include "Path.hh"
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

namespace sta {

using std::abs;
using std::max;
using std::min;
using std::isnormal;
using std::vector;
using std::map;

static bool
isPositiveUnate(const LibertyCell *cell,
		const LibertyPort *from,
		const LibertyPort *to);

static EnumNameMap<PwrActivityOrigin> pwr_activity_origin_map =
  {{PwrActivityOrigin::global, "global"},
   {PwrActivityOrigin::input, "input"},
   {PwrActivityOrigin::user, "user"},
   {PwrActivityOrigin::vcd, "vcd"},
   {PwrActivityOrigin::saif, "saif"},
   {PwrActivityOrigin::propagated, "propagated"},
   {PwrActivityOrigin::clock, "clock"},
   {PwrActivityOrigin::constant, "constant"},
   {PwrActivityOrigin::defaulted, "defaulted"},
   {PwrActivityOrigin::unknown, "unknown"}};

Power::Power(StaState *sta) :
  StaState(sta),
  global_activity_(),
  input_activity_(),            // default set in ensureActivities()
  seq_activity_map_(100, SeqPinHash(network_), SeqPinEqual()),
  activities_valid_(false),
  bdd_(sta),
  instance_powers_(InstanceIdLess(network_)),
  instance_powers_valid_(false),
  corner_(nullptr)
{
}

void
Power::clear()
{
  global_activity_.init();
  input_activity_.init();
  user_activity_map_.clear();
  seq_activity_map_.clear();
  activity_map_.clear();
  activities_valid_ = false;
  instance_powers_.clear();
  corner_ = nullptr;
}

void
Power::activitiesInvalid()
{
  activities_valid_ = false;
  instance_powers_valid_ = false;
}

void
Power::setGlobalActivity(float density,
			 float duty)
{
  global_activity_.set(density, duty, PwrActivityOrigin::global);
  activitiesInvalid();
}
  
void
Power::unsetGlobalActivity()
{
  global_activity_.init();
  activitiesInvalid();
}

void
Power::setInputActivity(float density,
			float duty)
{
  input_activity_.set(density, duty, PwrActivityOrigin::input);
  activitiesInvalid();
}

void
Power::unsetInputActivity()
{
  input_activity_.init();
  activitiesInvalid();
}

void
Power::setInputPortActivity(const Port *input_port,
			    float density,
		 	    float duty)
{
  Instance *top_inst = network_->topInstance();
  const Pin *pin = network_->findPin(top_inst, input_port);
  if (pin) {
    user_activity_map_[pin] = {density, duty, PwrActivityOrigin::user};
    activitiesInvalid();
  }
}

void
Power::unsetInputPortActivity(const Port *input_port)
{
  Instance *top_inst = network_->topInstance();
  const Pin *pin = network_->findPin(top_inst, input_port);
  if (pin) {
    user_activity_map_.erase(pin);
    activitiesInvalid();
  }
}

void
Power::setUserActivity(const Pin *pin,
                       float density,
                       float duty,
                       PwrActivityOrigin origin)
{
  user_activity_map_[pin] = {density, duty, origin};
  activitiesInvalid();
}

void
Power::unsetUserActivity(const Pin *pin)
{
  user_activity_map_.erase(pin);
  activitiesInvalid();
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
  debugPrint(debug_, "power_activity", 3, "set %s %.2e %.2f %s",
             network_->pathName(pin),
             activity.density(),
             activity.duty(),
             pwr_activity_origin_map.find(activity.origin()));
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
  activitiesInvalid();
}

bool
Power::hasSeqActivity(const Instance *reg,
		      LibertyPort *output)
{
  return seq_activity_map_.hasKey(SeqPin(reg, output));
}

PwrActivity &
Power::seqActivity(const Instance *reg,
		   LibertyPort *output)
{
  return seq_activity_map_[SeqPin(reg, output)];
}

SeqPinHash::SeqPinHash(const Network *network) :
  network_(network)
{
}

size_t
SeqPinHash::operator()(const SeqPin &pin) const
{
  return hashSum(network_->id(pin.first), pin.second->id());
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
             PowerResult &clock,
	     PowerResult &macro,
	     PowerResult &pad)
{
  total.clear();
  sequential.clear();
  combinational.clear();
  clock.clear();
  macro.clear();
  pad.clear();

  ensureActivities();
  ensureInstPowers(corner);
  for (auto [inst, inst_power] : instance_powers_) {
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      if (cell->isMacro()
	  || cell->isMemory()
          || cell->interfaceTiming())
	macro.incr(inst_power);
      else if (cell->isPad())
	pad.incr(inst_power);
      else if (inClockNetwork(inst))
	clock.incr(inst_power);
      else if (cell->hasSequentials())
	sequential.incr(inst_power);
      else
	combinational.incr(inst_power);
      total.incr(inst_power);
    }
  }
}

bool
Power::inClockNetwork(const Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyOutput()
        && !clk_network_->isClock(pin)) {
      delete pin_iter;
      return false;
    }
  }
  delete pin_iter;
  return true;
}

PowerResult
Power::power(const Instance *inst,
	     const Corner *corner)
{
  ensureActivities();
  ensureInstPowers(corner);
  if (network_->isHierarchical(inst)) {
    PowerResult result;
    powerInside(inst, corner, result);
    return result;
  }
  else
    return instance_powers_[inst];
}

void
Power::powerInside(const Instance *hinst,
                   const Corner *corner,
                   PowerResult &result)
{
  InstanceChildIterator *child_iter = network_->childIterator(hinst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (network_->isHierarchical(child))
      powerInside(child, corner, result);
    else
      result.incr(instance_powers_[child]);
  }
  delete child_iter;
}

typedef std::pair<Instance*, float> InstPower;

InstanceSeq
Power::highestPowerInstances(size_t count,
                             const Corner *corner)
{
  vector<InstPower> inst_pwrs;
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    PowerResult pwr = power(inst, corner);
    inst_pwrs.push_back({inst, pwr.total()});
  }
  delete inst_iter;

  sort(inst_pwrs.begin(), inst_pwrs.end(), [](InstPower &inst_pwr1,
                                              InstPower &inst_pwr2) {
    return inst_pwr1.second > inst_pwr2.second;
  });

  InstanceSeq insts;
  for (size_t i = 0; i < count; i++)
    insts.push_back(inst_pwrs[i].first);
  return insts;
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
  const TimingRole *role = edge->role();
  return SearchPredNonLatch2::searchThru(edge)
    && role != TimingRole::regClkToQ();
}

////////////////////////////////////////////////////////////////

class PropActivityVisitor : public VertexVisitor, StaState
{
public:
  PropActivityVisitor(Power *power,
		      BfsFwdIterator *bfs);
  virtual VertexVisitor *copy() const;
  virtual void visit(Vertex *vertex);
  InstanceSet &visitedRegs() { return visited_regs_; }
  void init();
  float maxChange() const { return max_change_; }

private:
  bool setActivityCheck(const Pin *pin,
                        PwrActivity &activity);

  static constexpr float change_tolerance_ = .001;
  InstanceSet visited_regs_;
  float max_change_;
  Power *power_;
  BfsFwdIterator *bfs_;
};

PropActivityVisitor::PropActivityVisitor(Power *power,
					 BfsFwdIterator *bfs) :
  StaState(power),
  visited_regs_(network_),
  max_change_(0.0),
  power_(power),
  bfs_(bfs)
{
}

VertexVisitor *
PropActivityVisitor::copy() const
{
  return new PropActivityVisitor(power_, bfs_);
}

void
PropActivityVisitor::init()
{
  max_change_ = 0.0;
}

void
PropActivityVisitor::visit(Vertex *vertex)
{
  Pin *pin = vertex->pin();
  Instance *inst = network_->instance(pin);
  debugPrint(debug_, "power_activity", 3, "visit %s",
             vertex->to_string(this).c_str());
  bool changed = false;
  if (power_->hasUserActivity(pin)) {
    PwrActivity &activity = power_->userActivity(pin);
    changed = setActivityCheck(pin, activity);
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
	  PwrActivity to_activity(from_activity.density(),
				  from_activity.duty(),
				  PwrActivityOrigin::propagated);
	  changed = setActivityCheck(pin, to_activity);
	}
      }
    }
    if (network_->isDriver(pin)) {
      LibertyPort *port = network_->libertyPort(pin);
      if (port) {
	FuncExpr *func = port->function();
        if (func == nullptr) {
          LibertyCell *test_cell = port->libertyCell()->testCell();
          if (test_cell) {
            port = test_cell->findLibertyPort(port->name());
            if (port)
              func = port->function();
          }
        }
	if (func) {
          PwrActivity activity = power_->evalActivity(func, inst);
	  changed = setActivityCheck(pin, activity);
	}
        if (port->isClockGateOut()) {
          const Pin *enable, *clk, *gclk;
          power_->clockGatePins(inst, enable, clk, gclk);
          if (enable && clk && gclk) {
            PwrActivity activity1 = power_->findActivity(clk);
            PwrActivity activity2 = power_->findActivity(enable);
            float p1 = activity1.duty();
            float p2 = activity2.duty();
            PwrActivity activity(activity1.density() * p2 + activity2.density() * p1,
                                 p1 * p2,
                                 PwrActivityOrigin::propagated);
            changed = setActivityCheck(gclk, activity);
            debugPrint(debug_, "power_activity", 3, "gated_clk %s %.2e %.2f",
                       network_->pathName(gclk),
                       activity.density(),
                       activity.duty());
          }
        }
      }
    }
  }
  if (changed) {
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      LibertyCell *test_cell = cell->libertyCell()->testCell();
      if (network_->isLoad(pin)) {
	if (cell->hasSequentials()
	    || (test_cell
		&& test_cell->hasSequentials())) {
	  debugPrint(debug_, "power_activity", 3, "pending seq %s",
		     network_->pathName(inst));
	  visited_regs_.insert(inst);
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
  }
}

// Return true if the activity changed.
bool
PropActivityVisitor::setActivityCheck(const Pin *pin,
                                      PwrActivity &activity)
{
  float min_rf_slew = power_->getMinRfSlew(pin);
  float max_density = (min_rf_slew > 0.0) ? 1.0 / min_rf_slew : INF;
  if (activity.density() > max_density)
    activity.setDensity(max_density);
  PwrActivity &prev_activity = power_->activity(pin);
  float density_delta = abs(activity.density() - prev_activity.density());
  float duty_delta = abs(activity.duty() - prev_activity.duty());
  if (density_delta > change_tolerance_
      || duty_delta > change_tolerance_
      || activity.origin() != prev_activity.origin()) {
    max_change_ = max(max_change_, density_delta);
    max_change_ = max(max_change_, duty_delta);
    power_->setActivity(pin, activity);
    return true;
  }
  else
    return false;
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

////////////////////////////////////////////////////////////////

PwrActivity
Power::evalActivity(FuncExpr *expr,
		    const Instance *inst)
{
  LibertyPort *func_port = expr->port();
  if (func_port &&  func_port->direction()->isInternal())
    return findSeqActivity(inst, func_port);
  else {
    DdNode *bdd = bdd_.funcBdd(expr);
    float duty = evalBddDuty(bdd, inst);
    float density = evalBddActivity(bdd, inst);

    Cudd_RecursiveDeref(bdd_.cuddMgr(), bdd);
    bdd_.clearVarMap();
    return PwrActivity(density, duty, PwrActivityOrigin::propagated);
  }
}

// Find duty when from_port is sensitized.
float
Power::evalDiffDuty(FuncExpr *expr,
                    LibertyPort *from_port,
                    const Instance *inst)
{
  DdNode *bdd = bdd_.funcBdd(expr);
  DdNode *var_node = bdd_.findNode(from_port);
  unsigned var_index = Cudd_NodeReadIndex(var_node);
  DdNode *diff = Cudd_bddBooleanDiff(bdd_.cuddMgr(), bdd, var_index);
  Cudd_Ref(diff);
  float duty = evalBddDuty(diff, inst);

  Cudd_RecursiveDeref(bdd_.cuddMgr(), diff);
  Cudd_RecursiveDeref(bdd_.cuddMgr(), bdd);
  bdd_.clearVarMap();
  return duty;
}

// As suggested by
// https://stackoverflow.com/questions/63326728/cudd-printminterm-accessing-the-individual-minterms-in-the-sum-of-products
float
Power::evalBddDuty(DdNode *bdd,
                   const Instance *inst)
{
  if (Cudd_IsConstant(bdd)) {
    if (bdd == Cudd_ReadOne(bdd_.cuddMgr()))
      return 1.0;
    else if (bdd == Cudd_ReadLogicZero(bdd_.cuddMgr()))
      return 0.0;
    else
      criticalError(1100, "unknown cudd constant");
  }
  else {
    float duty0 = evalBddDuty(Cudd_E(bdd), inst);
    float duty1 = evalBddDuty(Cudd_T(bdd), inst);
    unsigned int index = Cudd_NodeReadIndex(bdd);
    int var_index = Cudd_ReadPerm(bdd_.cuddMgr(), index);
    const LibertyPort *port = bdd_.varIndexPort(var_index);
    if (port->direction()->isInternal())
      return findSeqActivity(inst, const_cast<LibertyPort*>(port)).duty();
    else {
      const Pin *pin = findLinkPin(inst, port);
      if (pin) {
        PwrActivity var_activity = findActivity(pin);
        float var_duty = var_activity.duty();
        float duty = duty0 * (1.0 - var_duty) + duty1 * var_duty;
        if (Cudd_IsComplement(bdd))
          duty = 1.0 - duty;
        return duty;
      }
    }
  }
  return 0.0;
}

// https://www.brown.edu/Departments/Engineering/Courses/engn2912/Lectures/LP-02-logic-power-est.pdf
// F(x0, x1, .. ) is sensitized when F(Xi=1) xor F(Xi=0)
// F(Xi=1), F(Xi=0) are the cofactors of F wrt Xi.
float
Power::evalBddActivity(DdNode *bdd,
                       const Instance *inst)
{
  float density = 0.0;
  for (const auto [port, var_node] : bdd_.portVarMap()) {
    const Pin *pin = findLinkPin(inst, port);
    if (pin) {
      PwrActivity var_activity = findActivity(pin);
      unsigned int var_index = Cudd_NodeReadIndex(var_node);
      DdNode *diff = Cudd_bddBooleanDiff(bdd_.cuddMgr(), bdd, var_index);
      Cudd_Ref(diff);
      float diff_duty = evalBddDuty(diff, inst);
      Cudd_RecursiveDeref(bdd_.cuddMgr(), diff);
      float var_density = var_activity.density() * diff_duty;
      density += var_density;
      debugPrint(debug_, "power_activity", 3, "%s %.3e * %.3f = %.3e",
                 network_->pathName(pin),
                 var_activity.density(),
                 diff_duty,
                 var_density);
    }
  }
  return density;
}

////////////////////////////////////////////////////////////////

void
Power::ensureActivities()
{
  Stats stats(debug_, report_);
  if (!activities_valid_) {
    // No need to propagate activites if global activity is set.
    if (!global_activity_.isSet()) {
      // Clear existing activities.
      activity_map_.clear();
      seq_activity_map_.clear();

      // Initialize default input activity (after sdc is defined)
      // unless it has been set by command.
      if (input_activity_.origin() == PwrActivityOrigin::unknown) {
        float min_period = clockMinPeriod();
        float density = 0.1 / (min_period != 0.0
                               ? min_period
                               : units_->timeUnit()->scale());
        input_activity_.set(density, 0.5, PwrActivityOrigin::input);
      }
      ActivitySrchPred activity_srch_pred(this);
      BfsFwdIterator bfs(BfsIndex::other, &activity_srch_pred, this);
      seedActivities(bfs);
      PropActivityVisitor visitor(this, &bfs);
      // Propagate activities through combinational logic.
      bfs.visit(levelize_->maxLevel(), &visitor);
      // Propagate activiities through registers.
      InstanceSet regs = std::move(visitor.visitedRegs());
      int pass = 1;
      while (!regs.empty() && pass < max_activity_passes_) {
        visitor.init();
	for (const Instance *reg : regs)
	  // Propagate activiities across register D->Q.
	  seedRegOutputActivities(reg, bfs);
	// Propagate register output activities through
	// combinational logic.
	bfs.visit(levelize_->maxLevel(), &visitor);
        regs = std::move(visitor.visitedRegs());
        debugPrint(debug_, "power_activity", 1, "Pass %d change %.2f",
                   pass, visitor.maxChange());
        pass++;
      }
    }
    activities_valid_ = true;
  }
  stats.report("Power activities");
}

void
Power::seedActivities(BfsFwdIterator &bfs)
{
  for (Vertex *vertex : levelize_->roots()) {
    const Pin *pin = vertex->pin();
    // Clock activities are baked in.
    if (!sdc_->isLeafPinClock(pin)
	&& !network_->direction(pin)->isInternal()) {
      debugPrint(debug_, "power_activity", 3, "seed %s",
                 vertex->to_string(this).c_str());
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
  const SequentialSeq &seqs = cell->sequentials();
  if (!seqs.empty())
    seedRegOutputActivities(inst, nullptr, seqs, bfs);
  else {
    LibertyCell *test_cell = cell->testCell();
    if (test_cell) {
      const SequentialSeq &seqs = test_cell->sequentials();
      seedRegOutputActivities(inst, test_cell, seqs, bfs);
    }
  }
}

void
Power::seedRegOutputActivities(const Instance *inst,
                               const LibertyCell *test_cell,
                               const SequentialSeq &seqs,
                               BfsFwdIterator &bfs)
{
  for (Sequential *seq : seqs) {
    seedRegOutputActivities(inst, seq, seq->output(), false);
    seedRegOutputActivities(inst, seq, seq->outputInv(), true);
    // Enqueue register output pins with functions that reference
    // the sequential internal pins (IQ, IQN).
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      LibertyPort *port = network_->libertyPort(pin);
      if (test_cell)
        port = test_cell->findLibertyPort(port->name());
      if (port) {
        FuncExpr *func = port->function();
        Vertex *vertex = graph_->pinDrvrVertex(pin);
        if (vertex
            && func
            && (func->port() == seq->output()
                || func->port() == seq->outputInv())) {
          debugPrint(debug_, "power_reg", 1, "enqueue reg output %s",
                     vertex->to_string(this).c_str());
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
    PwrActivity in_activity = evalActivity(seq->data(), reg);
    float in_density = in_activity.density();
    float in_duty = in_activity.duty();
    // Default propagates input density/duty thru reg/latch.
    float out_density = in_density;
    float out_duty = in_duty;
    PwrActivity clk_activity = evalActivity(seq->clock(), reg);
    float clk_density = clk_activity.density();
    if (in_density > clk_density / 2) {
      if (seq->isRegister())
        out_density = 2 * in_duty * (1 - in_duty) * clk_density;
      else if (seq->isLatch()) {
        PwrActivity clk_activity = evalActivity(seq->clock(), reg);
        float clk_duty = clk_activity.duty();
        FuncExpr *clk_func = seq->clock();
        bool clk_invert = clk_func
          && clk_func->op() == FuncExpr::op_not
          && clk_func->left()->op() == FuncExpr::op_port;
        if (clk_invert)
          out_density = in_density * (1 - clk_duty);
        else
          out_density = in_density * clk_duty;
      }
    }
    if (invert)
      out_duty = 1.0 - out_duty;
    PwrActivity out_activity(out_density, out_duty, PwrActivityOrigin::propagated);
    setSeqActivity(reg, output, out_activity);
  }
}

////////////////////////////////////////////////////////////////

void
Power::ensureInstPowers(const Corner *corner)
{
  if (!instance_powers_valid_
      || corner != corner_) {
    findInstPowers(corner);
    instance_powers_valid_ = true;
  }
}

void
Power::findInstPowers(const Corner *corner)
{
  Stats stats(debug_, report_);
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      PowerResult inst_power = power(inst, cell, corner);
      instance_powers_[inst] = inst_power;
    }
  }
  delete inst_iter;
  corner_ = corner;
  stats.report("Find power");
}

PowerResult
Power::power(const Instance *inst,
	     LibertyCell *cell,
	     const Corner *corner)
{
  PowerResult result;
  findInternalPower(inst, cell, corner, result);
  findSwitchingPower(inst, cell, corner, result);
  findLeakagePower(inst, cell, corner, result);
  return result;
}

const Clock *
Power::findInstClk(const Instance *inst)
{
  const Clock *inst_clk = nullptr;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const Clock *clk = findClk(pin);
    if (clk) {
      inst_clk = clk;
      break;
    }
  }
  delete pin_iter;
  return inst_clk;
}

void
Power::findInternalPower(const Instance *inst,
                         LibertyCell *cell,
                         const Corner *corner,
                         // Return values.
                         PowerResult &result)
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    LibertyPort *to_port = network_->libertyPort(to_pin);
    if (to_port) {
      float load_cap = to_port->direction()->isAnyOutput()
        ? graph_delay_calc_->loadCap(to_pin, dcalc_ap)
        : 0.0;
      PwrActivity activity = findActivity(to_pin);
      if (to_port->direction()->isAnyOutput())
        findOutputInternalPower(to_port, inst, cell, activity,
                                load_cap, corner, result);
      if (to_port->direction()->isAnyInput())
        findInputInternalPower(to_pin, to_port, inst, cell, activity,
                               load_cap, corner, result);
    }
  }
  delete pin_iter;
}

void
Power::findInputInternalPower(const Pin *pin,
			      LibertyPort *port,
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
      debugPrint(debug_, "power", 2, "internal input %s/%s cap %s",
                 network_->pathName(inst),
                 port->name(),
                 units_->capacitanceUnit()->asString(load_cap));
      debugPrint(debug_, "power", 2, "       when  act/ns duty  energy    power");
      const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
      const Pvt *pvt = dcalc_ap->operatingConditions();
      Vertex *vertex = graph_->pinLoadVertex(pin);
      float internal = 0.0;
      for (InternalPower *pwr : internal_pwrs) {
        const char *related_pg_pin = pwr->relatedPgPin();
        float energy = 0.0;
        int rf_count = 0;
        for (const RiseFall *rf : RiseFall::range()) {
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
          const LibertyPort *out_corner_port = findExprOutPort(when);
          if (out_corner_port) {
            LibertyPort *out_port = findLinkPort(cell, out_corner_port);
            if (out_port) {
              FuncExpr *func = out_port->function();
              if (func && func->hasPort(port))
                duty = evalDiffDuty(func, port, inst);
              else
                duty = evalActivity(when, inst).duty();
            }
          }
          else
            duty = evalActivity(when, inst).duty();
        }
        float port_internal = energy * duty * activity.density();
        debugPrint(debug_, "power", 2,  " %3s %6s  %.2f  %.2f %9.2e %9.2e %s",
                   port->name(),
                   when ? when->to_string().c_str() : "",
                   activity.density() * 1e-9,
                   duty,
                   energy,
                   port_internal,
                   related_pg_pin ? related_pg_pin : "no pg_pin");
        internal += port_internal;
      }
      result.incrInternal(internal);
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

float
Power::getMinRfSlew(const Pin *pin)
{
  Vertex *vertex, *bidir_vertex;
  graph_->pinVertices(pin, vertex, bidir_vertex);
  if (vertex) {
    const MinMax *min_max = MinMax::min();
    Slew mm_slew = min_max->initValue();
    for (const DcalcAnalysisPt *dcalc_ap : corners_->dcalcAnalysisPts()) {
      DcalcAPIndex ap_index = dcalc_ap->index();
      const Slew &slew1 = graph_->slew(vertex, RiseFall::rise(), ap_index);
      const Slew &slew2 = graph_->slew(vertex, RiseFall::fall(), ap_index);
      Slew slew = delayAsFloat(slew1 + slew2) / 2.0;
      if (delayGreater(slew, mm_slew, min_max, this))
        mm_slew = slew;
    }
    return delayAsFloat(mm_slew);
  }
  return 0.0;
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

void
Power::findOutputInternalPower(const LibertyPort *to_port,
			       const Instance *inst,
			       LibertyCell *cell,
			       PwrActivity &to_activity,
			       float load_cap,
			       const Corner *corner,
			       // Return values.
			       PowerResult &result)
{
  debugPrint(debug_, "power", 2, "internal output %s/%s cap %s",
             network_->pathName(inst),
             to_port->name(),
             units_->capacitanceUnit()->asString(load_cap));
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  const Pvt *pvt = dcalc_ap->operatingConditions();
  LibertyCell *corner_cell = cell->cornerCell(dcalc_ap);
  const LibertyPort *to_corner_port = to_port->cornerPort(dcalc_ap);
  FuncExpr *func = to_port->function();

  map<const char*, float, StringLessIf> pg_duty_sum;
  for (InternalPower *pwr : corner_cell->internalPowers(to_corner_port)) {
    const LibertyPort *from_corner_port = pwr->relatedPort();
    if (from_corner_port) {
      const Pin *from_pin = findLinkPin(inst, from_corner_port);
      float from_density = findActivity(from_pin).density();
      float duty = findInputDuty(inst, func, pwr);
      const char *related_pg_pin = pwr->relatedPgPin();
      // Note related_pg_pin may be null.
      pg_duty_sum[related_pg_pin] += from_density * duty;
    }
  }

  debugPrint(debug_, "power", 2,
             "             when act/ns  duty  wgt   energy    power");
  float internal = 0.0;
  for (InternalPower *pwr : corner_cell->internalPowers(to_corner_port)) {
    FuncExpr *when = pwr->when();
    const char *related_pg_pin = pwr->relatedPgPin();
    float duty = findInputDuty(inst, func, pwr);
    Vertex *from_vertex = nullptr;
    bool positive_unate = true;
    const LibertyPort *from_corner_port = pwr->relatedPort();
    const Pin *from_pin = nullptr;
    if (from_corner_port) {
      positive_unate = isPositiveUnate(corner_cell, from_corner_port, to_corner_port);
      from_pin = findLinkPin(inst, from_corner_port);
      if (from_pin)
	from_vertex = graph_->pinLoadVertex(from_pin);
    }
    float energy = 0.0;
    int rf_count = 0;
    for (const RiseFall *to_rf : RiseFall::range()) {
      // Use unateness to find from_rf.
      const RiseFall *from_rf = positive_unate ? to_rf : to_rf->opposite();
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
      if (duty_sum != 0.0 && from_pin) {
        float from_density = findActivity(from_pin).density();
	weight = from_density * duty / duty_sum;
      }
    }
    float port_internal = weight * energy * to_activity.density();
    debugPrint(debug_, "power", 2,  "%3s -> %-3s %6s  %.3f %.3f %.3f %9.2e %9.2e %s",
               from_corner_port ? from_corner_port->name() : "-" ,
               to_port->name(),
               when ? when->to_string().c_str() : "",
               to_activity.density() * 1e-9,
               duty,
               weight,
               energy,
               port_internal,
               related_pg_pin ? related_pg_pin : "no pg_pin");
    internal += port_internal;
  }
  result.incrInternal(internal);
}

float
Power::findInputDuty(const Instance *inst,
                     FuncExpr *func,
                     InternalPower *pwr)

{
  const LibertyPort *from_corner_port = pwr->relatedPort();
  if (from_corner_port) {
    LibertyPort *from_port = findLinkPort(network_->libertyCell(inst),
                                          from_corner_port);
    const Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin) {
      FuncExpr *when = pwr->when();
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      if (func && func->hasPort(from_port)) {
	float duty = evalDiffDuty(func, from_port, inst);
	return duty;
      }
      else if (when)
	return evalActivity(when, inst).duty();
      else if (search_->isClock(from_vertex))
	return 0.5;
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
Power::findSwitchingPower(const Instance *inst,
                          LibertyCell *cell,
                          const Corner *corner,
                          // Return values.
                          PowerResult &result)
{
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  LibertyCell *corner_cell = cell->cornerCell(dcalc_ap);
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    const LibertyPort *to_port = network_->libertyPort(to_pin);
    if (to_port) {
      float load_cap = to_port->direction()->isAnyOutput()
        ? graph_delay_calc_->loadCap(to_pin, dcalc_ap)
        : 0.0;
      PwrActivity activity = findActivity(to_pin);
      if (to_port->direction()->isAnyOutput()) {
        float volt = portVoltage(corner_cell, to_port, dcalc_ap);
        float switching = .5 * load_cap * volt * volt * activity.density();
        debugPrint(debug_, "power", 2, "switching %s/%s activity = %.2e volt = %.2f %.3e",
                   cell->name(),
                   to_port->name(),
                   activity.density(),
                   volt,
                   switching);
        result.incrSwitching(switching);
      }
    }
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////


void
Power::findLeakagePower(const Instance *inst,
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
      PwrActivity cond_activity = evalActivity(when, inst);
      float cond_duty = cond_activity.duty();
      debugPrint(debug_, "power", 2, "leakage %s %s %.3e * %.2f",
                 cell->name(),
                 when->to_string().c_str(),
                 leak->power(),
                 cond_duty);
      cond_leakage += leak->power() * cond_duty;
      if (leak->power() > 0.0)
        cond_duty_sum += cond_duty;
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
  result.incrLeakage(leakage);
}

// External.
PwrActivity
Power::pinActivity(const Pin *pin)
{
  ensureActivities();
  return findActivity(pin);
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
    const Clock *clk = findClk(pin);
    float duty = clockDuty(clk);
    return PwrActivity(2.0 / clk->period(), duty, PwrActivityOrigin::clock);
  }
  else if (global_activity_.isSet())
    return global_activity_;
  else if (activity_map_.hasKey(pin)) {
    PwrActivity &activity = activity_map_[pin];
    if (activity.origin() != PwrActivityOrigin::unknown)
      return activity;
  }
  return PwrActivity(0.0, 0.0, PwrActivityOrigin::unknown);
}

float
Power::clockDuty(const Clock *clk)
{
  if (clk->isGenerated()) {
    const Clock *master = clk->masterClk();
    if (master == nullptr)
      return 0.5; // punt
    else
      return clockDuty(master);
  }
  else {
    const FloatSeq *waveform = clk->waveform();
    float rise_time = (*waveform)[0];
    float fall_time = (*waveform)[1];
    float duty = (fall_time - rise_time) / clk->period();
    return duty;
  }
}

PwrActivity
Power::findSeqActivity(const Instance *inst,
		       LibertyPort *port)
{
  if (global_activity_.isSet())
    return global_activity_;
  else if (hasSeqActivity(inst, port)) {
    PwrActivity &activity = seqActivity(inst, port);
    return activity;
  }
  return PwrActivity();
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
    LibertyPort *pg_port = cell->findLibertyPort(pg_port_name);
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
      Path *path = path_iter.next();
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

void
Power::reportActivityAnnotation(bool report_unannotated,
                                bool report_annotated)
{
  size_t vcd_count = 0;
  size_t saif_count = 0;
  size_t input_count = 0;
  for (auto const& [pin, activity] : user_activity_map_) {
    PwrActivityOrigin origin = activity.origin();
    switch (origin) {
    case PwrActivityOrigin::vcd:
      vcd_count++;
      break;
    case PwrActivityOrigin::saif:
      saif_count++;
      break;
    case PwrActivityOrigin::user:
      input_count++;
      break;
    default:
      break;
    }
  }
  if (vcd_count > 0)
    report_->reportLine("vcd         %5zu", vcd_count);
  if (saif_count > 0)
    report_->reportLine("saif        %5zu", saif_count);
  if (input_count > 0)
    report_->reportLine("input       %5zu", input_count);
  size_t pin_count = pinCount();
  size_t unannotated_count = pin_count - vcd_count - saif_count - input_count;
  report_->reportLine("unannotated %5zu", unannotated_count);

  if (report_annotated) {
    PinSeq annotated_pins;
    for (auto const& [pin, activity] : user_activity_map_)
      annotated_pins.push_back(pin);
    sort(annotated_pins.begin(), annotated_pins.end(), PinPathNameLess(sdc_network_));
    report_->reportLine("Annotated pins:");
    for (const Pin *pin : annotated_pins) {
      const PwrActivity &activity = user_activity_map_[pin];
      PwrActivityOrigin origin = activity.origin();
      const char *origin_name = pwr_activity_origin_map.find(origin);
      report_->reportLine("%5s %s",
                          origin_name,
                          sdc_network_->pathName(pin));
    }
  }
  if (report_unannotated) {
    PinSeq unannotated_pins;
    findUnannotatedPins(network_->topInstance(), unannotated_pins);
    LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      const Instance *inst = inst_iter->next();
      findUnannotatedPins(inst, unannotated_pins);
    }
    delete inst_iter;

    sort(unannotated_pins.begin(), unannotated_pins.end(),
         PinPathNameLess(sdc_network_));
    report_->reportLine("Unannotated pins:");
    for (const Pin *pin : unannotated_pins) {
      report_->reportLine(" %s", sdc_network_->pathName(pin));
    }
  }
}

void
Power::findUnannotatedPins(const Instance *inst,
                           PinSeq &unannotated_pins)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    LibertyPort *liberty_port = sdc_network_->libertyPort(pin);
    if (!network_->direction(pin)->isInternal()
	&& !network_->direction(pin)->isPowerGround()
	&& !(liberty_port && liberty_port->isPwrGnd())
        && user_activity_map_.find(pin) == user_activity_map_.end())
      unannotated_pins.push_back(pin);
  }
  delete pin_iter;
}

// leaf pins - internal pins - power/ground pins + top instance pins
size_t
Power::pinCount()
{
  size_t count = 0;
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    Instance *leaf = leaf_iter->next();
    InstancePinIterator *pin_iter = network_->pinIterator(leaf);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      LibertyPort *liberty_port = sdc_network_->libertyPort(pin);
      if (!network_->direction(pin)->isInternal()
	  && !network_->direction(pin)->isPowerGround()
	  && !(liberty_port && liberty_port->isPwrGnd()))
        count++;
    }
    delete pin_iter;
  }
  delete leaf_iter;

  InstancePinIterator *pin_iter = network_->pinIterator(network_->topInstance());
  while (pin_iter->hasNext()) {
    pin_iter->next();
    count++;
  }
  delete pin_iter;

  return count;
}

float
Power::clockMinPeriod()
{
  ClockSeq *clks = sdc_->clocks();
  if (clks && !clks->empty()) {
    float min_period = INF;
    for (const Clock *clk : *clks)
      min_period = min(min_period, clk->period());
    return min_period;
  }
  else
    return 0.0;
}

void
Power::deleteInstanceBefore(const Instance *)
{
  activities_valid_ = false;
  instance_powers_.clear();
  corner_ = nullptr;
}

void
Power::deletePinBefore(const Pin *)
{
  activities_valid_ = false;
  instance_powers_.clear();
  corner_ = nullptr;
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
PowerResult::incrInternal(float pwr)
{
  internal_ += pwr;
}

void
PowerResult::incrSwitching(float pwr)
{
  switching_ += pwr;
}

void
PowerResult::incrLeakage(float pwr)
{
  leakage_ += pwr;
}

void
PowerResult::incr(PowerResult &result)
{
  internal_ += result.internal_;
  switching_ += result.switching_;
  leakage_ += result.leakage_;
}

////////////////////////////////////////////////////////////////

PwrActivity::PwrActivity(float density,
			 float duty,
			 PwrActivityOrigin origin) :
  density_(density),
  duty_(duty),
  origin_(origin)
{
  check();
}

PwrActivity::PwrActivity() :
  density_(0.0),
  duty_(0.0),
  origin_(PwrActivityOrigin::unknown)
{
}

void
PwrActivity::setDensity(float density)
{
  density_ = density;
}

void
PwrActivity::setDuty(float duty)
{
  duty_ = duty;
}

void
PwrActivity::setOrigin(PwrActivityOrigin origin)
{
  origin_ = origin;
}

void
PwrActivity::init()
{
  density_ = 0.0;
  duty_ = 0.0;
  origin_ = PwrActivityOrigin::unknown;
}

void
PwrActivity::set(float density,
		 float duty,
		 PwrActivityOrigin origin)
{
  density_ = density;
  duty_ = duty;
  origin_ = origin;
  check();
}

void
PwrActivity::check()
{
  // Densities can get very small from multiplying probabilities
  // through deep chains of logic. Clip them to prevent floating
  // point anomalies.
  if (abs(density_) < min_density)
    density_ = 0.0;
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
