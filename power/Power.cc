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
#include "ContainerHelpers.hh"
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
#include "Mode.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Scene.hh"
#include "Path.hh"
#include "search/Levelize.hh"
#include "search/Sim.hh"
#include "Search.hh"
#include "Bfs.hh"
#include "ClkNetwork.hh"
#include "ReportPower.hh"

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
  scene_(nullptr),
  global_activity_(),
  input_activity_(),            // default set in ensureActivities.
  seq_activity_map_(100, SeqPinHash(network_), SeqPinEqual()),
  activities_valid_(false),
  bdd_(sta),
  instance_powers_(InstanceIdLess(network_)),
  instance_powers_valid_(false)
{
}

void
Power::clear()
{
  scene_ = nullptr;
  global_activity_.init();
  input_activity_.init();
  user_activity_map_.clear();
  seq_activity_map_.clear();
  activity_map_.clear();
  activities_valid_ = false;
  instance_powers_.clear();
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
  return user_activity_map_.contains(pin);
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
  return activity_map_.contains(pin);
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
  return seq_activity_map_.contains(SeqPin(reg, output));
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
  const auto& [inst, port] = pin;
  return hashSum(network_->id(inst), port->id());
}

bool
SeqPinEqual::operator()(const SeqPin &pin1,
			const SeqPin &pin2) const
{
  const auto& [inst1, port1] = pin1;
  const auto& [inst2, port2] = pin2;
  return inst1 == inst2
    && port1 == port2;
}

////////////////////////////////////////////////////////////////

void
Power::reportDesign(const Scene *scene,
                    int digits)
{
  PowerResult total, sequential, combinational, clock, macro, pad;
  power(scene, total, sequential, combinational, clock, macro, pad);
  ReportPower report_power(this);
 report_power.reportDesign(total, sequential, combinational, clock, macro, pad, digits);
}

void
Power::reportInsts(const InstanceSeq &insts,
                   const Scene *scene,
                   int digits)
{
  InstPowers inst_pwrs = sortInstsByPower(insts, scene);
  ReportPower report_power(this);
  report_power.reportInsts(inst_pwrs, digits);
}

void
Power::reportHighestInsts(size_t count,
                          const Scene *scene,
                          int digits)
{
  InstPowers inst_pwrs = highestInstPowers(count, scene);
  ReportPower report_power(this);
  report_power.reportInsts(inst_pwrs, digits);
}

void
Power::reportDesignJson(const Scene *scene,
                        int digits)
{
  PowerResult total, sequential, combinational, clock, macro, pad;
  power(scene, total, sequential, combinational, clock, macro, pad);
  
  report_->reportLine("{");
  reportPowerRowJson("Sequential", sequential, digits, ",");
  reportPowerRowJson("Combinational", combinational, digits, ",");
  reportPowerRowJson("Clock", clock, digits, ",");
  reportPowerRowJson("Macro", macro, digits, ",");
  reportPowerRowJson("Pad", pad, digits, ",");
  reportPowerRowJson("Total", total, digits, "");
  report_->reportLine("}");
}

void
Power::reportInstsJson(const InstanceSeq &insts,
                       const Scene *scene,
                       int digits)
{
  InstPowers inst_pwrs = sortInstsByPower(insts, scene);
  
  report_->reportLine("[");
  bool first = true;
  for (const InstPower &inst_pwr : inst_pwrs) {
    if (!first) {
      report_->reportLine(",");
    }
    first = false;
    reportPowerInstJson(inst_pwr.first, inst_pwr.second, digits);
  }
  report_->reportLine("]");
}

void
Power::reportPowerRowJson(const char *name,
                          const PowerResult &power,
                          int digits,
                          const char *separator)
{
  float internal = power.internal();
  float switching = power.switching();
  float leakage = power.leakage();
  float total = power.total();
  
  report_->reportLine("  \"%s\": {", name);
  report_->reportLine("    \"internal\": %.*e,", digits, internal);
  report_->reportLine("    \"switching\": %.*e,", digits, switching);
  report_->reportLine("    \"leakage\": %.*e,", digits, leakage);
  report_->reportLine("    \"total\": %.*e", digits, total);
  std::string line = "  }";
  if (separator && separator[0] != '\0')
    line += separator;
  report_->reportLineString(line);
}

