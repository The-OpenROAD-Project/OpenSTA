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

%module sdc

%{
#include "Sdc.hh"
#include "Wireload.hh"
#include "Clock.hh"
#include "PortDelay.hh"
#include "Property.hh"
#include "Sta.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class Clock
{
private:
  Clock();
  ~Clock();
};

class ClockEdge
{
private:
  ClockEdge();
  ~ClockEdge();
};

class ExceptionFrom
{
private:
  ExceptionFrom();
  ~ExceptionFrom();
};

class ExceptionThru
{
private:
  ExceptionThru();
  ~ExceptionThru();
};

class ExceptionTo
{
private:
  ExceptionTo();
  ~ExceptionTo();
};

class OperatingConditions
{
private:
  OperatingConditions();
  ~OperatingConditions();
};

%inline %{

void
write_sdc_cmd(const char *filename,
	      bool leaf,
	      bool compatible,
	      int digits,
              bool gzip,
	      bool no_timestamp)
{
  cmdLinkedNetwork();
  Sta::sta()->writeSdc(filename, leaf, compatible, digits, gzip, no_timestamp);
}

void
set_analysis_type_cmd(const char *analysis_type)
{
  AnalysisType type;
  if (stringEq(analysis_type, "single"))
    type = AnalysisType::single;
  else if (stringEq(analysis_type, "bc_wc"))
    type = AnalysisType::bc_wc;
  else if (stringEq(analysis_type, "on_chip_variation"))
    type = AnalysisType::ocv;
  else {
    Sta::sta()->report()->warn(2121, "unknown analysis type");
    type = AnalysisType::single;
  }
  Sta::sta()->setAnalysisType(type);
}

OperatingConditions *
operating_conditions(const MinMax *min_max)
{
  return Sta::sta()->operatingConditions(min_max);
}

void
set_operating_conditions_cmd(OperatingConditions *op_cond,
			     const MinMaxAll *min_max)
{
  Sta::sta()->setOperatingConditions(op_cond, min_max);
}

const char *
operating_condition_analysis_type()
{
  switch (Sta::sta()->sdc()->analysisType()){
  case AnalysisType::single:
    return "single";
  case AnalysisType::bc_wc:
    return "bc_wc";
  case AnalysisType::ocv:
    return "on_chip_variation";
  }
  // Prevent warnings from lame compilers.
  return "?";
}

void
set_instance_pvt(Instance *inst,
		 const MinMaxAll *min_max,
		 float process,
		 float voltage,
		 float temperature)
{
  cmdLinkedNetwork();
  Pvt pvt(process, voltage, temperature);
  Sta::sta()->setPvt(inst, min_max, pvt);
}

float
port_ext_pin_cap(const Port *port,
                 const Corner *corner,
		 const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return pin_cap;
}

void
set_port_ext_pin_cap(const Port *port,
                     const RiseFallBoth *rf,
                     const Corner *corner,
                     const MinMaxAll *min_max,
                     float cap)
{
  Sta::sta()->setPortExtPinCap(port, rf, corner, min_max, cap);
}

float
port_ext_wire_cap(const Port *port,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return wire_cap;
}

void
set_port_ext_wire_cap(const Port *port,
                      bool subtract_pin_cap,
                      const RiseFallBoth *rf,
                      const Corner *corner,
                      const MinMaxAll *min_max,
                      float cap)
{
  Sta::sta()->setPortExtWireCap(port, subtract_pin_cap, rf, corner, min_max, cap);
}

void
set_port_ext_fanout_cmd(const Port *port,
			int fanout,
                        const Corner *corner,
			const MinMaxAll *min_max)
{
  Sta::sta()->setPortExtFanout(port, fanout, corner, min_max);
}

float
port_ext_fanout(const Port *port,
                const Corner *corner,
                const MinMax *min_max)
{
  cmdLinkedNetwork();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, corner, min_max, pin_cap, wire_cap, fanout);
  return fanout;
}

void
set_net_wire_cap(const Net *net,
		 bool subtract_pin_cap,
		 const Corner *corner,
		 const MinMaxAll *min_max,
		 float cap)
{
  Sta::sta()->setNetWireCap(net, subtract_pin_cap, corner, min_max, cap);
}

void
set_wire_load_mode_cmd(const char *mode_name)
{
  WireloadMode mode = stringWireloadMode(mode_name);
  if (mode == WireloadMode::unknown)
    Sta::sta()->report()->warn(2122, "unknown wire load mode");
  else
    Sta::sta()->setWireloadMode(mode);
}

void
set_net_resistance(Net *net,
		   const MinMaxAll *min_max,
		   float res)
{
  Sta::sta()->setResistance(net, min_max, res);
}

void
set_wire_load_cmd(Wireload *wireload,
		  const MinMaxAll *min_max)
{
  Sta::sta()->setWireload(wireload, min_max);
}

void
set_wire_load_selection_group_cmd(WireloadSelection *selection,
				  const MinMaxAll *min_max)
{
  Sta::sta()->setWireloadSelection(selection, min_max);
}

void
make_clock(const char *name,
	   PinSet *pins,
	   bool add_to_pins,
	   float period,
	   FloatSeq *waveform,
	   char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeClock(name, pins, add_to_pins, period, waveform, comment);
}

void
make_generated_clock(const char *name,
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
		     char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeGeneratedClock(name, pins, add_to_pins,
				 src_pin, master_clk,
				 divide_by, multiply_by, duty_cycle, invert,
				 combinational, edges, edge_shifts,
				 comment);
}

void
remove_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClock(clk);
}

