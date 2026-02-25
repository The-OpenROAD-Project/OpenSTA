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
#include <map>
#include <unordered_map>
#include <mutex>

#include "StringUtil.hh"
#include "StringSet.hh"
#include "MinMax.hh"
#include "StaState.hh"
#include "NetworkClass.hh"
#include "LibertyClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "RiseFallValues.hh"
#include "Clock.hh"
#include "DataCheck.hh"
#include "CycleAccting.hh"
#include "ExceptionPath.hh"
#include "PinPair.hh"

namespace sta {

class OperatingConditions;
class PortExtCap;
class ClockGatingCheck;
class InputDriveCell;
class DisabledPorts;
class GraphLoop;
class DeratingFactors;
class DeratingFactorsGlobal;
class DeratingFactorsNet;
class DeratingFactorsCell;
class PatternMatch;
class FindNetCaps;
class ClkHpinDisable;
class FindClkHpinDisables;
class Scene;
class ClockPinIterator;
class ClockIterator;

using PinClockPair = std::pair<const Pin*, const Clock*>;

class ClockInsertionkLess
{
public:
  ClockInsertionkLess(const Network *network);
  bool operator()(const ClockInsertion *insert1,
                  const ClockInsertion *insert2) const;

private:
  const Network *network_;
};

class ClockLatencyLess
{
public:
  ClockLatencyLess(const Network *network);
  bool operator()(const ClockLatency *latency1,
                  const ClockLatency *latency2) const;

private:
  const Network *network_;
};

// This is symmetric with respect to the clocks
// in the pair so Pair(clk1, clk2) is the same
// as Pair(clk2, clk1).
class ClockPairLess
{
public:
  bool operator()(const ClockPair &pair1,
                  const ClockPair &pair2) const;
};

class PinClockPairLess
{
public:
  PinClockPairLess(const Network *network);
  bool operator()(const PinClockPair &pin_clk1,
                  const PinClockPair &pin_clk2) const;

protected:
  const Network *network_;
};

class ClkHpinDisableLess
{ 
public:
  ClkHpinDisableLess(const Network *network);
  bool operator()(const ClkHpinDisable *disable1,
                  const ClkHpinDisable *disable2) const;

private:
  const Network *network_;
};

class NetWireCaps : public MinMaxFloatValues
{
public:
  NetWireCaps();
  bool subtractPinCap(const MinMax *min_max);
  void setSubtractPinCap(bool subtrace_pin_cap,
                         const MinMax *min_max);

private:
  bool subtract_pin_cap_[MinMax::index_count];
};

using ClockNameMap = std::map<const char*,Clock*, CharPtrLess>;
using ClockPinMap = std::unordered_map<const Pin*, ClockSet*, PinIdHash>;
using InputDelaySet = std::set<InputDelay*>;
using InputDelaysPinMap = std::map<const Pin*, InputDelaySet*, PinIdLess>;
using OutputDelaySet = std::set<OutputDelay*>;
using OutputDelaysPinMap = std::map<const Pin*, OutputDelaySet*, PinIdLess>;
using PinExceptionsMap = std::unordered_map<const Pin*, ExceptionPathSet>;
using ClockExceptionsMap = std::unordered_map<const Clock*, ExceptionPathSet>;
using InstanceExceptionsMap = std::unordered_map<const Instance*,ExceptionPathSet>;
using NetExceptionsMap = std::unordered_map<const Net*, ExceptionPathSet>;
using EdgeExceptionsMap = std::unordered_map<EdgePins, ExceptionPathSet,
                                             PinPairHash, PinPairEqual>;
using InputDriveMap = std::map<const Port*,InputDrive*>;
using ExceptionPathPtHash = std::map<size_t, ExceptionPathSet>;
using ClockLatencies = std::set<ClockLatency*, ClockLatencyLess>;
using EdgeClockLatencyMap = std::map<PinPair, ClockLatency*, PinPairLess>;
using PinClockUncertaintyMap = std::map<const Pin*, ClockUncertainties*>;
using InterClockUncertaintySet = std::set<InterClockUncertainty*, InterClockUncertaintyLess>;
using ClockGatingCheckMap = std::map<const Clock*, ClockGatingCheck*>;
using InstanceClockGatingCheckMap = std::map<const Instance*, ClockGatingCheck*>;
using PinClockGatingCheckMap = std::map<const Pin*, ClockGatingCheck*>;
using ClockInsertions = std::set<ClockInsertion*, ClockInsertionkLess>;
using PinLatchBorrowLimitMap = std::map<const Pin*, float>;
using InstLatchBorrowLimitMap = std::map<const Instance*, float>;
using ClockLatchBorrowLimitMap = std::map<const Clock*, float>;
using DataCheckSet = std::set<DataCheck*, DataCheckLess>;
using DataChecksMap = std::map<const Pin*, DataCheckSet*>;
using NetResistanceMap = std::map<const Net*, MinMaxFloatValues>;
using PortSlewLimitMap = std::map<const Port*, MinMaxFloatValues>;
using PinSlewLimitMap = std::map<const Pin*, MinMaxFloatValues>;
using CellSlewLimitMap = std::map<const Cell*, MinMaxFloatValues>;
using CellCapLimitMap = std::map<const Cell*, MinMaxFloatValues>;
using PortCapLimitMap = std::map<const Port*, MinMaxFloatValues>;
using PinCapLimitMap = std::map<const Pin*, MinMaxFloatValues>;
using PortFanoutLimitMap = std::map<const Port*, MinMaxFloatValues>;
using CellFanoutLimitMap = std::map<const Cell*, MinMaxFloatValues>;
using PortExtCapMap = std::map<const Port*, PortExtCap, PortIdLess>;
using NetWireCapMap = std::map<const Net*, NetWireCaps, NetIdLess>;
using PinWireCapMap = std::map<const Pin*, NetWireCaps*, PinIdLess>;
using InstancePvtMap = std::map<const Instance*, Pvt*>;
using PinMinPulseWidthMap = std::map<const Pin*, RiseFallValues*>;
using ClockMinPulseWidthMap = std::map<const Clock*, RiseFallValues*>;
using InstMinPulseWidthMap = std::map<const Instance*, RiseFallValues*>;
using NetDeratingFactorsMap = std::map<const Net*, DeratingFactorsNet*>;
using InstDeratingFactorsMap = std::map<const Instance*, DeratingFactorsCell*>;
using CellDeratingFactorsMap = std::map<const LibertyCell*, DeratingFactorsCell*>;
using ClockGroupsSet = std::set<ClockGroups*>;
using ClockGroupsClkMap = std::map<const Clock*, ClockGroupsSet*>;
using ClockGroupsNameMap = std::map<const char*, ClockGroups*, CharPtrLess>;
using ClockSenseMap = std::map<PinClockPair, ClockSense, PinClockPairLess>;
using ClkHpinDisables = std::set<ClkHpinDisable*, ClkHpinDisableLess>;
using GroupPathSet = std::set<GroupPath*, ExceptionPathLess>;
using GroupPathMap = std::map<const char*, GroupPathSet*, CharPtrLess>;
using ClockPairSet = std::set<ClockPair, ClockPairLess>;
using NetVoltageMap = std::map<const Net*, MinMaxFloatValues>;

void
findLeafLoadPins(const Pin *pin,
                 const Network *network,
                 PinSet *leaf_pins);
void
findLeafDriverPins(const Pin *pin,
                   const Network *network,
                   PinSet *leaf_pins);

class Sdc : public StaState
{
public:
  Sdc(Mode *mode,
      StaState *sta);
  ~Sdc();
  Mode *mode() const { return mode_; }
  // Note that Search may reference a Filter exception removed by clear().
  void clear();
  void makeSceneBefore();
  // Return true if pin is referenced by any constraint.
  bool isConstrained(const Pin *pin) const;
  // Return true if inst is referenced by any constraint.
  // Does NOT include references by pins connected to the instance.
  bool isConstrained(const Instance *inst) const;
  // Return true if net is referenced by any constraint.
  // Does NOT include references by pins connected to the net.
  bool isConstrained(const Net *net) const;
  // Build data structures for search.
  void searchPreamble();
  void deleteNetBefore(const Net *net);
  void deleteInstanceBefore(const Instance *inst);

