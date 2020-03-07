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

#include <string>
#include "DisallowCopyAssign.hh"
#include "StringSeq.hh"
#include "StaState.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "ParasiticsClass.hh"
#include "VertexVisitor.hh"

struct Tcl_Interp;

namespace sta {

using std::string;
using ::Tcl_Interp;

// Don't include headers to minimize dependencies.
class MinMax;
class MinMaxAll;
class RiseFallBoth;
class RiseFall;
class ReportPath;
class CheckTiming;
class DcalcAnalysisPt;
class CheckSlewLimits;
class CheckMinPulseWidths;
class CheckMinPeriods;
class CheckMaxSkews;
class PatternMatch;
class CheckPeriods;
class LibertyReader;
class SearchPred;
class Corner;
class ClkSkews;
class ReportField;
class Power;
class PowerResult;
class ClockIterator;
class EquivCells;

typedef InstanceSeq::Iterator SlowDrvrIterator;
typedef Vector<const char*> CheckError;
typedef Vector<CheckError*> CheckErrorSeq;

enum class CmdNamespace { sta, sdc };

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

  virtual LibertyLibrary *readLiberty(const char *filename,
				      Corner *corner,
				      const MinMaxAll *min_max,
				      bool infer_latches);
  bool setMinLibrary(const char *min_filename,
		     const char *max_filename);
  // Network readers call this to notify the Sta to delete any previously
  // linked network.
  void readNetlistBefore();
  // Return true if successful.
  bool linkDesign(const char *top_cell_name);
  bool linkMakeBlackBoxes() const;
  void setLinkMakeBlackBoxes(bool make);