void
set_propagated_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->setPropagatedClock(clk);
}

void
set_propagated_clock_pin_cmd(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->setPropagatedClock(pin);
}

void
unset_propagated_clock_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removePropagatedClock(clk);
}

void
unset_propagated_clock_pin_cmd(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removePropagatedClock(pin);
}

void
set_clock_slew_cmd(Clock *clk,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockSlew(clk, rf, min_max, slew);
}

void
unset_clock_slew_cmd(Clock *clk)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockSlew(clk);
}

void
set_clock_latency_cmd(Clock *clk,
		      Pin *pin,
		      const RiseFallBoth *rf,
		      MinMaxAll *min_max, float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockLatency(clk, pin, rf, min_max, delay);
}

void
set_clock_insertion_cmd(Clock *clk,
			Pin *pin,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			const EarlyLateAll *early_late,
			float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockInsertion(clk, pin, rf, min_max, early_late, delay);
}

void
unset_clock_latency_cmd(Clock *clk,
			Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockLatency(clk, pin);
}

void
unset_clock_insertion_cmd(Clock *clk,
			  Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockInsertion(clk, pin);
}

void
set_clock_uncertainty_clk(Clock *clk,
			  const SetupHoldAll *setup_hold,
			  float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(clk, setup_hold, uncertainty);
}

void
unset_clock_uncertainty_clk(Clock *clk,
			    const SetupHoldAll *setup_hold)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(clk, setup_hold);
}

void
set_clock_uncertainty_pin(Pin *pin,
			  const MinMaxAll *min_max,
			  float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(pin, min_max, uncertainty);
}

void
unset_clock_uncertainty_pin(Pin *pin,
			    const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(pin, min_max);
}

void
set_inter_clock_uncertainty(Clock *from_clk,
			    const RiseFallBoth *from_tr,
			    Clock *to_clk,
			    const RiseFallBoth *to_tr,
			    const MinMaxAll *min_max,
			    float uncertainty)
{
  cmdLinkedNetwork();
  Sta::sta()->setClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max,
				  uncertainty);
}

void
unset_inter_clock_uncertainty(Clock *from_clk,
			      const RiseFallBoth *from_tr,
			      Clock *to_clk,
			      const RiseFallBoth *to_tr,
			      const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max);
}

void
set_clock_gating_check_cmd(const RiseFallBoth *rf,
			   const SetupHold *setup_hold,
			   float margin)
{
  Sta::sta()->setClockGatingCheck(rf, setup_hold, margin);
}

void
set_clock_gating_check_clk_cmd(Clock *clk,
			       const RiseFallBoth *rf,
			       const SetupHold *setup_hold,
			       float margin)
{
  Sta::sta()->setClockGatingCheck(clk, rf, setup_hold, margin);
}

void
set_clock_gating_check_pin_cmd(Pin *pin,
			       const RiseFallBoth *rf,
			       const SetupHold *setup_hold,
			       float margin,
			       LogicValue active_value)
{
  Sta::sta()->setClockGatingCheck(pin, rf, setup_hold, margin, active_value);
}