  // SWIG sdc interface.
  PortSeq allInputs(bool no_clks) const;
  PortSeq allOutputs() const;
  AnalysisType analysisType() const { return analysis_type_; }
  void setAnalysisType(AnalysisType analysis_type);
  void setOperatingConditions(OperatingConditions *op_cond,
                              const MinMaxAll *min_max);
  void setOperatingConditions(OperatingConditions *op_cond,
                              const MinMax *min_max);
  void setTimingDerate(TimingDerateType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate);
  // Delay type is always net for net derating.
  void setTimingDerate(const Net *net,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate);
  void setTimingDerate(const Instance *inst,
                       TimingDerateCellType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate);
  void setTimingDerate(const LibertyCell *cell,
                       TimingDerateCellType type,
                       PathClkOrData clk_data,
                       const RiseFallBoth *rf,
                       const EarlyLate *early_late,
                       float derate);
  float timingDerateInstance(const Pin *pin,
                             TimingDerateCellType type,
                             PathClkOrData clk_data,
                             const RiseFall *rf,
                             const EarlyLate *early_late) const;
  float timingDerateNet(const Pin *pin,
                        PathClkOrData clk_data,
                        const RiseFall *rf,
                        const EarlyLate *early_late) const;
  void unsetTimingDerate();
  static void swapDeratingFactors(Sdc *sdc1,
                                  Sdc *sdc2);