  // SDC Swig API.
  Instance *currentInstance() const;
  void setCurrentInstance(Instance *inst);
  virtual void setAnalysisType(AnalysisType analysis_type);
  void setOperatingConditions(OperatingConditions *op_cond,
			      const MinMaxAll *min_max);
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
		       TimingDerateType type,
		       PathClkOrData clk_data,
		       const RiseFallBoth *rf,
		       const EarlyLate *early_late,
		       float derate);
  void setTimingDerate(const LibertyCell *cell,
		       TimingDerateType type,
		       PathClkOrData clk_data,
		       const RiseFallBoth *rf,
		       const EarlyLate *early_late,
		       float derate);
  void unsetTimingDerate();
  void setInputSlew(Port *port,
		    const RiseFallBoth *rf,
		    const MinMaxAll *min_max,
		    float slew);
  // Port external pin load.
  float portExtPinCap(Port *port,
		      const RiseFall *rf,
		      const MinMax *min_max);
  // Set port external pin load (set_load -pin port).
  void setPortExtPinCap(Port *port,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			float cap);
  // Port external wire load.
  float portExtWireCap(Port *port,
		       const RiseFall *rf,
		       const MinMax *min_max);
  // Set port external wire load (set_load -wire port).
  void setPortExtWireCap(Port *port,
			 bool subtract_pin_cap,
			 const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 float cap);
  // Set net wire capacitance (set_load -wire net).
  void setNetWireCap(Net *net,
		     bool subtract_pin_load,
		     const Corner *corner,
		     const MinMaxAll *min_max,
		     float cap);
  // Port external fanout (used by wireload models).
  int portExtFanout(Port *port,
		    const MinMax *min_max);
  // Set port external fanout (used by wireload models).
  void setPortExtFanout(Port *port,
			int fanout,
			const MinMaxAll *min_max);
  // Remove all "set_load net" annotations.
  void removeNetLoadCaps() const;
  // pin_cap  = net pin capacitances + port external pin capacitance,
  // wire_cap = annotated net capacitance + port external wire capacitance.
  void connectedCap(Pin *drvr_pin,
		    const RiseFall *rf,
		    const Corner *corner,
		    const MinMax *min_max,
		    float &pin_cap,
		    float &wire_cap) const;
  void connectedCap(Net *net,
		    const RiseFall *rf,
		    const Corner *corner,
		    const MinMax *min_max,
		    float &pin_cap,
		    float &wire_cap) const;
  void setResistance(Net *net,
		     const MinMaxAll *min_max,
		     float res);
  void setDriveCell(LibertyLibrary *library,
		    LibertyCell *cell,
		    Port *port,
		    LibertyPort *from_port,
		    float *from_slews,
		    LibertyPort *to_port,
		    const RiseFallBoth *rf,
		    const MinMaxAll *min_max);
  void setDriveResistance(Port *port,
			  const RiseFallBoth *rf,
			  const MinMaxAll *min_max,
			  float res);
  void setLatchBorrowLimit(Pin *pin,
			   float limit);
  void setLatchBorrowLimit(Instance *inst,
			   float limit);
  void setLatchBorrowLimit(Clock *clk,
			   float limit);
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
  void setWireload(Wireload *wireload,
		   const MinMaxAll *min_max);
  void setWireloadMode(WireloadMode mode);
  void setWireloadSelection(WireloadSelection *selection,
			    const MinMaxAll *min_max);
  void setSlewLimit(Clock *clk,
		    const RiseFallBoth *rf,
		    const PathClkOrData clk_data,
		    const MinMax *min_max,
		    float slew);
  void setSlewLimit(Port *port,
		    const MinMax *min_max,
		    float slew);
  void setSlewLimit(Pin *pin,
		    const MinMax *min_max,
		    float slew);
  void setSlewLimit(Cell *cell,
		    const MinMax *min_max,
		    float slew);
  void setCapacitanceLimit(Cell *cell,
			   const MinMax *min_max,
			   float cap);
  void setCapacitanceLimit(Port *port,
			   const MinMax *min_max,
			   float cap);
  void setCapacitanceLimit(Pin *pin,
			   const MinMax *min_max,
			   float cap);
  void setFanoutLimit(Cell *cell,
		      const MinMax *min_max,
		      float fanout);
  void setFanoutLimit(Port *port,
		      const MinMax *min_max,
		      float fanout);
  void setMaxArea(float area);

  void makeClock(const char *name,
		 PinSet *pins,
		 bool add_to_pins,
		 float period,
		 FloatSeq *waveform,
		 char *comment);
  // edges size must be 3.
  void makeGeneratedClock(const char *name,
			  PinSet *pins,
			  bool add_to_pins,
			  Pin *src_pin,
			  Clock *master_clk,
			  Pin *pll_out,
			  Pin *pll_fdbk,
			  int divide_by,
			  int multiply_by,
			  float duty_cycle,
			  bool invert,
			  bool combinational,
			  IntSeq *edges,
			  FloatSeq *edge_shifts,
			  char *comment);
  void removeClock(Clock *clk);
  // Update period/waveform for generated clocks from source pin clock.
  void updateGeneratedClks();
  // Use Sdc::findClock
  Clock *findClock(const char *name) const __attribute__ ((deprecated));
  // Use findClocksMatching.
  void findClocksMatching(PatternMatch *pattern,
			  ClockSeq *clks) const __attribute__ ((deprecated));
  // Use Sdc::clockIterator.
  ClockIterator *clockIterator() const __attribute__ ((deprecated));
  // True if pin is defined as a clock source (pin may be hierarchical).
  bool isClockSrc(const Pin *pin) const;
  // Use Sdc::defaultArrivalClock.
  Clock *defaultArrivalClock() const __attribute__ ((deprecated));
  // Propagated (non-ideal) clocks.
  void setPropagatedClock(Clock *clk);
  void removePropagatedClock(Clock *clk);
  void setPropagatedClock(Pin *pin);
  void removePropagatedClock(Pin *pin);
  void setClockSlew(Clock *clock,
		    const RiseFallBoth *rf,
		    const MinMaxAll *min_max,
		    float slew);
  void removeClockSlew(Clock *clk);
  // Clock latency.
  // Latency can be on a clk, pin, or clk/pin combination.
  void setClockLatency(Clock *clk,
		       Pin *pin,
		       const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
		       float delay);
  void removeClockLatency(const Clock *clk,
			  const Pin *pin);
  // Clock insertion delay (source latency).
  void setClockInsertion(const Clock *clk,
			 const Pin *pin,
			 const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 const EarlyLateAll *early_late,
			 float delay);
  void removeClockInsertion(const Clock *clk,
			    const Pin *pin);
  // Clock uncertainty.
  virtual void setClockUncertainty(Clock *clk,
				   const SetupHoldAll *setup_hold,
				   float uncertainty);
  virtual void removeClockUncertainty(Clock *clk,
				      const SetupHoldAll *setup_hold);
  virtual void setClockUncertainty(Pin *pin,
				   const SetupHoldAll *setup_hold,
				   float uncertainty);
  virtual void removeClockUncertainty(Pin *pin,
				      const SetupHoldAll *setup_hold);
  // Inter-clock uncertainty.
  virtual void setClockUncertainty(Clock *from_clk,
				   const RiseFallBoth *from_rf,
				   Clock *to_clk,
				   const RiseFallBoth *to_rf,
				   const SetupHoldAll *setup_hold,
				   float uncertainty);
  virtual void removeClockUncertainty(Clock *from_clk,
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
  // nullptr name removes all.
  void removeClockGroupsLogicallyExclusive(const char *name);
  void removeClockGroupsPhysicallyExclusive(const char *name);
  void removeClockGroupsAsynchronous(const char *name);
  void makeClockGroup(ClockGroups *clk_groups,
		      ClockSet *clks);
  void setClockSense(PinSet *pins,
		     ClockSet *clks,
		     ClockSense sense);
  void setClockGatingCheck(const RiseFallBoth *rf,
			   const SetupHold *setup_hold,
			   float margin);
  void setClockGatingCheck(Clock *clk,
			   const RiseFallBoth *rf,
			   const SetupHold *setup_hold,
			   float margin);
  void setClockGatingCheck(Instance *inst,
			   const RiseFallBoth *rf,
			   const SetupHold *setup_hold,
			   float margin,
			   LogicValue active_value);
  void setClockGatingCheck(Pin *pin,
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
  // Hierarchical instances are NOT supported.
  void disable(Instance *inst,
	       LibertyPort *from,
	       LibertyPort *to);
  void removeDisable(Instance *inst,
		     LibertyPort *from,
		     LibertyPort *to);
  // set_disable_timing pin
  void disable(Pin *pin);
  void removeDisable(Pin *pin);
  // set_disable_timing [get_timing_arc -of_objects instance]]
  void disable(Edge *edge);
  void removeDisable(Edge *edge);
  // set_disable_timing [get_timing_arc -of_objects lib_cell]]
  void disable(TimingArcSet *arc_set);
  void removeDisable(TimingArcSet *arc_set);
  // Edge is disabled by constant.
  bool isDisabledConstant(Edge *edge);
  // Edge is default cond disabled by timing_disable_cond_default_arcs var.
  bool isDisabledCondDefault(Edge *edge);
  // Edge is disabled to prpath a clock from propagating.
  bool isDisabledClock(Edge *edge);
  // Return a set of constant pins that disabled edge.
  // Caller owns the returned set.
  PinSet *disabledConstantPins(Edge *edge);
  // Edge timing sense with propagated constants.
  TimingSense simTimingSense(Edge *edge);
  // Edge is disabled by set_disable_timing constraint.
  bool isDisabledConstraint(Edge *edge);
  // Edge is disabled to break combinational loops.
  bool isDisabledLoop(Edge *edge) const;
  // Edge is disabled internal bidirect output path.
  bool isDisabledBidirectInstPath(Edge *edge) const;
  // Edge is disabled bidirect net path.
  bool isDisabledBidirectNetPath(Edge *edge) const;
  bool isDisabledPresetClr(Edge *edge) const;
  // Return a vector of graph edges that are disabled, sorted by
  // from/to vertex names.  Caller owns the returned vector.
  EdgeSeq *disabledEdges();
  EdgeSeq *disabledEdgesSorted();
  void disableClockGatingCheck(Instance *inst);
  void disableClockGatingCheck(Pin *pin);
  void removeDisableClockGatingCheck(Instance *inst);
  void removeDisableClockGatingCheck(Pin *pin);
  void setLogicValue(Pin *pin,
		     LogicValue value);
  void setCaseAnalysis(Pin *pin,
		       LogicValue value);
  void removeCaseAnalysis(Pin *pin);
  void setInputDelay(Pin *pin,
		     const RiseFallBoth *rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     Pin *ref_pin,
		     bool source_latency_included,
		     bool network_latency_included,
		     const MinMaxAll *min_max,
		     bool add,
		     float delay);
  void removeInputDelay(Pin *pin,
			RiseFallBoth *rf, 
			Clock *clk,
			RiseFall *clk_rf, 
			MinMaxAll *min_max);
  void setOutputDelay(Pin *pin,
		      const RiseFallBoth *rf,
		      Clock *clk,
		      const RiseFall *clk_rf,
		      Pin *ref_pin,
		      bool source_latency_included,
		      bool network_latency_included,
		      const MinMaxAll *min_max,
		      bool add,
		      float delay);
  void removeOutputDelay(Pin *pin,
			 RiseFallBoth *rf, 
			 Clock *clk,
			 RiseFall *clk_rf, 
			 MinMaxAll *min_max);
  void makeFalsePath(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMaxAll *min_max,
		     const char *comment);
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
		     float delay,
		     const char *comment);
  void makeGroupPath(const char *name,
		     bool is_default,
		     ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const char *comment);
  bool isGroupPathName(const char *group_name);
  void resetPath(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to,
		 const MinMaxAll *min_max);
  // Make an exception -from specification.
  ExceptionFrom *makeExceptionFrom(PinSet *from_pins,
				   ClockSet *from_clks,
				   InstanceSet *from_insts,
				   const RiseFallBoth *from_rf);
  void checkExceptionFromPins(ExceptionFrom *from,
			      const char *file,
			      int line) const;
  bool exceptionFromInvalid(const Pin *pin) const;
  void deleteExceptionFrom(ExceptionFrom *from);
  // Make an exception -through specification.
  ExceptionThru *makeExceptionThru(PinSet *pins,
				   NetSet *nets,
				   InstanceSet *insts,
				   const RiseFallBoth *rf);
  void deleteExceptionThru(ExceptionThru *thru);
  // Make an exception -to specification.
  ExceptionTo *makeExceptionTo(PinSet *to_pins,
			       ClockSet *to_clks,
			       InstanceSet *to_insts,
			       const RiseFallBoth *rf,
 			       RiseFallBoth *end_rf);
  void checkExceptionToPins(ExceptionTo *to,
			    const char *file, int) const;
  void deleteExceptionTo(ExceptionTo *to);
  InstanceSet *findRegisterInstances(ClockSet *clks,
				     const RiseFallBoth *clk_rf,
				     bool edge_triggered,
				     bool latches);
  PinSet *findRegisterDataPins(ClockSet *clks,
			       const RiseFallBoth *clk_rf,
			       bool registers,
			       bool latches);
  PinSet *findRegisterClkPins(ClockSet *clks,
			      const RiseFallBoth *clk_rf,
			      bool registers,
			      bool latches);
  PinSet *findRegisterAsyncPins(ClockSet *clks,
				const RiseFallBoth *clk_rf,
				bool registers,
				bool latches);
  PinSet *findRegisterOutputPins(ClockSet *clks,
				 const RiseFallBoth *clk_rf,
				 bool registers,
				 bool latches);
  PinSet *
  findFaninPins(PinSeq *to,
		bool flat,
		bool startpoints_only,
		int inst_levels,
		int pin_levels,
		bool thru_disabled,
		bool thru_constants);
  InstanceSet *
  findFaninInstances(PinSeq *to,
		     bool flat,
		     bool startpoints_only,
		     int inst_levels,
		     int pin_levels,
		     bool thru_disabled,
		     bool thru_constants);
  PinSet *
  findFanoutPins(PinSeq *from,
		 bool flat,
		 bool endpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants);
  InstanceSet *
  findFanoutInstances(PinSeq *from,
		      bool flat,
		      bool endpoints_only,
		      int inst_levels,
		      int pin_levels,
		      bool thru_disabled,
		      bool thru_constants);

  // The set of clocks that reach pin.
  void clocks(const Pin *pin,
	      // Return value.
	      ClockSet &clks);

  // Return the pin with the min/max slew limit slack.
  // corner=nullptr checks all corners.
  Pin *pinMinSlewLimitSlack(const Corner *corner,
			    const MinMax *min_max);
  // Return all pins with min/max slew violations.
  // corner=nullptr checks all corners.
  PinSeq *pinSlewLimitViolations(const Corner *corner,
				 const MinMax *min_max);
  void reportSlewLimitShortHeader();
  void reportSlewLimitShort(Pin *pin,
			    const Corner *corner,
			    const MinMax *min_max);
  void reportSlewLimitVerbose(Pin *pin,
			      const Corner *corner,
			      const MinMax *min_max);
  void checkSlews(const Pin *pin,
		  const Corner *corner,
		  const MinMax *min_max,
		  // Return values.
		  const Corner *&corner1,
		  const RiseFall *&tr,
		  Slew &slew,
		  float &limit,
		  float &slack);
  // Min pulse width check with the least slack.
  // corner=nullptr checks all corners.
  MinPulseWidthCheck *minPulseWidthSlack(const Corner *corner);
  // All violating min pulse width checks.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &minPulseWidthViolations(const Corner *corner);
  // Min pulse width checks for pins.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &minPulseWidthChecks(PinSeq *pins,
					     const Corner *corner);
  // All min pulse width checks.
  // corner=nullptr checks all corners.
  MinPulseWidthCheckSeq &minPulseWidthChecks(const Corner *corner);
  void reportMpwChecks(MinPulseWidthCheckSeq *checks,
		       bool verbose);
  void reportMpwCheck(MinPulseWidthCheck *check,
		      bool verbose);

  // Min period check with the least slack.
  MinPeriodCheck *minPeriodSlack();
  // All violating min period checks.
  MinPeriodCheckSeq &minPeriodViolations();
  void reportChecks(MinPeriodCheckSeq *checks,
		    bool verbose);
  void reportCheck(MinPeriodCheck *check,
		   bool verbose);

  // Max skew check with the least slack.
  MaxSkewCheck *maxSkewSlack();
  // All violating min period checks.
  MaxSkewCheckSeq &maxSkewViolations();
  void reportChecks(MaxSkewCheckSeq *checks,
		    bool verbose);
  void reportCheck(MaxSkewCheck *check,
		   bool verbose);

  
  ////////////////////////////////////////////////////////////////
  // User visible but non SDC commands.

  // Instance specific process/voltage/temperature.
  // Defaults to operating condition if instance is not annotated.
  Pvt *pvt(Instance *inst,
	   const MinMax *min_max);
  void setPvt(Instance *inst,
	      const MinMaxAll *min_max,
	      float process,
	      float voltage,
	      float temperature);
  // Pvt may be shared among multiple instances.
  void setPvt(Instance *inst,
	      const MinMaxAll *min_max,
	      Pvt *pvt);
  // Clear all state except network.
  virtual void clear();
  // Remove all constraints.
  virtual void removeConstraints();
  // Notify the sta that the constraints have changed directly rather
  // than thru this sta API.
  virtual void constraintsChanged();
  // Namespace used by command interpreter.
  CmdNamespace cmdNamespace();
  void setCmdNamespace(CmdNamespace namespc);
  OperatingConditions *operatingConditions(const MinMax *min_max) const;
  // Set the delay on a timing arc.
  // Required/arrival times are incrementally updated.
  void setArcDelay(Edge *edge,
		   TimingArc *arc,
		   const Corner *corner,
		   const MinMaxAll *min_max,
		   ArcDelay delay);
  // Set annotated slew on a vertex for delay calculation.
  void setAnnotatedSlew(Vertex *vertex,
			const Corner *corner,
			const MinMaxAll *min_max,
			const RiseFallBoth *rf,
			float slew);
  void writeSdf(const char *filename,
		Corner *corner,
		char sdf_divider,
		int digits,
		bool gzip,
		bool no_timestamp,
		bool no_version);
  // Remove all delay and slew annotations.
  void removeDelaySlewAnnotations();
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
  // TCL variable sta_bidirect_net_paths_enabled.
  // Enable/disable timing from bidirect driver pins to their own loads.
  bool bidirectNetPathsEnabled() const;
  void setBidirectNetPathsEnabled(bool enabled);
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
  virtual CheckErrorSeq &checkTiming(bool no_input_delay,
				     bool no_output_delay,
				     bool reg_multiple_clks,
				     bool reg_no_clks,
				     bool unconstrained_endpoints,
				     bool loops,
				     bool generated_clks);
  // Path from/thrus/to filter.
  // from/thrus/to are owned and deleted by Search.
  // Returned sequence is owned by the caller.
  // PathEnds are owned by Search PathGroups and deleted on next call.
  virtual PathEndSeq *findPathEnds(ExceptionFrom *from,
				   ExceptionThruSeq *thrus,
				   ExceptionTo *to,
				   bool unconstrained,
				   // Use corner nullptr to report timing
				   // for all corners.
				   const Corner *corner,
				   // max for setup checks.
				   // min for hold checks.
				   // min_max for setup and hold checks.
				   const MinMaxAll *min_max,
				   // Number of path ends to report in
				   // each group.
				   int group_count,
				   // Number of paths to report for
				   // each endpoint.
				   int endpoint_count,
				   // endpoint_count paths report unique pins
				   // without rise/fall variations.
				   bool unique_pins,
				   // Min/max bounds for slack of
				   // returned path ends.
				   float slack_min,
				   float slack_max,
				   // Sort path ends by slack ignoring path groups.
				   bool sort_by_slack,
				   // Path groups to report.
				   // Null or empty list reports all groups.
				   PathGroupNameSet *group_names,
				   // Predicates to filter the type of path
				   // ends returned.
				   bool setup,
				   bool hold,
				   bool recovery,
				   bool removal,
				   bool clk_gating_setup,
				   bool clk_gating_hold);
  PathEndSeq *reportTiming(ExceptionFrom *from,
			   ExceptionThruSeq *thrus,
			   ExceptionTo *to,
			   // Use corner nullptr to report timing
			   // for all corners.
			   const Corner *corner,
			   // max for setup checks.
			   // min for hold checks.
			   // min_max for setup and hold checks.
			   const MinMaxAll *min_max,
			   // Number of path ends to report in
			   // each group.
			   int group_count,
			   // Number of paths to report for
			   // each endpoint.
			   int endpoint_count,
			   // endpoint_count paths report unique pins
			   // without rise/fall variations.
			   bool unique_pins,
			   // Min/max bounds for slack of
			   // returned path ends.
			   float slack_min,
			   float slack_max,
			   // Sort path ends by slack ignoring path groups.
			   bool sort_by_slack,
			   // Path groups to report.
			   // Null or empty list reports all groups.
			   PathGroupNameSet *group_names,
			   // Predicates to filter the type of path
			   // ends returned.
			   bool setup,
			   bool hold,
			   bool recovery,
			   bool removal,
			   bool clk_gating_setup,
			   bool clk_gating_hold) __attribute__ ((deprecated));
  void setReportPathFormat(ReportPathFormat format);
  void setReportPathFieldOrder(StringSeq *field_names);
  void setReportPathFields(bool report_input_pin,
			   bool report_net,
			   bool report_cap,
			   bool report_slew);
  ReportField *findReportPathField(const char *name);
  void setReportPathDigits(int digits);
  void setReportPathNoSplit(bool no_split);
  void setReportPathSigmas(bool report_sigmas);
  // Report clk skews for clks.
  void reportClkSkew(ClockSet *clks,
		     const Corner *corner,
		     const SetupHold *setup_hold,
		     int digits);
  // Header above reportPathEnd results.
  void reportPathEndHeader();
  // Footer below reportPathEnd results.
  void reportPathEndFooter();
  // Format report_path_endpoint only:
  //   Previous path end is used to detect path group changes
  //   so headers are reported by group.
  void reportPathEnd(PathEnd *end,
		     PathEnd *prev_end);
  void reportPathEnd(PathEnd *end);
  void reportPathEnds(PathEndSeq *ends);
  ReportPath *reportPath() { return report_path_; }
  void reportPath(Path *path);
  // Update arrival times for all pins.
  // If necessary updateTiming propagates arrivals around latch
  // loops until the arrivals converge.
  // If full=false update arrivals incrementally.
  // If full=true update all arrivals from scratch.
  void updateTiming(bool full);
  // Invalidate all delay calculations. Arrivals also invalidated.
  void delaysInvalid();
  // Invalidate all arrival and required times.
  void arrivalsInvalid();
  void setPathMinMax(const MinMaxAll *min_max) __attribute__ ((deprecated));
  void visitStartpoints(VertexVisitor *visitor);
  void visitEndpoints(VertexVisitor *visitor);
  // Find the fanin vertices for a group path.
  // Vertices in the clock network are NOT included.
  // Return value is owned by the caller.
  PinSet *findGroupPathPins(const char *group_path_name);
  // Find all required times after updateTiming().
  void findRequireds();
  string *reportDelayCalc(Edge *edge,
			  TimingArc *arc,
			  const Corner *corner,
			  const MinMax *min_max,
			  int digits);
  void writeSdc(const char *filename,
		// Map hierarchical pins and instances to leaf pins and instances.
		bool leaf,
		// Replace non-sdc get functions with OpenSTA equivalents.
		bool native,
		bool no_timestamp,
		int digits);

  // The sum of all negative endpoints slacks.
  // Incrementally updated.
  Slack totalNegativeSlack(const MinMax *min_max);
  Slack totalNegativeSlack(const Corner *corner,
			   const MinMax *min_max);
  // Worst endpoint slack and vertex.
  // Incrementally updated.
  void worstSlack(const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  void worstSlack(const Corner *corner,
		  const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  VertexPathIterator *vertexPathIterator(Vertex *vertex,
					 const RiseFall *rf,
					 const PathAnalysisPt *path_ap);
  VertexPathIterator *vertexPathIterator(Vertex *vertex,
					 const RiseFall *rf,
					 const MinMax *min_max);
  void
  vertexWorstArrivalPath(Vertex *vertex,
			 const RiseFall *rf,
			 const MinMax *min_max,
			 // Return value.
			 PathRef &worst_path);
  void
  vertexWorstArrivalPath(Vertex *vertex,
			 const MinMax *min_max,
			 // Return value.
			 PathRef &worst_path);
  void
  vertexWorstSlackPath(Vertex *vertex,
		       const MinMax *min_max,
		       // Return value.
		       PathRef &worst_path);
  void
  vertexWorstSlackPath(Vertex *vertex,
		       const RiseFall *rf,
		       const MinMax *min_max,
		       // Return value.
		       PathRef &worst_path);

  // The following arrival/required/slack functions incrementally
  // update timing to the level of the vertex.  They do NOT do multiple
  // passes required propagate arrivals around latch loops.
  // See Sta::updateTiming() to propagate arrivals around latch loops.
  Arrival vertexArrival(Vertex *vertex,
			const RiseFall *rf,
			const ClockEdge *clk_edge,
			const PathAnalysisPt *path_ap);
  // Min/max across all clock tags.
  Arrival vertexArrival(Vertex *vertex,
			const RiseFall *rf,
			const PathAnalysisPt *path_ap);
  Required vertexRequired(Vertex *vertex,
			  const MinMax *min_max);
  // Min/max across all clock tags.
  Required vertexRequired(Vertex *vertex,
			  const RiseFall *rf,
			  const PathAnalysisPt *path_ap);
  Required vertexRequired(Vertex *vertex,
			  const RiseFall *rf,
			  const ClockEdge *clk_edge,
			  const PathAnalysisPt *path_ap);

  Slack netSlack(const Net *net,
		 const MinMax *min_max);
  Slack pinSlack(const Pin *pin,
		 const RiseFall *rf,
		 const MinMax *min_max);
  Slack pinSlack(const Pin *pin,
		 const MinMax *min_max);
  Slack vertexSlack(Vertex *vertex,
		    const MinMax *min_max);
  Slack vertexSlack(Vertex *vertex,
		    const RiseFall *rf,
		    const MinMax *min_max);
  // Slack with respect to clk_edge.
  Slack vertexSlack(Vertex *vertex,
		    const RiseFall *rf,
		    const ClockEdge *clk_edge,
		    const PathAnalysisPt *path_ap);
  // Min slack across all clock tags.
  Slack vertexSlack(Vertex *vertex,
		    const RiseFall *rf,
		    const PathAnalysisPt *path_ap);
  void vertexSlacks(Vertex *vertex,
		    Slack (&slacks)[RiseFall::index_count][MinMax::index_count]);
  // Slew for one delay calc analysis pt(corner).
  Slew vertexSlew(Vertex *vertex,
		  const RiseFall *rf,
		  const DcalcAnalysisPt *dcalc_ap);
  // Slew across all corners.
  Slew vertexSlew(Vertex *vertex,
		  const RiseFall *rf,
		  const MinMax *min_max);
  ArcDelay arcDelay(Edge *edge,
		    TimingArc *arc,
		    const DcalcAnalysisPt *dcalc_ap);
  // True if the timing arc has been back-annotated.
  bool arcDelayAnnotated(Edge *edge,
			 TimingArc *arc,
			 DcalcAnalysisPt *dcalc_ap);
  // Set/unset the back-annotation flag for a timing arc.
  void setArcDelayAnnotated(Edge *edge,
			    TimingArc *arc,
			    DcalcAnalysisPt *dcalc_ap,
			    bool annotated);
  // Make sure levels are up to date and return vertex level.
  Level vertexLevel(Vertex *vertex);
  GraphLoopSeq *graphLoops();
  PathAnalysisPt *pathAnalysisPt(Path *path);
  DcalcAnalysisPt *pathDcalcAnalysisPt(Path *path);
  TagIndex tagCount() const;
  TagGroupIndex tagGroupCount() const;
  int clkInfoCount() const;
  int arrivalCount() const;
  int vertexArrivalCount(Vertex  *vertex) const;
  Vertex *maxArrivalCountVertex() const;

  LogicValue simLogicValue(const Pin *pin);
  // Iterator for instances sorted by max driver pin slew.
  // Caller owns iterator and iterator->container().
  SlowDrvrIterator *slowDrvrIterator();

  // Annotate hierarchical "instance" with parasitics.
  // The parasitic analysis point is ap_name.
  // The parasitics are used by delay calculation analysis points
  // specfied by min_max.
  // The parasitic memory footprint is much smaller if parasitic
  // networks (dspf) are reduced and deleted after reading each net
  // with reduce_to and delete_after_reduce.
  // Return true if successful.
  bool readSpef(const char *filename,
		Instance *instance,
		const MinMaxAll *min_max,
		bool increment,
		bool pin_cap_included,
		bool keep_coupling_caps,
		float coupling_cap_factor,
		ReduceParasiticsTo reduce_to,
		bool delete_after_reduce,
		bool save,
		bool quiet);
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

  // Network edit before/after methods.
  void makeInstanceAfter(Instance *inst);
  // Not used by Sta (connectPinAfter).
  void makePinAfter(Pin *pin) __attribute__ ((deprecated));
  // Replace the instance cell with to_cell.
  // equivCells(from_cell, to_cell) must be true.
  virtual void replaceEquivCellBefore(Instance *inst,
				      LibertyCell *to_cell);
  virtual void replaceEquivCellAfter(Instance *inst);
  // Replace the instance cell with to_cell.
  // equivCellPorts(from_cell, to_cell) must be true.
  virtual void replaceCellBefore(Instance *inst,
				 LibertyCell *to_cell);
  virtual void replaceCellAfter(Instance *inst);
  virtual void connectPinAfter(Pin *pin);
  virtual void disconnectPinBefore(Pin *pin);
  virtual void deleteNetBefore(Net *net);
  virtual void deleteInstanceBefore(Instance *inst);
  virtual void deletePinBefore(Pin *pin);

  ////////////////////////////////////////////////////////////////

  void setTclInterp(Tcl_Interp *interp);
  Tcl_Interp *tclInterp();
  void ensureLevelized();
  // Ensure that the timing graph has been built.
  Graph *ensureGraph();
  void ensureClkArrivals();
  Corner *cmdCorner() const;
  void setCmdCorner(Corner *corner);
  Corner *findCorner(const char *corner_name);
  bool multiCorner();
  void makeCorners(StringSet *corner_names);
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
  void delaysInvalidFrom(Instance *inst);
  // Delays and arrivals downsteam from pin are invalid.
  void delaysInvalidFrom(Pin *pin);
  void delaysInvalidFrom(Vertex *vertex);
  // Delays to driving pins of net (fanin) are invalid.
  // Arrivals downsteam from net are invalid.
  void delaysInvalidFromFanin(Net *net);
  void delaysInvalidFromFanin(Pin *pin);
  void delaysInvalidFromFanin(Vertex *vertex);
  void replaceCellPinInvalidate(LibertyPort *from_port,
				Vertex *vertex,
				LibertyCell *to_cell);

  // Power API.
  Power *power() { return power_; }
  const Power *power() const { return power_; }
  void power(const Corner *corner,
	     // Return values.
	     PowerResult &total,
	     PowerResult &sequential,
  	     PowerResult &combinational,
	     PowerResult &macro,
	     PowerResult &pad);
  void power(const Instance *inst,
	     const Corner *corner,
	     // Return values.
	     PowerResult &result);

  // Find equivalent cells in equiv_libs.
  // Optionally add mappings for cells in map_libs.
  void makeEquivCells(LibertyLibrarySeq *equiv_libs,
		      LibertyLibrarySeq *map_libs);
  LibertyCellSeq *equivCells(LibertyCell *cell);

protected:
  // Default constructors that are called by makeComponents in the Sta
  // constructor.  These can be redefined by a derived class to
  // specialize the sta components.
  virtual void makeReport();
  virtual void makeDebug();
  virtual void makeUnits();
  virtual void makeNetwork();
  virtual void makeSdcNetwork();
  virtual void makeSdc();
  virtual void makeGraph();
  virtual void makeCorners();
  virtual void makeLevelize();
  virtual void makeParasitics();
  virtual void makeArcDelayCalc();
  virtual void makeGraphDelayCalc();
  virtual void makeSim();
  virtual void makeSearch();
  virtual void makeLatches();
  virtual void makeCheckTiming();
  virtual void makeCheckSlewLimits();
  virtual void makeCheckMinPulseWidths();
  virtual void makeCheckMinPeriods();
  virtual void makeCheckMaxSkews();
  virtual void makeReportPath();
  virtual void makePower();
  virtual void makeObservers();
  NetworkEdit *networkCmdEdit();

  LibertyLibrary *readLibertyFile(const char *filename,
				  Corner *corner,
				  const MinMaxAll *min_max,
				  bool infer_latches,
				  Network *network);
  // Allow external Liberty reader to parse forms not used by Sta.
  virtual LibertyLibrary *readLibertyFile(const char *filename,
					  bool infer_latches,
					  Network *network);
  void delayCalcPreamble();
  void delaysInvalidFrom(Port *port);
  void delaysInvalidFromFanin(Port *port);
  void deleteEdge(Edge *edge);
  void netParasiticCaps(Net *net,
			const RiseFall *rf,
			const MinMax *min_max,
			float &pin_cap,
			float &wire_cap) const;
  Pin *findNetParasiticDrvrPin(Net *net) const;
  void exprConstantPins(FuncExpr *expr,
			Instance *inst,
			PinSet *pins);
  Slack vertexSlack1(Vertex *vertex,
		     const RiseFall *rf,
		     const ClockEdge *clk_edge,
		     const PathAnalysisPt *path_ap);
  void findRequired(Vertex *vertex);
  void connectDrvrPinAfter(Vertex *vertex);
  void connectLoadPinAfter(Vertex *vertex);
  Path *latchEnablePath(Path *q_path,
			Edge *d_q_edge,
			const ClockEdge *en_clk_edge);
  void clockSlewChanged(Clock *clk);
  void checkSlewLimitPreamble();
  void minPulseWidthPreamble();
  void minPeriodPreamble();
  void maxSkewPreamble();
  bool idealClockMode();
  void disableAfter();
  void findFaninPins(Vertex *vertex,
		     bool flat,
		     bool startpoints_only,
		     int inst_levels,
		     int pin_levels,
		     PinSet *fanin,
		     SearchPred &pred);
  void findFaninPins(Vertex *to,
		     bool flat,
		     int inst_levels,
		     int pin_levels,
		     VertexSet &visited,
		     SearchPred *pred,
		     int inst_level,
		     int pin_level);
  void findFanoutPins(Vertex *vertex,
		      bool flat,
		      bool endpoints_only,
		      int inst_levels,
		      int pin_levels,
		      PinSet *fanout,
		      SearchPred &pred);
  void findFanoutPins(Vertex *from,
		      bool flat,
		      int inst_levels,
		      int pin_levels,
		      VertexSet &visited,
		      SearchPred *pred,
		      int inst_level,
		      int pin_level);
  void findRegisterPreamble();
  bool crossesHierarchy(Edge *edge) const;
  void deleteLeafInstanceBefore(Instance *inst);
  void readLibertyAfter(LibertyLibrary *liberty,
			Corner *corner,
			const MinMax *min_max);
  void powerPreamble();
  void disableFanoutCrprPruning(Vertex *vertex,
			      int &fanou);
  LibertyPort *findCellPort(LibertyCell *cell,
			    PortDirection *dir);
  void replaceCell(Instance *inst,
		   Cell *to_cell,
		   LibertyCell *to_lib_cell);

  CmdNamespace cmd_namespace_;
  Instance *current_instance_;
  Corner *cmd_corner_;
  CheckTiming *check_timing_;
  CheckSlewLimits *check_slew_limits_;
  CheckMinPulseWidths *check_min_pulse_widths_;
  CheckMinPeriods *check_min_periods_;
  CheckMaxSkews *check_max_skews_;
  ClkSkews *clk_skews_;
  ReportPath *report_path_;
  Power *power_;
  Tcl_Interp *tcl_interp_;
  bool link_make_black_boxes_;
  bool update_genclks_;
  EquivCells *equiv_cells_;

  // Singleton sta used by tcl command interpreter.
  static Sta *sta_;

private:
  DISALLOW_COPY_AND_ASSIGN(Sta);
};

} // namespace
