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

#include <vector>
#include <string>
#include <string_view>
#include <functional>

#include "StringSeq.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "Scene.hh"
#include "GraphClass.hh"
#include "ParasiticsClass.hh"
#include "StaState.hh"
#include "VertexVisitor.hh"
#include "SearchClass.hh"
#include "PowerClass.hh"
#include "ArcDelayCalc.hh"
#include "CircuitSim.hh"
#include "Variables.hh"
#include "Property.hh"
#include "RiseFallMinMaxDelay.hh"

struct Tcl_Interp;

namespace sta {

// Don't include headers to minimize dependencies.
class MinMax;
class MinMaxAll;
class RiseFallBoth;
class RiseFall;
class VerilogReader;
class ReportPath;
class CheckTiming;
class CheckSlews;
class CheckFanouts;
class CheckCapacitances;
class CheckMinPulseWidths;
class CheckMinPeriods;
class CheckMaxSkews;
class PatternMatch;
class CheckPeriods;
class LibertyReader;
class SearchPred;
class Scene;
class ClkSkews;
class ReportField;
class EquivCells;
class StaSimObserver;
class GraphLoop;

using ModeNameMap = std::map<std::string, Mode*>;
using SceneNameMap = std::map<std::string, Scene*>;
using SlowDrvrIterator = Iterator<Instance*>;
using CheckError = StringSeq;
using CheckErrorSeq = std::vector<CheckError*>;
using StdStringSeq = std::vector<std::string>;
enum class CmdNamespace { sta, sdc };
using ParasiticsNameMap = std::map<std::string, Parasitics*>;
// Path::slack/arrival/required function.
using PathDelayFunc = std::function<Delay (const Path *path)>;
using GraphLoopSeq = std::vector<GraphLoop*>;

// Initialize sta functions that are not part of the Sta class.
void initSta();

// Call before exit to make leak detection simpler for purify and valgrind.
void
deleteAllMemory();

// The Lord, God, King, Master of the Timing Universe.
// This class is a FACADE used to present an API to the collection of
// objects that hold the collective state of the static timing analyzer.
// It should only hold pointers to objects so that only the referenced
// class declarations and not their definitions are needed by this header.
//
// The report object is not owned by the sta object.
class Sta : public StaState
{
public:
  Sta();
  // The Sta is a FACTORY for the components.
  // makeComponents calls the make{Component} virtual functions.
  // Ideally this would be called by the Sta constructor, but a
  // virtual function called in a base class constructor does not
  // call the derived class function.
  virtual void makeComponents();
  // Call copyState for each component to notify it that some
  // pointers to some components have changed.
  // This must be called after changing any of the StaState components.
  virtual void updateComponentsState();
  virtual ~Sta();

  // Singleton accessor used by tcl command interpreter.
  static Sta *sta();
  static void setSta(Sta *sta);

  // Default number of threads to use.
  virtual int defaultThreadCount() const;
  void setThreadCount(int thread_count);

  // define_corners compatibility.
  void makeScenes(StringSeq *scene_names);
  void makeScene(const std::string &name,
                 const std::string &mode_name,
                 const StdStringSeq &liberty_min_files,
                 const StdStringSeq &liberty_max_files,
                 const std::string &spef_min_file,
                 const std::string &spef_max_file);
  Scene *findScene(const std::string &name) const;
  // Pattern match name.
  SceneSeq findScenes(const std::string &name) const;
  SceneSeq findScenes(const std::string &name,
                      ModeSeq &modes) const;
  Scene *cmdScene() const;
  void setCmdScene(Scene *scene);
  SceneSeq makeSceneSeq(Scene *scene) const;

  Mode *cmdMode() const { return cmd_scene_->mode(); }
  const std::string &cmdModeName();
  void setCmdMode(const std::string &mode_name);
  Mode *findMode(const std::string &mode_name) const;
  ModeSeq findModes(const std::string &mode_name) const;
  Sdc *cmdSdc() const;

  virtual LibertyLibrary *readLiberty(const char *filename,
                                      Scene *scene,
                                      const MinMaxAll *min_max,
                                      bool infer_latches);
  // tmp public
  void readLibertyAfter(LibertyLibrary *liberty,
                        Scene *scene,
                        const MinMax *min_max);
  bool readVerilog(const char *filename);
  // Network readers call this to notify the Sta to delete any previously
  // linked network.
  void readNetlistBefore();
  // Return true if successful.
  bool linkDesign(const char *top_cell_name,
                  bool make_black_boxes);

