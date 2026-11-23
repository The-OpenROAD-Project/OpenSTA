// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <algorithm>  // max
#include <array>
#include <cmath>      // abs
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Bfs.hh"
#include "ClkNetwork.hh"
#include "Clock.hh"
#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Delay.hh"
#include "EnumNameMap.hh"
#include "Error.hh"
#include "FuncExpr.hh"
#include "Graph.hh"
#include "GraphClass.hh"
#include "GraphDelayCalc.hh"
#include "Hash.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Liberty.hh"
#include "LibertyClass.hh"
#include "MinMax.hh"
#include "Mode.hh"
#include "Network.hh"
#include "NetworkClass.hh"
#include "NetworkCmp.hh"
#include "Path.hh"
#include "PortDirection.hh"
#include "PowerClass.hh"
#include "ReportPower.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "SdcClass.hh"
#include "Search.hh"
#include "SearchClass.hh"
#include "Sequential.hh"
#include "Stats.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "Units.hh"
#include "VertexVisitor.hh"
#include "cudd.h"
#include "search/Levelize.hh"
#include "search/Sim.hh"

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

static bool
isPositiveUnate(const LibertyCell *cell,
                const LibertyPort *from,
                const LibertyPort *to);

static EnumNameMap<PwrActivityOrigin> pwr_activity_origin_map = {
    {PwrActivityOrigin::global, "global"},
    {PwrActivityOrigin::input, "input"},
    {PwrActivityOrigin::user, "user"},
    {PwrActivityOrigin::vcd, "vcd"},
    {PwrActivityOrigin::saif, "saif"},
    {PwrActivityOrigin::propagated, "propagated"},
    {PwrActivityOrigin::clock, "clock"},
    {PwrActivityOrigin::constant, "constant"},
    {PwrActivityOrigin::unknown, "unknown"}};

Power::Power(StaState *sta) :
  StaState(sta),
  bdd_(sta)
{
}

void
Power::clear()
{
  global_activity_.init();
  input_activity_.init();
  user_activity_map_.clear();
  clearNonSdc();
}

void
Power::clearNonSdc()
{
  // user_activity_map_ is kept; deletePinBefore removes deleted pins.
  seq_activity_map_.clear();
  activity_map_.clear();
  launch_period_cache_.clear();
  convergence_warned_ = false;
  slew_limit_warned_ = false;
  powerInvalid();
}

