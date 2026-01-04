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

#include "Sim.hh"

// https://davidkebo.com/cudd
#include "cudd.h"

#include "ContainerHelpers.hh"
#include "Error.hh"
#include "Mutex.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Stats.hh"
#include "FuncExpr.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Sequential.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "Mode.hh"

namespace sta {

static LogicValue
logicNot(LogicValue value);
static const Pin *
findDrvrPin(const Pin *pin,
            Network *network);

Sim::Sim(StaState *sta) :
  StaState(sta),
  mode_(nullptr),
  observer_(nullptr),
  valid_(false),
  incremental_(false),
  const_func_pins_(network_),
  const_func_pins_valid_(false),
  invalid_insts_(network_),
  invalid_drvr_pins_(network_),
  invalid_load_pins_(network_),
  instances_to_annotate_(network_),
  bdd_(sta)
{
}

Sim::~Sim()
{
  delete observer_;
}

void
Sim::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  // Notify sub-components.
  observer_->copyState(sta);
}

void
Sim::setMode(Mode *mode)
{
  mode_ = mode;
}

TimingSense
Sim::functionSense(const FuncExpr *expr,
                   const Pin *input_pin,
                   const Instance *inst)
{
  debugPrint(debug_, "sim", 4, "find sense pin %s %s",
             network_->pathName(input_pin),
             expr->to_string().c_str());
  bool increasing, decreasing;
  {
    LockGuard lock(bdd_lock_);
    DdNode *bdd = funcBddSim(expr, inst);
    DdManager *cudd_mgr = bdd_.cuddMgr();
    LibertyPort *input_port = network_->libertyPort(input_pin);
    DdNode *input_node = bdd_.ensureNode(input_port);
    unsigned int input_index = Cudd_NodeReadIndex(input_node);
    increasing = (Cudd_Increasing(cudd_mgr, bdd, input_index)
                  == Cudd_ReadOne(cudd_mgr));
    decreasing = (Cudd_Decreasing(cudd_mgr, bdd, input_index)
                  == Cudd_ReadOne(cudd_mgr));
    Cudd_RecursiveDeref(cudd_mgr, bdd);
    bdd_.clearVarMap();
  }
  TimingSense sense;
  if (increasing && decreasing)
    sense = TimingSense::none;
  else if (increasing)
    sense = TimingSense::positive_unate;
  else if (decreasing)
    sense = TimingSense::negative_unate;
  else
    sense = TimingSense::non_unate;
  debugPrint(debug_, "sim", 4, " %s", to_string(sense));
  return sense;
}

LogicValue
Sim::evalExpr(const FuncExpr *expr,
              const Instance *inst)
{
  LockGuard lock(bdd_lock_);
  DdNode *bdd = funcBddSim(expr, inst);
  LogicValue value = LogicValue::unknown;
  DdManager *cudd_mgr = bdd_.cuddMgr();
  if (bdd == Cudd_ReadLogicZero(cudd_mgr))
    value = LogicValue::zero;
  else if (bdd == Cudd_ReadOne(cudd_mgr))
    value = LogicValue::one;

  if (bdd) {
    Cudd_RecursiveDeref(bdd_.cuddMgr(), bdd);
    bdd_.clearVarMap();
  }
  return value;
}

// BDD with instance pin values substituted.
DdNode *
Sim::funcBddSim(const FuncExpr *expr,
                const Instance *inst)
{
  DdNode *bdd = bdd_.funcBdd(expr);
  DdManager *cudd_mgr = bdd_.cuddMgr();
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    const LibertyPort *port = network_->libertyPort(pin);
    DdNode *port_node = bdd_.findNode(port);
    if (port_node) {
      LogicValue value = simValue(pin);
      int var_index = Cudd_NodeReadIndex(port_node);
      switch (value) {
      case LogicValue::zero:
        bdd = Cudd_bddCompose(cudd_mgr, bdd, Cudd_ReadLogicZero(cudd_mgr), var_index);
        Cudd_Ref(bdd);
        break;
      case LogicValue::one:
        bdd = Cudd_bddCompose(cudd_mgr, bdd, Cudd_ReadOne(cudd_mgr), var_index);
        Cudd_Ref(bdd);
        break;
      default:
        break;
      }
    }
  }
  delete pin_iter;
  return bdd;
}

static LogicValue
logicNot(LogicValue value)
{
  static LogicValue logic_not[5] = {LogicValue::one, LogicValue::zero,
                                    LogicValue::unknown, LogicValue::unknown,
                                    LogicValue::unknown};
  return logic_not[int(value)];
}