  // SDC Swig API.
  Instance *currentInstance() const;
  void setCurrentInstance(Instance *inst);
  virtual void setAnalysisType(AnalysisType analysis_type,
                               Sdc *sdc);
  void setOperatingConditions(OperatingConditions *op_cond,
                              const MinMaxAll *min_max,
                              Sdc *sdc);
  void setTimingDerate(TimingDerateType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate,
                       Sdc *sdc);
  // Delay type is always net for net derating.
  void setTimingDerate(const Net *net,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate,
                       Sdc *sdc);
  void setTimingDerate(const Instance *inst,
                       TimingDerateCellType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate,
                       Sdc *sdc);
  void setTimingDerate(const LibertyCell *cell,
                       TimingDerateCellType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate,
                       Sdc *sdc);
  void unsetTimingDerate(Sdc *sdc);
  void setInputSlew(const Port *port,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max,
                    float slew,
                    Sdc *sdc);
  // Set port external pin load (set_load -pin port).
  void setPortExtPinCap(const Port *port,
                        const RiseFallBoth *rf,
                        const MinMaxAll *min_max,
                        float cap,
                        Sdc *sdc);
  void portExtCaps(const Port *port,
                   const MinMax *min_max,
                   const Sdc *sdc,
                   float &pin_cap,
                   float &wire_cap,
                   int &fanout);
  // Set port external wire load (set_load -wire port).
  void setPortExtWireCap(const Port *port,
                         const RiseFallBoth *rf,
                         const MinMaxAll *min_max,
                         float cap,
                         Sdc *sdc);
  // Set net wire capacitance (set_load -wire net).
  void setNetWireCap(const Net *net,
                     bool subtract_pin_load,
                     const MinMaxAll *min_max,
                     float cap,
                     Sdc *sdc);
  // Remove all "set_load net" annotations.
  void removeNetLoadCaps(Sdc *sdc) const;
  // Set port external fanout (used by wireload models).
  void setPortExtFanout(const Port *port,
                        int fanout,
                        const MinMaxAll *min_max,
                        Sdc *sdc);
  // Liberty port capacitance.
  float capacitance(const LibertyPort *port,
                    Scene *scene,
                    const MinMax *min_max);
  // pin_cap  = net pin capacitances + port external pin capacitance,
  // wire_cap = annotated net capacitance + port external wire capacitance.
  void connectedCap(const Pin *drvr_pin,
                    const RiseFall *rf,
                    const Scene *scene,
                    const MinMax *min_max,
                    float &pin_cap,
                    float &wire_cap) const;
  void connectedCap(const Net *net,
                    Scene *scene,
                    const MinMax *min_max,
                    float &pin_cap,
                    float &wire_cap) const;
  void setResistance(const Net *net,
                     const MinMaxAll *min_max,
                     float res,
                     Sdc *sdc);
  void setDriveCell(const LibertyLibrary *library,
                    const LibertyCell *cell,
                    const Port *port,
                    const LibertyPort *from_port,
                    float *from_slews,
                    const LibertyPort *to_port,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max,
                    Sdc *sdc);
  void setDriveResistance(const Port *port,
                          const RiseFallBoth *rf,
                          const MinMaxAll *min_max,
                          float res,
                          Sdc *sdc);
  void setLatchBorrowLimit(const Pin *pin,
                           float limit,
                           Sdc *sdc);
  void setLatchBorrowLimit(const Instance *inst,
                           float limit,
                           Sdc *sdc);
  void setLatchBorrowLimit(const Clock *clk,
                           float limit,
                           Sdc *sdc);
  void setMinPulseWidth(const RiseFallBoth *rf,
                        float min_width,
                        Sdc *sdc);
  void setMinPulseWidth(const Pin *pin,
                        const RiseFallBoth *rf,
                        float min_width,
                        Sdc *sdc);
  void setMinPulseWidth(const Instance *inst,
                        const RiseFallBoth *rf,
                        float min_width,
                        Sdc *sdc);
  void setMinPulseWidth(const Clock *clk,
                        const RiseFallBoth *rf,
                        float min_width,
                        Sdc *sdc);
  void setWireload(Wireload *wireload,
                   const MinMaxAll *min_max,
                   Sdc *sdc);
  void setWireloadMode(WireloadMode mode,
                       Sdc *sdc);
  void setWireloadSelection(WireloadSelection *selection,
                            const MinMaxAll *min_max,
                            Sdc *sdc);
  void setSlewLimit(Clock *clk,
                    const RiseFallBoth *rf,
                    const PathClkOrData clk_data,
                    const MinMax *min_max,
                    float slew,
                    Sdc *sdc);
  void setSlewLimit(Port *port,
                    const MinMax *min_max,
                    float slew,
                    Sdc *sdc);
  void setSlewLimit(Cell *cell,
                    const MinMax *min_max,
                    float slew,
                    Sdc *sdc);
  void setCapacitanceLimit(Cell *cell,
                           const MinMax *min_max,
                           float cap,
                           Sdc *sdc);
  void setCapacitanceLimit(Port *port,
                           const MinMax *min_max,
                           float cap,
                           Sdc *sdc);
  void setCapacitanceLimit(Pin *pin,
                           const MinMax *min_max,
                           float cap,
                           Sdc *sdc);
  void setFanoutLimit(Cell *cell,
                      const MinMax *min_max,
                      float fanout,
                      Sdc *sdc);
  void setFanoutLimit(Port *port,
                      const MinMax *min_max,
                      float fanout,
                      Sdc *sdc);
  void setMaxArea(float area,
                  Sdc *sdc);

