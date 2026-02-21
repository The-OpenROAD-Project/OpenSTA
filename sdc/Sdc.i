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

%module sdc
%import <std_string.i>

%{
#include <vector>

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
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  sta->writeSdc(sdc, filename, leaf, compatible, digits, gzip, no_timestamp);
}

void
set_analysis_type_cmd(const char *analysis_type)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  AnalysisType type;
  if (stringEq(analysis_type, "single"))
    type = AnalysisType::single;
  else if (stringEq(analysis_type, "bc_wc"))
    type = AnalysisType::bc_wc;
  else if (stringEq(analysis_type, "on_chip_variation"))
    type = AnalysisType::ocv;
  else {
    sta->report()->warn(2121, "unknown analysis type");
    type = AnalysisType::single;
  }
  sta->setAnalysisType(type, sdc);
}

OperatingConditions *
operating_conditions(const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  return sta->operatingConditions(min_max, sdc);
}

void
set_operating_conditions_cmd(OperatingConditions *op_cond,
                             const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setOperatingConditions(op_cond, min_max, sdc);
}

const char *
operating_condition_analysis_type()
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  switch (sdc->analysisType()) {
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  Pvt pvt(process, voltage, temperature);
  sta->setPvt(inst, min_max, pvt, sdc);
}

float
port_ext_pin_cap(const Port *port,
                 const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  float pin_cap, wire_cap;
  int fanout;
  sta->portExtCaps(port, min_max, sdc, pin_cap, wire_cap, fanout);
  return pin_cap;
}

void
set_port_ext_pin_cap(const Port *port,
                     const RiseFallBoth *rf,
                     const MinMaxAll *min_max,
                     float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setPortExtPinCap(port, rf, min_max, cap, sdc);
}

float
port_ext_wire_cap(const Port *port,
                  const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  float pin_cap, wire_cap;
  int fanout;
  sta->portExtCaps(port, min_max, sdc, pin_cap, wire_cap, fanout);
  return wire_cap;
}

void
set_port_ext_wire_cap(const Port *port,
                      const RiseFallBoth *rf,
                      const MinMaxAll *min_max,
                      float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setPortExtWireCap(port, rf, min_max, cap, sdc);
}

void
set_port_ext_fanout_cmd(const Port *port,
                        int fanout,
                        const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setPortExtFanout(port, fanout, min_max, sdc);
}

float
port_ext_fanout(const Port *port,
                const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  float pin_cap, wire_cap;
  int fanout;
  Sta::sta()->portExtCaps(port, min_max, sdc, pin_cap, wire_cap, fanout);
  return fanout;
}

void
set_net_wire_cap(const Net *net,
                 bool subtract_pin_cap,
                 const MinMaxAll *min_max,
                 float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setNetWireCap(net, subtract_pin_cap, min_max, cap, sdc);
}

void
set_wire_load_mode_cmd(const char *mode_name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  WireloadMode mode = stringWireloadMode(mode_name);
  if (mode == WireloadMode::unknown)
    sta->report()->warn(2122, "unknown wire load mode");
  else
    sta->setWireloadMode(mode, sdc);
}

void
set_wire_load_cmd(Wireload *wireload,
                  const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setWireload(wireload, min_max, sdc);
}

void
set_wire_load_selection_group_cmd(WireloadSelection *selection,
                                  const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setWireloadSelection(selection, min_max, sdc);
}

void
set_net_resistance(Net *net,
                   const MinMaxAll *min_max,
                   float res)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setResistance(net, min_max, res, sdc);
}

void
make_clock(const char *name,
           PinSet *pins,
           bool add_to_pins,
           float period,
           FloatSeq *waveform,
           char *comment)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->makeClock(name, pins, add_to_pins, period, waveform, comment, mode);
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
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->makeGeneratedClock(name, pins, add_to_pins,
                          src_pin, master_clk,
                          divide_by, multiply_by, duty_cycle, invert,
                          combinational, edges, edge_shifts,
                          comment, mode);
}

void
remove_clock_cmd(Clock *clk)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClock(clk, sdc);
}

void
set_propagated_clock_cmd(Clock *clk)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->setPropagatedClock(clk, mode);
}

void
set_propagated_clock_pin_cmd(Pin *pin)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->setPropagatedClock(pin, mode);
}

