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
#include "StaConfig.hh"  // CUDD
#include "Error.hh"
#include "Mutex.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Stats.hh"
#include "PortDirection.hh"
#include "FuncExpr.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "Sim.hh"

#if CUDD
// https://davidkebo.com/cudd
#include "cudd.h"
#endif // CUDD

namespace sta {

static Pin *
findDrvrPin(const Pin *pin,
	    Network *network);

#if CUDD

Sim::Sim(StaState *sta) :
  StaState(sta),
  observer_(nullptr),
  valid_(false),
  incremental_(false),
  const_func_pins_valid_(false),
  // cacheSize = 2^15
  cudd_manager_(Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, 32768, 0))
{
}

Sim::~Sim()
{
  delete observer_;
  if (Cudd_CheckZeroRef(cudd_manager_) > 0)
    internalErrorNoThrow("non-zero cudd reference counts");
  Cudd_Quit(cudd_manager_);
}

TimingSense
Sim::functionSense(const FuncExpr *expr,
		   const Pin *input_pin,
		   const Instance *inst)
{
  debugPrint2(debug_, "sim", 4, "find sense pin %s %s\n",
	      network_->pathName(input_pin),
	      expr->asString());
  bool increasing, decreasing;
  {
    UniqueLock lock(cudd_lock_);
    DdNode *bdd = funcBdd(expr, inst);
    LibertyPort *input_port = network_->libertyPort(input_pin);
    DdNode *input_node = ensureNode(input_port);
    unsigned int input_index = Cudd_NodeReadIndex(input_node);
    increasing = (Cudd_Increasing(cudd_manager_, bdd, input_index)
		  == Cudd_ReadOne(cudd_manager_));
    decreasing = (Cudd_Decreasing(cudd_manager_, bdd, input_index)
		  == Cudd_ReadOne(cudd_manager_));
    Cudd_RecursiveDeref(cudd_manager_, bdd);
    clearSymtab();
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
  debugPrint1(debug_, "sim", 4, " %s\n", timingSenseString(sense));
  return sense;
}

void
Sim::clearSymtab() const
{
  BddSymbolTable::Iterator sym_iter(symtab_);
  while (sym_iter.hasNext()) {
    DdNode *sym_node = sym_iter.next();
    Cudd_RecursiveDeref(cudd_manager_, sym_node);
  }
  symtab_.clear();
}

LogicValue
Sim::evalExpr(const FuncExpr *expr,
	      const Instance *inst) const
{
  UniqueLock lock(cudd_lock_);
  DdNode *bdd = funcBdd(expr, inst);
  LogicValue value = LogicValue::unknown;
  if (bdd == Cudd_ReadLogicZero(cudd_manager_))
    value = LogicValue::zero;
  else if (bdd == Cudd_ReadOne(cudd_manager_))
    value = LogicValue::one;
  if (bdd) {
    Cudd_RecursiveDeref(cudd_manager_, bdd);
    clearSymtab();
  }
  return value;
}

// Returns nullptr if the expression simply references an internal port.
DdNode *
Sim::funcBdd(const FuncExpr *expr,
	     const Instance *inst) const
{
  DdNode *left = nullptr;
  DdNode *right = nullptr;
  DdNode *result = nullptr;
  switch (expr->op()) {
  case FuncExpr::op_port: {
    LibertyPort *port = expr->port();
    Pin *pin = network_->findPin(inst, port);
    // Internal ports don't have instance pins.
    if (pin) {
      LogicValue value = logicValue(pin);
      switch (value) {
      case LogicValue::zero:
	result = Cudd_ReadLogicZero(cudd_manager_);
	break;
      case LogicValue::one:
	result = Cudd_ReadOne(cudd_manager_);
	break;
      default:
	result = ensureNode(port);
	break;
      }
    }
    break;
  }
  case FuncExpr::op_not:
    left = funcBdd(expr->left(), inst);
    if (left)
      result = Cudd_Not(left);
    break;
  case FuncExpr::op_or:
    left = funcBdd(expr->left(), inst);
    right = funcBdd(expr->right(), inst);
    if (left && right)
      result = Cudd_bddOr(cudd_manager_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_and:
    left = funcBdd(expr->left(), inst);
    right = funcBdd(expr->right(), inst);
    if (left && right)
      result = Cudd_bddAnd(cudd_manager_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_xor:
    left = funcBdd(expr->left(), inst);
    right = funcBdd(expr->right(), inst);
    if (left && right)
      result = Cudd_bddXor(cudd_manager_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_one:
    result = Cudd_ReadOne(cudd_manager_);
    break;
  case FuncExpr::op_zero:
    result = Cudd_ReadLogicZero(cudd_manager_);
    break;
  default:
    internalError("unknown function operator");
  }
  if (result)
    Cudd_Ref(result);
  if (left)
    Cudd_RecursiveDeref(cudd_manager_, left);
  if (right)
    Cudd_RecursiveDeref(cudd_manager_, right);
  return result;
}

DdNode *
Sim::ensureNode(LibertyPort *port) const
{
  const char *port_name = port->name();
  DdNode *node = symtab_.findKey(port_name);
  if (node == nullptr) {
    node = Cudd_bddNewVar(cudd_manager_);
    symtab_[port_name] = node;
    Cudd_Ref(node);
  }
  return node;
}

#else 
// No CUDD.

static LogicValue
logicNot(LogicValue value)
{
  static LogicValue logic_not[5] = {LogicValue::one, LogicValue::zero,
				    LogicValue::unknown, LogicValue::unknown,
				    LogicValue::unknown};
  return logic_not[int(value)];
}

static LogicValue
logicOr(LogicValue value1,
	LogicValue value2)
{
  static LogicValue logic_or[5][5] =
    {{LogicValue::zero,   LogicValue::one, LogicValue::unknown, LogicValue::unknown, LogicValue::unknown},
     {LogicValue::one,    LogicValue::one, LogicValue::one,     LogicValue::one,     LogicValue::one},
     {LogicValue::unknown,LogicValue::one, LogicValue::unknown, LogicValue::unknown, LogicValue::unknown},
     {LogicValue::unknown,LogicValue::one, LogicValue::unknown, LogicValue::unknown, LogicValue::unknown},
     {LogicValue::unknown,LogicValue::one, LogicValue::unknown, LogicValue::unknown, LogicValue::unknown}};
  return logic_or[int(value1)][int(value2)];
}

static LogicValue
logicAnd(LogicValue value1,
	LogicValue value2)
{
  static LogicValue logic_and[5][5] =
    {{LogicValue::zero,LogicValue::zero,   LogicValue::zero,   LogicValue::zero,    LogicValue::zero},
     {LogicValue::zero,LogicValue::one,    LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::zero,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::zero,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::zero,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown}};
  return logic_and[int(value1)][int(value2)];
}

static LogicValue
logicXor(LogicValue value1,
	 LogicValue value2)
{
  static LogicValue logic_xor[5][5]=
    {{LogicValue::zero, LogicValue::one,      LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::one,  LogicValue::zero,     LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::unknown,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::unknown,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown},
     {LogicValue::unknown,LogicValue::unknown,LogicValue::unknown,LogicValue::unknown, LogicValue::unknown}};
  return logic_xor[int(value1)][int(value2)];
}

static TimingSense
senseNot(TimingSense sense)
{
  static TimingSense sense_not[5] = {TimingSense::negative_unate,
				     TimingSense::positive_unate,
				     TimingSense::non_unate,
				     TimingSense::none,
				     TimingSense::unknown};
  return sense_not[int(sense)];
}

static TimingSense
senseAndOr(TimingSense sense1,
	   TimingSense sense2)
{
  static TimingSense sense_and_or[5][5] =
    {{TimingSense::positive_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::positive_unate, TimingSense::unknown},
     {TimingSense::non_unate, TimingSense::negative_unate,
      TimingSense::non_unate, TimingSense::negative_unate, TimingSense::unknown},
     {TimingSense::non_unate, TimingSense::non_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::unknown},
     {TimingSense::positive_unate, TimingSense::negative_unate,
      TimingSense::non_unate, TimingSense::none, TimingSense::unknown},
     {TimingSense::unknown, TimingSense::unknown,
      TimingSense::unknown, TimingSense::non_unate, TimingSense::unknown}};
  return sense_and_or[int(sense1)][int(sense2)];
}

static TimingSense
senseXor(TimingSense sense1,
	 TimingSense sense2)
{
  static TimingSense xor_sense[5][5] =
    {{TimingSense::non_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::non_unate, TimingSense::unknown},
     {TimingSense::non_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::non_unate, TimingSense::unknown},
     {TimingSense::non_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::non_unate, TimingSense::unknown},
     {TimingSense::non_unate, TimingSense::non_unate,
      TimingSense::non_unate, TimingSense::none, TimingSense::unknown},
     {TimingSense::unknown, TimingSense::unknown,
      TimingSense::unknown, TimingSense::unknown, TimingSense::unknown}};
  return xor_sense[int(sense1)][int(sense2)];
}

Sim::Sim(StaState *sta) :
  StaState(sta),
  observer_(nullptr),
  valid_(false),
  incremental_(false),
  const_func_pins_valid_(false)
{
}

Sim::~Sim()
{
  delete observer_;
}

TimingSense
Sim::functionSense(const FuncExpr *expr,
		   const Pin *input_pin,
		   const Instance *inst)
{
  TimingSense sense = TimingSense::none;
  LogicValue value = LogicValue::unknown;
  functionSense(expr, input_pin, inst, sense, value);
  return sense;
}

void
Sim::functionSense(const FuncExpr *expr,
		   const Pin *input_pin,
		   const Instance *inst,
		   // return values
		   TimingSense &sense,
		   LogicValue &value) const
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    Pin *pin = network_->findPin(inst, expr->port());
    if (pin) {
      if (pin == input_pin)
	sense = TimingSense::positive_unate;
      else
	sense = TimingSense::none;
      value = logicValue(pin);
    }
    else {
      sense = TimingSense::none;
      value = LogicValue::unknown;
    }
    break;
  }
  case FuncExpr::op_not: {
    TimingSense sense1;
    LogicValue value1;
    functionSense(expr->left(), input_pin, inst, sense1, value1);
    if (value1 == LogicValue::zero) {
      sense = TimingSense::none;
      value = LogicValue::one;
    }
    else if (value1 == LogicValue::one) {
      sense = TimingSense::none;
      value = LogicValue::zero;
    }
    else {
      sense = senseNot(sense1);
      value = LogicValue::unknown;
    }
    break;
  }
  case FuncExpr::op_or: {
    TimingSense sense1, sense2;
    LogicValue value1, value2;
    functionSense(expr->left(), input_pin, inst, sense1, value1);
    functionSense(expr->right(), input_pin, inst, sense2, value2);
    if (value1 == LogicValue::one || value2 == LogicValue::one) {
      sense = TimingSense::none;
      value = LogicValue::one;
    }
    else if (value1 == LogicValue::zero) {
      sense = sense2;
      value = value2;
    }
    else if (value2 == LogicValue::zero) {
      sense = sense1;
      value = value1;
    }
    else {
      sense = senseAndOr(sense1, sense2);
      value = LogicValue::unknown;
    }
    break;
  }
  case FuncExpr::op_and: {
    TimingSense sense1, sense2;
    LogicValue value1, value2;
    functionSense(expr->left(), input_pin, inst, sense1, value1);
    functionSense(expr->right(), input_pin, inst, sense2, value2);
    if (value1 == LogicValue::zero || value2 == LogicValue::zero) {
      sense = TimingSense::none;
      value = LogicValue::zero;
    }
    else if (value1 == LogicValue::one) {
      sense = sense2;
      value = value2;
    }
    else if (value2 == LogicValue::one) {
      sense = sense1;
      value = value1;
    }
    else {
      sense = senseAndOr(sense1, sense2);
      value = LogicValue::unknown;
    }
    break;
  }
  case FuncExpr::op_xor: {
    TimingSense sense1, sense2;
    LogicValue value1, value2;
    functionSense(expr->left(), input_pin, inst, sense1, value1);
    functionSense(expr->right(), input_pin, inst, sense2, value2);
    if ((value1 == LogicValue::zero && value2 == LogicValue::zero)
	|| (value1 == LogicValue::one && value2 == LogicValue::one)) {
      sense = TimingSense::none;
      value = LogicValue::zero;
    }
    else if ((value1 == LogicValue::zero && value2 == LogicValue::one)
	     || (value1 == LogicValue::one && value2 == LogicValue::zero)) {
      sense = TimingSense::none;
      value = LogicValue::one;
    }
    else if (value1 == LogicValue::zero) {
      sense = sense2;
      value = value2;
    }
    else if (value1 == LogicValue::one) {
      sense = senseNot(sense2);
      value = logicNot(value2);
    }
    else if (value2 == LogicValue::zero) {
      sense = sense1;
      value = value1;
    }
    else if (value2 == LogicValue::one) {
      sense = senseNot(sense1);
      value = logicNot(value1);
    }
    else {
      sense = senseXor(sense1, sense2);
      value = logicXor(value1, value2);
    }
    break;
  }
  case FuncExpr::op_one:
    sense = TimingSense::none;
    value = LogicValue::one;
    break;
  case FuncExpr::op_zero:
    sense = TimingSense::none;
    value = LogicValue::zero;
    break;
  }
}

LogicValue
Sim::evalExpr(const FuncExpr *expr,
	      const Instance *inst) const
{
  switch (expr->op()) {
  case FuncExpr::op_port: {
    Pin *pin = network_->findPin(inst, expr->port()->name());
    if (pin)
      return logicValue(pin);
    else
      // Internal ports don't have instance pins.
      return LogicValue::unknown;
  }
  case FuncExpr::op_not:
    return logicNot(evalExpr(expr->left(), inst));
  case FuncExpr::op_or:
    return logicOr(evalExpr(expr->left(),inst),
		   evalExpr(expr->right(),inst));
  case FuncExpr::op_and:
    return logicAnd(evalExpr(expr->left(),inst),
		    evalExpr(expr->right(),inst));
  case FuncExpr::op_xor:
    return  logicXor(evalExpr(expr->left(),inst),
		     evalExpr(expr->right(),inst));
  case FuncExpr::op_one:
    return LogicValue::one;
  case FuncExpr::op_zero:
    return LogicValue::zero;
  }
  // Prevent warnings from lame compilers.
  return LogicValue::zero;
}

#endif // CUDD

void
Sim::clear()
{
  valid_ = false;
  incremental_ = false;
  const_func_pins_.clear();
  const_func_pins_valid_ = false;
  instances_with_const_pins_.clear();
  instances_to_annotate_.clear();
  invalid_insts_.clear();
  invalid_drvr_pins_.clear();
  invalid_load_pins_.clear();
}

void
Sim::setObserver(SimObserver *observer)
{
  delete observer_;
  observer_ = observer;
}

void
Sim::ensureConstantsPropagated()
{
  if (!valid_) {
    Stats stats(debug_);
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
    propagateConstants();
    annotateGraphEdges();
    valid_ = true;
    incremental_ = true;

    stats.report("Propagate constants");
  }
}

void
Sim::seedInvalidConstants()
{
  InstanceSet::Iterator inst_iter(invalid_insts_);
  while (inst_iter.hasNext()) {
    Instance *inst = inst_iter.next();
    eval_queue_.push(inst);
  }
}

void
Sim::propagateToInvalidLoads()
{
  PinSet::Iterator load_iter(invalid_load_pins_);
  while (load_iter.hasNext()) {
    Pin *load_pin = load_iter.next();
    Net *net = network_->net(load_pin);
    if (net && network_->isGround(net))
      setPinValue(load_pin, LogicValue::zero, true);
    else if (net && network_->isPower(net))
      setPinValue(load_pin, LogicValue::one, true);
    else {
      Pin *drvr_pin = findDrvrPin(load_pin, network_);
      if (drvr_pin)
	propagateDrvrToLoad(drvr_pin, load_pin);
    }
  }
  invalid_load_pins_.clear();
}

void
Sim::propagateFromInvalidDrvrsToLoads()
{
  PinSet::Iterator drvr_iter(invalid_drvr_pins_);
  while (drvr_iter.hasNext()) {
    Pin *drvr_pin = drvr_iter.next();
    LogicValue value = logicValue(drvr_pin);
    PinConnectedPinIterator *load_iter=network_->connectedPinIterator(drvr_pin);
    while (load_iter->hasNext()) {
      Pin *load_pin = load_iter->next();
      if (load_pin != drvr_pin
	  && network_->isLoad(load_pin))
	setPinValue(load_pin, value, true);
    }
    delete load_iter;
  }
  invalid_drvr_pins_.clear();
}

void
Sim::propagateDrvrToLoad(Pin *drvr_pin,
			 Pin *load_pin)
{
  LogicValue value = logicValue(drvr_pin);
  setPinValue(load_pin, value, true);
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
	Pin *pin = pin_iter->next();
	recordConstPinFunc(pin);
      }
      delete pin_iter;
    }
    delete inst_iter;
    const_func_pins_valid_ = true;
  }
}