  void makeClock(const char *name,
                 PinSet *pins,
                 bool add_to_pins,
                 float period,
                 FloatSeq *waveform,
                 char *comment,
                 const Mode *mode);
  // edges size must be 3.
  void makeGeneratedClock(const char *name,
                          PinSet *pins,
                          bool add_to_pins,
                          Pin *src_pin,
                          Clock *master_clk,
                          int divide_by,
                          int multiply_by,
                          float duty_cycle,
                          bool invert,
                          bool combinational,
                          IntSeq *edges,
                          FloatSeq *edge_shifts,
                          char *comment,
                          const Mode *mode);
  void removeClock(Clock *clk,
                   Sdc *sdc);
  // Update period/waveform for generated clocks from source pin clock.
  void updateGeneratedClks();
  // True if pin is defined as a clock source (pin may be hierarchical).
  bool isClockSrc(const Pin *pin,
                  const Sdc *sdc) const;
  // Propagated (non-ideal) clocks.
  void setPropagatedClock(Clock *clk,
                          const Mode *mode);
  void removePropagatedClock(Clock *clk,
                             const Mode *mode);
  void setPropagatedClock(Pin *pin,
                          const Mode *mode);
  void removePropagatedClock(Pin *pin,
                             const Mode *mode);
  void setClockSlew(Clock *clock,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max,
                    float slew,
                    Sdc *sdc);
  void removeClockSlew(Clock *clk,
                       Sdc *sdc);
  // Clock latency.
  // Latency can be on a clk, pin, or clk/pin combination.
  void setClockLatency(Clock *clk,
                       Pin *pin,
                       const RiseFallBoth *rf,
                       const MinMaxAll *min_max,
                       float delay,
                       Sdc *sdc);
  void removeClockLatency(const Clock *clk,
                          const Pin *pin,
                          Sdc *sdc);
  // Clock insertion delay (source latency).
  void setClockInsertion(const Clock *clk,
                         const Pin *pin,
                         const RiseFallBoth *rf,
                         const MinMaxAll *min_max,
                         const EarlyLateAll *early_late,
                         float delay,
                         Sdc *sdc);
  void removeClockInsertion(const Clock *clk,
                            const Pin *pin,
                            Sdc *sdc);
  // Clock uncertainty.
  void setClockUncertainty(Clock *clk,
                           const SetupHoldAll *setup_hold,
                           float uncertainty);
  void removeClockUncertainty(Clock *clk,
                              const SetupHoldAll *setup_hold);
  void setClockUncertainty(Pin *pin,
                           const SetupHoldAll *setup_hold,
                           float uncertainty,
                           Sdc *sdc);
  void removeClockUncertainty(Pin *pin,
                              const SetupHoldAll *setup_hold,
                              Sdc *sdc);
  // Inter-clock uncertainty.
  void setClockUncertainty(Clock *from_clk,
                           const RiseFallBoth *from_rf,
                           Clock *to_clk,
                           const RiseFallBoth *to_rf,
                           const SetupHoldAll *setup_hold,
                           float uncertainty,
                           Sdc *sdc);
  void removeClockUncertainty(Clock *from_clk,
                              const RiseFallBoth *from_rf,
                              Clock *to_clk,
                              const RiseFallBoth *to_rf,
                              const SetupHoldAll *setup_hold,
                              Sdc *sdc);
  ClockGroups *makeClockGroups(const char *name,
                               bool logically_exclusive,
                               bool physically_exclusive,
                               bool asynchronous,
                               bool allow_paths,
                               const char *comment,
                               Sdc *sdc);
  // nullptr name removes all.
  void removeClockGroupsLogicallyExclusive(const char *name,
                                           Sdc *sdc);
  void removeClockGroupsPhysicallyExclusive(const char *name,
                                            Sdc *sdc);
  void removeClockGroupsAsynchronous(const char *name,
                                     Sdc *sdc);
  void makeClockGroup(ClockGroups *clk_groups,
                      ClockSet *clks,
                      Sdc *sdc);
  void setClockSense(PinSet *pins,
                     ClockSet *clks,
                     ClockSense sense,
                     Sdc *sdc);
  void setClockGatingCheck(const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           Sdc *sdc);
  void setClockGatingCheck(Clock *clk,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           Sdc *sdc);
  void setClockGatingCheck(Instance *inst,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           LogicValue active_value,
                           Sdc *sdc);
  void setClockGatingCheck(Pin *pin,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           LogicValue active_value,
                           Sdc *sdc);
  void setDataCheck(Pin *from,
                    const RiseFallBoth *from_rf,
                    Pin *to,
                    const RiseFallBoth *to_rf,
                    Clock *clk,
                    const SetupHoldAll *setup_hold,
                    float margin,
                    Sdc *sdc);
  void removeDataCheck(Pin *from,
                       const RiseFallBoth *from_rf,
                       Pin *to,
                       const RiseFallBoth *to_rf,
                       Clock *clk,
                       const SetupHoldAll *setup_hold,
                       Sdc *sdc);
  // set_disable_timing cell [-from] [-to]
  // Disable all edges thru cell if from/to are null.
  // Bus and bundle ports are NOT supported.
  void disable(LibertyCell *cell,
               LibertyPort *from,
               LibertyPort *to,
               Sdc *sdc);
  void removeDisable(LibertyCell *cell,
                     LibertyPort *from,
                     LibertyPort *to,
                     Sdc *sdc);
  // set_disable_timing liberty port.
  // Bus and bundle ports are NOT supported.
  void disable(LibertyPort *port,
               Sdc *sdc);
  void removeDisable(LibertyPort *port,
                     Sdc *sdc);
  // set_disable_timing port (top level instance port).
  // Bus and bundle ports are NOT supported.
  void disable(Port *port,
               Sdc *sdc);
  void removeDisable(Port *port,
                     Sdc *sdc);
  // set_disable_timing instance [-from] [-to].
  // Disable all edges thru instance if from/to are null.
  // Bus and bundle ports are NOT supported.
  // Hierarchical instances are NOT supported.
  void disable(Instance *inst,
               LibertyPort *from,
               LibertyPort *to,
               Sdc *sdc);
  void removeDisable(Instance *inst,
                     LibertyPort *from,
                     LibertyPort *to,
                     Sdc *sdc);
  // set_disable_timing pin
  void disable(Pin *pin,
               Sdc *sdc);
  void removeDisable(Pin *pin,
                     Sdc *sdc);
  // set_disable_timing [get_timing_arc -of_objects instance]]
  void disable(Edge *edge,
               Sdc *sdc);
  void removeDisable(Edge *edge,
                     Sdc *sdc);
  // set_disable_timing [get_timing_arc -of_objects lib_cell]]
  void disable(TimingArcSet *arc_set,
               Sdc *sdc);
  void removeDisable(TimingArcSet *arc_set,
                     Sdc *sdc);
  [[nodiscard]] bool isConstant(const Pin *pin,
                                const Mode *mode) const;
  // Edge is disabled by constant.
  [[nodiscard]] bool isDisabledConstant(Edge *edge,
                                        const Mode *mode);
  // Return a set of constant pins that disabled edge.
  // Caller owns the returned set.
  PinSet disabledConstantPins(Edge *edge,
                              const Mode *mode);
  // Edge timing sense with propagated constants.
  TimingSense simTimingSense(Edge *edge,
                             const Mode *mode);
  // Edge is disabled by set_disable_timing constraint.
  [[nodiscard]] bool isDisabledConstraint(Edge *edge,
                                          const Sdc *sdc);
  // Edge is disabled to break combinational loops.
  [[nodiscard]] bool isDisabledLoop(Edge *edge) const;
  // Edge is disabled internal bidirect output path.
  [[nodiscard]] bool isDisabledBidirectInstPath(Edge *edge) const;
  // Edge is disabled bidirect net path.
  [[nodiscard]] bool isDisabledBidirectNetPath(Edge *edge) const;
  [[nodiscard]] bool isDisabledPresetClr(Edge *edge) const;
  // Return a vector of graph edges that are disabled, sorted by
  // from/to vertex names.  Caller owns the returned vector.
  EdgeSeq disabledEdges(const Mode *mode);
  EdgeSeq disabledEdgesSorted(const Mode *mode);
  void disableClockGatingCheck(Instance *inst,
                               Sdc *sdc);
  void disableClockGatingCheck(Pin *pin,
                               Sdc *sdc);
  void removeDisableClockGatingCheck(Instance *inst,
                                     Sdc *sdc);
  void removeDisableClockGatingCheck(Pin *pin,
                                     Sdc *sdc);
  void setLogicValue(Pin *pin,
                     LogicValue value,
                     Mode *mode);
  void setCaseAnalysis(Pin *pin,
                       LogicValue value,
                       Mode *mode);
  void removeCaseAnalysis(Pin *pin,
                          Mode *mode);
  void setInputDelay(const Pin *pin,
                     const RiseFallBoth *rf,
                     const Clock *clk,
                     const RiseFall *clk_rf,
                     const Pin *ref_pin,
                     bool source_latency_included,
                     bool network_latency_included,
                     const MinMaxAll *min_max,
                     bool add,
                     float delay,
                     Sdc *sdc);
  void removeInputDelay(const Pin *pin,
                        const RiseFallBoth *rf, 
                        const Clock *clk,
                        const RiseFall *clk_rf, 
                        const MinMaxAll *min_max,
                        Sdc *sdc);
  void setOutputDelay(const Pin *pin,
                      const RiseFallBoth *rf,
                      const Clock *clk,
                      const RiseFall *clk_rf,
                      const Pin *ref_pin,
                      bool source_latency_included,
                      bool network_latency_included,
                      const MinMaxAll *min_max,
                      bool add,
                      float delay,
                      Sdc *sdc);
  void removeOutputDelay(const Pin *pin,
                         const RiseFallBoth *rf, 
                         const Clock *clk,
                         const RiseFall *clk_rf, 
                         const MinMaxAll *min_max,
                         Sdc *sdc);
  void makeFalsePath(ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const MinMaxAll *min_max,
                     const char *comment,
                     Sdc *sdc);
  void makeMulticyclePath(ExceptionFrom *from,
                          ExceptionThruSeq *thrus,
                          ExceptionTo *to,
                          const MinMaxAll *min_max,
                          bool use_end_clk,
                          int path_multiplier,
                          const char *comment,
                          Sdc *sdc);
  void makePathDelay(ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const MinMax *min_max,
                     bool ignore_clk_latency,
                     bool break_path,
                     float delay,
                     const char *comment,
                     Sdc *sdc);
  void makeGroupPath(const char *name,
                     bool is_default,
                     ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const char *comment,
                     Sdc *sdc);
  // Deprecated 10/24/2025
  bool isGroupPathName(const char *group_name,
                       const Sdc *sdc) __attribute__ ((deprecated));
  bool isPathGroupName(const char *group_name,
                       const Sdc *sdc) const;
  StdStringSeq pathGroupNames(const Sdc *sdc) const;
  void resetPath(ExceptionFrom *from,
                 ExceptionThruSeq *thrus,
                 ExceptionTo *to,
                 const MinMaxAll *min_max,
                 Sdc *sdc);
  // Make an exception -from specification.
  ExceptionFrom *makeExceptionFrom(PinSet *from_pins,
                                   ClockSet *from_clks,
                                   InstanceSet *from_insts,
                                   const RiseFallBoth *from_rf,
                                   const Sdc *sdc);
  void checkExceptionFromPins(ExceptionFrom *from,
                              const char *file,
                              int line,
                              const Sdc *sdc) const;
  void deleteExceptionFrom(ExceptionFrom *from);
  // Make an exception -through specification.
  ExceptionThru *makeExceptionThru(PinSet *pins,
                                   NetSet *nets,
                                   InstanceSet *insts,
                                   const RiseFallBoth *rf,
                                   const Sdc *sdc);
  void deleteExceptionThru(ExceptionThru *thru);
  // Make an exception -to specification.
  ExceptionTo *makeExceptionTo(PinSet *to_pins,
                               ClockSet *to_clks,
                               InstanceSet *to_insts,
                               const RiseFallBoth *rf,
                               const RiseFallBoth *end_rf,
                               const Sdc *sdc);
  void checkExceptionToPins(ExceptionTo *to,
                            const char *file,
                            int line,
                            const Sdc *sdc) const;
  void deleteExceptionTo(ExceptionTo *to);