  void setInputSlew(const Port *port,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max,
                    float slew);
  // Set the rise/fall drive resistance on design port.
  void setDriveResistance(const Port *port,
                          const RiseFallBoth *rf,
                          const MinMaxAll *min_max,
                          float res);
  // Set the drive on design port using external cell timing arcs of
  // cell driven by from_slews between from_port and to_port.
  void setDriveCell(const LibertyLibrary *library,
                    const LibertyCell *cell,
                    const Port *port,
                    const LibertyPort *from_port,
                    float *from_slews,
                    const LibertyPort *to_port,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max);
  void setLatchBorrowLimit(const Pin *pin,
                           float limit);
  void setLatchBorrowLimit(const Instance *inst,
                           float limit);
  void setLatchBorrowLimit(const Clock *clk,
                           float limit);
  // Return the latch borrow limit respecting precedence if multiple
  // limits apply.
  void latchBorrowLimit(const Pin *data_pin,
                        const Pin *enable_pin,
                        const Clock *clk,
                        // Return values.
                        float &limit,
                        bool &exists);
  void setMinPulseWidth(const RiseFallBoth *rf,
                        float min_width);
  void setMinPulseWidth(const Pin *pin,
                        const RiseFallBoth *rf,
                        float min_width);
  void setMinPulseWidth(const Instance *inst,
                        const RiseFallBoth *rf,
                        float min_width);
  void setMinPulseWidth(const Clock *clk,
                        const RiseFallBoth *rf,
                        float min_width);
  // Return min pulse with respecting precedence.
  void minPulseWidth(const Pin *pin,
                     const Clock *clk,
                     const RiseFall *hi_low,
                     float &min_width,
                     bool &exists) const;
  void setSlewLimit(Clock *clk,
                    const RiseFallBoth *rf,
                    const PathClkOrData clk_data,
                    const MinMax *min_max,
                    float slew);
  bool haveClkSlewLimits() const;
  void slewLimit(const Clock *clk,
                 const RiseFall *rf,
                 const PathClkOrData clk_data,
                 const MinMax *min_max,
                 float &slew,
                 bool &exists) const;
  void slewLimit(Port *port,
                 const MinMax *min_max,
                 float &slew,
                 bool &exists) const;
  void setSlewLimit(Port *port,
                    const MinMax *min_max,
                    float slew);
  void slewLimit(Cell *cell,
                 const MinMax *min_max,
                 float &slew,
                 bool &exists) const;
  void setSlewLimit(Cell *cell,
                    const MinMax *min_max,
                    float slew);
  void capacitanceLimit(Port *port,
                        const MinMax *min_max,
                        float &cap,
                        bool &exists) const;
  void capacitanceLimit(Pin *pin,
                        const MinMax *min_max,
                        float &cap,
                        bool &exists) const;
  void capacitanceLimit(Cell *cell,
                        const MinMax *min_max,
                        float &cap,
                        bool &exists) const;
  void setCapacitanceLimit(Port *port,
                           const MinMax *min_max,
                           float cap);
  void setCapacitanceLimit(Pin *pin,
                           const MinMax *min_max,
                           float cap);
  void setCapacitanceLimit(Cell *cell,
                           const MinMax *min_max,
                           float cap);
  void fanoutLimit(Port *port,
                   const MinMax *min_max,
                   float &fanout,
                   bool &exists) const;
  void setFanoutLimit(Port *port,
                      const MinMax *min_max,
                      float fanout);
  void fanoutLimit(Cell *cell,
                   const MinMax *min_max,
                   float &fanout,
                   bool &exists) const;
  void setFanoutLimit(Cell *cell,
                      const MinMax *min_max,
                      float fanout);
  void setMaxArea(float area);
  float maxArea() const;
  Clock *makeClock(const char *name,
                   PinSet *pins,
                   bool add_to_pins,
                   float period,
                   FloatSeq *waveform,
                   const char *comment);
  // edges size must be 3.
  Clock *makeGeneratedClock(const char *name,
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
                            const char *comment);
  // Invalidate all generated clock waveforms.
  void invalidateGeneratedClks() const;
  void removeClock(Clock *clk);
  // Clock used for inputs without defined arrivals.
  ClockEdge *defaultArrivalClockEdge() const;
  Clock *defaultArrivalClock() const { return default_arrival_clk_; }
  // Propagated (non-ideal) clocks.
  void setPropagatedClock(Clock *clk);
  void removePropagatedClock(Clock *clk);
  void setPropagatedClock(Pin *pin);
  void removePropagatedClock(Pin *pin);
  bool isPropagatedClock(const Pin *pin) const;
  void setClockSlew(Clock *clk,
                    const RiseFallBoth *rf,
                    const MinMaxAll *min_max,
                    float slew);
  void removeClockSlew(Clock *clk);
  // Latency can be on a clk, pin, or clk/pin combination.
  void setClockLatency(Clock *clk,
                       Pin *pin,
                       const RiseFallBoth *rf,
                       const MinMaxAll *min_max,
                       float delay);
  void removeClockLatency(const Clock *clk,
                          const Pin *pin);
  ClockLatency *clockLatency(Edge *edge) const;
  bool hasClockLatency(const Pin *pin) const;
  void clockLatency(Edge *edge,
                    const RiseFall *rf,
                    const MinMax *min_max,
                    // Return values.
                    float &latency,
                    bool &exists) const;
  ClockLatencies *clockLatencies() { return &clk_latencies_; }
  const ClockLatencies *clockLatencies() const { return &clk_latencies_; }
  // Clock latency on pin with respect to clk.
  // This does NOT check for latency on clk (without pin).
  void clockLatency(const Clock *clk,
                    const Pin *pin,
                    const RiseFall *rf,
                    const MinMax *min_max,
                    // Return values.
                    float &latency,
                    bool &exists) const;
  void clockLatency(const Clock *clk,
                    const RiseFall *rf,
                    const MinMax *min_max,
                    // Return values.
                    float &latency,
                    bool &exists) const;
  float clockLatency(const Clock *clk,
                     const RiseFall *rf,
                     const MinMax *min_max) const;
  // Clock insertion delay (set_clk_latency -source).
  // Insertion delay can be on a clk, pin, or clk/pin combination.
  void setClockInsertion(const Clock *clk,
                         const Pin *pin,
                         const RiseFallBoth *rf,
                         const MinMaxAll *min_max,
                         const EarlyLateAll *early_late,
                         float delay);
  void setClockInsertion(const Clock *clk, const Pin *pin,
                         const RiseFall *rf,
                         const MinMax *min_max,
                         const EarlyLate *early_late,
                         float delay);
  void removeClockInsertion(const Clock *clk,
                            const Pin *pin);
  static void swapClockInsertions(Sdc *sdc1,
                                  Sdc *sdc2);
  bool hasClockInsertion(const Pin *pin) const;
  float clockInsertion(const Clock *clk,
                       const RiseFall *rf,
                       const MinMax *min_max,
                       const EarlyLate *early_late) const;
  // Respects precedence of pin/clk and set_input_delay on clk pin.
  void clockInsertion(const Clock *clk,
                      const Pin *pin,
                      const RiseFall *rf,
                      const MinMax *min_max,
                      const EarlyLate *early_late,
                      // Return values.
                      float &insertion,
                      bool &exists) const;
  const ClockInsertions &clockInsertions() const { return clk_insertions_; }
  // Clock uncertainty.
  void setClockUncertainty(Pin *pin,
                           const SetupHoldAll *setup_hold,
                           float uncertainty);
  void removeClockUncertainty(Pin *pin,
                              const SetupHoldAll *setup_hold);
  void setClockUncertainty(Clock *from_clk,
                           const RiseFallBoth *from_rf,
                           Clock *to_clk,
                           const RiseFallBoth *to_rf,
                           const SetupHoldAll *setup_hold,
                           float uncertainty);
  void removeClockUncertainty(Clock *from_clk,
                              const RiseFallBoth *from_rf,
                              Clock *to_clk,
                              const RiseFallBoth *to_rf,
                              const SetupHoldAll *setup_hold);
  ClockGroups *makeClockGroups(const char *name,
                               bool logically_exclusive,
                               bool physically_exclusive,
                               bool asynchronous,
                               bool allow_paths,
                               const char *comment);
  void makeClockGroup(ClockGroups *clk_groups,
                      ClockSet *clks);
  void removeClockGroups(const char *name);
  // nullptr name removes all.
  void removeClockGroupsLogicallyExclusive(const char *name);
  void removeClockGroupsPhysicallyExclusive(const char *name);
  void removeClockGroupsAsynchronous(const char *name);
  bool sameClockGroup(const Clock *clk1,
                      const Clock *clk2) const;
  // Clocks explicitly excluded by set_clock_group.
  bool sameClockGroupExplicit(const Clock *clk1,
                              const Clock *clk2);
  void setClockSense(PinSet *pins,
                     ClockSet *clks,
                     ClockSense sense);
  bool clkStopPropagation(const Pin *pin,
                          const Clock *clk) const;
  bool clkStopPropagation(const Clock *clk,
                          const Pin *from_pin,
                          const RiseFall *from_rf,
                          const Pin *to_pin,
                          const RiseFall *to_rf) const;
  void setClockGatingCheck(const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin);
  void setClockGatingCheck(Instance *inst,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           LogicValue active_value);
  void setClockGatingCheck(Clock *clk,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin);
  void setClockGatingCheck(const Pin *pin,
                           const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin,
                           LogicValue active_value);
  void setDataCheck(Pin *from,
                    const RiseFallBoth *from_rf,
                    Pin *to,
                    const RiseFallBoth *to_rf,
                    Clock *clk,
                    const SetupHoldAll *setup_hold,
                    float margin);
  void removeDataCheck(Pin *from,
                       const RiseFallBoth *from_rf,
                       Pin *to,
                       const RiseFallBoth *to_rf,
                       Clock *clk,
                       const SetupHoldAll *setup_hold);
  DataCheckSet *dataChecksFrom(const Pin *from) const;
  DataCheckSet *dataChecksTo(const Pin *to) const;
  void setInputDelay(const Pin *pin,
                     const RiseFallBoth *rf,
                     const Clock *clk,
                     const RiseFall *clk_rf,
                     const Pin *ref_pin,
                     bool source_latency_included,
                     bool network_latency_included,
                     const MinMaxAll *min_max,
                     bool add,
                     float delay);
  void removeInputDelay(const Pin *pin,
                        const RiseFallBoth *rf,
                        const Clock *clk,
                        const RiseFall *clk_rf,
                        const MinMaxAll *min_max);
  void setOutputDelay(const Pin *pin,
                      const RiseFallBoth *rf,
                      const Clock *clk,
                      const RiseFall *clk_tr,
                      const Pin *ref_pin,
                      bool source_latency_included,
                      bool network_latency_included,
                      const MinMaxAll *min_max,
                      bool add,
                      float delay);
  void removeOutputDelay(const Pin *pin,
                         const RiseFallBoth *rf,
                         const Clock *clk,
                         const RiseFall *clk_rf,
                         const MinMaxAll *min_max);
  static void swapPortDelays(Sdc *sdc1,
                             Sdc *sdc2);