void
Sim::recordConstPinFunc(Pin *pin)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    FuncExpr *expr = port->function();
    if (expr
	// Tristate outputs do not force the output to be constant.
	&& port->tristateEnable() == nullptr
	&& (expr->op() == FuncExpr::op_zero
	    || expr->op() == FuncExpr::op_one))
      const_func_pins_.insert(pin);
  }
}

void
Sim::deleteInstanceBefore(Instance *inst)
{
  instances_with_const_pins_.erase(inst);
  invalid_insts_.erase(inst);
}

void
Sim::makePinAfter(Pin *pin)
{
  // Incrementally update const_func_pins_.
  recordConstPinFunc(pin);
}

void
Sim::deletePinBefore(Pin *pin)
{
  // Incrementally update const_func_pins_.
  const_func_pins_.erase(pin);
  invalid_load_pins_.erase(pin);
  invalid_drvr_pins_.erase(pin);
  invalid_insts_.insert(network_->instance(pin));
}

void
Sim::connectPinAfter(Pin *pin)
{
  // Incrementally update const_func_pins_.
  recordConstPinFunc(pin);
  if (incremental_) {
    if (network_->isLoad(pin))
      invalid_load_pins_.insert(pin);
    if (network_->isDriver(pin))
      invalid_drvr_pins_.insert(pin);
    valid_ = false;
  }
}