void
Sim::clear()
{
  valid_ = false;
  incremental_ = false;
  const_func_pins_.clear();
  const_func_pins_valid_ = false;
  instances_to_annotate_.clear();
  invalid_insts_.clear();
  invalid_drvr_pins_.clear();
  invalid_load_pins_.clear();
  clearSimValues();
}

void
Sim::setObserver(SimObserver *observer)
{
  delete observer_;
  observer_ = observer;
}

SimObserver::SimObserver(StaState *sta) :
  StaState(sta)
{
}

LogicValue
Sim::simValue(const Vertex *vertex) const
{
  if (vertex->hasSimValue()) {
    LogicValue value;
    bool exists;
    findKeyValue(sim_value_map_, vertex->pin(), value, exists);
    if (exists)
      return value;
  }
  return LogicValue::unknown;
}

LogicValue
Sim::simValue(const Pin *pin) const
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex)
    return simValue(vertex);
  LogicValue value;
  bool exists;
  findKeyValue(sim_value_map_, pin, value, exists);
  if (exists)
    return value;
  else
    return LogicValue::unknown;
}

bool
Sim::isConstant(const Vertex *vertex) const
{
  LogicValue value = simValue(vertex);
  return value == LogicValue::zero
    || value == LogicValue::one;
}

bool
Sim::isConstant(const Pin *pin) const
{
  LogicValue value = simValue(pin);
  return value == LogicValue::zero
    || value == LogicValue::one;
}

TimingSense
Sim::simTimingSense(const Edge *edge) const
{
  if (edge->hasSimSense()) {
    TimingSense sense;
    bool exists;
    findKeyValue(edge_timing_sense_map_, edge, sense, exists);
    if (exists)
      return sense;
  }
  return TimingSense::unknown;
}

void
Sim::setSimTimingSense(Edge *edge,
                       TimingSense sense)
{
  if (sense == TimingSense::unknown)
    edge_timing_sense_map_.erase(edge);
  else {
    edge_timing_sense_map_[edge] = sense;
    edge->setHasSimSense(true);
  }
}

bool
Sim::isDisabledCond(const Edge *edge) const
{
  return edge->hasDisabledCond()
    && edge_disabled_cond_set_.contains(edge);
}

////////////////////////////////////////////////////////////////

void
Sim::ensureConstantsPropagated()
{
  if (!valid_) {
    Stats stats(debug_, report_);
    ensureConstantFuncPins();
    instances_to_annotate_.clear();
    if (incremental_) {
      seedInvalidConstants();
      propagateToInvalidLoads();
      propagateFromInvalidDrvrsToLoads();
    }
    else {
      clearSimValues();
      seedConstants();
    }
    invalid_insts_.clear();
    propagateConstants(false);
    findDisabledEdges();
    valid_ = true;
    incremental_ = true;

    stats.report("Propagate constants");
  }
}

void
Sim::findLogicConstants()
{
  clear();
  ensureConstantFuncPins();
  setConstFuncPins();
  enqueueConstantPinInputs();
  propagateConstants(true);
  valid_ = true;
}

void
Sim::seedInvalidConstants()
{
  for (const Instance *inst : invalid_insts_)
    eval_queue_.push(inst);
}

void
Sim::propagateToInvalidLoads()
{
  for (const Pin *load_pin : invalid_load_pins_) {
    const Net *net = network_->net(load_pin);
    if (net && network_->isGround(net))
      setPinValue(load_pin, LogicValue::zero);
    else if (net && network_->isPower(net))
      setPinValue(load_pin, LogicValue::one);
    else {
      const Pin *drvr_pin = findDrvrPin(load_pin, network_);
      if (drvr_pin)
        propagateDrvrToLoad(drvr_pin, load_pin);
    }
  }
  invalid_load_pins_.clear();
}

void
Sim::propagateFromInvalidDrvrsToLoads()
{
  for (const Pin *drvr_pin : invalid_drvr_pins_) {
    LogicValue value = const_func_pins_.contains(drvr_pin)
      ? pinConstFuncValue(drvr_pin)
      : simValue(drvr_pin);
    PinConnectedPinIterator *load_iter=network_->connectedPinIterator(drvr_pin);
    while (load_iter->hasNext()) {
      const Pin *load_pin = load_iter->next();
      if (load_pin != drvr_pin
          && network_->isLoad(load_pin))
        setPinValue(load_pin, value);
    }
    delete load_iter;
  }
  invalid_drvr_pins_.clear();
}