void
set_clock_gating_check_instance_cmd(Instance *inst,
				    const RiseFallBoth *rf,
				    const SetupHold *setup_hold,
				    float margin,
				    LogicValue active_value)
{
  Sta::sta()->setClockGatingCheck(inst, rf, setup_hold, margin, active_value);
}

void
set_data_check_cmd(Pin *from,
		   const RiseFallBoth *from_rf,
		   Pin *to,
		   const RiseFallBoth *to_rf,
		   Clock *clk,
		   const SetupHoldAll *setup_hold,
		   float margin)
{
  Sta::sta()->setDataCheck(from, from_rf, to, to_rf, clk, setup_hold, margin);
}

void
unset_data_check_cmd(Pin *from,
		     const RiseFallBoth *from_tr,
		     Pin *to,
		     const RiseFallBoth *to_tr,
		     Clock *clk,
		     const SetupHoldAll *setup_hold)
{
  Sta::sta()->removeDataCheck(from, from_tr, to, to_tr, clk, setup_hold);
}

void
set_input_delay_cmd(Pin *pin,
		    RiseFallBoth *rf,
		    Clock *clk,
		    RiseFall *clk_rf,
		    Pin *ref_pin,
		    bool source_latency_included,
		    bool network_latency_included,
		    MinMaxAll *min_max,
		    bool add,
		    float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setInputDelay(pin, rf, clk, clk_rf, ref_pin,
			    source_latency_included, network_latency_included,
			    min_max, add, delay);
}

void
unset_input_delay_cmd(Pin *pin,
		      RiseFallBoth *rf, 
		      Clock *clk,
		      RiseFall *clk_rf, 
		      MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeInputDelay(pin, rf, clk, clk_rf, min_max);
}

void
set_output_delay_cmd(Pin *pin,
		     const RiseFallBoth *rf,
		     Clock *clk,
		     const RiseFall *clk_rf,
		     Pin *ref_pin,
		     bool source_latency_included,
		     bool network_latency_included,
		     const MinMaxAll *min_max,
		     bool add,
		     float delay)
{
  cmdLinkedNetwork();
  Sta::sta()->setOutputDelay(pin, rf, clk, clk_rf, ref_pin,
			     source_latency_included, network_latency_included,
			     min_max, add, delay);
}

void
unset_output_delay_cmd(Pin *pin,
		       RiseFallBoth *rf, 
		       Clock *clk,
		       RiseFall *clk_rf, 
		       MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->removeOutputDelay(pin, rf, clk, clk_rf, min_max);
}

void
disable_cell(LibertyCell *cell,
	     LibertyPort *from,
	     LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(cell, from, to);
}

void
unset_disable_cell(LibertyCell *cell,
		   LibertyPort *from,
		   LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(cell, from, to);
}

void
disable_lib_port(LibertyPort *port)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(port);
}

void
unset_disable_lib_port(LibertyPort *port)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(port);
}

void
disable_port(Port *port)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(port);
}

void
unset_disable_port(Port *port)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(port);
}

void
disable_instance(Instance *instance,
		 LibertyPort *from,
		 LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(instance, from, to);
}

void
unset_disable_instance(Instance *instance,
		       LibertyPort *from,
		       LibertyPort *to)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(instance, from, to);
}

void
disable_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(pin);
}

void
unset_disable_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(pin);
}

void
disable_edge(Edge *edge)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(edge);
}

void
unset_disable_edge(Edge *edge)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(edge);
}

void
disable_timing_arc_set(TimingArcSet *arc_set)
{
  cmdLinkedNetwork();
  Sta::sta()->disable(arc_set);
}

void
unset_disable_timing_arc_set(TimingArcSet *arc_set)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisable(arc_set);
}

void
disable_clock_gating_check_inst(Instance *inst)
{
  cmdLinkedNetwork();
  Sta::sta()->disableClockGatingCheck(inst);
}

void
disable_clock_gating_check_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->disableClockGatingCheck(pin);
}

void
unset_disable_clock_gating_check_inst(Instance *inst)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisableClockGatingCheck(inst);
}

void
unset_disable_clock_gating_check_pin(Pin *pin)
{
  cmdLinkedNetwork();
  Sta::sta()->removeDisableClockGatingCheck(pin);
}

EdgeSeq
disabled_edges_sorted()
{
  cmdLinkedNetwork();
  return Sta::sta()->disabledEdgesSorted();
}