void
Power::deletePinBefore(const Pin *pin)
{
  unsetUserActivity(pin);
  activity_map_.erase(pin);
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
  debugPrint(debug_, "power_activity", 3, "set {} {:.2e} {:.2f} {}",
             network_->pathName(pin), activity.density(), activity.duty(),
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
  const auto &[inst, port] = pin;
  return hashSum(network_->id(inst), port->id());
}

bool
SeqPinEqual::operator()(const SeqPin &pin1,
                        const SeqPin &pin2) const
{
  const auto &[inst1, port1] = pin1;
  const auto &[inst2, port2] = pin2;
  return inst1 == inst2 && port1 == port2;
}

////////////////////////////////////////////////////////////////

void
Power::reportDesign(const Scene *scene,
                    int digits)
{
  PowerResult total, sequential, combinational, clock, macro, pad;
  power(scene, total, sequential, combinational, clock, macro, pad);
  ReportPower report_power(this);
  report_power.reportDesign(total, sequential, combinational, clock, macro, pad,
                            digits);
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

  report_->report("{{");
  reportPowerRowJson("Sequential", sequential, digits, ",");
  reportPowerRowJson("Combinational", combinational, digits, ",");
  reportPowerRowJson("Clock", clock, digits, ",");
  reportPowerRowJson("Macro", macro, digits, ",");
  reportPowerRowJson("Pad", pad, digits, ",");
  reportPowerRowJson("Total", total, digits, "");
  report_->report("}}");
}

void
Power::reportInstsJson(const InstanceSeq &insts,
                       const Scene *scene,
                       int digits)
{
  InstPowers inst_pwrs = sortInstsByPower(insts, scene);

  report_->report("[");
  bool first = true;
  for (const InstPower &inst_pwr : inst_pwrs) {
    if (!first) {
      report_->report(",");
    }
    first = false;
    reportPowerInstJson(inst_pwr.first, inst_pwr.second, digits);
  }
  report_->report("]");
}

void
Power::reportPowerRowJson(std::string_view type,
                          const PowerResult &power,
                          int digits,
                          std::string_view eol)
{
  float internal = power.internal();
  float switching = power.switching();
  float leakage = power.leakage();
  float total = power.total();

  report_->report("  \"{}\": {{", type);
  report_->report("    \"internal\": {:.{}e},", internal, digits);
  report_->report("    \"switching\": {:.{}e},", switching, digits);
  report_->report("    \"leakage\": {:.{}e},", leakage, digits);
  report_->report("    \"total\": {:.{}e}", total, digits);
  std::string line = "  }";
  if (!eol.empty())
    line += eol;
  report_->reportLine(line);
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

  report_->report("{{");
  report_->report("  \"name\": \"{}\",", network_->pathName(inst));
  report_->report("  \"internal\": {:.{}e},", internal, digits);
  report_->report("  \"switching\": {:.{}e},", switching, digits);
  report_->report("  \"leakage\": {:.{}e},", leakage, digits);
  report_->report("  \"total\": {:.{}e}", total, digits);
  report_->report("}}");
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
    inst_pwrs.emplace_back(inst, inst_power);
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
      if (cell->isMacro() || cell->isMemory() || cell->interfaceTiming())
        macro.incr(inst_power);
      else if (cell->isPad())
        pad.incr(inst_power);
      else if (inClockNetwork(inst, clk_network))
        clock.incr(inst_power);
      else if (cell->isSequential())
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
    if (network_->direction(pin)->isAnyOutput() && !clk_network->isClock(pin)) {
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
    inst_pwrs.emplace_back(inst, pwr);
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
  PropActivityVisitor(const PropActivityVisitor &visitor);
  VertexVisitor *copy() const override;
  void visit(Vertex *vertex) override;
  InstanceSet &visitedRegs() { return visited_regs_; }
  void init();
  float maxChange() const { return max_change_; }
  const Pin *maxChangePin() const { return max_change_pin_; }
  // A pin whose propagated activity was limited to 1/slew, if any.
  const Pin *slewLimitedPin() const { return slew_limited_pin_; }

private:
  bool setActivityCheck(const Pin *pin,
                        PwrActivity &activity);

  static constexpr float change_tolerance_ = .01;
  InstanceSet visited_regs_;
  float max_change_{0.0};
  const Pin *max_change_pin_{nullptr};
  const Pin *slew_limited_pin_{nullptr};
  BfsFwdIterator *bfs_;
  Power *power_;
  const Mode *mode_;
};

PropActivityVisitor::PropActivityVisitor(Power *power,
                                         const Mode *mode,
                                         BfsFwdIterator *bfs) :
  StaState(power),
  visited_regs_(network_),
  bfs_(bfs),
  power_(power),
  mode_(mode)
{
}

PropActivityVisitor::PropActivityVisitor(const PropActivityVisitor &visitor) :
  PropActivityVisitor(visitor.power_, visitor.mode_, visitor.bfs_)
{
}

VertexVisitor *
PropActivityVisitor::copy() const
{
  return new PropActivityVisitor(*this);
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
  debugPrint(debug_, "power_activity", 3, "visit {}",
             vertex->to_string(this));
  bool changed = false;
  if (power_->hasUserActivity(pin)) {
    // Copy so the annotation is not modified.
    PwrActivity activity = power_->userActivity(pin);
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
          PwrActivity to_activity(from_activity.density(), from_activity.duty(),
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
            float p2 = std::clamp(activity2.duty(), 0.0F, 1.0F);
            // The gate is enabled when the enable or the test enable is 1.
            float p_test = power_->clockGateTestDuty(inst);
            p2 = 1.0F - (1.0F - p2) * (1.0F - p_test);
            // The enable is latched, so only clock edges make gated clock edges.
            PwrActivity activity(activity1.density() * p2,
                                 p1 * p2, PwrActivityOrigin::propagated);
            changed = setActivityCheck(gclk, activity);
            debugPrint(debug_, "power_activity", 3, "gated_clk {} {:.2e} {:.2f}",
                       network_->pathName(gclk), activity.density(),
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
        if (cell->isSequential()
            || (test_cell && test_cell->isSequential())) {
          debugPrint(debug_, "power_activity", 3, "pending seq {}",
                     network_->pathName(inst));
          visited_regs_.insert(inst);
        }
        // Gated clock cells latch the enable so there is no EN->GCLK timing arc.
        if (cell->isClockGate()) {
          const Pin *enable, *clk, *gclk;
          power_->clockGatePins(inst, enable, clk, gclk);
          if (gclk)
            visited_regs_.insert(inst);
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
    return std::abs(value - prev) / prev;
}

// Return true if the activity changed.
bool
PropActivityVisitor::setActivityCheck(const Pin *pin,
                                      PwrActivity &activity)
{
  // Propagated activity is limited to one transition per launching clock
  // period (two for clock nets), and to 1/slew. Annotated activities are not
  // limited, and neither are loads, which copy their driver's (already
  // limited) activity.
  if (activity.origin() == PwrActivityOrigin::propagated
      && network_->isDriver(pin)) {
    float max_prop_density = power_->maxPropagatedDensity(pin);
    if (activity.density() > max_prop_density) {
      debugPrint(debug_, "power_activity", 2, "limit {} {:.2e} -> {:.2e}",
                 network_->pathName(pin), activity.density(), max_prop_density);
      activity.setDensity(max_prop_density);
    }
    float slew = power_->getAvgRfSlew(pin);
    if (slew > 0.0 && !delayInf(slew, this)
        && activity.density() > 1.0 / slew) {
      activity.setDensity(1.0 / slew);
      slew_limited_pin_ = pin;
    }
  }
  PwrActivity &prev_activity = power_->activity(pin);
  float density_delta = percentChange(activity.density(), prev_activity.density());
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
    || activity.origin() != prev_activity.origin();
  power_->setActivity(pin, activity);
  return changed;
}

////////////////////////////////////////////////////////////////

// Probability that any test enable is 1. The test enables are taken as
// independent of each other and of the enable.
// Test pins without a driver or a constant are ignored, as in
// Sim::clockGateOutValue, even with set_power_activity on the pin; use
// set_case_analysis to hold an undriven test pin.
float
Power::clockGateTestDuty(const Instance *inst)
{
  float p_off = 1.0;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const LibertyPort *port = network_->libertyPort(pin);
    if (port && port->isClockGateTest()) {
      PinSet *drvrs = network_->drivers(pin);
      LogicValue value = scene_->mode()->sim()->simValue(pin);
      if ((drvrs && !drvrs->empty()) || value != LogicValue::unknown)
        p_off *= 1.0F - std::clamp(findActivity(pin).duty(), 0.0F, 1.0F);
    }
  }
  delete pin_iter;
  return 1.0F - p_off;
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
    // Skip pins without a liberty port.
    if (port) {
      if (port->isClockGateEnable())
        enable = pin;
      if (port->isClockGateClock())
        clk = pin;
      if (port->isClockGateOut())
        gclk = pin;
    }
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////

PwrActivity
Power::evalActivity(FuncExpr *expr,
                    const Instance *inst)
{
  LibertyPort *func_port = expr->port();
  if (func_port && func_port->direction()->isInternal())
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
                    FuncExpr *when,
                    const Instance *inst)
{
  DdNode *bdd = bdd_.funcBdd(expr);
  DdNode *var_node = bdd_.findNode(from_port);
  unsigned var_index = Cudd_NodeReadIndex(var_node);
  DdNode *diff = Cudd_bddBooleanDiff(bdd_.cuddMgr(), bdd, var_index);
  Cudd_Ref(diff);
  // Weight state dependent internal power groups by P(when).
  if (when) {
    DdNode *when_bdd = whenBdd(when, inst);
    DdNode *diff_when = Cudd_bddAnd(bdd_.cuddMgr(), diff, when_bdd);
    Cudd_Ref(diff_when);
    Cudd_RecursiveDeref(bdd_.cuddMgr(), when_bdd);
    Cudd_RecursiveDeref(bdd_.cuddMgr(), diff);
    diff = diff_when;
  }
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
      criticalError(2400, "unknown cudd constant");
  }
  else {
    float duty0 = evalBddDuty(Cudd_E(bdd), inst);
    float duty1 = evalBddDuty(Cudd_T(bdd), inst);
    unsigned int index = Cudd_NodeReadIndex(bdd);
    int var_index = Cudd_ReadPerm(bdd_.cuddMgr(), index);
    const LibertyPort *port = bdd_.varIndexPort(var_index);
    // Expand internal (IQ) variables too; returning their duty ignored the
    // cofactors and complement (e.g. "!IQ").
    float var_duty;
    if (port->direction()->isInternal())
      var_duty = findSeqActivity(inst, const_cast<LibertyPort *>(port)).duty();
    else {
      const Pin *pin = findLinkPin(inst, port);
      if (pin == nullptr)
        return 0.0;
      var_duty = findActivity(pin).duty();
    }
    float duty = duty0 * (1.0 - var_duty) + duty1 * var_duty;
    if (Cudd_IsComplement(bdd))
      duty = 1.0 - duty;
    return duty;
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
  float change_density;
  if (evalBddChangeDensity(bdd, inst, change_density))
    return change_density;
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
      debugPrint(debug_, "power_activity", 3, "{} {:.3e} * {:.3f} = {:.3e}",
                 network_->pathName(pin), var_activity.density(), diff_duty,
                 var_density);
    }
  }
  return density;
}

// Summing density * P(boolean difference) over inputs (evalBddActivity)
// counts simultaneous input transitions as separate output transitions,
// which overestimates deep synchronous logic by orders of magnitude
// (issue #333). Instead use the probability that the function value
// differs between consecutive clock periods, with each input an
// independent two state process. Based on PR #438.
// Returns false for clock inputs, which switch twice per period.
bool
Power::evalBddChangeDensity(DdNode *bdd,
                            const Instance *inst,
                            // Return value.
                            float &density)
{
  // Sample once per period of the fastest clock launching the inputs.
  float period = INF;
  std::vector<std::pair<unsigned, PwrActivity>> var_activities;
  for (const auto [port, var_node] : bdd_.portVarMap()) {
    PwrActivity activity;
    if (port->direction()->isInternal())
      activity = findSeqActivity(inst, const_cast<LibertyPort *>(port));
    else {
      const Pin *pin = findLinkPin(inst, port);
      if (pin == nullptr || scene_->mode()->clkNetwork()->isClock(pin))
        return false;
      activity = findActivity(pin);
      float pin_period = launchClkPeriod(pin);
      if (pin_period > 0.0)
        period = std::min(period, pin_period);
    }
    var_activities.emplace_back(Cudd_NodeReadIndex(var_node), activity);
  }
  if (period == INF)
    period = clockMinPeriod(scene_->mode()->sdc());
  if (period == 0.0)
    period = units_->timeUnit()->scale();

  // var_probs[var index][previous value][current value].
  std::map<unsigned, std::array<std::array<float, 2>, 2>> var_probs;
  for (const auto &[var_index, activity] : var_activities) {
    float duty = std::clamp(activity.duty(), 0.0F, 1.0F);
    // Changes per period cannot exceed 2 * min(duty, 1 - duty).
    float half_change = std::min(activity.density() * period,
                                 2.0F * std::min(duty, 1.0F - duty)) / 2.0F;
    var_probs[var_index] = {{{1.0F - duty - half_change, half_change},
                             {half_change, duty - half_change}}};
  }

  DdManager *cudd_mgr = bdd_.cuddMgr();
  auto level = [cudd_mgr](DdNode *node) {
    return Cudd_IsConstant(node)
      ? std::numeric_limits<int>::max()
      : Cudd_ReadPerm(cudd_mgr, Cudd_NodeReadIndex(node));
  };
  auto cofactor = [](DdNode *node, unsigned var_index, int value) {
    DdNode *regular = Cudd_Regular(node);
    if (Cudd_IsConstant(regular) || Cudd_NodeReadIndex(regular) != var_index)
      return node;
    DdNode *child = value ? Cudd_T(regular) : Cudd_E(regular);
    return Cudd_IsComplement(node) ? Cudd_Not(child) : child;
  };
  // Probability that from (previous period) and to (current period) differ.
  std::map<std::pair<DdNode *, DdNode *>, float> cache;
  std::function<float(DdNode *, DdNode *)> change_prob =
    [&](DdNode *from, DdNode *to) {
      if (Cudd_IsConstant(from) && Cudd_IsConstant(to))
        return from == to ? 0.0F : 1.0F;
      auto key = std::make_pair(from, to);
      auto itr = cache.find(key);
      if (itr != cache.end())
        return itr->second;
      unsigned var_index = Cudd_NodeReadIndex(level(from) <= level(to) ? from : to);
      auto probs_itr = var_probs.find(var_index);
      if (probs_itr == var_probs.end())
        return 0.0F;
      const auto &probs = probs_itr->second;
      float prob = 0.0F;
      for (int prev : {0, 1}) {
        for (int curr : {0, 1})
          prob += probs[prev][curr] * change_prob(cofactor(from, var_index, prev),
                                                  cofactor(to, var_index, curr));
      }
      cache[key] = prob;
      return prob;
    };
  density = change_prob(bdd, bdd) / period;
  return true;
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
    instance_powers_valid_ = false;
    launch_period_cache_.clear();
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
        if (min_period == 0.0)
          min_period = units_->timeUnit()->scale();
        float density = 0.1 / min_period;
        input_activity_.set(density, 0.5, PwrActivityOrigin::input);
      }
      ActivitySrchPred activity_srch_pred(this);
      BfsFwdIterator bfs(BfsIndex::other, &activity_srch_pred, this);
      launch_period_cache_.clear();
      seedActivities(bfs);
      seedFuncLessOutputActivities(bfs);
      PropActivityVisitor visitor(this, scene_->mode(), &bfs);
      // Propagate activities through combinational logic.
      bfs.visit(levelize_->maxLevel(), &visitor);
      // Propagate activiities through registers.
      InstanceSet regs = std::move(visitor.visitedRegs());
      int pass = 1;
      while (!regs.empty() && pass <= max_activity_passes_) {
        visitor.init();
        for (const Instance *reg : regs)
          // Propagate activiities across register D->Q.
          seedRegOutputActivities(reg, bfs);
        // Propagate register output activities through
        // combinational logic.
        bfs.visit(levelize_->maxLevel(), &visitor);
        regs = std::move(visitor.visitedRegs());
        debugPrint(debug_, "power_activity", 1, "Pass {} change {:.2f} {}",
                   pass,
                   visitor.maxChange(),
                   network_->pathName(visitor.maxChangePin()));
        pass++;
      }
      if (!regs.empty() && !convergence_warned_) {
        report_->warn(2402, "power activity propagation through registers did "
                      "not converge after {} passes.", max_activity_passes_);
        convergence_warned_ = true;
      }
      const Pin *slew_limited_pin = visitor.slewLimitedPin();
      if (slew_limited_pin && !slew_limit_warned_) {
        report_->warn(2403, "propagated activity limited to 1/slew on some "
                      "pins, e.g. {}.", network_->pathName(slew_limited_pin));
        slew_limit_warned_ = true;
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
      debugPrint(debug_, "power_activity", 3, "seed {}",
                 vertex->to_string(this));
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
    else if (cell->isClockGate())
      seedClkGateOutputActivities(inst, bfs);
  }
}

void
Power::seedRegOutputActivities(const Instance *inst,
                               const LibertyCell *test_cell,
                               const SequentialSeq &seqs,
                               BfsFwdIterator &bfs)
{
  for (const Sequential &seq : seqs) {
    seedRegOutputActivities(inst, seq, seq.output(), false);
    seedRegOutputActivities(inst, seq, seq.outputInv(), true);
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
        // Include functions of the state such as "!IQ", not only the state
        // port itself.
        if (vertex && func
            && (func->hasPort(seq.output()) || func->hasPort(seq.outputInv()))) {
          debugPrint(debug_, "power_reg", 1, "enqueue reg output {}",
                     vertex->to_string(this));
          bfs.enqueue(vertex);
        }
      }
    }
    delete pin_iter;
  }
}

void
Power::seedRegOutputActivities(const Instance *reg,
                               const Sequential &seq,
                               LibertyPort *output,
                               bool invert)
{
  const Pin *out_pin = network_->findPin(reg, output);
  if (!hasUserActivity(out_pin)) {
    PwrActivity in_activity = evalActivity(seq.data(), reg);
    float in_density = in_activity.density();
    float in_duty = in_activity.duty();
    // Default propagates input density/duty thru reg/latch.
    float out_density = in_density;
    float out_duty = in_duty;
    PwrActivity clk_activity = evalActivity(seq.clock(), reg);
    float clk_density = clk_activity.density();
    // A register output changes at most once per active clock edge
    // (clk_density / 2), with probability at most 2 * min(duty, 1 - duty),
    // so the output is continuous in the input density.
    if (seq.isRegister()) {
      out_density = std::min(in_density,
                             std::min(in_duty, 1 - in_duty) * clk_density);
      // The state has no activity yet when next_state refers to it (e.g.
      // "(D&DE) | (IQ&!DE)"), so solve the hold loop instead.
      evalRegHoldActivity(reg, seq, clk_density, out_density, out_duty);
    }
    else if (in_density > clk_density / 2 && seq.isLatch()) {
      PwrActivity clk_activity = evalActivity(seq.clock(), reg);
      float clk_duty = clk_activity.duty();
      FuncExpr *clk_func = seq.clock();
      bool clk_invert = clk_func && clk_func->op() == FuncExpr::Op::not_
          && clk_func->left()->op() == FuncExpr::Op::port;
      if (clk_invert)
        out_density = in_density * (1 - clk_duty);
      else
        out_density = in_density * clk_duty;
    }
    if (invert)
      out_duty = 1.0 - out_duty;
    PwrActivity out_activity(out_density, out_duty, PwrActivityOrigin::propagated);
    setSeqActivity(reg, output, out_activity);
  }
}

// For a next_state that refers to the register's own state, each active
// clock edge sets the state with probability u = P(next_state | IQ=0) and
// clears it with probability v = P(!next_state | IQ=1). As a two-state
// process the duty is u / (u + v) and the state changes 2uv / (u + v) times
// per active edge. That assumes inputs that are random every cycle, which
// is too high for slow inputs (an enable that is always on makes the cell
// a plain register), so density is only lowered from its value on entry.
// Returns false if next_state does not refer to the state.
bool
Power::evalRegHoldActivity(const Instance *reg,
                           const Sequential &seq,
                           float clk_density,
                           // Return values.
                           float &density,
                           float &duty)
{
  FuncExpr *data = seq.data();
  LibertyPort *iq = seq.output();
  LibertyPort *iqn = seq.outputInv();
  if (data == nullptr
      || !((iq && data->hasPort(iq)) || (iqn && data->hasPort(iqn))))
    return false;
  DdManager *mgr = bdd_.cuddMgr();
  DdNode *bdd = bdd_.funcBdd(data);
  float state_duty[2];
  for (int value : {0, 1}) {
    DdNode *node = bdd;
    Cudd_Ref(node);
    for (const auto &[port, port_value] : {std::pair(iq, value),
                                           std::pair(iqn, 1 - value)}) {
      DdNode *var = port ? bdd_.findNode(port) : nullptr;
      if (var) {
        DdNode *cofactor = Cudd_bddCompose(mgr, node,
                                           port_value ? Cudd_ReadOne(mgr)
                                                      : Cudd_ReadLogicZero(mgr),
                                           Cudd_NodeReadIndex(var));
        Cudd_Ref(cofactor);
        Cudd_RecursiveDeref(mgr, node);
        node = cofactor;
      }
    }
    state_duty[value] = evalBddDuty(node, reg);
    Cudd_RecursiveDeref(mgr, node);
  }
  Cudd_RecursiveDeref(mgr, bdd);
  bdd_.clearVarMap();
  float set = state_duty[0];
  float clear = 1.0 - state_duty[1];
  if (set + clear > 0.0) {
    duty = set / (set + clear);
    // There are clk_density / 2 active edges.
    density = std::min(density, set * clear / (set + clear) * clk_density);
  }
  else {
    // The state never changes; its value is unknown.
    duty = 0.5;
    density = 0.0;
  }
  return true;
}

void
Power::seedClkGateOutputActivities(const Instance *inst,
                                   BfsFwdIterator &bfs)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    LibertyPort *port = network_->libertyPort(pin);
    if (port && port->isClockGateOut()) {
      Vertex *vertex = graph_->pinDrvrVertex(pin);
      if (vertex)
        bfs.enqueue(vertex);
    }
  }
  delete pin_iter;
}