  InstanceSet findRegisterInstances(ClockSet *clks,
                                    const RiseFallBoth *clk_rf,
                                    bool edge_triggered,
                                    bool latches,
                                    const Mode *mode);
  PinSet findRegisterDataPins(ClockSet *clks,
                              const RiseFallBoth *clk_rf,
                              bool registers,
                              bool latches,
                              const Mode *mode);
  PinSet findRegisterClkPins(ClockSet *clks,
                             const RiseFallBoth *clk_rf,
                             bool registers,
                             bool latches,
                             const Mode *mode);
  PinSet findRegisterAsyncPins(ClockSet *clks,
                               const RiseFallBoth *clk_rf,
                               bool registers,
                               bool latches,
                               const Mode *mode);
  PinSet findRegisterOutputPins(ClockSet *clks,
                                const RiseFallBoth *clk_rf,
                                bool registers,
                                bool latches,
                                const Mode *mode);
  PinSet findFaninPins(PinSeq *to,
                       bool flat,
                       bool startpoints_only,
                       int inst_levels,
                       int pin_levels,
                       bool thru_disabled,
                       bool thru_constants,
                       const Mode *mode);
  InstanceSet
  findFaninInstances(PinSeq *to,
                     bool flat,
                     bool startpoints_only,
                     int inst_levels,
                     int pin_levels,
                     bool thru_disabled,
                     bool thru_constants,
                     const Mode *mode);
  PinSet
  findFanoutPins(PinSeq *from,
                 bool flat,
                 bool endpoints_only,
                 int inst_levels,
                 int pin_levels,
                 bool thru_disabled,
                 bool thru_constants,
                 const Mode *mode);
  InstanceSet
  findFanoutInstances(PinSeq *from,
                      bool flat,
                      bool endpoints_only,
                      int inst_levels,
                      int pin_levels,
                      bool thru_disabled,
                      bool thru_constants,
                      const Mode *mode);

  // The set of clocks that arrive at vertex in the clock network.
  ClockSet clocks(const Pin *pin,
                  const Mode *mode);
  // Clock domains for a pin.
  ClockSet clockDomains(const Pin *pin,
                        const Mode *mode);

  ////////////////////////////////////////////////////////////////
  // net=null check all nets
  void reportSlewChecks(const Net *net,
                        size_t max_count,
                        bool violators,
                        bool verbose,
                        const SceneSeq &scenes,
                        const MinMax *min_max);
  void checkSlewsPreamble();
  // requires checkSlewsPreamble()
  void checkSlew(const Pin *pin,
                 const SceneSeq &scenes,
                 const MinMax *min_max,
                 bool check_clks,
                 // Return values.
                 Slew &slew,
                 float &limit,
                 float &slack,
                 const RiseFall *&rf,
                 const Scene *&Scene);
  void maxSlewCheck(// Return values.
                    const Pin *&pin,
                    Slew &slew,
                    float &slack,
                    float &limit);
  void findSlewLimit(const LibertyPort *port,
                     const Scene *scene,
                     const MinMax *min_max,
                     // Return values.
                     float &limit,
                     bool &exists);
  size_t maxSlewViolationCount();

  ////////////////////////////////////////////////////////////////
  // net == nullptr to check all.
  void reportFanoutChecks(const Net *net,
                          size_t max_count,
                          bool violators,
                          bool verbose,
                          const SceneSeq &scenes,
                          const MinMax *min_max);
  void checkFanoutPreamble();
  // requires checkFanoutPreamble()
  void checkFanout(const Pin *pin,
                   const Mode *mode,
                   const MinMax *min_max,
                   // Return values.
                   float &fanout,
                   float &limit,
                   float &slack);
  // Return the pin etc with max fanout check min slack.
  void maxFanoutMinSlackPin(const ModeSeq &modes,
                            // Return values.
                            const Pin *&pin,
                            float &fanout,
                            float &limit,
                            float &slack,
                            const Mode *&mode);
  size_t fanoutViolationCount(const MinMax *min_max,
                              const ModeSeq &modes);

  ////////////////////////////////////////////////////////////////
  // net=null check all nets
  void reportCapacitanceChecks(const Net *net,
                               size_t max_count,
                               bool violators,
                               bool verbose,
                               const SceneSeq &scenes,
                               const MinMax *min_max);
  size_t maxCapacitanceViolationCount();
  void checkCapacitancesPreamble(const SceneSeq &scenes);
  // requires checkCapacitanceLimitPreamble()
  void checkCapacitance(const Pin *pin,
                        const SceneSeq &scenes,
                        const MinMax *min_max,
                        // Return values.
                        float &capacitance,
                        float &limit,
                        float &slack,
                        const RiseFall *&rf,
                        const Scene *&scene);
  void maxCapacitanceCheck(// Return values.
                           const Pin *&pin,
                           float &capacitance,
                           float &slack,
                           float &limit);

  ////////////////////////////////////////////////////////////////
  void reportMinPulseWidthChecks(const Net *net,
                                 size_t max_count,
                                 bool violators,
                                 bool verbose,
                                 const SceneSeq &scenes);

  ////////////////////////////////////////////////////////////////
  void reportMinPeriodChecks(const Net *net,
                             size_t max_count,
                             bool violators,
                             bool verbose,
                             const SceneSeq &scenes);

  ////////////////////////////////////////////////////////////////
  void reportMaxSkewChecks(const Net *net,
                           size_t max_count,
                           bool violators,
                           bool verbose,
                           const SceneSeq &scenes);
  
  ////////////////////////////////////////////////////////////////
  // User visible but non SDC commands.

