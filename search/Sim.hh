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

#pragma once

#include <queue>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "StaConfig.hh"  // CUDD
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "Bdd.hh"

namespace sta {

class SimObserver;

using SimValueMap = std::unordered_map<const Pin*, LogicValue>;
using EdgeDisabledCondSet = std::unordered_set<const Edge*>;
using EdgeTimingSenseMap = std::unordered_map<const Edge*, TimingSense>;
using EvalQueue = std::queue<const Instance*>;

// Propagate constants from constraints and netlist tie high/low
// connections thru gates.
class Sim : public StaState
{
public:
  Sim(StaState *sta);
  virtual ~Sim();
  virtual void copyState(const StaState *sta);
  void clear();
  void setMode(Mode *mode);
  // Set the observer for simulation value changes.
  void setObserver(SimObserver *observer);
  void ensureConstantsPropagated();
  void constantsInvalid();

  // Dertived by Sim from set_case_analysis or set_logic constant.
  LogicValue simValue(const Pin *pin) const;
  LogicValue simValue(const Vertex *vertex) const;
  // Constant zero/one from simulation.
  bool isConstant(const Pin *pin) const;
  bool isConstant(const Vertex *vertex) const;
  // Timing sense for the to_pin function after simplifying the
  // function based constants on the instance pins.
  TimingSense simTimingSense(const Edge *edge) const;
  // Edge is disabled by constants in condition (when) function.
  bool isDisabledCond(const Edge *edge) const;

  LogicValue evalExpr(const FuncExpr *expr,
                      const Instance *inst);
  // Timing sense for the function between from_pin and to_pin
  // after simplifying the function based constants on the pins.
  TimingSense functionSense(const Instance *inst,
                            const Pin *from_pin,
                            const Pin *to_pin);
  // Propagate liberty constant functions and pins tied high/low through
  // combinational logic and registers (ignores Sdc).
  // Used by OpenROAD/Restructure.cpp
  void findLogicConstants();

  // Edge cond (liberty "when") function is disabled (evals to zero).
  bool isDisabledCond(Edge *edge,
                      const Instance *inst,
                      const Pin *from_pin,
                      const Pin *to_pin);
  // isDisabledCond but also return the cond expression that causes
  // the disable.  This can differ from the edge cond expression
  // when the default timing edge is disabled by another edge between
  // the same pins that is enabled.
  void isDisabledCond(Edge *edge,
                      const Instance *inst,
                      const Pin *from_pin,
                      const Pin *to_pin,
                      // Return values.
                      bool &is_disabled,
                      FuncExpr *&disable_cond);
  // Edge mode function is disabled (evals zero).
  bool isDisabledMode(Edge *edge,
                      const Instance *inst);
  void isDisabledMode(Edge *edge,
                      const Instance *inst,
                      // Return values.
                      bool &is_disabled,
                      FuncExpr *&disable_cond);

  // Network edits.
  void deleteInstanceBefore(const Instance *inst);
  void makePinAfter(const Pin *pin);
  void deletePinBefore(const Pin *pin);
  void connectPinAfter(const Pin *pin);
  void disconnectPinBefore(const Pin *pin);
  void pinSetFuncAfter(const Pin *pin);
  const SimValueMap &simValues() const { return sim_value_map_; }

protected:
  void ensureConstantFuncPins();
  void recordConstPinFunc(const Pin *pin);
  void seedConstants();
  void seedInvalidConstants();
  void propagateConstants(bool thru_sequentials);
  void setConstraintConstPins(const LogicValueMap &pin_value_map);
  void setConstFuncPins();
  LogicValue pinConstFuncValue(const Pin *pin);
  void enqueueConstantPinInputs();
  void setSimValue(const Pin *pin,
                   LogicValue value);
  void setPinValue(const Pin *pin,
                   LogicValue value);
  void setSimTimingSense(Edge *edge,
                         TimingSense sense);
  void setIsDisabledCond(Edge *edge,
                         bool disabled);
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

  void findDisabledEdges();
  void findDisabledEdges(const Instance *inst);
  void findDisabledEdges(const Instance *inst,
                         const Pin *pin,
                         Vertex *vertex);
  
  void removePropagatedValue(const Pin *pin);
  void propagateFromInvalidDrvrsToLoads();
  void propagateToInvalidLoads();
  void propagateDrvrToLoad(const Pin *drvr_pin,
                           const Pin *load_pin);
  DdNode *funcBddSim(const FuncExpr *expr,
                     const Instance *inst);

  Mode *mode_;
  SimObserver *observer_;
  bool valid_;
  bool incremental_;
  // Constants propagated by Sim.cc
  SimValueMap sim_value_map_;
  EdgeDisabledCondSet edge_disabled_cond_set_;
  EdgeTimingSenseMap edge_timing_sense_map_;
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
  InstanceSet instances_to_annotate_;
  Bdd bdd_;
  mutable std::mutex bdd_lock_;
};

// Abstract base class for Sim value change observer.
class SimObserver : public StaState
{
public:
  SimObserver(StaState *sta);
  virtual ~SimObserver() {}
  virtual void valueChangeAfter(const Pin *pin) = 0;
  virtual void faninEdgesChangeAfter(const Pin *pin) = 0;
  virtual void fanoutEdgesChangeAfter(const Pin *pin) = 0;
};

} // namespace