  // Set port external pin load (set_load -pin_load port).
  void setPortExtPinCap(const Port *port,
                        const RiseFall *rf,
                        const MinMax *min_max,
                        float cap);
  // Set port external wire load (set_load -wire port).
  void setPortExtWireCap(const Port *port,
                         const RiseFall *rf,
                         const MinMax *min_max,
                         float cap);
  static void swapPortExtCaps(Sdc *sdc1,
                              Sdc *sdc2);
  // Remove all "set_load net" annotations.
  void removeNetLoadCaps();
  void setNetWireCap(const Net *net,
                     bool subtract_pin_cap,
                     const MinMax *min_max,
                     float wire_cap);
  bool hasNetWireCap(const Net *net) const;
  // True if driver pin net has wire capacitance.
  bool drvrPinHasWireCap(const Pin *pin) const;
  // Net wire capacitance (set_load -wire net).
  void drvrPinWireCap(const Pin *drvr_pin,
                      const MinMax *min_max,
                      // Return values.
                      float &cap,
                      bool &exists,
                      bool &subtract_pin_cap) const;
  // Pin capacitance derated by operating conditions and instance pvt.
  float pinCapacitance(const Pin *pin,
                       const RiseFall *rf,
                       const Scene *scene,
                       const MinMax *min_max) const;
  void setResistance(const Net *net,
                     const MinMaxAll *min_max,
                     float res);
  void resistance(const Net *net,
                  const MinMax *min_max,
                  float &res,
                  bool &exists) const;
  const NetResistanceMap &netResistances() const { return net_res_map_; }
  void setPortExtFanout(const Port *port,
                        const MinMax *min_max,
                        int fanout);
  // set_disable_timing cell [-from] [-to]
  // Disable all edges thru cell if from/to are null.
  // Bus and bundle ports are NOT supported.
  void disable(LibertyCell *cell,
               LibertyPort *from,
               LibertyPort *to);
  void removeDisable(LibertyCell *cell,
                     LibertyPort *from,
                     LibertyPort *to);
  // set_disable_timing liberty port.
  // Bus and bundle ports are NOT supported.
  void disable(LibertyPort *port);
  void removeDisable(LibertyPort *port);
  // set_disable_timing port (top level instance port).
  // Bus and bundle ports are NOT supported.
  void disable(Port *port);
  void removeDisable(Port *port);
  // set_disable_timing instance [-from] [-to].
  // Disable all edges thru instance if from/to are null.
  // Bus and bundle ports are NOT supported.
  void disable(Instance *inst,
               LibertyPort *from,
               LibertyPort *to);
  void removeDisable(Instance *inst,
                     LibertyPort *from,
                     LibertyPort *to);
  // set_disable_timing pin
  void disable(const Pin *pin);
  void removeDisable(Pin *pin);
  // set_disable_timing [get_timing_arc -of_objects instance]]
  void disable(Edge *edge);
  void removeDisable(Edge *edge);
  // set_disable_timing [get_timing_arc -of_objects lib_cell]]
  void disable(TimingArcSet *arc_set);
  void removeDisable(TimingArcSet *arc_set);
  // Disable a wire edge.  From/to pins musts be on the same net.
  // There is no SDC equivalent to this.
  void disableWire(const Pin *from,
                   const Pin *to);
  void removeDisableWire(Pin *from,
                         Pin *to);
  [[nodiscard]] bool isDisabledWire(const Pin *from,
                                     const Pin *to) const;
  [[nodiscard]] bool isDisabledConstraint(const Pin *pin) const;
  // Edge disabled by hierarchical pin disable or instance/cell port pair.
  // Disables do NOT apply to timing checks.
  // inst can be either the from_pin or to_pin instance because it
  // is only referenced when they are the same (non-wire edge).
  [[nodiscard]] bool isDisabled(const Instance *inst,
                                 const Pin *from_pin,
                                 const Pin *to_pin,
                                 const TimingRole *role) const;
  [[nodiscard]] bool isDisabled(const Edge *edge) const;
  [[nodiscard]] bool isDisabled(TimingArcSet *arc_set) const;
  const DisabledCellPortsMap *disabledCellPorts() const;
  const DisabledInstancePortsMap *disabledInstancePorts() const;
  const PinSet *disabledPins() const { return &disabled_pins_; }
  const PortSet *disabledPorts() const { return &disabled_ports_; }
  const LibertyPortSet *disabledLibPorts() const { return &disabled_lib_ports_; }
  const EdgeSet *disabledEdges() const { return &disabled_edges_; }
  [[nodiscard]] bool isDisabledConstraint(const Edge *edge) const;

  void disableClockGatingCheck(Instance *inst);
  void disableClockGatingCheck(Pin *pin);
  void removeDisableClockGatingCheck(Instance *inst);
  void removeDisableClockGatingCheck(Pin *pin);
  bool isDisableClockGatingCheck(const Pin *pin) const;
  bool isDisableClockGatingCheck(const Instance *inst) const;
  // set_LogicValue::zero, set_LogicValue::one, set_logic_dc
  void setLogicValue(const Pin *pin,
                     LogicValue value);
  // set_case_analysis
  void setCaseAnalysis(const Pin *pin,
                       LogicValue value);
  void removeCaseAnalysis(const Pin *pin);
  void logicValue(const Pin *pin,
                  LogicValue &value,
                  bool &exists) const;
  void caseLogicValue(const Pin *pin,
                      LogicValue &value,
                      bool &exists) const;
  // Pin has set_case_analysis or set_logic constant value.
  bool hasLogicValue(const Pin *pin) const;

  // The from/thrus/to arguments passed into the following functions
  // that make exceptions are owned by the constraints once they are
  // passed in.  The constraint internals may change or delete them do
  // to exception merging.
  void makeFalsePath(ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const MinMaxAll *min_max,
                     const char *comment);
  // Loop paths are false paths used to disable paths around
  // combinational loops when dynamic loop breaking is enabled.
  void makeLoopExceptions();
  void makeLoopExceptions(GraphLoop *loop);
  void deleteLoopExceptions();
  void makeMulticyclePath(ExceptionFrom *from,
                          ExceptionThruSeq *thrus,
                          ExceptionTo *to,
                          const MinMaxAll *min_max,
                          bool use_end_clk,
                          int path_multiplier,
                          const char *comment);
  void makePathDelay(ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const MinMax *min_max,
                     bool ignore_clk_latency,
                     bool break_path,
                     float delay,
                     const char *comment);
  bool pathDelaysWithoutTo() const { return path_delays_without_to_; }
  // Delete matching false/multicycle/path_delay exceptions.
  // Caller owns from, thrus, to exception points (and must delete them).
  void resetPath(ExceptionFrom *from,
                 ExceptionThruSeq *thrus,
                 ExceptionTo *to,
                 const MinMaxAll *min_max);
  void makeGroupPath(const char *name,
                     bool is_default,
                     ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to,
                     const char *comment);
  bool isGroupPathName(const char *group_name) const;
  const GroupPathMap &groupPaths() const { return group_path_map_; }
  void addException(ExceptionPath *exception);
  // The pin/clk/instance/net set arguments passed into the following
  // functions that make exception from/thru/to's are owned by the
  // constraints once they are passed in.
  ExceptionFrom *makeExceptionFrom(PinSet *from_pins,
                                   ClockSet *from_clks,
                                   InstanceSet *from_insts,
                                   const RiseFallBoth *from_rf) const;
  bool isExceptionStartpoint(const Pin *pin) const;
  // Make an exception -through specification.
  ExceptionThru *makeExceptionThru(PinSet *pins,
                                   NetSet *nets,
                                   InstanceSet *insts,
                                   const RiseFallBoth *rf) const;
  bool isExceptionEndpoint(const Pin *pin) const;
  // Make an exception -to specification.
  ExceptionTo *makeExceptionTo(PinSet *pins,
                               ClockSet *clks,
                               InstanceSet *insts,
                               const RiseFallBoth *rf,
                               const RiseFallBoth *end_rf) const;