void
Sim::propagateDrvrToLoad(const Pin *drvr_pin,
                         const Pin *load_pin)
{
  LogicValue value = simValue(drvr_pin);
  setPinValue(load_pin, value);
}

void
Sim::constantsInvalid()
{
  valid_ = false;
  incremental_ = false;
}

void
Sim::ensureConstantFuncPins()
{
  if (!const_func_pins_valid_) {
    LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
    while (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstancePinIterator *pin_iter = network_->pinIterator(inst);
      while (pin_iter->hasNext()) {
        const Pin *pin = pin_iter->next();
        recordConstPinFunc(pin);
      }
      delete pin_iter;
    }
    delete inst_iter;
    const_func_pins_valid_ = true;
  }
}

void
Sim::recordConstPinFunc(const Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    FuncExpr *expr = port->function();
    if (expr
        // Tristate outputs do not force the output to be constant.
        && port->tristateEnable() == nullptr
        && (expr->op() == FuncExpr::Op::zero
            || expr->op() == FuncExpr::Op::one))
      const_func_pins_.insert(pin);
  }
}

void
Sim::deleteInstanceBefore(const Instance *inst)
{
  invalid_insts_.erase(inst);
}

void
Sim::makePinAfter(const Pin *pin)
{
  // Incrementally update const_func_pins_.
  recordConstPinFunc(pin);
}

void
Sim::deletePinBefore(const Pin *pin)
{
  sim_value_map_.erase(pin);
  // Incrementally update const_func_pins_.
  const_func_pins_.erase(pin);
  invalid_load_pins_.erase(pin);
  invalid_drvr_pins_.erase(pin);
  invalid_insts_.insert(network_->instance(pin));
}

void
Sim::connectPinAfter(const Pin *pin)
{
  if (incremental_) {
    recordConstPinFunc(pin);
    if (network_->isLoad(pin))
      invalid_load_pins_.insert(pin);
    if (network_->isDriver(pin))
      invalid_drvr_pins_.insert(pin);
    valid_ = false;
  }
}

void
Sim::disconnectPinBefore(const Pin *pin)
{
  if (incremental_) {
    if (network_->isLoad(pin)) {
      invalid_load_pins_.insert(pin);
      removePropagatedValue(pin);
    }
    if (network_->isDriver(pin))
      invalid_drvr_pins_.insert(pin);
  }
}

void
Sim::pinSetFuncAfter(const Pin *pin)
{
  if (incremental_)
    valid_ = false;
  // Incrementally update const_func_pins_.
  const_func_pins_.erase(pin);
  recordConstPinFunc(pin);
}

void
Sim::seedConstants()
{
  const Sdc *sdc = mode_->sdc();
  // Propagate constants from inputs tied hi/low in the network.
  enqueueConstantPinInputs();
  // Propagate set_logic_zero/one/dc constants.
  setConstraintConstPins(sdc->logicValues());
  // Propagate set_case_analysis constants.
  setConstraintConstPins(sdc->caseLogicValues());
  // Propagate 0/1 constant functions.
  setConstFuncPins();
}

void
Sim::propagateConstants(bool thru_sequentials)
{
  while (!eval_queue_.empty()) {
    const Instance *inst = eval_queue_.front();
    eval_queue_.pop();
    evalInstance(inst, thru_sequentials);
  }
}

void
Sim::setConstraintConstPins(const LogicValueMap &value_map)
{
  for (const auto [pin, value] : value_map) {
    debugPrint(debug_, "sim", 2, "case pin %s = %c",
               network_->pathName(pin),
               logicValueString(value));
    if (network_->isHierarchical(pin)) {
      // Set the logic value on pins inside the instance of a hierarchical pin.
      bool pin_is_output = network_->direction(pin)->isAnyOutput();
      PinConnectedPinIterator *pin_iter=network_->connectedPinIterator(pin);
      while (pin_iter->hasNext()) {
        const Pin *pin1 = pin_iter->next();
        if (network_->isLeaf(pin1)
            && network_->direction(pin1)->isAnyInput()
            && ((pin_is_output && !network_->isInside(pin1, pin))
                || (!pin_is_output && network_->isInside(pin1, pin))))
          setPinValue(pin1, value);
      }
      delete pin_iter;
    }
    else
      setPinValue(pin, value);
  }
}

