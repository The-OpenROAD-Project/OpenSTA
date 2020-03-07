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

#pragma once

#include <queue>
#include <mutex>
#include "StaConfig.hh"  // CUDD
#include "DisallowCopyAssign.hh"
#include "Map.hh"
#include "StaState.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"

struct DdNode;
struct DdManager;

namespace sta {

class SimObserver;

typedef Map<const Pin*, LogicValue> PinValueMap;
typedef std::queue<const Instance*> EvalQueue;
typedef Map<const char*, DdNode*, CharPtrLess> BddSymbolTable;

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
		      const Instance *inst) const;
  LogicValue logicValue(const Pin *pin) const;
  bool logicZeroOne(const Pin *pin) const;
  bool logicZeroOne(const Vertex *vertex) const;
  // Timing sense for the function between from_pin and to_pin
  // after simplifying the function based constants on the pins.
  virtual TimingSense functionSense(const Instance *inst,
				    const Pin *from_pin,
				    const Pin *to_pin);

  // Network edits.
  void deleteInstanceBefore(Instance *inst);
  void makePinAfter(Pin *pin);
  void deletePinBefore(Pin *pin);
  void connectPinAfter(Pin *pin);
  void disconnectPinBefore(Pin *pin);
  void pinSetFuncAfter(Pin *pin);

protected:
  void ensureConstantFuncPins();
  void recordConstPinFunc(Pin *pin);
  virtual void seedConstants();
  void seedInvalidConstants();
  void propagateConstants();
  void setConstraintConstPins(LogicValueMap *pin_value_map,
			      bool propagate);
  void setConstFuncPins(bool propagate);
  void enqueueConstantPinInputs(bool propagate);
  virtual void setPinValue(const Pin *pin,
			   LogicValue value,
			   bool propagate);
  void enqueue(const Instance *inst);
  void evalInstance(const Instance *inst);
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
  void propagateDrvrToLoad(Pin *drvr_pin,
			   Pin *load_pin);
  void setSimValue(Vertex *vertex,
		   LogicValue value);

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

#ifdef CUDD
  DdNode *funcBdd(const FuncExpr *expr,
		  const Instance *inst) const;
  DdNode *ensureNode(LibertyPort *port) const;
  void clearSymtab() const;

  DdManager *cudd_manager_;
  mutable BddSymbolTable symtab_;
  mutable std::mutex cudd_lock_;
#endif // CUDD

private:
  DISALLOW_COPY_AND_ASSIGN(Sim);
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

private:
  DISALLOW_COPY_AND_ASSIGN(SimObserver);
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
	       const Sim *sim);

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
	       const Sim *sim,
	       bool &is_disabled,
	       FuncExpr *&disable_cond);


// Edge mode function is disabled (evals to zero).
bool
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       const Sim *sim);
void
isModeDisabled(Edge *edge,
	       const Instance *inst,
	       const Network *network,
	       const Sim *sim,
	       bool &is_disabled,
	       FuncExpr *&disable_cond);

// Edge is disabled because by test scan enable.
//   from scan_data_in and scan_enable=1
//   from scan_in and scan_enable=0
bool
isTestDisabled(const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim);

void
isTestDisabled(const Instance *inst,
	       const Pin *from_pin,
	       const Pin *to_pin,
	       const Network *network,
	       const Sim *sim,
	       bool &is_disabled,
	       Pin *&scan_enable);

} // namespace