  FilterPath *makeFilterPath(ExceptionFrom *from,
                             ExceptionThruSeq *thrus,
                             ExceptionTo *to);
  void makeFilter(ExceptionFrom *from,
                  ExceptionThruSeq *thrus);
  FilterPath *filter() const { return filter_; }
  void deleteFilter();

  Wireload *wireload(const MinMax *min_max) const;
  void setWireload(Wireload *wireload,
                   const MinMaxAll *min_max);
  WireloadMode wireloadMode() const { return wireload_mode_; };
  void setWireloadMode(WireloadMode mode);
  const WireloadSelection *wireloadSelection(const MinMax *min_max);
  void setWireloadSelection(const WireloadSelection *selection,
                            const MinMaxAll *min_max);

  // STA interface.
  InputDelaySet *refPinInputDelays(const Pin *ref_pin) const;
  const LogicValueMap &logicValues() const { return logic_value_map_; }
  const LogicValueMap &caseLogicValues() const { return case_value_map_; }
  // Returns nullptr if set_operating_conditions has not been called.
  OperatingConditions *operatingConditions(const MinMax *min_max) const;
  // Instance specific process/voltage/temperature.
  const Pvt *pvt(const Instance *inst,
                 const MinMax *min_max) const;
  // Pvt may be shared among multiple instances.
  void setPvt(const Instance *inst,
              const MinMaxAll *min_max,
              const Pvt &pvt);
  void voltage(const MinMax *min_max,
               // Return values.
               float &voltage,
               bool &exists) const;
  void voltage(const Net *net,
               const MinMax *min_max,
               // Return values.
               float &voltage,
               bool &exists) const;
  void setVoltage(const MinMax *min_max,
                  float voltage);
  void setVoltage(const Net *net,
                  const MinMax *min_max,
                  float voltage);
  InputDrive *findInputDrive(Port *port) const;
  Clock *findClock(const char *name) const;
  ClockSeq findClocksMatching(PatternMatch *pattern) const;
  // True if pin is defined as a clock source (pin may be hierarchical).
  bool isClock(const Pin *pin) const;
  // True if pin is a clock source vertex.
  bool isLeafPinClock(const Pin *pin) const;
  // True if pin is a non-generated clock source vertex.
  bool isLeafPinNonGeneratedClock(const Pin *pin) const;
  // Find the clocks defined for pin.
  ClockSet *findClocks(const Pin *pin) const;
  ClockSet *findLeafPinClocks(const Pin *pin) const;
  const ClockSeq &clocks() const { return clocks_; }
  ClockSeq sortedClocks() const;
  bool clkDisabledByHpinThru(const Clock *clk,
                             const Pin *from_pin,
                             const Pin *to_pin) const;
  void clkHpinDisablesInvalid();
  const ClockUncertainties *clockUncertainties(const Pin *pin) const;
  void clockUncertainty(const Pin *pin,
                        const SetupHold *setup_hold,
                        // Return values.
                        float &uncertainty,
                        bool &exists);
  // Inter-clock uncertainty.
  void clockUncertainty(const Clock *src_clk,
                        const RiseFall *src_rf,
                        const Clock *tgt_clk,
                        const RiseFall *tgt_rf,
                        const SetupHold *setup_hold,
                        // Return values.
                        float &uncertainty,
                        bool &exists) const;
  void clockGatingMarginEnablePin(const Pin *enable_pin,
                                  const RiseFall *enable_rf,
                                  const SetupHold *setup_hold,
                                  // Return values.
                                  bool &exists,
                                  float &margin) const;
  void clockGatingMarginInstance(Instance *inst,
                                 const RiseFall *enable_rf,
                                 const SetupHold *setup_hold,
                                 // Return values.
                                 bool &exists,
                                 float &margin) const;
  void clockGatingMarginClkPin(const Pin *clk_pin,
                               const RiseFall *enable_rf,
                               const SetupHold *setup_hold,
                               // Return values.
                               bool &exists,
                               float &margin) const;
  void clockGatingMarginClk(const Clock *clk,
                            const RiseFall *enable_rf,
                            const SetupHold *setup_hold,
                            // Return values.
                            bool &exists,
                            float &margin) const;
  void clockGatingMargin(const RiseFall *enable_rf,
                         const SetupHold *setup_hold,
                         // Return values.
                         bool &exists,
                         float &margin) const;
  // Gated clock active (non-controlling) logic value.
  LogicValue clockGatingActiveValue(const Pin *clk_pin,
                                    const Pin *enable_pin) const;
  // Find the cycle accounting info for paths that start at src clock
  // edge and end at target clock edge.
  CycleAccting *cycleAccting(const ClockEdge *src,
                             const ClockEdge *tgt);
  // Report clock to clock relationships that exceed max_cycle_count.
  void reportClkToClkMaxCycleWarnings();

  const InputDelaySet &inputDelays() const { return input_delays_; }
  // Pin -> input delays.
  const InputDelaysPinMap &inputDelayPinMap() const { return input_delay_pin_map_; }
  // Input delays on leaf_pin.
  InputDelaySet *inputDelaysLeafPin(const Pin *leaf_pin) const;
  bool hasInputDelay(const Pin *leaf_pin) const;
  // Pin is internal (not top level port) and has an input arrival.
  bool isInputDelayInternal(const Pin *pin) const;

  const OutputDelaySet &outputDelays() const { return output_delays_; }
  // Pin -> output delays.
  const OutputDelaysPinMap &outputDelaysPinMap() const { return output_delay_pin_map_; }
  // Output delays on leaf_pin.
  OutputDelaySet *outputDelaysLeafPin(const Pin *leaf_pin) const;
  [[nodiscard]] bool hasOutputDelay(const Pin *leaf_pin) const;