void
Sim::disconnectPinBefore(Pin *pin)
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
Sim::pinSetFuncAfter(Pin *pin)
{
  if (incremental_) {
    Instance *inst = network_->instance(pin);
    if (instances_with_const_pins_.hasKey(inst))
      invalid_insts_.insert(inst);
    valid_ = false;
  }
  // Incrementally update const_func_pins_.
  const_func_pins_.erase(pin);
  recordConstPinFunc(pin);
}

void
Sim::seedConstants()
{
  // Propagate constants from inputs tied hi/low in the network.
  enqueueConstantPinInputs(true);
  // Propagate set_LogicValue::zero, set_LogicValue::one, set_logic_dc constants.
  setConstraintConstPins(sdc_->logicValues(), true);
  // Propagate set_case_analysis constants.
  setConstraintConstPins(sdc_->caseLogicValues(), true);
  // Propagate 0/1 constant functions.
  setConstFuncPins(true);
}

void
Sim::propagateConstants()
{
  while (!eval_queue_.empty()) {
    const Instance *inst = eval_queue_.front();
    eval_queue_.pop();
    evalInstance(inst);
  }
}

void
Sim::setConstraintConstPins(LogicValueMap *value_map,
			    bool propagate)
{
  LogicValueMap::ConstIterator value_iter(value_map);
  while (value_iter.hasNext()) {
    LogicValue value;
    const Pin *pin;
    value_iter.next(pin, value);
    debugPrint2(debug_, "sim", 2, "case pin %s = %c\n",
		network_->pathName(pin),
		logicValueString(value));
    if (network_->isHierarchical(pin)) {
      // Set the logic value on pins inside the instance of a hierarchical pin.
      bool pin_is_output = network_->direction(pin)->isAnyOutput();
      PinConnectedPinIterator *pin_iter=network_->connectedPinIterator(pin);
      while (pin_iter->hasNext()) {
	Pin *pin1 = pin_iter->next();
	if (network_->isLeaf(pin1)
	    && network_->direction(pin1)->isAnyInput()
	    && ((pin_is_output && !network_->isInside(pin1, pin))
		|| (!pin_is_output && network_->isInside(pin1, pin))))
	  setPinValue(pin1, value, propagate);
      }
      delete pin_iter;
    }
    else
      setPinValue(pin, value, propagate);
  }
}