// Propagate constants from outputs with constant functions
// (tie high and tie low cell instances).
void
Sim::setConstFuncPins()
{
  for (const Pin *pin : const_func_pins_) {
    LogicValue value = pinConstFuncValue(pin);
    setPinValue(pin, value);
    debugPrint(debug_, "sim", 2, "func pin %s = %c",
               network_->pathName(pin),
               logicValueString(value));
  }
}

LogicValue
Sim::pinConstFuncValue(const Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    FuncExpr *expr = port->function();
    if (expr->op() == FuncExpr::Op::zero)
      return LogicValue::zero;
    else if (expr->op() == FuncExpr::Op::one)
      return LogicValue::one;
  }
  return LogicValue::unknown;
}

void
Sim::enqueueConstantPinInputs()
{
  ConstantPinIterator *const_iter = network_->constantPinIterator();
  while (const_iter->hasNext()) {
    LogicValue value;
    const Pin *pin;
    const_iter->next(pin, value);
    debugPrint(debug_, "sim", 2, "network constant pin %s = %c",
               network_->pathName(pin),
               logicValueString(value));
    setPinValue(pin, value);
  }
  delete const_iter;
}

void
Sim::removePropagatedValue(const Pin *pin)
{
  Instance *inst = network_->instance(pin);
  invalid_insts_.insert(inst);
  valid_ = false;

  const Sdc *sdc = mode_->sdc();
  LogicValue constraint_value;
  bool exists;
  sdc->caseLogicValue(pin, constraint_value, exists);
  if (!exists) {
    sdc->logicValue(pin, constraint_value, exists);
    if (!exists) {
      debugPrint(debug_, "sim", 2, "pin %s remove prop constant",
                 network_->pathName(pin));
      setSimValue(pin, LogicValue::unknown);
    }
  }
}

void
Sim::setPinValue(const Pin *pin,
                 LogicValue value)
{
  const Sdc *sdc = mode_->sdc();
  LogicValue constraint_value;
  bool exists;
  sdc->caseLogicValue(pin, constraint_value, exists);
  if (!exists)
    sdc->logicValue(pin, constraint_value, exists);
  if (exists
      && value != constraint_value) {
    if (value != LogicValue::unknown)
      report_->warn(1521, "propagated logic value %c differs from constraint value of %c on pin %s.",
                    logicValueString(value),
                    logicValueString(constraint_value),
                    sdc_network_->pathName(pin));
  }
  else {
    debugPrint(debug_, "sim", 3, "pin %s = %c",
               network_->pathName(pin),
               logicValueString(value));
    bool value_changed = false;
    value_changed |= value != simValue(pin);
    setSimValue(pin, value);
    if (value_changed) {
      Instance *inst = network_->instance(pin);
      instances_to_annotate_.insert(inst);

      if (network_->isLeaf(inst)
          && network_->direction(pin)->isAnyInput()) {
        if (eval_queue_.empty() 
            || (eval_queue_.back() != inst))
          eval_queue_.push(inst);
      }
      else if (network_->isDriver(pin)) {
        // Enqueue instances with input pins connected to net.
        PinConnectedPinIterator *pin_iter=network_->connectedPinIterator(pin);
        while (pin_iter->hasNext()) {
          const Pin *pin1 = pin_iter->next();
          if (pin1 != pin
              && network_->isLoad(pin1))
            setPinValue(pin1, value);
        }
        delete pin_iter;
      }
    }
  }
}

void
Sim::evalInstance(const Instance *inst,
                  bool thru_sequentials)
{
  debugPrint(debug_, "sim", 2, "eval %s", network_->pathName(inst));
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      PortDirection *dir = port->direction();
      if (dir->isAnyOutput()) {
        LogicValue value = LogicValue::unknown;
        FuncExpr *expr = port->function();
        LibertyCell *cell = port->libertyCell();
        if (expr) {
          FuncExpr *tri_en_expr = port->tristateEnable();
          if (tri_en_expr) {
            if (evalExpr(tri_en_expr, inst) == LogicValue::one) {
              value = evalExpr(expr, inst);
              debugPrint(debug_, "sim", 2, " %s tri_en=1 %s = %c",
                         port->name(),
                         expr->to_string().c_str(),
                         logicValueString(value));
            }
          }
          else {
            LibertyPort *expr_port = expr->port();
            Sequential *sequential = (thru_sequentials && expr_port) 
              ? cell->outputPortSequential(expr_port)
              : nullptr;
            if (sequential) {
              value = evalExpr(sequential->data(), inst);
              if (expr_port == sequential->outputInv())
                value = logicNot(value);
              debugPrint(debug_, "sim", 2, " %s seq %s = %c",
                         port->name(),
                         expr->to_string().c_str(),
                         logicValueString(value));
            }
            else {
              value = evalExpr(expr, inst);
              debugPrint(debug_, "sim", 2, " %s %s = %c",
                         port->name(),
                         expr->to_string().c_str(),
                         logicValueString(value));
            }
          }
        }
        else if (port->isClockGateOut()) {
          value = clockGateOutValue(inst);
          debugPrint(debug_, "sim", 2, " %s gated_clk = %c",
                     port->name(),
                     logicValueString(value));
        }
        if (value != simValue(pin))
          setPinValue(pin, value);
      }
    }
  }
  delete pin_iter;
}