  // Clear all state except network, scenes and liberty libraries.
  void clear();
  // Clear all state except network, scenes liberty libraries, and sdc.
  void clearNonSdc();
  // Namespace used by command interpreter.
  CmdNamespace cmdNamespace();
  void setCmdNamespace(CmdNamespace namespc);
  OperatingConditions *operatingConditions(const MinMax *min_max,
                                           const Sdc *sdc) const;
  // Set the delay on a timing arc.
  // Required/arrival times are incrementally updated.
  void setArcDelay(Edge *edge,
                   TimingArc *arc,
                   const Scene *scene,
                   const MinMaxAll *min_max,
                   ArcDelay delay);
  // Set annotated slew on a vertex for delay calculation.
  void setAnnotatedSlew(Vertex *vertex,
                        const Scene *scene,
                        const MinMaxAll *min_max,
                        const RiseFallBoth *rf,
                        float slew);
  void writeSdf(const char *filename,
                const Scene *scene,
                char divider,
                bool include_typ,
                int digits,
                bool gzip,
                bool no_timestamp,
                bool no_version);
  // Remove all delay and slew annotations.
  void removeDelaySlewAnnotations();
  // Instance specific process/voltage/temperature.
  // Defaults to operating condition if instance is not annotated.
  const Pvt *pvt(Instance *inst,
                 const MinMax *min_max,
                 Sdc *sdc);
  void setPvt(Instance *inst,
              const MinMaxAll *min_max,
              float process,
              float voltage,
              float temperature,
              Sdc *sdc);
  // Pvt may be shared among multiple instances.
  void setPvt(const Instance *inst,
              const MinMaxAll *min_max,
              const Pvt &pvt,
              Sdc *sdc);
  void setVoltage(const MinMax *min_max,
                  float voltage,
                  Sdc *sdc);
  void setVoltage(const Net *net,
                  const MinMax *min_max,
                  float voltage,
                  Sdc *sdc);

  CheckErrorSeq &checkTiming(const Mode *mode,
                             bool no_input_delay,
                             bool no_output_delay,
                             bool reg_multiple_clks,
                             bool reg_no_clks,
                             bool unconstrained_endpoints,
                             bool loops,
                             bool generated_clks);
  // Path from/thrus/to filter.
  // from/thrus/to are owned and deleted by Search.
  // PathEnds in the returned PathEndSeq are owned by Search PathGroups
  // and deleted on next call.
  PathEndSeq findPathEnds(ExceptionFrom *from,
                          ExceptionThruSeq *thrus,
                          ExceptionTo *to,
                          bool unconstrained,
                          const SceneSeq &scenes,
                          // max for setup checks.
                          // min for hold checks.
                          // min_max for setup and hold checks.
                          const MinMaxAll *min_max,
                          // Number of path ends to report in
                          // each group.
                          int group_path_count,
                          // Number of paths to report for
                          // each endpoint.
                          int endpoint_path_count,
                          // endpoint_path_count paths report unique pins
                          // without rise/fall variations.
                          bool unique_pins,
                          // endpoint_path_count paths report paths with
                          // unique pins and rise/fall edges.
                          bool unique_edges,
                          // Min/max bounds for slack of
                          // returned path ends.
                          float slack_min,
                          float slack_max,
                          // Sort path ends by slack ignoring path groups.
                          bool sort_by_slack,
                          // Path groups to report.
                          // Empty list reports all groups.
                          StdStringSeq &group_names,
                          // Predicates to filter the type of path
                          // ends returned.
                          bool setup,
                          bool hold,
                          bool recovery,
                          bool removal,
                          bool clk_gating_setup,
                          bool clk_gating_hold);
  void setReportPathFormat(ReportPathFormat format);
  void setReportPathFieldOrder(StringSeq *field_names);
  void setReportPathFields(bool report_input_pin,
                           bool report_hier_pins,
                           bool report_net,
                           bool report_cap,
                           bool report_slew,
                           bool report_fanout,
                           bool report_src_attr);
  ReportField *findReportPathField(const char *name);
  void setReportPathDigits(int digits);
  void setReportPathNoSplit(bool no_split);
  void setReportPathSigmas(bool report_sigmas);
  // Header above reportPathEnd results.
  void reportPathEndHeader();
  // Footer below reportPathEnd results.
  void reportPathEndFooter();
  // Format report_path_endpoint only:
  //   Previous path end is used to detect path group changes
  //   so headers are reported by group.
  void reportPathEnd(PathEnd *end,
                     PathEnd *prev_end,
                     bool last);
  void reportPathEnd(PathEnd *end);
  void reportPathEnds(PathEndSeq *ends);
  ReportPath *reportPath() { return report_path_; }
  void reportPath(const Path *path);

  // Report clk skews for clks.
  void reportClkSkew(ConstClockSeq &clks,
                     const SceneSeq &scenes,
                     const SetupHold *setup_hold,
                     bool include_internal_latency,
                     int digits);
  float findWorstClkSkew(const SetupHold *setup_hold,
                         bool include_internal_latency);

  void reportClkLatency(ConstClockSeq &clks,
                        const SceneSeq &scenes,
                        bool include_internal_latency,
                        int digits);
  // Find min/max/rise/fall delays for clk.
  ClkDelays findClkDelays(const Clock *clk,
                          const Scene *scene,
                          bool include_internal_latency);

  // Update arrival times for all pins.
  // If necessary updateTiming propagates arrivals around latch
  // loops until the arrivals converge.
  // If full=false update arrivals incrementally.
  // If full=true update all arrivals from scratch.
  // NOTE WELL: There is rarely any reason to call updateTiming directly because
  // arrival/required/slack functions implicitly update timing incrementally.
  // If you are calling this function you are either very confused or there is
  // bug that should be reported.
  void updateTiming(bool full);
  // Invalidate all delay calculations. Arrivals also invalidated.
  void delaysInvalid() const;
  // Invalidate all arrival and required times.
  void arrivalsInvalid();
  PinSet startpointPins();
  PinSet endpointPins();
  VertexSet &endpoints();
  int endpointViolationCount(const MinMax *min_max);
  // Find all required times after updateTiming().
  void findRequireds();
  std::string reportDelayCalc(Edge *edge,
                              TimingArc *arc,
                              const Scene *scene,
                              const MinMax *min_max,
                              int digits);
  void writeSdc(const Sdc *sdc,
                const char *filename,
                // Map hierarchical pins and instances to leaf pins and instances.
                bool leaf,
                // Replace non-sdc get functions with OpenSTA equivalents.
                bool native,
                int digits,
                bool gzip,
                bool no_timestamp);
  // The sum of all negative endpoints slacks.
  // Incrementally updated.
  Slack totalNegativeSlack(const MinMax *min_max);
  Slack totalNegativeSlack(const Scene *scene,
                           const MinMax *min_max);
  // Worst endpoint slack and vertex.
  // Incrementally updated.
  Slack worstSlack(const MinMax *min_max);
  void worstSlack(const MinMax *min_max,
                  // Return values.
                  Slack &worst_slack,
                  Vertex *&worst_vertex);
  void worstSlack(const Scene *scene,
                  const MinMax *min_max,
                  // Return values.
                  Slack &worst_slack,
                  Vertex *&worst_vertex);
  Path *vertexWorstArrivalPath(Vertex *vertex,
                               const RiseFall *rf,
                               const MinMax *min_max);
  Path *vertexWorstArrivalPath(Vertex *vertex,
                               const MinMax *min_max);
  Path *vertexWorstRequiredPath(Vertex *vertex,
                                const RiseFall *rf,
                                const MinMax *min_max);
  Path *vertexWorstRequiredPath(Vertex *vertex,
                                const MinMax *min_max);
  Path *vertexWorstSlackPath(Vertex *vertex,
                             const MinMax *min_max);
  Path *vertexWorstSlackPath(Vertex *vertex,
                             const RiseFall *rf,
                             const MinMax *min_max);