// Propagate constants from outputs with constant functions
// (tie high and tie low cell instances).
void
Sim::setConstFuncPins(bool propagate)
{
  PinSet::Iterator const_pin_iter(const_func_pins_);
  while (const_pin_iter.hasNext()) {
    Pin *pin = const_pin_iter.next();
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      FuncExpr *expr = port->function();
      if (expr->op() == FuncExpr::op_zero) {
	debugPrint1(debug_, "sim", 2, "func pin %s = 0\n",
		    network_->pathName(pin));
	setPinValue(pin, LogicValue::zero, propagate);
      }
      else if (expr->op() == FuncExpr::op_one) {
	debugPrint1(debug_, "sim", 2, "func pin %s = 1\n",
		    network_->pathName(pin));
	setPinValue(pin, LogicValue::one, propagate);
      }
    }
  }
}

void
Sim::enqueueConstantPinInputs(bool propagate)
{
  ConstantPinIterator *const_iter = network_->constantPinIterator();
  while (const_iter->hasNext()) {
    LogicValue value;
    Pin *pin;
    const_iter->next(pin, value);
    debugPrint2(debug_, "sim", 2, "network constant pin %s = %c\n",
		network_->pathName(pin),
		logicValueString(value));
    setPinValue(pin, value, propagate);
  }
  delete const_iter;
}