void
unset_propagated_clock_cmd(Clock *clk)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->removePropagatedClock(clk, mode);
}

void
unset_propagated_clock_pin_cmd(Pin *pin)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  sta->removePropagatedClock(pin, mode);
}

void
set_clock_slew_cmd(Clock *clk,
                   const RiseFallBoth *rf,
                   const MinMaxAll *min_max,
                   float slew)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockSlew(clk, rf, min_max, slew, sdc);
}

void
unset_clock_slew_cmd(Clock *clk)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockSlew(clk, sdc);
}

void
set_clock_latency_cmd(Clock *clk,
                      Pin *pin,
                      const RiseFallBoth *rf,
                      MinMaxAll *min_max, float delay)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockLatency(clk, pin, rf, min_max, delay, sdc);
}

void
set_clock_insertion_cmd(Clock *clk,
                        Pin *pin,
                        const RiseFallBoth *rf,
                        const MinMaxAll *min_max,
                        const EarlyLateAll *early_late,
                        float delay)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockInsertion(clk, pin, rf, min_max, early_late, delay, sdc);
}

void
unset_clock_latency_cmd(Clock *clk,
                        Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockLatency(clk, pin, sdc);
}

void
unset_clock_insertion_cmd(Clock *clk,
                          Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockInsertion(clk, pin, sdc);
}

void
set_clock_uncertainty_clk(Clock *clk,
                          const SetupHoldAll *setup_hold,
                          float uncertainty)
{
  Sta *sta = Sta::sta();
  sta->setClockUncertainty(clk, setup_hold, uncertainty);
}

void
unset_clock_uncertainty_clk(Clock *clk,
                            const SetupHoldAll *setup_hold)
{
  Sta *sta = Sta::sta();
  sta->removeClockUncertainty(clk, setup_hold);
}

void
set_clock_uncertainty_pin(Pin *pin,
                          const MinMaxAll *min_max,
                          float uncertainty)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockUncertainty(pin, min_max, uncertainty, sdc);
}

void
unset_clock_uncertainty_pin(Pin *pin,
                            const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockUncertainty(pin, min_max, sdc);
}

void
set_inter_clock_uncertainty(Clock *from_clk,
                            const RiseFallBoth *from_tr,
                            Clock *to_clk,
                            const RiseFallBoth *to_tr,
                            const MinMaxAll *min_max,
                            float uncertainty)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max,
                           uncertainty, sdc);
}

void
unset_inter_clock_uncertainty(Clock *from_clk,
                              const RiseFallBoth *from_tr,
                              Clock *to_clk,
                              const RiseFallBoth *to_tr,
                              const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockUncertainty(from_clk, from_tr, to_clk, to_tr, min_max, sdc);
}

void
set_clock_gating_check_cmd(const RiseFallBoth *rf,
                           const SetupHold *setup_hold,
                           float margin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockGatingCheck(rf, setup_hold, margin, sdc);
}

void
set_clock_gating_check_clk_cmd(Clock *clk,
                               const RiseFallBoth *rf,
                               const SetupHold *setup_hold,
                               float margin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockGatingCheck(clk, rf, setup_hold, margin, sdc);
}

void
set_clock_gating_check_pin_cmd(Pin *pin,
                               const RiseFallBoth *rf,
                               const SetupHold *setup_hold,
                               float margin,
                               LogicValue active_value)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockGatingCheck(pin, rf, setup_hold, margin, active_value, sdc);
}

void
set_clock_gating_check_instance_cmd(Instance *inst,
                                    const RiseFallBoth *rf,
                                    const SetupHold *setup_hold,
                                    float margin,
                                    LogicValue active_value)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setClockGatingCheck(inst, rf, setup_hold, margin, active_value, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setDataCheck(from, from_rf, to, to_rf, clk, setup_hold, margin, sdc);
}

void
unset_data_check_cmd(Pin *from,
                     const RiseFallBoth *from_tr,
                     Pin *to,
                     const RiseFallBoth *to_tr,
                     Clock *clk,
                     const SetupHoldAll *setup_hold)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDataCheck(from, from_tr, to, to_tr, clk, setup_hold, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setInputDelay(pin, rf, clk, clk_rf, ref_pin,
                     source_latency_included, network_latency_included,
                     min_max, add, delay, sdc);
}