  const PortExtCap *portExtCap(const Port *port) const;
  bool hasPortExtCap(const Port *port) const;
  void portExtCap(const Port *port,
                  const RiseFall *rf,
                  const MinMax *min_max,
                  // Return values.
                  float &pin_cap,
                  bool &has_pin_cap,
                  float &wire_cap,
                  bool &has_wire_cap,
                  int &fanout,
                  bool &has_fanout) const;
  float portExtCap(const Port *port,
                   const RiseFall *rf,
                   const MinMax *min_max) const;
  // Connected total capacitance.
  //  pin_cap  = pin capacitance + port external pin
  //  wire_cap = port external wire capacitance + net wire capacitance
  void connectedCap(const Pin *pin,
                    const RiseFall *rf,
                    const Scene *scene,
                    const MinMax *min_max,
                    float &pin_cap,
                    float &wire_cap,
                    float &fanout,
                    bool &has_net_load) const;
  void pinCaps(const Pin *pin,
               const RiseFall *rf,
               const Scene *scene,
               const MinMax *min_max,
               float &pin_cap,
               float &wire_cap,
               float &fanout) const;
  void portExtFanout(const Port *port,
                     const MinMax *min_max,
                     // Return values.
                     int &fanout,
                     bool &exists) const;
  int portExtFanout(Port *port,
                    const MinMax *min_max) const;
  // Return true if search should proceed from pin/clk (no false paths
  // start at pin/clk).  When thru is true, consider -thru exceptions
  // that start at pin/net/instance also).  Transition tr applies to
  // pin, not clk.
  bool exceptionFromStates(const Pin *pin,
                           const RiseFall *rf,
                           const Clock *clk,
                           const RiseFall *clk_rf,
                           const MinMax *min_max,
                           ExceptionStateSet *&states);
  bool exceptionFromStates(const Pin *pin,
                           const RiseFall *rf,
                           const Clock *clk,
                           const RiseFall *clk_rf,
                           const MinMax *min_max,
                           bool include_filter,
                           ExceptionStateSet *&states);
  void exceptionFromClkStates(const Pin *pin,
                              const RiseFall *rf,
                              const Clock *clk,
                              const RiseFall *clk_rf,
                              const MinMax *min_max,
                              ExceptionStateSet *&states);
  void filterRegQStates(const Pin *to_pin,
                        const RiseFall *to_rf,
                        const MinMax *min_max,
                        ExceptionStateSet *&states) const;
  // Return hierarchical -thru exceptions that start between
  // from_pin and to_pin.
  void exceptionThruStates(const Pin *from_pin,
                           const Pin *to_pin,
                           const RiseFall *to_rf,
                           const MinMax *min_max,
                           ExceptionStateSet *&states);
  // Find the highest priority exception with first exception pt at
  // pin/clk end.
  void exceptionTo(ExceptionPathType type,
                   const Pin *pin,
                   const RiseFall *rf,
                   const ClockEdge *clk_edge,
                   const MinMax *min_max,
                   bool match_min_max_exactly,
                   // Return values.
                   ExceptionPath *&hi_priority_exception,
                   int &hi_priority);
  bool exceptionMatchesTo(ExceptionPath *exception,
                          const Pin *pin,
                          const RiseFall *rf,
                          const ClockEdge *clk_edge,
                          const MinMax *min_max,
                          bool match_min_max_exactly,
                          bool require_to_pin) const;
  void groupPathsTo(const Pin *pin,
                    const RiseFall *rf,
                    const ClockEdge *clk_edge,
                    const MinMax *min_max,
                    // Return value.
                    ExceptionPathSeq &group_paths);
  bool isCompleteTo(ExceptionState *state,
                    const Pin *pin,
                    const RiseFall *rf,
                    const ClockEdge *clk_edge,
                    const MinMax *min_max,
                    bool match_min_max_exactly,
                    bool require_to_pin) const;
  bool isCompleteTo(ExceptionState *state,
                    const Pin *pin,
                    const RiseFall *rf,
                    const MinMax *min_max) const;
  bool isPathDelayInternalFrom(const Pin *pin) const;
  bool isPathDelayInternalFromBreak(const Pin *pin) const;
  const PinSet &pathDelayInternalFrom() const;
  bool isPathDelayInternalTo(const Pin *pin) const;
  bool isPathDelayInternalToBreak(const Pin *pin) const;
  const ExceptionPathSet &exceptions() const { return exceptions_; }
  void deleteExceptions();
  void deleteException(ExceptionPath *exception);
  void recordException(ExceptionPath *exception);
  void unrecordException(ExceptionPath *exception);

  // Network edit before/after methods.
  void deletePinBefore(const Pin *pin);
  void connectPinAfter(const Pin *pin);
  void clkHpinDisablesChanged(const Pin *pin);
  void makeClkHpinDisable(const Clock *clk,
                          const Pin *drvr,
                          const Pin *load);
  void ensureClkHpinDisables();

  ////////////////////////////////////////////////////////////////
  //
  // Sdc/Mode dependewnt state
  //
  ////////////////////////////////////////////////////////////////