void
Sim::removePropagatedValue(const Pin *pin)
{
  Instance *inst = network_->instance(pin);
  if (instances_with_const_pins_.hasKey(inst)) {
    invalid_insts_.insert(inst);
    valid_ = false;

    LogicValue constraint_value;
    bool exists;
    sdc_->caseLogicValue(pin, constraint_value, exists);
    if (!exists) {
      sdc_->logicValue(pin, constraint_value, exists);
      if (!exists) {
	debugPrint1(debug_, "sim", 2, "pin %s remove prop constant\n",
		    network_->pathName(pin));
	Vertex *vertex = graph_->pinLoadVertex(pin);
	setSimValue(vertex, LogicValue::unknown);
      }
    }
  }
}

void
Sim::setPinValue(const Pin *pin,
		 LogicValue value,
		 bool propagate)
{
  LogicValue constraint_value;
  bool exists;
  sdc_->caseLogicValue(pin, constraint_value, exists);
  if (!exists)
    sdc_->logicValue(pin, constraint_value, exists);
  if (exists
      && value != constraint_value) {
    if (value != LogicValue::unknown)
      report_->warn("propagated logic value %c differs from constraint value of %c on pin %s.\n",
		    logicValueString(value),
		    logicValueString(constraint_value),
		    sdc_network_->pathName(pin));
  }
  else {
    debugPrint2(debug_, "sim", 3, "pin %s = %c\n",
		network_->pathName(pin),
		logicValueString(value));
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    // Set vertex constant flags.
    if (vertex)
      setSimValue(vertex, value);
    if (bidirect_drvr_vertex)
      setSimValue(bidirect_drvr_vertex, value);
    Instance *inst = network_->instance(pin);
    if (logicValueZeroOne(value))
      instances_with_const_pins_.insert(inst);
    instances_to_annotate_.insert(inst);
    if (propagate) {
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
	  Pin *pin1 = pin_iter->next();
	  if (pin1 != pin
	      && network_->isLoad(pin1))
	    setPinValue(pin1, value, propagate);
	}
	delete pin_iter;
      }
    }
  }
}