bool
timing_arc_disabled(Edge *edge,
		    TimingArc *arc)
{
  Graph *graph = Sta::sta()->graph();
  return !searchThru(edge, arc, graph);
}

void
make_false_path(ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const MinMaxAll *min_max,
		const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeFalsePath(from, thrus, to, min_max, comment);
}

void
make_multicycle_path(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to,
		     const MinMaxAll *min_max,
		     bool use_end_clk,
		     int path_multiplier,
		     const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makeMulticyclePath(from, thrus, to, min_max, use_end_clk,
				 path_multiplier, comment);
}

void
make_path_delay(ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const MinMax *min_max,
		bool ignore_clk_latency,
		float delay,
		const char *comment)
{
  cmdLinkedNetwork();
  Sta::sta()->makePathDelay(from, thrus, to, min_max, 
			    ignore_clk_latency, delay, comment);
}

void
reset_path_cmd(ExceptionFrom *
	       from, ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       const MinMaxAll *min_max)
{
  cmdLinkedNetwork();
  Sta::sta()->resetPath(from, thrus, to, min_max);
  // from/to and thru are owned and deleted by the caller.
  // ExceptionThruSeq thrus arg is made by TclListSeqExceptionThru
  // in the swig converter so it is deleted here.
  delete thrus;
}

void
make_group_path(const char *name,
		bool is_default,
		ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const char *comment)
{
  cmdLinkedNetwork();
  if (name[0] == '\0')
    name = nullptr;
  Sta::sta()->makeGroupPath(name, is_default, from, thrus, to, comment);
}

bool
is_path_group_name(const char *name)
{
  cmdLinkedNetwork();
  return Sta::sta()->isGroupPathName(name);
}

ExceptionFrom *
make_exception_from(PinSet *from_pins,
		    ClockSet *from_clks,
		    InstanceSet *from_insts,
		    const RiseFallBoth *from_tr)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionFrom(from_pins, from_clks, from_insts,
				       from_tr);
}

void
delete_exception_from(ExceptionFrom *from)
{
  Sta::sta()->deleteExceptionFrom(from);
}

void
check_exception_from_pins(ExceptionFrom *from,
			  const char *file,
			  int line)
{
  Sta::sta()->checkExceptionFromPins(from, file, line);
}

ExceptionThru *
make_exception_thru(PinSet *pins,
		    NetSet *nets,
		    InstanceSet *insts,
		    const RiseFallBoth *rf)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionThru(pins, nets, insts, rf);
}

void
delete_exception_thru(ExceptionThru *thru)
{
  Sta::sta()->deleteExceptionThru(thru);
}

ExceptionTo *
make_exception_to(PinSet *to_pins,
		  ClockSet *to_clks,
		  InstanceSet *to_insts,
		  const RiseFallBoth *rf,
 		  RiseFallBoth *end_rf)
{
  cmdLinkedNetwork();
  return Sta::sta()->makeExceptionTo(to_pins, to_clks, to_insts, rf, end_rf);
}

void
delete_exception_to(ExceptionTo *to)
{
  Sta::sta()->deleteExceptionTo(to);
}

void
check_exception_to_pins(ExceptionTo *to,
			const char *file,
			int line)
{
  Sta::sta()->checkExceptionToPins(to, file, line);
}

ClockGroups *
make_clock_groups(const char *name,
		  bool logically_exclusive,
		  bool physically_exclusive,
		  bool asynchronous,
		  bool allow_paths,
		  const char *comment)
{
  return Sta::sta()->makeClockGroups(name, logically_exclusive,
				     physically_exclusive, asynchronous,
				     allow_paths, comment);
}

void
clock_groups_make_group(ClockGroups *clk_groups,
			ClockSet *clks)
{
  Sta::sta()->makeClockGroup(clk_groups, clks);
}

void
unset_clock_groups_logically_exclusive(const char *name)
{
  Sta::sta()->removeClockGroupsLogicallyExclusive(name);
}

void
unset_clock_groups_physically_exclusive(const char *name)
{
  Sta::sta()->removeClockGroupsPhysicallyExclusive(name);
}

void
unset_clock_groups_asynchronous(const char *name)
{
  Sta::sta()->removeClockGroupsAsynchronous(name);
}

// Debugging function.
bool
same_clk_group(Clock *clk1,
	       Clock *clk2)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  return sdc->sameClockGroupExplicit(clk1, clk2);
}