  // Find the min clock period for rise/rise and fall/fall paths of a clock
  // using the slack. This does NOT correctly predict min period when there
  // are paths between different clocks.
  float findClkMinPeriod(const Clock *clk,
                         bool include_port_paths);

  // The following arrival/required/slack functions incrementally
  // update timing to the level of the vertex.  They do NOT do multiple
  // passes required propagate arrivals around latch loops.
  // See Sta::updateTiming() to propagate arrivals around latch loops.
  Arrival arrival(const Pin *pin,
                  const RiseFallBoth *rf,
                  const MinMax *min_max);
  Arrival arrival(Vertex *vertex,
                  const RiseFallBoth *rf,
                  const SceneSeq &scenes,
                  const MinMax *min_max);

  Required required(Vertex *vertex,
                    const RiseFallBoth *rf,
                    const SceneSeq &scenes,
                    const MinMax *min_max);

  Slack slack(const Net *net,
              const MinMax *min_max);
  Slack slack(const Pin *pin,
              const RiseFallBoth *rf,
              const SceneSeq &scenes,
              const MinMax *min_max);

  Slack slack(Vertex *vertex,
              const MinMax *min_max);
  Slack slack(Vertex *vertex,
              const RiseFall *rf,
              const MinMax *min_max);
  Slack slack(Vertex *vertex,
              const RiseFallBoth *rf,
              const SceneSeq &scenes,
              const MinMax *min_max);

  void slacks(Vertex *vertex,
              Slack (&slacks)[RiseFall::index_count][MinMax::index_count]);
  // Worst slack for an endpoint in a path group.
  Slack endpointSlack(const Pin *pin,
                      const std::string &path_group_name,
                      const MinMax *min_max);

  void reportArrivalWrtClks(const Pin *pin,
                            const Scene *scene,
                            int digits);
  void reportRequiredWrtClks(const Pin *pin,
                             const Scene *scene,
                             int digits);
  void reportSlackWrtClks(const Pin *pin,
                          const Scene *scene,
                          int digits);

  Slew slew(Vertex *vertex,
            const RiseFallBoth *rf,
            const SceneSeq &scenes,
            const MinMax *min_max);

  ArcDelay arcDelay(Edge *edge,
                    TimingArc *arc,
                    DcalcAPIndex ap_index);
  // True if the timing arc has been back-annotated.
  bool arcDelayAnnotated(Edge *edge,
                         TimingArc *arc,
                         const Scene *scene,
                         const MinMax *min_max);
  // Set/unset the back-annotation flag for a timing arc.
  void setArcDelayAnnotated(Edge *edge,
                            TimingArc *arc,
                            const Scene *scene,
                            const MinMax *min_max,
                            bool annotated);
  // Make sure levels are up to date and return vertex level.
  Level vertexLevel(Vertex *vertex);
  GraphLoopSeq &graphLoops();
  TagIndex tagCount() const;
  TagGroupIndex tagGroupCount() const;
  int clkInfoCount() const;
  int pathCount() const;
  int vertexPathCount(Vertex  *vertex) const;
  Vertex *maxPathCountVertex() const;

  // Propagate liberty constant functions and pins tied high/low through
  // combinational logic and registers. This is mode/sdc independent.
  // Used by OpenROAD/Restructure.cpp
  void findLogicConstants();
  LogicValue simLogicValue(const Pin *pin,
                           const Mode *mode);
  // Clear propagated sim constants.
  void clearLogicConstants();

  // Instances sorted by max driver pin slew.
  InstanceSeq slowDrivers(int count);

  Parasitics *makeConcreteParasitics(std::string name,
                                     std::string filename);
  // Annotate hierarchical "instance" with parasitics.
  // The parasitic analysis point is ap_name.
  // The parasitic memory footprint is much smaller if parasitic
  // networks (dspf) are reduced and deleted after reading each net
  // with reduce_to and delete_after_reduce.
  // Return true if successful.
  bool readSpef(const std::string &name,
                const std::string &filename,
                Instance *instance,
                Scene *scene,
                const MinMaxAll *min_max,
                bool pin_cap_included,
                bool keep_coupling_caps,
                float coupling_cap_factor,
                bool reduce);
  Parasitics *findParasitics(const std::string &name);
  void reportParasiticAnnotation(const std::string &spef_name,
                                 bool report_unannotated);
  // Parasitics.
  void findPiElmore(Pin *drvr_pin,
                    const RiseFall *rf,
                    const MinMax *min_max,
                    float &c2,
                    float &rpi,
                    float &c1,
                    bool &exists) const;
  void findElmore(Pin *drvr_pin,
                  Pin *load_pin,
                  const RiseFall *rf,
                  const MinMax *min_max,
                  float &elmore,
                  bool &exists) const;
  void makePiElmore(Pin *drvr_pin,
                    const RiseFall *rf,
                    const MinMaxAll *min_max,
                    float c2,
                    float rpi,
                    float c1);
  void setElmore(Pin *drvr_pin,
                 Pin *load_pin,
                 const RiseFall *rf,
                 const MinMaxAll *min_max,
                 float elmore);
  void deleteParasitics();
  Parasitic *makeParasiticNetwork(const Net *net,
                                  bool includes_pin_caps,
                                  const Scene *scene,
                                  const MinMax *min_max);

  ////////////////////////////////////////////////////////////////
  // TCL network edit function support.

  virtual Instance *makeInstance(const char *name,
                                 LibertyCell *cell,
                                 Instance *parent);
  virtual void deleteInstance(Instance *inst);
  // replace_cell
  virtual void replaceCell(Instance *inst,
                           Cell *to_cell);
  virtual void replaceCell(Instance *inst,
                           LibertyCell *to_lib_cell);
  virtual Net *makeNet(const char *name,
                       Instance *parent);
  virtual void deleteNet(Net *net);
  // connect_net
  virtual void connectPin(Instance *inst,
                          Port *port,
                          Net *net);
  virtual void connectPin(Instance *inst,
                          LibertyPort *port,
                          Net *net);
  // disconnect_net
  virtual void disconnectPin(Pin *pin);
  virtual void makePortPin(const char *port_name,
                           PortDirection *dir);
  // Notify STA that the network has changed without using the network
  // editing API. For example, reading a netlist without using the
  // builtin network readers.
  void networkChanged();
  // Network changed but all SDC references to instance/net/pin/port are preserved.
  void networkChangedNonSdc();
  void deleteLeafInstanceBefore(const Instance *inst);
  void deleteInstancePinsBefore(const Instance *inst);