  // Vertices are constrained if they have one or more of the
  // following timing constraints:
  //   output delay constraints
  //   data check constraints
  //   path delay constraints
  bool isConstrainedEnd(const Pin *pin) const;

protected:
  void portMembers(const Port *port,
                   PortSeq &ports) const;
  void initVariables();
  void clearCycleAcctings();
  void deleteExceptionsReferencing(Clock *clk);
  void deleteClkPinMappings(Clock *clk);
  void makeClkPinMappings(Clock *clk);
  void deletePinClocks(Clock *defining_clk,
                       PinSet *pins);
  void makeDefaultArrivalClock();
  InputDrive *ensureInputDrive(const Port *port);
  ExceptionPath *findMergeMatch(ExceptionPath *exception);
  void addException1(ExceptionPath *exception);
  void addException2(ExceptionPath *exception);
  void recordExceptionPins(ExceptionPath *exception);
  void recordPathDelayInternalFrom(ExceptionPath *exception);
  void unrecordPathDelayInternalFrom(ExceptionPath *exception);
  bool pathDelayFrom(const Pin *pin);
  void recordPathDelayInternalTo(ExceptionPath *exception);
  void unrecordPathDelayInternalTo(ExceptionPath *exception);
  bool pathDelayTo(const Pin *pin);
  bool hasLibertyCheckTo(const Pin *pin);
  void deleteMatchingExceptions(ExceptionPath *exception);
  void findMatchingExceptions(ExceptionPath *exception,
                              ExceptionPathSet &matches);
  void checkForThruHpins(ExceptionPath *exception);
  void findMatchingExceptionsFirstFrom(ExceptionPath *exception,
                                       ExceptionPathSet &matches);
  void findMatchingExceptionsFirstThru(ExceptionPath *exception,
                                       ExceptionPathSet &matches);
  void findMatchingExceptionsFirstTo(ExceptionPath *exception,
                                     ExceptionPathSet &matches);
  void findMatchingExceptionsClks(ExceptionPath *exception,
                                  ClockSet *clks,
                                  ClockExceptionsMap &exception_map,
                                  ExceptionPathSet &matches);
  void findMatchingExceptionsPins(ExceptionPath *exception,
                                  PinSet *pins,
                                  PinExceptionsMap &exception_map,
                                  ExceptionPathSet &matches);
  void findMatchingExceptionsInsts(ExceptionPath *exception,
                                   InstanceSet *insts,
                                   InstanceExceptionsMap &exception_map,
                                   ExceptionPathSet &matches);
  void findMatchingExceptions(ExceptionPath *exception,
                              ExceptionPathSet *potential_matches,
                              ExceptionPathSet &matches);
  void expandExceptionExcluding(ExceptionPath *exception,
                                ExceptionPath *excluding,
                                ExceptionPathSet &expanded_matches);
  void recordException1(ExceptionPath *exception);
  void recordExceptionFirstPts(ExceptionPath *exception);
  void recordExceptionFirstFrom(ExceptionPath *exception);
  void recordExceptionFirstThru(ExceptionPath *exception);
  void recordExceptionFirstTo(ExceptionPath *exception);
  void recordExceptionClks(ExceptionPath *exception,
                           ClockSet *clks,
                           ClockExceptionsMap &exception_map);
  void recordExceptionInsts(ExceptionPath *exception,
                            InstanceSet *insts,
                            InstanceExceptionsMap &exception_map);
  void recordExceptionPins(ExceptionPath *exception,
                           PinSet *pins,
                           PinExceptionsMap &exception_map);
  void recordExceptionNets(ExceptionPath *exception,
                           NetSet *nets,
                           NetExceptionsMap &exception_map);
  void recordExceptionHpin(ExceptionPath *exception,
                           Pin *pin,
                           PinExceptionsMap &exception_map);
  void recordExceptionEdges(ExceptionPath *exception,
                            EdgePinsSet *edges,
                            EdgeExceptionsMap &exception_map);
  void recordMergeHash(ExceptionPath *exception, ExceptionPt *missing_pt);
  void recordMergeHashes(ExceptionPath *exception);
  void unrecordExceptionFirstPts(ExceptionPath *exception);
  void unrecordExceptionPins(ExceptionPath *exception);
  void unrecordExceptionClks(ExceptionPath *exception,
                             ClockSet *clks,
                             ClockExceptionsMap &exception_map);
  void unrecordExceptionPins(ExceptionPath *exception,
                             PinSet *pins,
                             PinExceptionsMap &exception_map);
  void unrecordExceptionInsts(ExceptionPath *exception,
                              InstanceSet *insts,
                              InstanceExceptionsMap &exception_map);
  void unrecordExceptionEdges(ExceptionPath *exception,
                              EdgePinsSet *edges,
                              EdgeExceptionsMap &exception_map);
  void unrecordExceptionNets(ExceptionPath *exception,
                             NetSet *nets,
                             NetExceptionsMap &exception_map);
  void unrecordExceptionHpin(ExceptionPath *exception,
                             Pin *pin,
                             PinExceptionsMap &exception_map);
  void unrecordMergeHashes(ExceptionPath *exception);
  void unrecordMergeHash(ExceptionPath *exception,
                         ExceptionPt *missing_pt);
  void mergeException(ExceptionPath *exception);
  void expandException(ExceptionPath *exception,
                       ExceptionPathSet &expansions);
  bool exceptionFromStates(const ExceptionPathSet *exceptions,
                           const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max,
                           bool include_filter,
                           ExceptionStateSet *&states) const;
  void exceptionThruStates(const ExceptionPathSet *exceptions,
                           const RiseFall *to_rf,
                           const MinMax *min_max,
                           // Return value.
                           ExceptionStateSet *&states) const;
  void exceptionTo(const ExceptionPathSet *to_exceptions,
                   ExceptionPathType type,
                   const Pin *pin,
                   const RiseFall *rf,
                   const ClockEdge *clk_edge,
                   const MinMax *min_max,
                   bool match_min_max_exactly,
                   // Return values.
                   ExceptionPath *&hi_priority_exception,
                   int &hi_priority) const;
  void exceptionTo(ExceptionPath *exception,
                   ExceptionPathType type,
                   const Pin *pin,
                   const RiseFall *rf,
                   const ClockEdge *clk_edge,
                   const MinMax *min_max,
                   bool match_min_max_exactly,
                   // Return values.
                   ExceptionPath *&hi_priority_exception,
                   int &hi_priority) const;
  void groupPathsTo(const ExceptionPathSet *to_exceptions,
                    const Pin *pin,
                    const RiseFall *rf,
                    const ClockEdge *clk_edge,
                    const MinMax *min_max,
                    // Return value.
                    ExceptionPathSeq &group_paths) const;
  void makeLoopPath(ExceptionThruSeq *thrus);
  void makeLoopException(const Pin *loop_input_pin,
                         const Pin *loop_pin,
                         const Pin *loop_prev_pin);
  void makeLoopExceptionThru(const Pin *pin,
                             ExceptionThruSeq *thrus);
  void deleteConstraints();
  InputDelay *findInputDelay(const Pin *pin,
                             const ClockEdge *clk_edge);
  InputDelay *makeInputDelay(const Pin *pin,
                             const ClockEdge *clk_edge);
  void deleteInputDelays(const Pin *pin,
                         InputDelay *except);
  void deleteInputDelaysReferencing(const Clock *clk);
  void deleteInputDelay(InputDelay *input_delay);
  OutputDelay *findOutputDelay(const Pin *pin,
                               const ClockEdge *clk_edge);
  OutputDelay *makeOutputDelay(const Pin *pin,
                               const ClockEdge *clk_edge);
  void deleteOutputDelays(const Pin *pin,
                          OutputDelay *except);
  void deleteOutputDelaysReferencing(const Clock *clk);
  void deleteOutputDelay(OutputDelay *output_delay);
  void deleteClockInsertion(ClockInsertion *insertion);
  void deleteClockInsertionsReferencing(Clock *clk);
  void deleteInterClockUncertainty(InterClockUncertainty *uncertainties);
  void deleteInterClockUncertaintiesReferencing(Clock *clk);
  void deleteLatchBorrowLimitsReferencing(Clock *clk);
  void deleteMinPulseWidthReferencing(Clock *clk);
  void deleteMasterClkRefs(Clock *clk);
  // Liberty library to look for defaults.
  LibertyLibrary *defaultLibertyLibrary();
  void annotateGraphConstrainOutputs();
  void deleteClockLatenciesReferencing(Clock *clk);
  void deleteClockLatency(ClockLatency *latency);
  void deleteDeratingFactors();

  void annotateHierClkLatency();
  void annotateHierClkLatency(const Pin *hpin,
                              ClockLatency *latency);
  void netCaps(const Pin *drvr_pin,
               const RiseFall *rf,
               const Scene *scene,
               const MinMax *min_max,
               // Return values.
               float &pin_cap,
               float &wire_cap,
               float &fanout,
               bool &has_net_load) const;
  // connectedCap pin_cap.
  float connectedPinCap(const Pin *pin,
                        const RiseFall *rf,
                        const Scene *scene,
                        const MinMax *min_max);
  float portCapacitance(Instance *inst,
                        LibertyPort *port,
                        const RiseFall *rf,
                        const Scene *scene,
                        const MinMax *min_max) const;
  void removeClockGroups(ClockGroups *groups);
  void ensureClkGroupExclusions();
  void makeClkGroupExclusions(ClockGroups *clk_groups);
  void makeClkGroupExclusions1(ClockGroupSet *groups);
  void makeClkGroupExclusions(ClockGroupSet *groups);
  void makeClkGroupSame(ClockGroup *group);
  void clearClkGroupExclusions();
  char *makeClockGroupsName();
  void setClockSense(const Pin *pin,
                     const Clock *clk,
                     ClockSense sense);
  bool clkStopSense(const Pin *to_pin,
                    const Clock *clk,
                    const RiseFall *from_rf,
                    const RiseFall *to_rf) const;
  void disconnectPinBefore(const Pin *pin,
                           ExceptionPathSet *exceptions);
  void clockGroupsDeleteClkRefs(Clock *clk);
  void clearGroupPathMap();