void
unset_input_delay_cmd(Pin *pin,
                      RiseFallBoth *rf, 
                      Clock *clk,
                      RiseFall *clk_rf, 
                      MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeInputDelay(pin, rf, clk, clk_rf, min_max, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setOutputDelay(pin, rf, clk, clk_rf, ref_pin,
                      source_latency_included, network_latency_included,
                      min_max, add, delay, sdc);
}

void
unset_output_delay_cmd(Pin *pin,
                       RiseFallBoth *rf, 
                       Clock *clk,
                       RiseFall *clk_rf, 
                       MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeOutputDelay(pin, rf, clk, clk_rf, min_max, sdc);
}

void
disable_cell(LibertyCell *cell,
             LibertyPort *from,
             LibertyPort *to)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(cell, from, to, sdc);
}

void
unset_disable_cell(LibertyCell *cell,
                   LibertyPort *from,
                   LibertyPort *to)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(cell, from, to, sdc);
}

void
disable_lib_port(LibertyPort *port)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(port, sdc);
}

void
unset_disable_lib_port(LibertyPort *port)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(port, sdc);
}

void
disable_port(Port *port)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(port, sdc);
}

void
unset_disable_port(Port *port)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(port, sdc);
}

void
disable_instance(Instance *instance,
                 LibertyPort *from,
                 LibertyPort *to)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(instance, from, to, sdc);
}

void
unset_disable_instance(Instance *instance,
                       LibertyPort *from,
                       LibertyPort *to)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(instance, from, to, sdc);
}

void
disable_pin(Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(pin, sdc);
}

void
unset_disable_pin(Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(pin, sdc);
}

void
disable_edge(Edge *edge)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(edge, sdc);
}

void
unset_disable_edge(Edge *edge)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(edge, sdc);
}

void
disable_timing_arc_set(TimingArcSet *arc_set)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disable(arc_set, sdc);
}

void
unset_disable_timing_arc_set(TimingArcSet *arc_set)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisable(arc_set, sdc);
}

void
disable_clock_gating_check_inst(Instance *inst)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disableClockGatingCheck(inst, sdc);
}

void
disable_clock_gating_check_pin(Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->disableClockGatingCheck(pin, sdc);
}

void
unset_disable_clock_gating_check_inst(Instance *inst)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisableClockGatingCheck(inst, sdc);
}

void
unset_disable_clock_gating_check_pin(Pin *pin)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeDisableClockGatingCheck(pin, sdc);
}

EdgeSeq
disabled_edges_sorted()
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  return sta->disabledEdgesSorted(mode);
}

bool
timing_arc_disabled(Edge *edge,
                    TimingArc *arc)
{
  Sta *sta = Sta::sta();
  const Mode *mode = sta->cmdMode();
  return !searchThru(edge, arc, mode);
}

void
make_false_path(ExceptionFrom *from,
                ExceptionThruSeq *thrus,
                ExceptionTo *to,
                const MinMaxAll *min_max,
                const char *comment)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->makeFalsePath(from, thrus, to, min_max, comment, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->makeMulticyclePath(from, thrus, to, min_max, use_end_clk,
                          path_multiplier, comment, sdc);
}

void
make_path_delay(ExceptionFrom *from,
                ExceptionThruSeq *thrus,
                ExceptionTo *to,
                const MinMax *min_max,
                bool ignore_clk_latency,
                bool break_path,
                float delay,
                const char *comment)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->makePathDelay(from, thrus, to, min_max, 
                     ignore_clk_latency, break_path,
                     delay, comment, sdc);
}

void
reset_path_cmd(ExceptionFrom *
               from, ExceptionThruSeq *thrus,
               ExceptionTo *to,
               const MinMaxAll *min_max)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->resetPath(from, thrus, to, min_max, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  if (name[0] == '\0')
    name = nullptr;
  sta->makeGroupPath(name, is_default, from, thrus, to, comment, sdc);
}

bool
is_path_group_name(const char *name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  return Sta::sta()->isPathGroupName(name, sdc);
}

ExceptionFrom *
make_exception_from(PinSet *from_pins,
                    ClockSet *from_clks,
                    InstanceSet *from_insts,
                    const RiseFallBoth *from_rf)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  return sta->makeExceptionFrom(from_pins, from_clks, from_insts,
                                from_rf, sdc);
}