  // Network edit before/after methods.
  void makeInstanceAfter(const Instance *inst);
  // Replace the instance cell with to_cell.
  // equivCells(from_cell, to_cell) must be true.
  virtual void replaceEquivCellBefore(const Instance *inst,
                                      const LibertyCell *to_cell);
  virtual void replaceEquivCellAfter(const Instance *inst);
  // Replace the instance cell with to_cell.
  // equivCellPorts(from_cell, to_cell) must be true.
  virtual void replaceCellBefore(const Instance *inst,
                                 const LibertyCell *to_cell);
  virtual void replaceCellAfter(const Instance *inst);
  virtual void makePortPinAfter(Pin *pin);
  virtual void connectPinAfter(const Pin *pin);
  virtual void disconnectPinBefore(const Pin *pin);
  virtual void deleteNetBefore(const Net *net);
  virtual void deleteInstanceBefore(const Instance *inst);
  virtual void deletePinBefore(const Pin *pin);

  ////////////////////////////////////////////////////////////////

  void ensureClkNetwork(const Mode *mode);
  void clkPinsInvalid(const Mode *mode);
  // The following functions assume ensureClkNetwork() has been called.
  bool isClock(const Pin *pin,
               const Mode *mode) const;
  bool isClock(const Net *net,
               const Mode *mode) const;
  bool isIdealClock(const Pin *pin,
                    const Mode *mode) const;
  bool isPropagatedClock(const Pin *pin,
                         const Mode *mode) const;
  const PinSet *pins(const Clock *clk,
                     const Mode *mode);

  ////////////////////////////////////////////////////////////////

  void setTclInterp(Tcl_Interp *interp);
  Tcl_Interp *tclInterp();
  // Ensure a network has been read, and linked.
  Network *ensureLinked();
  // Ensure a network has been read, linked and liberty libraries exist.
  Network *ensureLibLinked();
  void ensureLevelized();
  // Ensure that the timing graph has been built.
  Graph *ensureGraph();
  void ensureClkArrivals();

  // Find all arc delays and vertex slews with delay calculator.
  virtual void findDelays();
  // Find arc delays and vertex slews thru to level of to_vertex.
  virtual void findDelays(Vertex *to_vertex);
  // Find arc delays and vertex slews thru to level.
  virtual void findDelays(Level level);
  // Percentage (0.0:1.0) change in delay that causes downstream
  // delays to be recomputed during incremental delay calculation.
  // Defaults to 0.0 for maximum accuracy and slowest incremental speed.
  void setIncrementalDelayTolerance(float tol);
  // Make graph and find delays.
  void searchPreamble();

  // Define the delay calculator implementation.
  void setArcDelayCalc(const char *delay_calc_name);

  void setDebugLevel(const char *what,
                     int level);

  // Delays and arrivals downsteam from inst are invalid.
  void delaysInvalidFrom(const Instance *inst);
  // Delays and arrivals downsteam from pin are invalid.
  void delaysInvalidFrom(const Pin *pin);
  void delaysInvalidFrom(Vertex *vertex);
  // Delays to driving pins of net (fanin) are invalid.
  // Arrivals downsteam from net are invalid.
  void delaysInvalidFromFanin(const Net *net);
  void delaysInvalidFromFanin(const Pin *pin);
  void delaysInvalidFromFanin(Vertex *vertex);
  void replaceCellPinInvalidate(const LibertyPort *from_port,
                                Vertex *vertex,
                                const LibertyCell *to_cell);

  // Power API.
  void reportPowerDesign(const Scene *scene,
                         int digits);
  void reportPowerInsts(const InstanceSeq &insts,
                        const Scene *scene,
                        int digits);
  void reportPowerHighestInsts(size_t count,
                               const Scene *scene,
                               int digits);
  void reportPowerDesignJson(const Scene *scene,
                             int digits);
  void reportPowerInstsJson(const InstanceSeq &insts,
                            const Scene *scene,
                            int digits);
  Power *power() { return power_; }
  const Power *power() const { return power_; }
  void power(const Scene *scene,
             // Return values.
             PowerResult &total,
             PowerResult &sequential,
             PowerResult &combinational,
             PowerResult &clock,
             PowerResult &macro,
             PowerResult &pad);
  PowerResult power(const Instance *inst,
                    const Scene *scene);
  PwrActivity activity(const Pin *pin,
                       const Scene *scene);

  void writeTimingModel(const char *lib_name,
                        const char *cell_name,
                        const char *filename,
                        const Scene *scene);

  // Find equivalent cells in equiv_libs.
  // Optionally add mappings for cells in map_libs.
  void makeEquivCells(LibertyLibrarySeq *equiv_libs,
                      LibertyLibrarySeq *map_libs);
  LibertyCellSeq *equivCells(LibertyCell *cell);

  void writePathSpice(Path *path,
                      const char *spice_filename,
                      const char *subckt_filename,
                      const char *lib_subckt_filename,
                      const char *model_filename,
                      const char *power_name,
                      const char *gnd_name,
                      CircuitSim ckt_sim);

  ////////////////////////////////////////////////////////////////
  // TCL Variables

  // TCL variable sta_crpr_enabled.
  // Common Reconvergent Clock Removal (CRPR).
  // Timing check source/target common clock path overlap for search
  // with analysis mode on_chip_variation.
  bool crprEnabled() const;
  void setCrprEnabled(bool enabled);
  // TCL variable sta_crpr_mode.
  CrprMode crprMode() const;
  void setCrprMode(CrprMode mode);
  // TCL variable sta_pocv_enabled.
  // Parametric on chip variation (statisical sta).
  bool pocvEnabled() const;
  void setPocvEnabled(bool enabled);
  // Number of std deviations from mean to use for normal distributions.
  void setSigmaFactor(float factor);
  // TCL variable sta_propagate_gated_clock_enable.
  // Propagate gated clock enable arrivals.
  bool propagateGatedClockEnable() const;
  void setPropagateGatedClockEnable(bool enable);
  // TCL variable sta_preset_clear_arcs_enabled.
  // Enable search through preset/clear arcs.
  bool presetClrArcsEnabled() const;
  void setPresetClrArcsEnabled(bool enable);
  // TCL variable sta_cond_default_arcs_enabled.
  // Enable/disable default arcs when conditional arcs exist.
  bool condDefaultArcsEnabled() const;
  void setCondDefaultArcsEnabled(bool enabled);
  // TCL variable sta_internal_bidirect_instance_paths_enabled.
  // Enable/disable timing from bidirect pins back into the instance.
  bool bidirectInstPathsEnabled() const;
  void setBidirectInstPathsEnabled(bool enabled);
  // TCL variable sta_recovery_removal_checks_enabled.
  bool recoveryRemovalChecksEnabled() const;
  void setRecoveryRemovalChecksEnabled(bool enabled);
  // TCL variable sta_gated_clock_checks_enabled.
  bool gatedClkChecksEnabled() const;
  void setGatedClkChecksEnabled(bool enabled);
  // TCL variable sta_dynamic_loop_breaking.
  bool dynamicLoopBreaking() const;
  void setDynamicLoopBreaking(bool enable);
  // TCL variable sta_propagate_all_clocks.
  // Clocks defined after sta_propagate_all_clocks is true
  // are propagated (existing clocks are not effected).
  bool propagateAllClocks() const;
  void setPropagateAllClocks(bool prop);
  // TCL var sta_clock_through_tristate_enabled.
  bool clkThruTristateEnabled() const;
  void setClkThruTristateEnabled(bool enable);
  // TCL variable sta_input_port_default_clock.
  bool useDefaultArrivalClock() const;
  void setUseDefaultArrivalClock(bool enable);
  ////////////////////////////////////////////////////////////////