LogicValue
Sim::clockGateOutValue(const Instance *inst)
{
  LibertyCell *cell = network_->libertyCell(inst);
  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    if (port->isClockGateClock()
        || port->isClockGateEnable()) {
      Pin *gclk_pin = network_->findPin(inst, port);
      if (gclk_pin
          && simValue(gclk_pin) == LogicValue::zero)
        return LogicValue::zero;
    }
  }
  return LogicValue::unknown;
}

void
Sim::setSimValue(const Pin *pin,
                 LogicValue value)
{
  if (value != simValue(pin)) {
    if (value == LogicValue::unknown)
      sim_value_map_.erase(pin);
    else {
      sim_value_map_[pin] = value;
      Vertex *vertex, *bidirect_drvr_vertex;
      graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
      if (vertex)
        vertex->setHasSimValue(true);
      if (bidirect_drvr_vertex)
        bidirect_drvr_vertex->setHasSimValue(true);
    }
    if (observer_)
      observer_->valueChangeAfter(pin);
  }
}

TimingSense
Sim::functionSense(const Instance *inst,
                   const Pin *from_pin,
                   const Pin *to_pin)
{
  if (isConstant((from_pin)))
    return TimingSense::none;
  else {
    LibertyPort *from_port = network_->libertyPort(from_pin);
    LibertyPort *to_port = network_->libertyPort(to_pin);
    if (to_port) {
      const FuncExpr *func = to_port->function();
      if (func) {
        PortDirection *to_dir = to_port->direction();
        if (to_dir->isAnyTristate()) {
          FuncExpr *tri_func = to_port->tristateEnable();
          if (tri_func) {
            if (func->hasPort(from_port)) {
              // from_pin is an input to the to_pin function.
              LogicValue tri_enable = evalExpr(tri_func, inst);
              if (tri_enable == LogicValue::zero)
                // Tristate is disabled.
                return TimingSense::none;
              else
                return functionSense(func, from_pin, inst);
            }
          }
          else {
            // Missing tristate enable function.
            if (func->hasPort(from_port))
              // from_pin is an input to the to_pin function.
              return functionSense(func, from_pin, inst);
          }
        }
        else {
          if (func->hasPort(from_port))
            // from_pin is an input to the to_pin function.
            return functionSense(func, from_pin, inst);
        }
      }
    }
    return TimingSense::unknown;
  }
}

static const Pin *
findDrvrPin(const Pin *pin,
            Network *network)
{
  PinSet *drvrs = network->drivers(pin);
  if (drvrs && !drvrs->empty())
    return *drvrs->begin();
  else
    return nullptr;
}

void
Sim::clearSimValues()
{
  for (auto const [pin, value] : sim_value_map_)
    observer_->valueChangeAfter(pin);
  sim_value_map_.clear();
  edge_timing_sense_map_.clear();
  edge_disabled_cond_set_.clear();
}

////////////////////////////////////////////////////////////////

void
Sim::setIsDisabledCond(Edge *edge,
                       bool disabled)
{
  if (!disabled)
    edge_disabled_cond_set_.erase(edge);
  else {
    edge_disabled_cond_set_.insert(edge);
    edge->setHasDisabledCond(true);
  }
}

bool
Sim::isDisabledCond(Edge *edge,
                    const Instance *inst,
                    const Pin *from_pin,
                    const Pin *to_pin)
{
  bool is_disabled;
  FuncExpr *disable_cond;
  isDisabledCond(edge, inst, from_pin, to_pin, is_disabled, disable_cond);
  return is_disabled;
}