void
delete_exception_from(ExceptionFrom *from)
{
  Sta *sta = Sta::sta();
  sta->deleteExceptionFrom(from);
}

void
check_exception_from_pins(ExceptionFrom *from,
                          const char *file,
                          int line)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  sta->checkExceptionFromPins(from, file, line, sdc);
}

ExceptionThru *
make_exception_thru(PinSet *pins,
                    NetSet *nets,
                    InstanceSet *insts,
                    const RiseFallBoth *rf)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  return sta->makeExceptionThru(pins, nets, insts, rf, sdc);
}

void
delete_exception_thru(ExceptionThru *thru)
{
  Sta *sta = Sta::sta();
  sta->deleteExceptionThru(thru);
}

ExceptionTo *
make_exception_to(PinSet *to_pins,
                  ClockSet *to_clks,
                  InstanceSet *to_insts,
                  const RiseFallBoth *rf,
                  RiseFallBoth *end_rf)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  return sta->makeExceptionTo(to_pins, to_clks, to_insts, rf, end_rf, sdc);
}

void
delete_exception_to(ExceptionTo *to)
{
  Sta *sta = Sta::sta();
  sta->deleteExceptionTo(to);
}

void
check_exception_to_pins(ExceptionTo *to,
                        const char *file,
                        int line)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  sta->checkExceptionToPins(to, file, line, sdc);
}

////////////////////////////////////////////////////////////////

ClockGroups *
make_clock_groups(const char *name,
                  bool logically_exclusive,
                  bool physically_exclusive,
                  bool asynchronous,
                  bool allow_paths,
                  const char *comment)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  return sta->makeClockGroups(name, logically_exclusive,
                              physically_exclusive, asynchronous,
                              allow_paths, comment, sdc);
}

void
clock_groups_make_group(ClockGroups *clk_groups,
                        ClockSet *clks)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->makeClockGroup(clk_groups, clks, sdc);
}

void
unset_clock_groups_logically_exclusive(const char *name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockGroupsLogicallyExclusive(name, sdc);
}

void
unset_clock_groups_physically_exclusive(const char *name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockGroupsPhysicallyExclusive(name, sdc);
}

void
unset_clock_groups_asynchronous(const char *name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->removeClockGroupsAsynchronous(name, sdc);
}

// Debugging function.
bool
same_clk_group(Clock *clk1,
               Clock *clk2)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
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
  Sdc *sdc = sta->cmdSdc();
  if (positive)
    sta->setClockSense(pins, clks, ClockSense::positive, sdc);
  else if (negative)
    sta->setClockSense(pins, clks, ClockSense::negative, sdc);
  else if (stop_propagation)
    sta->setClockSense(pins, clks, ClockSense::stop, sdc);
  else
    sta->report()->critical(2123, "unknown clock sense");
}

void
set_input_slew_cmd(Port *port,
                   const RiseFallBoth *rf,
                   const MinMaxAll *min_max,
                   float slew)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setInputSlew(port, rf, min_max, slew, sdc);
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
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  float from_slews[RiseFall::index_count];
  from_slews[RiseFall::riseIndex()] = from_slew_rise;
  from_slews[RiseFall::fallIndex()] = from_slew_fall;
  sta->setDriveCell(library, cell, port, from_port, from_slews,
                    to_port, rf, min_max, sdc);
}

void
set_drive_resistance_cmd(Port *port,
                         const RiseFallBoth *rf,
                         const MinMaxAll *min_max,
                         float res)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setDriveResistance(port, rf, min_max, res, sdc);
}

void
set_slew_limit_clk(Clock *clk,
                   const RiseFallBoth *rf,
                   PathClkOrData clk_data,
                   const MinMax *min_max,
                   float slew)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setSlewLimit(clk, rf, clk_data, min_max, slew, sdc);
}

void
set_slew_limit_port(Port *port,
                    const MinMax *min_max,
                    float slew)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setSlewLimit(port, min_max, slew, sdc);
}

void
set_slew_limit_cell(Cell *cell,
                    const MinMax *min_max,
                    float slew)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setSlewLimit(cell, min_max, slew, sdc);
}

void
set_port_capacitance_limit(Port *port,
                           const MinMax *min_max,
                           float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setCapacitanceLimit(port, min_max, cap, sdc);
}