  Properties &properties() { return properties_; }

protected:
  // Default constructors that are called by makeComponents in the Sta
  // constructor.  These can be redefined by a derived class to
  // specialize the sta components.
  virtual void makeVariables();
  virtual void makeReport();
  virtual void makeDebug();
  virtual void makeUnits();
  virtual void makeNetwork();
  virtual void makeSdcNetwork();
  virtual void makeGraph();
  virtual void makeDefaultScene();
  virtual void makeLevelize();
  virtual void makeArcDelayCalc();
  virtual void makeGraphDelayCalc();
  virtual void makeSearch();
  virtual void makeLatches(); 
  virtual void makeCheckTiming();
  virtual void makeCheckSlews();
  virtual void makeCheckFanouts();
  virtual void makeCheckCapacitances();
  virtual void makeCheckMinPulseWidths();
  virtual void makeCheckMinPeriods();
  virtual void makeCheckMaxSkews();
  virtual void makeReportPath();
  virtual void makePower();
  virtual void makeClkSkews();
  virtual void makeObservers();
  NetworkEdit *networkCmdEdit();

  LibertyLibrary *readLibertyFile(const char *filename,
                                  Scene *scene,
                                  const MinMaxAll *min_max,
                                  bool infer_latches);
  // Allow external Liberty reader to parse forms not used by Sta.
  virtual LibertyLibrary *readLibertyFile(const char *filename,
                                          bool infer_latches);
  void delayCalcPreamble();
  void delaysInvalidFrom(const Port *port);
  void delaysInvalidFromFanin(const Port *port);
  void deleteEdge(Edge *edge);
  void netParasiticCaps(Net *net,
                        const RiseFall *rf,
                        const MinMax *min_max,
                        float &pin_cap,
                        float &wire_cap) const;
  const Pin *findNetParasiticDrvrPin(const Net *net) const;
  void exprConstantPins(FuncExpr *expr,
                        const Instance *inst,
                        const Mode *mode,
                        // Return value.
                        PinSet &pins);
  void findRequired(Vertex *vertex);

  void reportDelaysWrtClks(const Pin *pin,
                           const Scene *scene,
                           int digits,
                           PathDelayFunc get_path_delay);
  void reportDelaysWrtClks(Vertex *vertex,
                           const Scene *scene,
                           int digits,
                           PathDelayFunc get_path_delay);
  void reportDelaysWrtClks(Vertex *vertex,
                           const ClockEdge *clk_edge,
                           const Scene *scene,
                           int digits,
                           PathDelayFunc get_path_delay);
  RiseFallMinMaxDelay findDelaysWrtClks(Vertex *vertex,
                                        const ClockEdge *clk_edge,
                                        const Scene *scene,
                                        PathDelayFunc get_path_delay);
  std::string formatDelay(const RiseFall *rf,
                          const MinMax *min_max,
                          const RiseFallMinMaxDelay &delays,
                          int digits);

  void connectDrvrPinAfter(Vertex *vertex);
  void connectLoadPinAfter(Vertex *vertex);
  Path *latchEnablePath(Path *q_path,
                        Edge *d_q_edge,
                        const ClockEdge *en_clk_edge);
  void clockSlewChanged(Clock *clk);
  void maxSkewPreamble();
  bool idealClockMode();
  void disableAfter();
  void findFaninPins(Vertex *vertex,
                     bool flat,
                     bool startpoints_only,
                     int inst_levels,
                     int pin_levels,
                     PinSet &fanin,
                     SearchPred &pred,
                     const Mode *mode);
  void findFaninPins(Vertex *to,
                     bool flat,
                     int inst_levels,
                     int pin_levels,
                     VertexSet &visited,
                     SearchPred *pred,
                     int inst_level,
                     int pin_level,
                     const Mode *mode);
  void findFanoutPins(Vertex *vertex,
                      bool flat,
                      bool endpoints_only,
                      int inst_levels,
                      int pin_levels,
                      PinSet &fanout,
                      SearchPred &pred,
                      const Mode *mode);
  void findFanoutPins(Vertex *from,
                      bool flat,
                      int inst_levels,
                      int pin_levels,
                      VertexSet &visited,
                      SearchPred *pred,
                      int inst_level,
                      int pin_level,
                     const Mode *mode);
  void findRegisterPreamble(const Mode *mode);
  bool crossesHierarchy(Edge *edge) const;
  void powerPreamble();
  void powerPreamble(const Scene *scene);
  virtual void replaceCell(Instance *inst,
                           Cell *to_cell,
                           LibertyCell *to_lib_cell);
  void clkSkewPreamble();
  void setCmdNamespace1(CmdNamespace namespc);
  void setThreadCount1(int thread_count);
  void updateLibertyScenes();
  void updateSceneLiberty(Scene *scene,
                          const StdStringSeq &liberty_files,
                          const MinMax *min_max);
  LibertyLibrary *findLibertyFileBasename(const std::string &filename) const;

  Scene *makeScene(const std::string &name,
                   Mode *mode,
                   Parasitics *parasitics_min,
                   Parasitics *parasitics_max);
  Scene *makeScene(const std::string &name,
                   Mode *mode,
                   Parasitics *parasitics);
  void deleteScenes();

  Scene *cmd_scene_;
  CmdNamespace cmd_namespace_;
  Instance *current_instance_;
  SceneNameMap scene_name_map_;
  ModeNameMap mode_name_map_;
  ParasiticsNameMap parasitics_name_map_;
  VerilogReader *verilog_reader_;
  CheckTiming *check_timing_;
  CheckSlews *check_slews_;
  CheckFanouts *check_fanouts_;
  CheckCapacitances *check_capacitances_;
  CheckMinPulseWidths *check_min_pulse_widths_;
  CheckMinPeriods *check_min_periods_;
  CheckMaxSkews *check_max_skews_;
  ClkSkews *clk_skews_;
  ReportPath *report_path_;
  Power *power_;
  Tcl_Interp *tcl_interp_;
  bool update_genclks_;
  EquivCells *equiv_cells_;
  Properties properties_;

  // Singleton sta used by tcl command interpreter.
  static Sta *sta_;
};

} // namespace