void
Power::reportPowerInstJson(const Instance *inst,
                           const PowerResult &power,
                           int digits)
{
  float internal = power.internal();
  float switching = power.switching();
  float leakage = power.leakage();
  float total = power.total();
  
  const char *inst_name = network_->pathName(inst);
  report_->reportLine("{");
  report_->reportLine("  \"name\": \"%s\",", inst_name);
  report_->reportLine("  \"internal\": %.*e,", digits, internal);
  report_->reportLine("  \"switching\": %.*e,", digits, switching);
  report_->reportLine("  \"leakage\": %.*e,", digits, leakage);
  report_->reportLine("  \"total\": %.*e", digits, total);
  report_->reportLine("}");
}

static bool
instPowerGreater(const InstPower &pwr1,
                 const InstPower &pwr2)
{
  return pwr1.second.total() > pwr2.second.total();
}

InstPowers
Power::sortInstsByPower(const InstanceSeq &insts,
                        const Scene *scene)
{
  // Collect instance powers.
  InstPowers inst_pwrs;
  for (const Instance *inst : insts) {
    PowerResult inst_power = power(inst, scene);
    inst_pwrs.push_back(std::make_pair(inst, inst_power));
  }
  
  // Sort by total power (descending)
  sort(inst_pwrs, instPowerGreater);
  return inst_pwrs;
}

////////////////////////////////////////////////////////////////

void
Power::power(const Scene *scene,
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

  ensureActivities(scene);
  ensureInstPowers();
  ClkNetwork *clk_network = scene_->mode()->clkNetwork();
  for (auto [inst, inst_power] : instance_powers_) {
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      if (cell->isMacro()
	  || cell->isMemory()
          || cell->interfaceTiming())
	macro.incr(inst_power);
      else if (cell->isPad())
	pad.incr(inst_power);
      else if (inClockNetwork(inst, clk_network))
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
Power::inClockNetwork(const Instance *inst,
                      const ClkNetwork *clk_network)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyOutput()
        && !clk_network->isClock(pin)) {
      delete pin_iter;
      return false;
    }
  }
  delete pin_iter;
  return true;
}

PowerResult
Power::power(const Instance *inst,
             const Scene *scene)
{
  ensureActivities(scene);
  ensureInstPowers();
  if (network_->isHierarchical(inst)) {
    PowerResult result;
    powerInside(inst, scene, result);
    return result;
  }
  else
    return instance_powers_[inst];
}

void
Power::powerInside(const Instance *hinst,
                   const Scene *scene,
                   PowerResult &result)
{
  InstanceChildIterator *child_iter = network_->childIterator(hinst);
  while (child_iter->hasNext()) {
    Instance *child = child_iter->next();
    if (network_->isHierarchical(child))
      powerInside(child, scene, result);
    else
      result.incr(instance_powers_[child]);
  }
  delete child_iter;
}

////////////////////////////////////////////////////////////////

InstPowers
Power::highestInstPowers(size_t count,
                         const Scene *scene)
{
  InstPowers inst_pwrs;
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    PowerResult pwr = power(inst, scene);
    inst_pwrs.push_back(std::make_pair(inst, pwr));
  }
  delete inst_iter;

  sort(inst_pwrs, instPowerGreater);
  if (inst_pwrs.size() > count)
    inst_pwrs.resize(count);
  return inst_pwrs;
}

////////////////////////////////////////////////////////////////

class ActivitySrchPred : public SearchPred
{
public:
  ActivitySrchPred(const StaState *sta);
  bool searchFrom(const Vertex *from_vertex,
                  const Mode *mode) const override;
  bool searchThru(Edge *edge,
                  const Mode *mode) const override;
  bool searchTo(const Vertex *to_vertex,
                const Mode *mode) const override;
};