void
set_pin_capacitance_limit(Pin *pin,
                          const MinMax *min_max,
                          float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setCapacitanceLimit(pin, min_max, cap, sdc);
}

void
set_cell_capacitance_limit(Cell *cell,
                           const MinMax *min_max,
                           float cap)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setCapacitanceLimit(cell, min_max, cap, sdc);
}

void
set_latch_borrow_limit_pin(Pin *pin,
                           float limit)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setLatchBorrowLimit(pin, limit, sdc);
}

void
set_latch_borrow_limit_inst(Instance *inst,
                            float limit)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setLatchBorrowLimit(inst, limit, sdc);
}

void
set_latch_borrow_limit_clk(Clock *clk, float limit)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setLatchBorrowLimit(clk, limit, sdc);
}

void
set_min_pulse_width_global(const RiseFallBoth *rf,
                           float min_width)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setMinPulseWidth(rf, min_width, sdc);
}

void
set_min_pulse_width_pin(Pin *pin,
                        const RiseFallBoth *rf,
                        float min_width)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setMinPulseWidth(pin, rf, min_width, sdc);
}

void
set_min_pulse_width_clk(Clock *clk,
                        const RiseFallBoth *rf,
                        float min_width)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setMinPulseWidth(clk, rf, min_width, sdc);
}

void
set_min_pulse_width_inst(Instance *inst,
                         const RiseFallBoth *rf,
                         float min_width)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setMinPulseWidth(inst, rf, min_width, sdc);
}

void
set_max_area_cmd(float area)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setMaxArea(area, sdc);
}

void
set_port_fanout_limit(Port *port,
                      const MinMax *min_max,
                      float fanout)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setFanoutLimit(port, min_max, fanout, sdc);
}

void
set_cell_fanout_limit(Cell *cell,
                      const MinMax *min_max,
                      float fanout)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setFanoutLimit(cell, min_max, fanout, sdc);
}

void
set_logic_value_cmd(Pin *pin,
                    LogicValue value)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  sta->setLogicValue(pin, value, mode);
}

void
set_case_analysis_cmd(Pin *pin,
                      LogicValue value)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  sta->setCaseAnalysis(pin, value, mode);
}

void
unset_case_analysis_cmd(Pin *pin)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  sta->removeCaseAnalysis(pin, mode);
}

void
set_timing_derate_cmd(TimingDerateType type,
                      PathClkOrData clk_data,
                      const RiseFallBoth *rf,
                      const EarlyLate *early_late,
                      float derate)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setTimingDerate(type, clk_data, rf, early_late, derate, sdc);
}

void
set_timing_derate_net_cmd(const Net *net,
                          PathClkOrData clk_data,
                          const RiseFallBoth *rf,
                          const EarlyLate *early_late,
                          float derate)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setTimingDerate(net, clk_data, rf, early_late, derate, sdc);
}

void
set_timing_derate_inst_cmd(const Instance *inst,
                           TimingDerateCellType type,
                           PathClkOrData clk_data,
                           const RiseFallBoth *rf,
                           const EarlyLate *early_late,
                           float derate)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setTimingDerate(inst, type, clk_data, rf, early_late, derate, sdc);
}

void
set_timing_derate_cell_cmd(const LibertyCell *cell,
                           TimingDerateCellType type,
                           PathClkOrData clk_data,
                           const RiseFallBoth *rf,
                           const EarlyLate *early_late,
                           float derate)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setTimingDerate(cell, type, clk_data, rf, early_late, derate, sdc);
}

void
unset_timing_derate_cmd()
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->unsetTimingDerate(sdc);
}

Clock *
find_clock(const char *name)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  return sdc->findClock(name);
}

bool
is_clock_src(const Pin *pin)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  return sta->isClockSrc(pin, sdc);
}

Clock *
default_arrival_clock()
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  return sdc->defaultArrivalClock();
}

ClockSeq
find_clocks_matching(const char *pattern,
                     bool regexp,
                     bool nocase)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  PatternMatch matcher(pattern, regexp, nocase, sta->tclInterp());
  return sdc->findClocksMatching(&matcher);
}

void
update_generated_clks()
{
  Sta::sta()->updateGeneratedClks();
}

bool
is_clock(Pin *pin)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  return sta->isClock(pin, mode);
}

