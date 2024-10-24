// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#pragma once

#include <queue>
#include <mutex>

#include "StaConfig.hh"  // CUDD
#include "Map.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "Bdd.hh"

namespace sta {

class SimObserver;

typedef Map<const Pin*, LogicValue> PinValueMap;
typedef std::queue<const Instance*> EvalQueue;

// Propagate constants from constraints and netlist tie high/low
// connections thru gates.
class Sim : public StaState
{
public:
  explicit Sim(StaState *sta);
  virtual ~Sim();
  void clear();
  // Set the observer for simulation value changes.
  void setObserver(SimObserver *observer);
  void ensureConstantsPropagated();
  void constantsInvalid();
  LogicValue evalExpr(const FuncExpr *expr,
		      const Instance *inst);
  LogicValue logicValue(const Pin *pin) const;
  bool logicZeroOne(const Pin *pin) const;
  bool logicZeroOne(const Vertex *vertex) const;
  // Timing sense for the function between from_pin and to_pin
  // after simplifying the function based constants on the pins.
  virtual TimingSense functionSense(const Instance *inst,
				    const Pin *from_pin,
				    const Pin *to_pin);
  void findLogicConstants();

  // Network edits.
  void deleteInstanceBefore(const Instance *inst);
  void makePinAfter(const Pin *pin);
  void deletePinBefore(const Pin *pin);
  void connectPinAfter(const Pin *pin);
  void disconnectPinBefore(const Pin *pin);
  void pinSetFuncAfter(const Pin *pin);

protected:
  void ensureConstantFuncPins();
  void recordConstPinFunc(const Pin *pin);
  virtual void seedConstants();
  void seedInvalidConstants();
  void propagateConstants(bool thru_sequentials);
  void setConstraintConstPins(LogicValueMap &pin_value_map);
  void setConstFuncPins();
  LogicValue pinConstFuncValue(const Pin *pin);
  void enqueueConstantPinInputs();
  virtual void setPinValue(const Pin *pin,
			   LogicValue value);
  void enqueue(const Instance *inst);
  void evalInstance(const Instance *inst,
                    bool thru_sequentials);
  LogicValue clockGateOutValue(const Instance *inst);
  TimingSense functionSense(const FuncExpr *expr,
			    const Pin *input_pin,
			    const Instance *inst);
  void functionSense(const FuncExpr *expr,
		     const Pin *input_pin,
		     const Instance *inst,
		     // return values
		     TimingSense &sense,
		     LogicValue &value) const;
  void clearSimValues();
  virtual void clearInstSimValues(const Instance *inst);
  void annotateGraphEdges();
  void annotateVertexEdges(const Instance *inst,
			   const Pin *pin,
			   Vertex *vertex,
			   bool annotate);
  void annotateVertexEdges(const Instance *inst,
			   bool annotate);
  void removePropagatedValue(const Pin *pin);
  void propagateFromInvalidDrvrsToLoads();
  void propagateToInvalidLoads();
  void propagateDrvrToLoad(const Pin *drvr_pin,
			   const Pin *load_pin);
  void setSimValue(Vertex *vertex,
		   LogicValue value);
  DdNode *funcBddSim(const FuncExpr *expr,
                     const Instance *inst);

  SimObserver *observer_;
  bool valid_;
  bool incremental_;
  // Cache of pins that have constant functions (tie high and tie low
  // cell instances).
  PinSet const_func_pins_;
  bool const_func_pins_valid_;
  // Instances that require incremental constant propagation.
  InstanceSet invalid_insts_;
  // Driver pins waiting to propagate constant to loads.
  PinSet invalid_drvr_pins_;
  // Load pins that waiting for the driver constant to propagate.
  PinSet invalid_load_pins_;
  EvalQueue eval_queue_;
  // Instances with constant pin values for annotateVertexEdges.
  InstanceSet instances_with_const_pins_;
  InstanceSet instances_to_annotate_;
  Bdd bdd_;
  mutable std::mutex bdd_lock_;
};

// Abstract base class for Sim value change observer.
class SimObserver
{
public:
  SimObserver() {}
  virtual ~SimObserver() {}
  virtual void valueChangeAfter(Vertex *vertex) = 0;
  virtual void faninEdgesChangeAfter(Vertex *vertex) = 0;
  virtual void fanoutEdgesChangeAfter(Vertex *vertex) = 0;
};

bool
logicValueZeroOne(LogicValue value);

// Edge cond (liberty "when") function is disabled (evals to zero).
bool
isCondDisabled(Edge *edge,
	       const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       Sim *sim);

// isCondDisabled but also return the cond expression that causes
// the disable.  This can differ from the edge cond expression
// when the default timing edge is disabled by another edge between
// the same pins that is enabled.
void
isCondDisabled(Edge *edge,
	       const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       Sim *sim,
	       bool &is_disabled,
	       FuncExpr *&disable_cond);


// Edge mode function is disabled (evals to zero).
bool
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       Sim *sim);
void
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       Sim *sim,
	       bool &is_disabled,
	       FuncExpr *&disable_cond);

} // namespace