ActivitySrchPred::ActivitySrchPred(const StaState *sta) :
  SearchPred(sta)
{
}

bool
ActivitySrchPred::searchFrom(const Vertex *from_vertex,
                             const Mode *mode) const
{
  const Pin *from_pin = from_vertex->pin();
  const Sdc *sdc = mode->sdc();
  return !sdc->isDisabledConstraint(from_pin);
}

bool
ActivitySrchPred::searchThru(Edge *edge,
                             const Mode *mode) const
{
  const Sdc *sdc = mode->sdc();
  const TimingRole *role = edge->role();
  return !(edge->role()->isTimingCheck()
           || sdc->isDisabledConstraint(edge)
           || sdc->isDisabledCondDefault(edge)
           || edge->isBidirectInstPath()
           || edge->isDisabledLoop()
           || role == TimingRole::regClkToQ()
           || role->isLatchDtoQ());
}

bool
ActivitySrchPred::searchTo(const Vertex *,
                           const Mode *) const
{
  return true;
}

////////////////////////////////////////////////////////////////

class PropActivityVisitor : public VertexVisitor, StaState
{
public:
  PropActivityVisitor(Power *power,
                      const Mode *mode,
		      BfsFwdIterator *bfs);
  virtual VertexVisitor *copy() const;
  virtual void visit(Vertex *vertex);
  InstanceSet &visitedRegs() { return visited_regs_; }
  void init();
  float maxChange() const { return max_change_; }
  const Pin *maxChangePin() const { return max_change_pin_; }

private:
  bool setActivityCheck(const Pin *pin,
                        PwrActivity &activity);

  static constexpr float change_tolerance_ = .01;
  InstanceSet visited_regs_;
  float max_change_;
  const Pin *max_change_pin_;
  BfsFwdIterator *bfs_;
  Power *power_;
  const Mode *mode_;
};

PropActivityVisitor::PropActivityVisitor(Power *power,
                                         const Mode *mode,
					 BfsFwdIterator *bfs) :
  StaState(power),
  visited_regs_(network_),
  max_change_(0.0),
  max_change_pin_(nullptr),
  bfs_(bfs),
  power_(power),
  mode_(mode)
{
}

VertexVisitor *
PropActivityVisitor::copy() const
{
  return new PropActivityVisitor(power_, mode_, bfs_);
}

void
PropActivityVisitor::init()
{
  max_change_ = 0.0;
  max_change_pin_ = nullptr;
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
      bfs_->enqueueAdjacentVertices(vertex, mode_);
    }
  }
}

static float
percentChange(float value,
              float prev)
{
  if (prev == 0.0) {
    if (value == 0.0)
      return 0.0;
    else
      return 1.0;
  }
  else
    return abs(value - prev) / prev;
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
  float density_delta = percentChange(activity.density(),
                                      prev_activity.density());
  float duty_delta = percentChange(activity.duty(), prev_activity.duty());
  if (density_delta > max_change_) {
    max_change_ = density_delta;
    max_change_pin_ = pin;
  }
  if (duty_delta > max_change_) {
    max_change_ = duty_delta;
    max_change_pin_ = pin;
  }
  bool changed = density_delta > change_tolerance_
    || duty_delta > change_tolerance_
    || activity.origin() != prev_activity.origin();;
  power_->setActivity(pin, activity);
  return changed;
}