// variable sta_clock_through_tristate_enabled
bool
clk_thru_tristate_enabled()
{
  return Sta::sta()->clkThruTristateEnabled();
}

// variable sta_clock_through_tristate_enabled
void
set_clk_thru_tristate_enabled(bool enabled)
{
  Sta::sta()->setClkThruTristateEnabled(enabled);
}

PortSeq
all_inputs_cmd(bool no_clocks)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  sta->ensureLinked();
  return sdc->allInputs(no_clocks);
}

PortSeq
all_outputs_cmd()
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  sta->ensureLinked();
  return sdc->allOutputs();
}

////////////////////////////////////////////////////////////////

// all_register variants

InstanceSet
find_register_instances(ClockSet *clks,
                        const RiseFallBoth *clk_tr,
                        bool edge_triggered,
                        bool latches)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  InstanceSet insts = sta->findRegisterInstances(clks, clk_tr,
                                                 edge_triggered,
                                                 latches, mode);
  delete clks;
  return insts;
}

PinSet
find_register_data_pins(ClockSet *clks,
                        const RiseFallBoth *clk_tr,
                        bool edge_triggered,
                        bool latches)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  PinSet pins = sta->findRegisterDataPins(clks, clk_tr, edge_triggered,
                                          latches, mode);
  delete clks;
  return pins;
}

PinSet
find_register_clk_pins(ClockSet *clks,
                       const RiseFallBoth *clk_tr,
                       bool edge_triggered,
                       bool latches)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  PinSet pins = sta->findRegisterClkPins(clks, clk_tr, edge_triggered,
                                         latches, mode);
  delete clks;
  return pins;
}

PinSet
find_register_async_pins(ClockSet *clks,
                         const RiseFallBoth *clk_tr,
                         bool edge_triggered,
                         bool latches)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  PinSet pins = sta->findRegisterAsyncPins(clks, clk_tr, edge_triggered,
                                           latches, mode);
  delete clks;
  return pins;
}

PinSet
find_register_output_pins(ClockSet *clks,
                          const RiseFallBoth *clk_tr,
                          bool edge_triggered,
                          bool latches)
{
  Sta *sta = Sta::sta();
  Mode *mode = sta->cmdMode();
  PinSet pins = sta->findRegisterOutputPins(clks, clk_tr, edge_triggered,
                                            latches, mode);
  delete clks;
  return pins;
}

////////////////////////////////////////////////////////////////

template <typename T> std::vector<T*>
filter_objects(const char *property,
               const char *op,
               const char *pattern,
               std::vector<T*> *objects)
{
  std::vector<T*> filtered_objects;
  if (objects) {
    Sta *sta = Sta::sta();
    Properties &properties = sta->properties();
    bool exact_match = stringEq(op, "==");
    bool pattern_match = stringEq(op, "=~");
    bool not_match = stringEq(op, "!=");
    bool not_pattern_match = stringEq(op, "!~");
    for (T *object : *objects) {
      PropertyValue value(properties.getProperty(object, property));
      std::string prop_str = value.to_string(sta->network());
      const char *prop = prop_str.c_str();
      if (!prop_str.empty()
          && ((exact_match && stringEq(prop, pattern))
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

StringSeq
group_path_names()
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  StringSeq pg_names;
  for (auto const& [name, group] : sdc->groupPaths())
    pg_names.push_back(name);
  return pg_names;
}

////////////////////////////////////////////////////////////////

void
set_voltage_global(const MinMax *min_max,
                   float voltage)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setVoltage(min_max, voltage, sdc);
}

void
set_voltage_net(const Net *net,
                const MinMax *min_max,
                float voltage)
{
  Sta *sta = Sta::sta();
  Sdc *sdc = sta->cmdSdc();
  sta->setVoltage(net, min_max, voltage, sdc);
}

////////////////////////////////////////////////////////////////

char
pin_case_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
  LogicValue value = LogicValue::unknown;
  bool exists;
  sdc->caseLogicValue(pin, value, exists);
  return logicValueString(value);
}

char
pin_logic_value(const Pin *pin)
{
  Sta *sta = Sta::sta();
  const Sdc *sdc = sta->cmdSdc();
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
const RiseFall *transition() { return self->transition(); }
float time() { return self->time(); }
}