void
Sim::evalInstance(const Instance *inst)
{
  debugPrint1(debug_, "sim", 2, "eval %s\n", network_->pathName(inst));
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    PortDirection *dir = network_->direction(pin);
    if (dir->isAnyOutput()) {
      LibertyPort *port = network_->libertyPort(pin);
      if (port) {
	FuncExpr *expr = port->function();
	if (expr) {
	  LogicValue value = evalExpr(expr, inst);
	  FuncExpr *tri_en_expr = port->tristateEnable();
	  if (tri_en_expr == nullptr
	      || evalExpr(tri_en_expr, inst) == LogicValue::one) {
	    debugPrint3(debug_, "sim", 2, " %s %s = %c\n",
			port->name(),
			expr->asString(),
			logicValueString(value));
	    if (value != logicValue(pin))
	      setPinValue(pin, value, true);
	  }
	}
      }
    }
  }
  delete pin_iter;
}

void
Sim::setSimValue(Vertex *vertex,
		 LogicValue value)
{
  if (value != vertex->simValue()) {
    vertex->setSimValue(value);
    if (observer_)
      observer_->valueChangeAfter(vertex);
  }
}

TimingSense
Sim::functionSense(const Instance *inst,
		   const Pin *from_pin,
		   const Pin *to_pin)
{
  if (logicZeroOne(from_pin))
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

LogicValue
Sim::logicValue(const Pin *pin) const
{
  Vertex *vertex = graph_->pinLoadVertex(pin);
  if (vertex)
    return vertex->simValue();
  else {
    if (network_->isHierarchical(pin)) {
      Pin *drvr_pin = findDrvrPin(pin, network_);
      if (drvr_pin)
	return logicValue(drvr_pin);
    }
    return LogicValue::unknown;
  }
}

static Pin *
findDrvrPin(const Pin *pin,
	    Network *network)
{
  PinSet *drvrs = network->drivers(pin);
  if (drvrs) {
    PinSet::Iterator drvr_iter(drvrs);
    if (drvr_iter.hasNext())
      return drvr_iter.next();
  }
  return nullptr;
}

bool
logicValueZeroOne(LogicValue value)
{
  return value == LogicValue::zero || value == LogicValue::one;
}

bool
Sim::logicZeroOne(const Pin *pin) const
{
  return logicValueZeroOne(logicValue(pin));
}

bool
Sim::logicZeroOne(const Vertex *vertex) const
{
  return logicValueZeroOne(vertex->simValue());
}

void
Sim::clearSimValues()
{
  InstanceSet::Iterator inst_iter(instances_with_const_pins_);
  while (inst_iter.hasNext()) {
    const Instance *inst = inst_iter.next();
    // Clear sim values on all pins before evaling functions.
    clearInstSimValues(inst);
    annotateVertexEdges(inst, false);
  }
  instances_with_const_pins_.clear();
}

void
Sim::clearInstSimValues(const Instance *inst)
{
  debugPrint1(debug_, "sim", 4, "clear %s\n",
	      network_->pathName(inst));
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Vertex *vertex, *bidirect_drvr_vertex;
    graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
    if (vertex)
      setSimValue(vertex, LogicValue::unknown);
    if (bidirect_drvr_vertex)
      setSimValue(bidirect_drvr_vertex, LogicValue::unknown);
  }
  delete pin_iter;
}