////////////////////////////////////////////////////////////////

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
Power::ensureActivities(const Scene *scene)
{
  Stats stats(debug_, report_);
  if (scene != scene_) {
    scene_ = scene;
    activities_valid_ = false;
    instance_powers_.clear();
  }

  if (!activities_valid_) {
    // No need to propagate activites if global activity is set.
    if (!global_activity_.isSet()) {
      // Clear existing activities.
      activity_map_.clear();
      seq_activity_map_.clear();

      // Initialize default input activity (after sdc is defined)
      // unless it has been set by command.
      if (input_activity_.origin() == PwrActivityOrigin::unknown) {
        float min_period = clockMinPeriod(scene_->mode()->sdc());
        float density = 0.1 / (min_period != 0.0
                               ? min_period
                               : units_->timeUnit()->scale());
        input_activity_.set(density, 0.5, PwrActivityOrigin::input);
      }
      ActivitySrchPred activity_srch_pred(this);
      BfsFwdIterator bfs(BfsIndex::other, &activity_srch_pred, this);
      seedActivities(bfs);
      PropActivityVisitor visitor(this, scene_->mode(), &bfs);
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
        debugPrint(debug_, "power_activity", 1, "Pass %d change %.2f %s",
                   pass,
                   visitor.maxChange(),
                   network_->pathName(visitor.maxChangePin()));
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
    if (!scene_->mode()->sdc()->isLeafPinClock(pin)
	&& !network_->direction(pin)->isInternal()) {
      debugPrint(debug_, "power_activity", 3, "seed %s",
                 vertex->to_string(this).c_str());
      if (hasUserActivity(pin))
	setActivity(pin, userActivity(pin));
      else
	// Default inputs without explicit activities to the input default.
	setActivity(pin, input_activity_);
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      bfs.enqueueAdjacentVertices(vertex, scene_->mode());
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
          && clk_func->op() == FuncExpr::Op::not_
          && clk_func->left()->op() == FuncExpr::Op::port;
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
Power::ensureInstPowers()
{
  if (!instance_powers_valid_) {
    findInstPowers();
    instance_powers_valid_ = true;
  }
}

void
Power::findInstPowers()
{
  Stats stats(debug_, report_);
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      PowerResult inst_power = power(inst, cell, scene_);
      instance_powers_[inst] = inst_power;
    }
  }
  delete inst_iter;
  stats.report("Find power");
}

PowerResult
Power::power(const Instance *inst,
	     LibertyCell *cell,
             const Scene *scene)
{
  PowerResult result;
  findInternalPower(inst, cell, scene, result);
  findSwitchingPower(inst, cell, scene, result);
  findLeakagePower(inst, cell, scene, result);
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
                         const Scene *scene,
                         // Return values.
                         PowerResult &result)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    LibertyPort *to_port = network_->libertyPort(to_pin);
    if (to_port) {
      float load_cap = to_port->direction()->isAnyOutput()
        ? graph_delay_calc_->loadCap(to_pin, scene, MinMax::max())
        : 0.0;
      PwrActivity activity = findActivity(to_pin);
      if (to_port->direction()->isAnyOutput())
        findOutputInternalPower(to_port, inst, cell, activity,
                                load_cap, scene, result);
      if (to_port->direction()->isAnyInput())
        findInputInternalPower(to_pin, to_port, inst, cell, activity,
                               load_cap, scene, result);
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
                              const Scene *scene,
			      // Return values.
			      PowerResult &result)
{
  const MinMax *min_max = MinMax::max();
  LibertyCell *scene_cell = cell->sceneCell(scene, min_max);
  const LibertyPort *scene_port = port->scenePort(scene, min_max);
  if (scene_cell && scene_port) {
    const InternalPowerSeq &internal_pwrs = scene_cell->internalPowers(scene_port);
    if (!internal_pwrs.empty()) {
      debugPrint(debug_, "power", 2, "internal input %s/%s cap %s",
                 network_->pathName(inst),
                 port->name(),
                 units_->capacitanceUnit()->asString(load_cap));
      debugPrint(debug_, "power", 2, "       when  act/ns duty  energy    power");
      const Pvt *pvt = scene->sdc()->operatingConditions(MinMax::max());
      Vertex *vertex = graph_->pinLoadVertex(pin);
      float internal = 0.0;
      for (InternalPower *pwr : internal_pwrs) {
        const char *related_pg_pin = pwr->relatedPgPin();
        float energy = 0.0;
        int rf_count = 0;
        for (const RiseFall *rf : RiseFall::range()) {
          float slew = getSlew(vertex, rf, scene);
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
          const LibertyPort *out_scene_port = findExprOutPort(when);
          if (out_scene_port) {
            LibertyPort *out_port = findLinkPort(cell, out_scene_port);
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
               const Scene *scene)
{

  const MinMax *min_max = MinMax::max();
  const Pin *pin = vertex->pin();
  const ClkNetwork *clk_network = scene->mode()->clkNetwork();
  if (clk_network->isIdealClock(pin))
    return clk_network->idealClkSlew(pin, rf, min_max);
  else {
    DcalcAPIndex slew_index = scene->dcalcAnalysisPtIndex(min_max);
    return delayAsFloat(graph_->slew(vertex, rf, slew_index));
  }
}

float
Power::getMinRfSlew(const Pin *pin)
{
  Vertex *vertex, *bidir_vertex;
  graph_->pinVertices(pin, vertex, bidir_vertex);
  if (vertex) {
    const MinMax *min_max = MinMax::min();
    Slew mm_slew = min_max->initValue();
    for (DcalcAPIndex ap_index = 0;
         ap_index < dcalcAnalysisPtCount();
         ap_index++) {
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
  case FuncExpr::Op::port:
    port = expr->port();
    if (port && port->direction()->isAnyOutput())
      return expr->port();
    return nullptr;
  case FuncExpr::Op::not_:
    port = findExprOutPort(expr->left());
    if (port)
      return port;
    return nullptr;
  case FuncExpr::Op::or_:
  case FuncExpr::Op::and_:
  case FuncExpr::Op::xor_:
    port = findExprOutPort(expr->left());
    if (port)
      return port;
    port = findExprOutPort(expr->right());
    if (port)
      return port;
    return nullptr;
  case FuncExpr::Op::one:
  case FuncExpr::Op::zero:
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
                               const Scene *scene,
			       // Return values.
			       PowerResult &result)
{
  debugPrint(debug_, "power", 2, "internal output %s/%s cap %s",
             network_->pathName(inst),
             to_port->name(),
             units_->capacitanceUnit()->asString(load_cap));
  const MinMax *min_max = MinMax::max();
  const Pvt *pvt = scene->sdc()->operatingConditions(min_max);
  LibertyCell *scene_cell = cell->sceneCell(scene, min_max);
  const LibertyPort *to_scene_port = to_port->scenePort(scene, min_max);
  FuncExpr *func = to_port->function();

  map<const char*, float, StringLessIf> pg_duty_sum;
  for (InternalPower *pwr : scene_cell->internalPowers(to_scene_port)) {
    const LibertyPort *from_scene_port = pwr->relatedPort();
    if (from_scene_port) {
      const Pin *from_pin = findLinkPin(inst, from_scene_port);
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
  for (InternalPower *pwr : scene_cell->internalPowers(to_scene_port)) {
    FuncExpr *when = pwr->when();
    const char *related_pg_pin = pwr->relatedPgPin();
    float duty = findInputDuty(inst, func, pwr);
    Vertex *from_vertex = nullptr;
    bool positive_unate = true;
    const LibertyPort *from_scene_port = pwr->relatedPort();
    const Pin *from_pin = nullptr;
    if (from_scene_port) {
      positive_unate = isPositiveUnate(scene_cell, from_scene_port, to_scene_port);
      from_pin = findLinkPin(inst, from_scene_port);
      if (from_pin)
	from_vertex = graph_->pinLoadVertex(from_pin);
    }
    float energy = 0.0;
    int rf_count = 0;
    for (const RiseFall *to_rf : RiseFall::range()) {
      // Use unateness to find from_rf.
      const RiseFall *from_rf = positive_unate ? to_rf : to_rf->opposite();
      float slew = from_vertex
        ? getSlew(from_vertex, from_rf, scene)
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
               from_scene_port ? from_scene_port->name() : "-" ,
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
  const LibertyPort *from_scene_port = pwr->relatedPort();
  if (from_scene_port) {
    LibertyPort *from_port = findLinkPort(network_->libertyCell(inst),
                                          from_scene_port);
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
      else if (scene_->mode()->clkNetwork()->isClock(from_vertex->pin()))
	return 0.5;
      return 0.5;
    }
  }
  return 0.0;
}

// Hack to find cell port that corresponds to scene_port.
LibertyPort *
Power::findLinkPort(const LibertyCell *cell,
                    const LibertyPort *scene_port)
{
  return cell->findLibertyPort(scene_port->name());
}

Pin *
Power::findLinkPin(const Instance *inst,
                   const LibertyPort *scene_port)
{
  const LibertyCell *cell = network_->libertyCell(inst);
  LibertyPort *port = findLinkPort(cell, scene_port);
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
                          const Scene *scene,
                          // Return values.
                          PowerResult &result)
{
  LibertyCell *scene_cell = cell->sceneCell(scene, MinMax::max());
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *to_pin = pin_iter->next();
    const LibertyPort *to_port = network_->libertyPort(to_pin);
    if (to_port) {
      float load_cap = to_port->direction()->isAnyOutput()
        ? graph_delay_calc_->loadCap(to_pin, scene, MinMax::max())
        : 0.0;
      PwrActivity activity = findActivity(to_pin);
      if (to_port->direction()->isAnyOutput()) {
        float volt = portVoltage(scene_cell, to_port, scene, MinMax::max());
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
                        const Scene *scene,
			// Return values.
			PowerResult &result)
{
  LibertyCell *scene_cell = cell->sceneCell(scene, MinMax::max());
  float cond_leakage = 0.0;
  bool found_cond = false;
  float uncond_leakage = 0.0;
  bool found_uncond = false;
  float cond_duty_sum = 0.0;
  for (LeakagePower *leak : *scene_cell->leakagePowers()) {
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
Power::pinActivity(const Pin *pin,
                   const Scene *scene)
{
  ensureActivities(scene);
  return findActivity(pin);
}

PwrActivity
Power::findActivity(const Pin *pin)
{
  const Mode *mode = scene_->mode();
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex && mode->clkNetwork()->isClock(pin)) {
    PwrActivity *activity = findKeyValuePtr(activity_map_, pin);
    if (activity
        && activity->origin() != PwrActivityOrigin::unknown)
      return *activity;
    const Clock *clk = findClk(pin);
    float duty = clockDuty(clk);
    return PwrActivity(2.0 / clk->period(), duty, PwrActivityOrigin::clock);
  }
  else if (global_activity_.isSet())
    return global_activity_;
  else {
    PwrActivity *activity = findKeyValuePtr(activity_map_, pin);
    if (activity
        && activity->origin() != PwrActivityOrigin::unknown)
      return *activity;
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
                   const Scene *scene,
                   const MinMax *min_max)
{
  return pgNameVoltage(cell, port->relatedPowerPin(), scene, min_max);
}

float
Power::pgNameVoltage(LibertyCell *cell,
		     const char *pg_port_name,
                     const Scene *scene,
                     const MinMax *min_max)
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

  Pvt *pvt = scene->sdc()->operatingConditions(min_max);
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
    sort(annotated_pins, PinPathNameLess(sdc_network_));
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

    sort(unannotated_pins, PinPathNameLess(sdc_network_));
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
        && !user_activity_map_.contains(pin))
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
Power::clockMinPeriod(const Sdc *sdc)
{
  const ClockSeq &clks = sdc->clocks();
  if (!clks.empty()) {
    float min_period = INF;
    for (const Clock *clk : clks)
      min_period = min(min_period, clk->period());
    return min_period;
  }
  else
    return 0.0;
}

void
Power::powerInvalid()
{
  activities_valid_ = false;
  instance_powers_.clear();
  scene_ = nullptr;
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