void
set_clock_sense_cmd(PinSet *pins,
		    ClockSet *clks,
		    bool positive,
		    bool negative,
		    bool stop_propagation)
{
  Sta *sta = Sta::sta();
  if (positive)
    sta->setClockSense(pins, clks, ClockSense::positive);
  else if (negative)
    sta->setClockSense(pins, clks, ClockSense::negative);
  else if (stop_propagation)
    sta->setClockSense(pins, clks, ClockSense::stop);
  else
    sta->report()->critical(1577, "unknown clock sense");
}

void
set_input_slew_cmd(Port *port,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setInputSlew(port, rf, min_max, slew);
}

void
set_drive_cell_cmd(LibertyLibrary *library,
		   LibertyCell *cell,
		   Port *port,
		   LibertyPort *from_port,
		   float from_slew_rise,
		   float from_slew_fall,
		   LibertyPort *to_port,
		   const RiseFallBoth *rf,
		   const MinMaxAll *min_max)
{
  float from_slews[RiseFall::index_count];
  from_slews[RiseFall::riseIndex()] = from_slew_rise;
  from_slews[RiseFall::fallIndex()] = from_slew_fall;
  Sta::sta()->setDriveCell(library, cell, port, from_port, from_slews,
			   to_port, rf, min_max);
}

void
set_drive_resistance_cmd(Port *port,
			 const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 float res)
{
  cmdLinkedNetwork();
  Sta::sta()->setDriveResistance(port, rf, min_max, res);
}

void
set_slew_limit_clk(Clock *clk,
		   const RiseFallBoth *rf,
		   PathClkOrData clk_data,
		   const MinMax *min_max,
		   float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(clk, rf, clk_data, min_max, slew);
}

void
set_slew_limit_port(Port *port,
		    const MinMax *min_max,
		    float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(port, min_max, slew);
}

void
set_slew_limit_cell(Cell *cell,
		    const MinMax *min_max,
		    float slew)
{
  cmdLinkedNetwork();
  Sta::sta()->setSlewLimit(cell, min_max, slew);
}

void
set_port_capacitance_limit(Port *port,
			   const MinMax *min_max,
			   float cap)
{
  Sta::sta()->setCapacitanceLimit(port, min_max, cap);
}

void
set_pin_capacitance_limit(Pin *pin,
			  const MinMax *min_max,
			  float cap)
{
  Sta::sta()->setCapacitanceLimit(pin, min_max, cap);
}

void
set_cell_capacitance_limit(Cell *cell,
			   const MinMax *min_max,
			   float cap)
{
  Sta::sta()->setCapacitanceLimit(cell, min_max, cap);
}

void
set_latch_borrow_limit_pin(Pin *pin,
			   float limit)
{
  Sta::sta()->setLatchBorrowLimit(pin, limit);
}

void
set_latch_borrow_limit_inst(Instance *inst,
			    float limit)
{
  Sta::sta()->setLatchBorrowLimit(inst, limit);
}

void
set_latch_borrow_limit_clk(Clock *clk, float limit)
{
  Sta::sta()->setLatchBorrowLimit(clk, limit);
}

void
set_min_pulse_width_global(const RiseFallBoth *rf,
			   float min_width)
{
  Sta::sta()->setMinPulseWidth(rf, min_width);
}

void
set_min_pulse_width_pin(Pin *pin,
			const RiseFallBoth *rf,
			float min_width)
{
  Sta::sta()->setMinPulseWidth(pin, rf, min_width);
}

void
set_min_pulse_width_clk(Clock *clk,
			const RiseFallBoth *rf,
			float min_width)
{
  Sta::sta()->setMinPulseWidth(clk, rf, min_width);
}

void
set_min_pulse_width_inst(Instance *inst,
			 const RiseFallBoth *rf,
			 float min_width)
{
  Sta::sta()->setMinPulseWidth(inst, rf, min_width);
}

void
set_max_area_cmd(float area)
{
  Sta::sta()->setMaxArea(area);
}

void
set_port_fanout_limit(Port *port,
		      const MinMax *min_max,
		      float fanout)
{
  Sta::sta()->setFanoutLimit(port, min_max, fanout);
}

void
set_cell_fanout_limit(Cell *cell,
		      const MinMax *min_max,
		      float fanout)
{
  Sta::sta()->setFanoutLimit(cell, min_max, fanout);
}

void
set_logic_value_cmd(Pin *pin,
		    LogicValue value)
{
  Sta::sta()->setLogicValue(pin, value);
}