// Annotate graph edges disabled by constant values.
void
Sim::annotateGraphEdges()
{
  InstanceSet::Iterator inst_iter(instances_to_annotate_);
  while (inst_iter.hasNext()) {
    const Instance *inst = inst_iter.next();
    annotateVertexEdges(inst, true);
  }
}

void
Sim::annotateVertexEdges(const Instance *inst,
			 bool annotate)
{
  debugPrint2(debug_, "sim", 4, "annotate %s %s\n",
	      network_->pathName(inst),
	      annotate ? "true" : "false");
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Vertex *vertex = graph_->pinDrvrVertex(pin);
    if (vertex)
      annotateVertexEdges(inst, pin, vertex, annotate);
  }
  delete pin_iter;
}

void
Sim::annotateVertexEdges(const Instance *inst,
			 const Pin *pin,
			 Vertex *vertex,
			 bool annotate)
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
      if (annotate) {
	// Set timing sense on edges in instances that have constant pins.
  	if (logicZeroOne(from_vertex))
  	  sense = TimingSense::none;
 	else
	  sense = functionSense(inst, from_pin, pin);

	if (sense != TimingSense::none)
	  // Disable conditional timing edges based on constant pins.
	  is_disabled_cond = isCondDisabled(edge, inst, from_pin, 
					    pin, network_,sim_)
	    // Disable mode conditional timing
	    // edges based on constant pins.
	    || isModeDisabled(edge,inst,network_,sim_)
	    || isTestDisabled(inst, from_pin, pin,
			      network_, sim_);
      }
      bool disables_changed = false;
      if (sense != edge->simTimingSense()) {
	edge->setSimTimingSense(sense);
	disables_changed = true;
	fanin_disables_changed = true;
      }
      if (is_disabled_cond != edge->isDisabledCond()) {
	edge->setIsDisabledCond(is_disabled_cond);
	disables_changed = true;
	fanin_disables_changed = true;
      }
      if (observer_ && disables_changed)
	observer_->fanoutEdgesChangeAfter(from_vertex);
    }
  }
  if (observer_ && fanin_disables_changed)
    observer_->faninEdgesChangeAfter(vertex);
}