  Mode *mode_;
  AnalysisType analysis_type_;
  OperatingConditions *operating_conditions_[MinMax::index_count];
  InstancePvtMap instance_pvt_maps_[MinMax::index_count];
  MinMaxFloatValues voltages_;
  NetVoltageMap net_voltage_map_;
  DeratingFactorsGlobal *derating_factors_;
  NetDeratingFactorsMap net_derating_factors_;
  InstDeratingFactorsMap inst_derating_factors_;
  CellDeratingFactorsMap cell_derating_factors_;
  // Clock sequence retains clock definition order.
  // This is important for getting consistent regression results,
  // which iterating over the name map can't provide.
  ClockSeq clocks_;
  // Clocks are assigned an index.
  int clk_index_;
  // Default clock used for unclocked input arrivals.
  Clock *default_arrival_clk_;
  ClockNameMap clock_name_map_;
  ClockPinMap clock_pin_map_;
  // Clocks on hierarchical pins are indexed by the load pins.
  ClockPinMap clock_leaf_pin_map_;
  ClkHpinDisables clk_hpin_disables_;
  bool clk_hpin_disables_valid_;
  PinSet propagated_clk_pins_;
  ClockLatencies clk_latencies_;
  EdgeClockLatencyMap edge_clk_latency_map_;
  ClockInsertions clk_insertions_;
  PinClockUncertaintyMap pin_clk_uncertainty_map_;
  InterClockUncertaintySet inter_clk_uncertainties_;
  // clk_groups name -> clk_groups
  ClockGroupsNameMap clk_groups_name_map_;
  // clk to clk paths excluded by clock groups.
  ClockPairSet clk_group_exclusions_;
  // clks in the same set_clock_group set.
  ClockPairSet clk_group_same_;
  ClockSenseMap clk_sense_map_;
  ClockGatingCheck *clk_gating_check_;
  ClockGatingCheckMap clk_gating_check_map_;
  InstanceClockGatingCheckMap inst_clk_gating_check_map_;
  PinClockGatingCheckMap pin_clk_gating_check_map_;
  CycleAcctings cycle_acctings_;
  std::mutex cycle_acctings_lock_;
  DataChecksMap data_checks_from_map_;
  DataChecksMap data_checks_to_map_;

  InputDelaySet input_delays_;
  InputDelaysPinMap input_delay_pin_map_;
  InputDelaysPinMap input_delay_ref_pin_map_;
  // Input delays on hierarchical pins are indexed by the load pins.
  InputDelaysPinMap input_delay_leaf_pin_map_;
  InputDelaysPinMap input_delay_internal_pin_map_;
  int input_delay_index_;

  OutputDelaySet output_delays_;
  OutputDelaysPinMap output_delay_pin_map_;
  OutputDelaysPinMap output_delay_ref_pin_map_;
  // Output delays on hierarchical pins are indexed by the load pins.
  OutputDelaysPinMap output_delay_leaf_pin_map_;

  PortSlewLimitMap port_slew_limit_map_;
  CellSlewLimitMap cell_slew_limit_map_;
  bool have_clk_slew_limits_;
  CellCapLimitMap cell_cap_limit_map_;
  PortCapLimitMap port_cap_limit_map_;
  PinCapLimitMap pin_cap_limit_map_;
  PortFanoutLimitMap port_fanout_limit_map_;
  CellFanoutLimitMap cell_fanout_limit_map_;
  // External parasitics on top level ports.
  //  set_load port
  //  set_fanout_load port
  PortExtCapMap port_ext_cap_map_;
  // set_load net
  NetWireCapMap net_wire_cap_map_;
  PinWireCapMap drvr_pin_wire_cap_map_;
  NetResistanceMap net_res_map_;
  PinSet disabled_pins_;
  PortSet disabled_ports_;
  LibertyPortSet disabled_lib_ports_;
  PinPairSet disabled_wire_edges_;
  EdgeSet disabled_edges_;
  DisabledCellPortsMap disabled_cell_ports_;
  DisabledInstancePortsMap disabled_inst_ports_;
  InstanceSet disabled_clk_gating_checks_inst_;
  PinSet disabled_clk_gating_checks_pin_;
  ExceptionPathSet exceptions_;
  size_t exception_id_; // Unique ID for exceptions.

  bool have_thru_hpin_exceptions_;
  // First pin/clock/instance/net/edge exception point to exception set map.
  PinExceptionsMap first_from_pin_exceptions_;
  ClockExceptionsMap first_from_clk_exceptions_;
  InstanceExceptionsMap first_from_inst_exceptions_;
  PinExceptionsMap first_thru_pin_exceptions_;
  InstanceExceptionsMap first_thru_inst_exceptions_;
  // Nets that have exception -thru nets.
  NetExceptionsMap first_thru_net_exceptions_;
  PinExceptionsMap first_to_pin_exceptions_;
  ClockExceptionsMap first_to_clk_exceptions_;
  InstanceExceptionsMap first_to_inst_exceptions_;
  PinExceptionsMap pin_exceptions_;
  // Edges that traverse hierarchical exception pins.
  EdgeExceptionsMap first_thru_edge_exceptions_;
  // Exception hash with one missing from/thru/to point, used for merging.
  ExceptionPathPtHash exception_merge_hash_;
  // Path delay -from pin internal startpoints.
  PinSet path_delay_internal_from_;
  // Path delay -from pin internal -from w/o -probe.
  PinSet path_delay_internal_from_break_;
  // Path delay -to pin internal -to.
  PinSet path_delay_internal_to_;
  // Path delay -to pin internal -to w/o -probe.
  PinSet path_delay_internal_to_break_;
  // There is a path delay exception without a -to.
  bool path_delays_without_to_;
  // Group path exception names.
  GroupPathMap group_path_map_;

  // Filter exception to tag arrivals for
  // report_timing -from pin|inst -through.
  // -to is always nullptr.
  FilterPath *filter_;

  InputDriveMap input_drive_map_;
  // set_LogicValue::one/zero/dc
  LogicValueMap logic_value_map_;
  // set_case_analysis
  LogicValueMap case_value_map_;
  PinLatchBorrowLimitMap pin_latch_borrow_limit_map_;
  InstLatchBorrowLimitMap inst_latch_borrow_limit_map_;
  ClockLatchBorrowLimitMap clk_latch_borrow_limit_map_;
  RiseFallValues min_pulse_width_;
  PinMinPulseWidthMap pin_min_pulse_width_map_;
  InstMinPulseWidthMap inst_min_pulse_width_map_;
  ClockMinPulseWidthMap clk_min_pulse_width_map_;
  float max_area_;
  Wireload *wireload_[MinMax::index_count];
  WireloadMode wireload_mode_;
  const WireloadSelection *wireload_selection_[MinMax::index_count];

private:
  friend class WriteSdc;
  friend class FindNetCaps;
  friend class GroupPathIterator;
};

} // namespace