// Outputs of cells without a liberty function or sequential model (memories,
// macros, statetable cells) are never reached by propagation. Seed them like
// primary inputs, using any annotated (vcd/saif/user) activity.
void
Power::seedFuncLessOutputActivities(BfsFwdIterator &bfs)
{
  const Mode *mode = scene_->mode();
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *cell = network_->libertyCell(inst);
    if (cell && cell->sequentials().empty() && cell->testCell() == nullptr
        && !cell->isClockGate()) {
      InstancePinIterator *pin_iter = network_->pinIterator(inst);
      while (pin_iter->hasNext()) {
        const Pin *pin = pin_iter->next();
        LibertyPort *port = network_->libertyPort(pin);
        Vertex *vertex = graph_->pinDrvrVertex(pin);
        if (port && port->direction()->isAnyOutput() && port->function() == nullptr
            && vertex && !mode->clkNetwork()->isClock(pin)) {
          setActivity(pin, hasUserActivity(pin) ? userActivity(pin) : input_activity_);
          bfs.enqueueAdjacentVertices(vertex, mode);
        }
      }
      delete pin_iter;
    }
  }
  delete inst_iter;
}

// Period of the fastest clock launching paths through pin, or of the
// fastest clock in the design if none does. Zero if there are no clocks.
float
Power::launchClkPeriod(const Pin *pin)
{
  auto cache_itr = launch_period_cache_.find(pin);
  if (cache_itr != launch_period_cache_.end())
    return cache_itr->second;
  const Clock *clk = findClk(pin);
  float period = clk ? clk->period() : 0.0;
  if (period <= 0.0)
    period = clockMinPeriod(scene_->mode()->sdc());
  launch_period_cache_[pin] = period;
  return period;
}