void
Sim::isDisabledCond(Edge *edge,
                    const Instance *inst,
                    const Pin *from_pin,
                    const Pin *to_pin,
                    // Return values.
                    bool &is_disabled,
                    FuncExpr *&disable_cond)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  FuncExpr *cond = arc_set->cond();
  if (cond) {
    LogicValue cond_value = evalExpr(cond, inst);
    disable_cond = cond;
    is_disabled = (cond_value == LogicValue::zero);
  }
  else {
    // Unconditional "default" arc set is disabled if another
    // conditional arc from/to the same pins is enabled (condition
    // evals to logic one).
    LibertyCell *cell = network_->libertyCell(inst);
    LibertyPort *from_port = network_->libertyPort(from_pin);
    LibertyPort *to_port = network_->libertyPort(to_pin);
    is_disabled = false;
    for (TimingArcSet *cond_set : cell->timingArcSets(from_port, to_port)) {
      FuncExpr *cond = cond_set->cond();
      if (cond && evalExpr(cond, inst) == LogicValue::one) {
        disable_cond = cond;
        is_disabled = true;
        break;
      }
    }
  }
}

bool
Sim::isDisabledMode(Edge *edge,
                    const Instance *inst)
{
  bool is_disabled;
  FuncExpr *disable_cond;
  isDisabledMode(edge, inst, is_disabled, disable_cond);
  return is_disabled;
}

void
Sim::isDisabledMode(Edge *edge,
                    const Instance *inst,
                    // Return values.
                    bool &is_disabled,
                    FuncExpr *&disable_cond)
{
  // Default values.
  is_disabled = false;
  disable_cond = 0;
  TimingArcSet *arc_set = edge->timingArcSet();
  const char *mode_name = arc_set->modeName();
  const char *mode_value = arc_set->modeValue();
  if (mode_name && mode_value) {
    LibertyCell *cell = network_->libertyCell(inst);
    ModeDef *mode_def = cell->findModeDef(mode_name);
    if (mode_def) {
      ModeValueDef *value_def = mode_def->findValueDef(mode_value);
      if (value_def) {
        FuncExpr *cond = value_def->cond();
        if (cond) {
          LogicValue cond_value = evalExpr(cond, inst);
          if (cond_value == LogicValue::zero) {
            // For a mode value to be disabled by having a value of
            // logic zero one mode value must logic one.
            for (const auto [name, value_def] : *mode_def->values()) {
              if (value_def) {
                FuncExpr *cond1 = value_def->cond();
                if (cond1) {
                  LogicValue cond_value1 = evalExpr(cond1, inst);
                  if (cond_value1 == LogicValue::one) {
                    disable_cond = cond;
                    is_disabled = true;
                    break;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

// Find graph edges disabled by constant values.
void
Sim::findDisabledEdges()
{
   for (const Instance *inst : instances_to_annotate_)
     findDisabledEdges(inst);
   instances_to_annotate_.clear();
}

void
Sim::findDisabledEdges(const Instance *inst)
{
  debugPrint(debug_, "sim", 4, "annotate %s", network_->pathName(inst));
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Vertex *vertex = graph_->pinDrvrVertex(pin);
    if (vertex)
      findDisabledEdges(inst, pin, vertex);
  }
  delete pin_iter;
}

void
Sim::findDisabledEdges(const Instance *inst,
                       const Pin *pin,
                       Vertex *vertex)
{
  bool fanin_disables_changed = false;
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (!edge->role()->isWire()) {
      Vertex *from_vertex = edge->from(graph_);
      Pin *from_pin = from_vertex->pin();
      TimingSense sense = TimingSense::unknown;
      bool is_disabled_cond = false;
      // Set timing sense on edges in instances that have constant pins.
      if (isConstant(from_vertex))
        sense = TimingSense::none;
      else
        sense = functionSense(inst, from_pin, pin);

      if (sense != TimingSense::none)
        // Disable conditional timing edges based on constant pins.
        is_disabled_cond = isDisabledCond(edge, inst, from_pin, pin)
          // Disable mode conditional timing
          // edges based on constant pins.
          || isDisabledMode(edge,inst);

      bool disables_changed = false;
      if (sense != simTimingSense(edge)) {
        setSimTimingSense(edge, sense);
        disables_changed = true;
        fanin_disables_changed = true;
      }
      if (is_disabled_cond != isDisabledCond(edge)) {
        setIsDisabledCond(edge, is_disabled_cond);
        disables_changed = true;
        fanin_disables_changed = true;
      }
      if (observer_ && disables_changed)
        observer_->fanoutEdgesChangeAfter(from_vertex->pin());
    }
  }
  if (observer_ && fanin_disables_changed)
    observer_->faninEdgesChangeAfter(vertex->pin());
}

} // namespace