bool
isCondDisabled(Edge *edge,
	       const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim)
{
  bool is_disabled;
  FuncExpr *disable_cond;
  isCondDisabled(edge, inst, from_pin, to_pin, network, sim,
		 is_disabled, disable_cond);
  return is_disabled;
}

void
isCondDisabled(Edge *edge,
	       const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim,
	       bool &is_disabled,
	       FuncExpr *&disable_cond)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  FuncExpr *cond = arc_set->cond();
  if (cond) {
    LogicValue cond_value = sim->evalExpr(cond, inst);
    disable_cond = cond;
    is_disabled = (cond_value == LogicValue::zero);
  }
  else {
    // Unconditional "default" arc set is disabled if another
    // conditional arc from/to the same pins is enabled (condition
    // evals to logic one).
    LibertyCell *cell = network->libertyCell(inst);
    LibertyPort *from_port = network->libertyPort(from_pin);
    LibertyPort *to_port = network->libertyPort(to_pin);
    LibertyCellTimingArcSetIterator cond_iter(cell, from_port, to_port);
    is_disabled = false;
    while (cond_iter.hasNext()) {
      TimingArcSet *cond_set = cond_iter.next();
      FuncExpr *cond = cond_set->cond();
      if (cond && sim->evalExpr(cond, inst) == LogicValue::one) {
	disable_cond = cond;
	is_disabled = true;
	break;
      }
    }
  }
}

bool
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       const Sim *sim)
{
  bool is_disabled;
  FuncExpr *disable_cond;
  isModeDisabled(edge, inst, network, sim,
		 is_disabled, disable_cond);
  return is_disabled;
}

void
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       const Sim *sim,
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
    LibertyCell *cell = network->libertyCell(inst);
    ModeDef *mode_def = cell->findModeDef(mode_name);
    if (mode_def) {
      ModeValueDef *value_def = mode_def->findValueDef(mode_value);
      if (value_def) {
	FuncExpr *cond = value_def->cond();
	if (cond) {
	  LogicValue cond_value = sim->evalExpr(cond, inst);
	  if (cond_value == LogicValue::zero) {
	    // For a mode value to be disabled by having a value of
	    // logic zero one mode value must logic one.
	    ModeValueMap::Iterator iter(mode_def->values());
	    while (iter.hasNext()) {
	      ModeValueDef *value_def1 = iter.next();
	      if (value_def1) {
		FuncExpr *cond1 = value_def1->cond();
		if (cond1) {
		  LogicValue cond_value1 = sim->evalExpr(cond1, inst);
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

bool
isTestDisabled(const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim)
{
  bool is_disabled;
  Pin *scan_enable;
  isTestDisabled(inst, from_pin, to_pin, network, sim,
		 is_disabled, scan_enable);
  return is_disabled;
}

void
isTestDisabled(const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim,
	       bool &is_disabled,
	       Pin *&scan_enable)
{
  is_disabled = false;
  LibertyCell *cell = network->libertyCell(inst);
  if (cell) {
    TestCell *test = cell->testCell();
    if (test) {
      LibertyPort *from_port = network->libertyPort(from_pin);
      LibertyPort *to_port = network->libertyPort(to_pin);
      LibertyPort *data_in_port = test->dataIn();
      LibertyPort *scan_in_port = test->scanIn();
      if (from_port == data_in_port
	  || to_port == data_in_port
	  || from_port == scan_in_port
	  || to_port == scan_in_port) {
	LibertyPort *scan_enable_port = test->scanEnable();
	if (scan_enable_port) {
	  scan_enable = network->findPin(inst, scan_enable_port);
	  if (scan_enable) {
	    LogicValue scan_enable_value = sim->logicValue(scan_enable);
	    is_disabled = ((scan_enable_value == LogicValue::zero
			    && (from_port == scan_in_port
				|| to_port == scan_in_port))
			   || (scan_enable_value == LogicValue::one
			       && (from_port == data_in_port
				   || to_port == data_in_port)));
	  }
	}
      }
    }
  }
}

} // namespace