float
Power::maxPropagatedDensity(const Pin *pin)
{
  float period = launchClkPeriod(pin);
  // Without clocks, use the sampling period of evalBddChangeDensity and of
  // the default input activity: one time unit, so the limit is one
  // transition per ns with ns libraries and per ps with ps libraries.
  if (period <= 0.0)
    period = units_->timeUnit()->scale();
  float transitions_per_period =
      scene_->mode()->clkNetwork()->isClock(pin) ? 2.0 : 1.0;
  return transitions_per_period / period;
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
  debugPrint(debug_, "power", 2, "find power {}", sdc_network_->pathName(inst));
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
        findOutputInternalPower(to_port, inst, cell, activity, load_cap, scene,
                                result);
      if (to_port->direction()->isAnyInput())
        findInputInternalPower(to_pin, to_port, inst, cell, activity, load_cap,
                               scene, result);
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
    const InternalPowerPtrSeq &internal_pwrs =
        scene_cell->internalPowers(scene_port);
    if (!internal_pwrs.empty()) {
      debugPrint(debug_, "power", 2, "internal input {}/{} cap {}",
                 network_->pathName(inst), port->name(),
                 units_->capacitanceUnit()->asString(load_cap));
      debugPrint(debug_, "power", 2, "       when  act/ns duty  energy    power");
      const Pvt *pvt = scene->sdc()->operatingConditions(MinMax::max());
      Vertex *vertex = graph_->pinLoadVertex(pin);
      float internal = 0.0;
      for (const InternalPower *pwr : internal_pwrs) {
        LibertyPort *related_pg_pin = pwr->relatedPgPin();
        float energy = 0.0;
        int rf_count = 0;
        for (const RiseFall *rf : RiseFall::range()) {
          float slew = getSlew(vertex, rf, scene);
          if (!delayInf(slew, this)) {
            float table_energy = pwr->power(rf, pvt, slew, load_cap);
            energy += table_energy;
            rf_count++;
          }
        }
        if (rf_count)
          energy /= rf_count;  // average non-inf energies
        float duty = 1.0;      // fallback default
        FuncExpr *when = pwr->when();
        // Weight each group by P(when).
        if (when)
          duty = whenDuty(when, inst);
        float port_internal = energy * duty * activity.density();
        debugPrint(debug_, "power", 2, " {} {}  {:.2f}  {:.2f} {:9.2e} {:9.2e} {}",
                   port->name(), when ? when->to_string() : "",
                   activity.density() * 1e-9, duty, energy, port_internal,
                   related_pg_pin ? related_pg_pin->name() : "no pg_pin");
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
Power::getAvgRfSlew(const Pin *pin)
{
  float slew = 0.0;
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex)
    slew = (getSlew(vertex, RiseFall::rise(), scene_)
            + getSlew(vertex, RiseFall::fall(), scene_)) / 2.0;
  return slew;
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
  debugPrint(debug_, "power", 2, "internal output {}/{} cap {}",
             network_->pathName(inst), to_port->name(),
             units_->capacitanceUnit()->asString(load_cap));
  const MinMax *min_max = MinMax::max();
  const Pvt *pvt = scene->sdc()->operatingConditions(min_max);
  LibertyCell *scene_cell = cell->sceneCell(scene, min_max);
  const LibertyPort *to_scene_port = to_port->scenePort(scene, min_max);
  FuncExpr *func = to_port->function();

  std::map<LibertyPort *, float> pg_duty_sum;
  for (const InternalPower *pwr : scene_cell->internalPowers(to_scene_port)) {
    const LibertyPort *from_scene_port = pwr->relatedPort();
    if (from_scene_port) {
      const Pin *from_pin = findLinkPin(inst, from_scene_port);
      float from_density = findActivity(from_pin).density();
      float duty = findInputDuty(inst, func, pwr);
      LibertyPort *related_pg_pin = pwr->relatedPgPin();
      // Note related_pg_pin may be null.
      pg_duty_sum[related_pg_pin] += from_density * duty;
    }
  }

  debugPrint(debug_, "power", 2,
             "             when act/ns  duty  wgt   energy    power");
  float internal = 0.0;
  for (const InternalPower *pwr : scene_cell->internalPowers(to_scene_port)) {
    FuncExpr *when = pwr->when();
    LibertyPort *related_pg_pin = pwr->relatedPgPin();
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
      if (!delayInf(slew, this)) {
	float table_energy = pwr->power(to_rf, pvt, slew, load_cap);
	energy += table_energy;
	rf_count++;
      }
    }
    if (rf_count)
      energy /= rf_count;  // average non-inf energies
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
    debugPrint(debug_, "power", 2, "{} -> {} {}  {:.3f} {:.3f} {:.3f} {:9.2e} {:9.2e} {}",
               from_scene_port ? from_scene_port->name() : "-", to_port->name(),
               when ? when->to_string() : "", to_activity.density() * 1e-9,
               duty, weight, energy, port_internal,
               related_pg_pin ? related_pg_pin->name() : "no pg_pin");
    internal += port_internal;
  }
  result.incrInternal(internal);
}

float
Power::findInputDuty(const Instance *inst,
                     FuncExpr *func,
                     const InternalPower *pwr)
{
  const LibertyPort *from_scene_port = pwr->relatedPort();
  if (from_scene_port) {
    LibertyPort *from_port =
        findLinkPort(network_->libertyCell(inst), from_scene_port);
    const Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin) {
      FuncExpr *when = pwr->when();
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      if (func && func->hasPort(from_port)) {
        float duty = evalDiffDuty(func, from_port, when, inst);
        return duty;
      }
      else if (when)
        return whenDuty(when, inst);
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
    return sense == TimingSense::positive_unate || sense == TimingSense::non_unate;
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
        debugPrint(debug_, "power", 2,
                   "switching {}/{} activity = {:.2e} volt = {:.2f} {:.3e}",
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

static bool
funcHasInternalPort(const FuncExpr *expr)
{
  bool has_internal = false;
  if (expr) {
    if (expr->op() == FuncExpr::Op::port)
      has_internal = expr->port()->direction()->isInternal();
    else
      has_internal = funcHasInternalPort(expr->left())
        || funcHasInternalPort(expr->right());
  }
  return has_internal;
}

float
Power::whenDuty(FuncExpr *when,
                const Instance *inst)
{
  DdNode *bdd = whenBdd(when, inst);
  float duty = evalBddDuty(bdd, inst);
  Cudd_RecursiveDeref(bdd_.cuddMgr(), bdd);
  bdd_.clearVarMap();
  return duty;
}

DdNode *
Power::whenBdd(FuncExpr *when,
               const Instance *inst)
{
  DdManager *cudd_mgr = bdd_.cuddMgr();
  DdNode *bdd = bdd_.funcBdd(when);
  const Sim *sim = scene_->mode()->sim();
  // Combinational outputs only: sequential outputs (Q = IQ) keep the pin
  // activity, which may be annotated. Outputs held constant (case analysis)
  // keep the constant. Tristate and bidirect pins can differ from their
  // functions, so they are kept too.
  std::map<const LibertyPort *, bool> substitutable;
  auto canSubstitute = [&](const LibertyPort *port) {
    bool can;
    auto itr = substitutable.find(port);
    if (itr != substitutable.end())
      can = itr->second;
    else {
      FuncExpr *func = port->function();
      const Pin *pin = findLinkPin(inst, port);
      can = port->direction()->isOutput() && port->tristateEnable() == nullptr
        && func && !func->hasPort(port) && !funcHasInternalPort(func)
        && !(pin && sim->isConstant(pin));
      substitutable.emplace(port, can);
    }
    return can;
  };
  // Repeat until nothing changes, because a function can name other
  // outputs; the pass limit stops cyclic functions.
  size_t passes = network_->libertyCell(inst)->portBitCount() + 1;
  bool changed = true;
  while (changed && passes > 0) {
    changed = false;
    passes--;
    std::vector<std::pair<const LibertyPort *, DdNode *>> port_vars(
      bdd_.portVarMap().begin(), bdd_.portVarMap().end());
    int *support = Cudd_SupportIndex(cudd_mgr, bdd);
    for (const auto &[port, var_node] : port_vars) {
      unsigned var_index = Cudd_NodeReadIndex(var_node);
      if (support[var_index] && canSubstitute(port)) {
        DdNode *func_bdd = bdd_.funcBdd(port->function());
        DdNode *composed = Cudd_bddCompose(cudd_mgr, bdd, func_bdd,
                                           var_index);
        Cudd_Ref(composed);
        if (composed != bdd)
          changed = true;
        Cudd_RecursiveDeref(cudd_mgr, bdd);
        Cudd_RecursiveDeref(cudd_mgr, func_bdd);
        bdd = composed;
      }
    }
    free(support);
  }
  return bdd;
}

// Leakage totals for one power/gnd pin.
class LeakageSummary
{
public:
  bool cond_exists{false};
  float cond_leakage{0.0};
  float cond_duty_sum{0.0};
  bool cond_true_exists{false};
  float cond_true_leakage{0.0};
  bool uncond_exists{false};
  float uncond_leakage{0.0};
};

void
Power::findLeakagePower(const Instance *inst,
                        LibertyCell *cell,
                        const Scene *scene,
                        // Return values.
                        PowerResult &result)
{
  LibertyCell *scene_cell = cell->sceneCell(scene, MinMax::max());
  std::map<LibertyPort *, LeakageSummary> leakage_summaries;
  Sim *sim = scene->mode()->sim();
  for (const LeakagePower &pwr : scene_cell->leakagePowers()) {
    LibertyPort *pg_port = pwr.relatedPgPort();
    if (pg_port == nullptr || pg_port->pwrGndType() == PwrGndType::primary_power) {
      std::string pg_name = pg_port ? pg_port->name() : "?";
      LeakageSummary &sum = leakage_summaries[pg_port];
      float leakage = pwr.power();
      FuncExpr *when = pwr.when();
      if (when) {
        LogicValue when_value = sim->evalExpr(when, inst);
        if (when_value == LogicValue::one) {
          debugPrint(debug_, "power", 2, "leakage {}/{} {}=1 {:.3e}",
                     cell->name(),
                     pg_name,
                     when->to_string(),
                     leakage);
          sum.cond_true_leakage = leakage;
          sum.cond_true_exists = true;
        }
        else {
          float cond_duty = whenDuty(when, inst);
          debugPrint(debug_, "power", 2, "leakage {} {} {} {:.3e} * {:.2f}",
                     cell->name(),
                     pg_name,
                     when->to_string(),
                     leakage, cond_duty);
          // Leakage power average weighted by duty.
          sum.cond_leakage += leakage * cond_duty;
          // States with zero leakage are covered too.
          sum.cond_duty_sum += cond_duty;
          sum.cond_exists = true;
        }
      }
      else {
        debugPrint(debug_, "power", 2, "leakage {} {} -- {:.3e}",
                   cell->name(),
                   pg_name,
                   leakage);
        sum.uncond_leakage = leakage;
        sum.uncond_exists = true;
      }
    }
  }

  float cell_leakage;
  bool cell_leakage_exists;
  cell->leakagePower(cell_leakage, cell_leakage_exists);

  if (!leakage_summaries.empty()) {
    for (const auto &[pg_port, sum] : leakage_summaries) {
      float leakage = 0.0;
      if (sum.cond_true_exists)
        leakage = sum.cond_true_leakage;
      else if (sum.cond_exists) {
        leakage = sum.cond_leakage;
        // States no condition covers use the leakage_power group without a
        // when condition, or else cell_leakage_power.
        float duty = std::max(1.0F - sum.cond_duty_sum, 0.0F);
        if (sum.uncond_exists)
          leakage += sum.uncond_leakage * duty;
        else if (cell_leakage_exists)
          leakage += cell_leakage * duty;
      }
      // Without conditional leakage groups, use the unconditional group.
      else if (sum.uncond_exists)
        leakage = sum.uncond_leakage;
      debugPrint(debug_, "power", 2, "leakage {}/{} {:.3e}",
                 cell->name(),
                 pg_port ? pg_port->name() : "?",
                 leakage);
      result.incrLeakage(leakage);
    }
  }
  else
    result.incrLeakage(cell_leakage);
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
  // Constant pins (case analysis, set_logic) do not switch.
  LogicValue sim_value = mode->sim()->simValue(pin);
  if (sim_value == LogicValue::zero)
    return PwrActivity(0.0, 0.0, PwrActivityOrigin::constant);
  if (sim_value == LogicValue::one)
    return PwrActivity(0.0, 1.0, PwrActivityOrigin::constant);
  if (vertex && mode->clkNetwork()->isClock(pin)) {
    PwrActivity *activity = findKeyValuePtr(activity_map_, pin);
    if (activity && activity->origin() != PwrActivityOrigin::unknown)
      return *activity;
    const Clock *clk = findClk(pin);
    if (clk) {
      float duty = clockDuty(clk);
      return PwrActivity(2.0 / clk->period(), duty, PwrActivityOrigin::clock);
    }
    else
      return PwrActivity();
  }
  else if (global_activity_.isSet())
    return global_activity_;
  else {
    PwrActivity *activity = findKeyValuePtr(activity_map_, pin);
    if (activity && activity->origin() != PwrActivityOrigin::unknown)
      return *activity;
  }
  return PwrActivity(0.0, 0.0, PwrActivityOrigin::unknown);
}

float
Power::clockDuty(const Clock *clk)
{
  // Use the generated clock's own waveform (divide_by, edges) when known.
  if (clk->isGenerated() && clk->waveform().size() >= 2 && clk->period() > 0.0) {
    float duty = (clk->waveform()[1] - clk->waveform()[0]) / clk->period();
    if (duty > 0.0 && duty < 1.0)
      return duty;
  }
  if (clk->isGenerated()) {
    const Clock *master = clk->masterClk();
    if (master == nullptr)
      return 0.5;  // punt
    else
      return clockDuty(master);
  }
  else {
    const FloatSeq &waveform = clk->waveform();
    float rise_time = waveform[0];
    float fall_time = waveform[1];
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
  return pgPortVoltage(cell, port->relatedPowerPort(), scene, min_max);
}

float
Power::pgPortVoltage(LibertyCell *cell,
                     const LibertyPort *pg_port,
                     const Scene *scene,
                     const MinMax *min_max)
{
  if (pg_port) {
    const std::string &volt_name = pg_port->voltageName();
    LibertyLibrary *library = cell->libertyLibrary();
    float voltage;
    bool exists;
    library->supplyVoltage(volt_name, voltage, exists);
    if (exists)
      return voltage;
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
    // Only paths of the scene being analyzed; other modes can have faster
    // clocks. No min/max or rise/fall filter.
    VertexPathIterator path_iter(to_vertex, scene_, nullptr, nullptr, this);
    while (path_iter.hasNext()) {
      Path *path = path_iter.next();
      const Clock *path_clk = path->clock(this);
      if (path_clk && (clk == nullptr || path_clk->period() < clk->period()))
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
  for (auto const &[pin, activity] : user_activity_map_) {
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
    report_->report("vcd         {:>5}", vcd_count);
  if (saif_count > 0)
    report_->report("saif        {:>5}", saif_count);
  if (input_count > 0)
    report_->report("input       {:>5}", input_count);
  size_t pin_count = pinCount();
  size_t unannotated_count = pin_count - vcd_count - saif_count - input_count;
  report_->report("unannotated {:>5}", unannotated_count);

  if (report_annotated) {
    PinSeq annotated_pins;
    for (auto const &[pin, activity] : user_activity_map_)
      annotated_pins.push_back(pin);
    sort(annotated_pins, PinPathNameLess(sdc_network_));
    report_->report("Annotated pins:");
    for (const Pin *pin : annotated_pins) {
      const PwrActivity &activity = user_activity_map_[pin];
      PwrActivityOrigin origin = activity.origin();
      const std::string &origin_name = pwr_activity_origin_map.find(origin);
      report_->report("{:>5} {}", origin_name, sdc_network_->pathName(pin));
    }
  }
  if (report_unannotated) {
    PinSeq unannotated_pins;
    PinIterator *pin_iter = network_->pinIterator();
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

    sort(unannotated_pins, PinPathNameLess(sdc_network_));
    report_->report("Unannotated pins:");
    for (const Pin *pin : unannotated_pins) {
      report_->report(" {}", sdc_network_->pathName(pin));
    }
  }
}

// leaf pins - internal pins - power/ground pins + top instance pins
size_t
Power::pinCount()
{
  size_t count = 0;
  PinIterator *pin_iter = network_->pinIterator();
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isTopLevelPort(pin))
      count++;
    else {
      LibertyPort *liberty_port = sdc_network_->libertyPort(pin);
      if (!network_->direction(pin)->isInternal()
          && !network_->direction(pin)->isPowerGround()
          && !(liberty_port && liberty_port->isPwrGnd()))
        count++;
    }
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
      min_period = std::min(min_period, clk->period());
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
  instance_powers_valid_ = false;
  scene_ = nullptr;
}

////////////////////////////////////////////////////////////////

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
  if (std::abs(density_) < min_density)
    density_ = 0.0;
}

bool
PwrActivity::isSet() const
{
  return origin_ != PwrActivityOrigin::unknown;
}

const std::string &
PwrActivity::originName() const
{
  return pwr_activity_origin_map.find(origin_);
}

}  // namespace sta