void
set_case_analysis_cmd(Pin *pin,
		      LogicValue value)
{
  Sta::sta()->setCaseAnalysis(pin, value);
}

void
unset_case_analysis_cmd(Pin *pin)
{
  Sta::sta()->removeCaseAnalysis(pin);
}

void
set_timing_derate_cmd(TimingDerateType type,
		      PathClkOrData clk_data,
		      const RiseFallBoth *rf,
		      const EarlyLate *early_late,
		      float derate)
{
  Sta::sta()->setTimingDerate(type, clk_data, rf, early_late, derate);
}

void
set_timing_derate_net_cmd(const Net *net,
			  PathClkOrData clk_data,
			  const RiseFallBoth *rf,
			  const EarlyLate *early_late,
			  float derate)
{
  Sta::sta()->setTimingDerate(net, clk_data, rf, early_late, derate);
}

void
set_timing_derate_inst_cmd(const Instance *inst,
			   TimingDerateCellType type,
			   PathClkOrData clk_data,
			   const RiseFallBoth *rf,
			   const EarlyLate *early_late,
			   float derate)
{
  Sta::sta()->setTimingDerate(inst, type, clk_data, rf, early_late, derate);
}

void
set_timing_derate_cell_cmd(const LibertyCell *cell,
			   TimingDerateCellType type,
			   PathClkOrData clk_data,
			   const RiseFallBoth *rf,
			   const EarlyLate *early_late,
			   float derate)
{
  Sta::sta()->setTimingDerate(cell, type, clk_data, rf, early_late, derate);
}

void
unset_timing_derate_cmd()
{
  Sta::sta()->unsetTimingDerate();
}

Clock *
find_clock(const char *name)
{
  cmdLinkedNetwork();
  return Sta::sta()->sdc()->findClock(name);
}

bool
is_clock_src(const Pin *pin)
{
  return Sta::sta()->isClockSrc(pin);
}

Clock *
default_arrival_clock()
{
  return Sta::sta()->sdc()->defaultArrivalClock();
}

ClockSeq
find_clocks_matching(const char *pattern,
		     bool regexp,
		     bool nocase)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  return sdc->findClocksMatching(&matcher);
}

void
update_generated_clks()
{
  cmdLinkedNetwork();
  Sta::sta()->updateGeneratedClks();
}

bool
is_clock(Pin *pin)
{
  Sta *sta = Sta::sta();
  return sta->isClock(pin);
}

bool
is_ideal_clock(Pin *pin)
{
  Sta *sta = Sta::sta();
  return sta->isIdealClock(pin);
}

bool
is_clock_search(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Graph *graph = sta->graph();
  Search *search = sta->search();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  return search->isClock(vertex);
}

bool
is_genclk_src(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Graph *graph = sta->graph();
  Search *search = sta->search();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  return search->isGenClkSrc(vertex);
}

bool
pin_is_constrained(Pin *pin)
{
  return Sta::sta()->sdc()->isConstrained(pin);
}

bool
instance_is_constrained(Instance *inst)
{
  return Sta::sta()->sdc()->isConstrained(inst);
}

bool
net_is_constrained(Net *net)
{
  return Sta::sta()->sdc()->isConstrained(net);
}

bool
clk_thru_tristate_enabled()
{
  return Sta::sta()->clkThruTristateEnabled();
}

void
set_clk_thru_tristate_enabled(bool enabled)
{
  Sta::sta()->setClkThruTristateEnabled(enabled);
}

void
remove_constraints()
{
  Sta::sta()->removeConstraints();
}

PortSeq
all_inputs_cmd(bool no_clocks)
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  return sta->sdc()->allInputs(no_clocks);
}

PortSeq
all_outputs_cmd()
{
  Sta *sta = Sta::sta();
  cmdLinkedNetwork();
  return sta->sdc()->allOutputs();
}

template <typename T> Vector<T*>
filter_objects(const char *property,
	       const char *op,
	       const char *pattern,
	       Vector<T*> *objects)
{
  Vector<T*> filtered_objects;
  if (objects) {
    Sta *sta = Sta::sta();
    bool exact_match = stringEq(op, "==");
    bool pattern_match = stringEq(op, "=~");
    bool not_match = stringEq(op, "!=");
    bool not_pattern_match = stringEq(op, "!~");
    for (T *object : *objects) {
      PropertyValue value(getProperty(object, property, sta));
      const char *prop = value.asString(sta->network());
      if (prop &&
          ((exact_match && stringEq(prop, pattern))
           || (not_match && !stringEq(prop, pattern))
           || (pattern_match && patternMatch(pattern, prop))
           || (not_pattern_match && !patternMatch(pattern, prop))))
        filtered_objects.push_back(object);
    }
    delete objects;
  }
  return filtered_objects;
}

PortSeq
filter_ports(const char *property,
	     const char *op,
	     const char *pattern,
	     PortSeq *ports)
{
  return filter_objects<const Port>(property, op, pattern, ports);
}

InstanceSeq
filter_insts(const char *property,
	     const char *op,
	     const char *pattern,
	     InstanceSeq *insts)
{
  return filter_objects<const Instance>(property, op, pattern, insts);
}

PinSeq
filter_pins(const char *property,
	    const char *op,
	    const char *pattern,
	    PinSeq *pins)
{
  return filter_objects<const Pin>(property, op, pattern, pins);
}

ClockSeq
filter_clocks(const char *property,
	      const char *op,
	      const char *pattern,
	      ClockSeq *clocks)
{
  return filter_objects<Clock>(property, op, pattern, clocks);
}

LibertyCellSeq
filter_lib_cells(const char *property,
		 const char *op,
		 const char *pattern,
		 LibertyCellSeq *cells)
{
  return filter_objects<LibertyCell>(property, op, pattern, cells);
}

LibertyPortSeq
filter_lib_pins(const char *property,
		const char *op,
		const char *pattern,
		LibertyPortSeq *pins)
{
  return filter_objects<LibertyPort>(property, op, pattern, pins);
}

LibertyLibrarySeq
filter_liberty_libraries(const char *property,
			 const char *op,
			 const char *pattern,
			 LibertyLibrarySeq *libs)
{
  return filter_objects<LibertyLibrary>(property, op, pattern, libs);
}

NetSeq
filter_nets(const char *property,
	    const char *op,
	    const char *pattern,
	    NetSeq *nets)
{
  return filter_objects<const Net>(property, op, pattern, nets);
}

EdgeSeq
filter_timing_arcs(const char *property,
		   const char *op,
		   const char *pattern,
		   EdgeSeq *edges)
{
  return filter_objects<Edge>(property, op, pattern, edges);
}

////////////////////////////////////////////////////////////////

void
set_voltage_global(const MinMax *min_max,
                   float voltage)
{
  Sta::sta()->setVoltage(min_max, voltage);
}

void
set_voltage_net(const Net *net,
                const MinMax *min_max,
                float voltage)
{
  Sta::sta()->setVoltage(net, min_max, voltage);
}

////////////////////////////////////////////////////////////////

PinSet
group_path_pins(const char *group_path_name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  if (sdc->isGroupPathName(group_path_name))
    return sta->findGroupPathPins(group_path_name);
  else
    return PinSet(sta->network());
}

////////////////////////////////////////////////////////////////

char
pin_case_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  LogicValue value = LogicValue::unknown;
  bool exists;
  sdc->caseLogicValue(pin, value, exists);
  return logicValueString(value);
}

char
pin_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->sdc();
  LogicValue value = LogicValue::unknown;
  bool exists;
  sdc->logicValue(pin, value, exists);
  return logicValueString(value);
}

////////////////////////////////////////////////////////////////
//
// Variables
//
////////////////////////////////////////////////////////////////

bool
propagate_all_clocks()
{
  return Sta::sta()->propagateAllClocks();
}

void
set_propagate_all_clocks(bool prop)
{
  Sta::sta()->setPropagateAllClocks(prop);
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend Clock {
float period() { return self->period(); }
FloatSeq *waveform() { return self->waveform(); }
float time(RiseFall *rf) { return self->edge(rf)->time(); }
bool is_generated() { return self->isGenerated(); }
bool waveform_valid() { return self->waveformValid(); }
bool is_virtual() { return self->isVirtual(); }
bool is_propagated() { return self->isPropagated(); }
const PinSet &sources() { return self->pins(); }

float
slew(const RiseFall *rf,
     const MinMax *min_max)
{
  return self->slew(rf, min_max);
}

}

%extend ClockEdge {
Clock *clock() { return self->clock(); }
RiseFall *transition() { return self->transition(); }
float time() { return self->time(); }
}
