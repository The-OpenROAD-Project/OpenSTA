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

%module search

%{

#include "Units.hh"
#include "PathGroup.hh"
#include "Search.hh"
#include "search/Levelize.hh"
#include "search/ReportPath.hh"
#include "PathExpanded.hh"
#include "Bfs.hh"
#include "Sta.hh"

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class VertexPathIterator
{
private:
  VertexPathIterator();
  ~VertexPathIterator();
};

class Path
{
private:
  Path();
  ~Path();
};

class PathEnd
{
private:
  PathEnd();
  ~PathEnd();
};

class MinPulseWidthCheck
{
private:
  MinPulseWidthCheck();
  ~MinPulseWidthCheck();
};

class MinPulseWidthCheckSeq
{
private:
  MinPulseWidthCheckSeq();
  ~MinPulseWidthCheckSeq();
};

class MinPulseWidthCheckSeqIterator
{
private:
  MinPulseWidthCheckSeqIterator();
  ~MinPulseWidthCheckSeqIterator();
};

class Corner
{
private:
  Corner();
  ~Corner();
};

%inline %{

int group_path_count_max = PathGroup::group_path_count_max;

////////////////////////////////////////////////////////////////

// Initialize sta after delete_all_memory.
void
init_sta()
{
  initSta();
}

// Clear all state except network.
void
clear_sta()
{
  Sta::sta()->clear();
}

void
make_sta(Tcl_Interp *interp)
{
  Sta *sta = new Sta;
  Sta::setSta(sta);
  sta->makeComponents();
  sta->setTclInterp(interp);
}

Tcl_Interp *
tcl_interp()
{
  return Sta::sta()->tclInterp();
}

void
clear_network()
{
  Sta *sta = Sta::sta();
  sta->network()->clear();
}

void
delete_all_memory()
{
  deleteAllMemory();
}

////////////////////////////////////////////////////////////////

void
find_timing_cmd(bool full)
{
  Sta::sta()->updateTiming(full);
}

void
arrivals_invalid()
{
  Sta *sta = Sta::sta();
  sta->arrivalsInvalid();
}

PinSet
startpoints()
{
  return Sta::sta()->startpointPins();
}

PinSet
endpoints()
{
  return Sta::sta()->endpointPins();
}

size_t
endpoint_path_count()
{
  return Sta::sta()->endpointPins().size();
}

void
find_requireds()
{
  Sta::sta()->findRequireds();
}

Slack
total_negative_slack_cmd(const MinMax *min_max)
{
  return Sta::sta()->totalNegativeSlack(min_max);
}

Slack
total_negative_slack_corner_cmd(const Corner *corner,
				const MinMax *min_max)
{
  return Sta::sta()->totalNegativeSlack(corner, min_max);
}

Slack
worst_slack_cmd(const MinMax *min_max)
{
  Slack worst_slack;
  Vertex *worst_vertex;
  Sta::sta()->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_slack;
}

Vertex *
worst_slack_vertex(const MinMax *min_max)
{
  Slack worst_slack;
  Vertex *worst_vertex;
  Sta::sta()->worstSlack(min_max, worst_slack, worst_vertex);
  return worst_vertex;;
}

Slack
worst_slack_corner(const Corner *corner,
		   const MinMax *min_max)
{
  Slack worst_slack;
  Vertex *worst_vertex;
  Sta::sta()->worstSlack(corner, min_max, worst_slack, worst_vertex);
  return worst_slack;
}

Path *
vertex_worst_arrival_path(Vertex *vertex,
			  const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->ensureLibLinked();
  return sta->vertexWorstArrivalPath(vertex, min_max);
}

Path *
vertex_worst_arrival_path_rf(Vertex *vertex,
			     const RiseFall *rf,
			     MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->ensureLibLinked();
  return sta->vertexWorstArrivalPath(vertex, rf, min_max);
}

Path *
vertex_worst_slack_path(Vertex *vertex,
			const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->ensureLibLinked();
  return sta->vertexWorstSlackPath(vertex, min_max);
}

int
tag_group_path_count()
{
  return Sta::sta()->tagGroupCount();
}

void
report_tag_groups()
{
  Sta::sta()->search()->reportTagGroups();
}

void
report_tag_arrivals_cmd(Vertex *vertex,
			bool report_tag_index)
{
  Sta::sta()->search()->reportArrivals(vertex, report_tag_index);
}

void
report_path_count_histogram()
{
  Sta::sta()->search()->reportPathCountHistogram();
}

int
tag_count()
{
  return Sta::sta()->tagCount();
}

void
report_tags()
{
  Sta::sta()->search()->reportTags();
}

void
report_clk_infos()
{
  Sta::sta()->search()->reportClkInfos();
}

int
clk_info_count()
{
  return Sta::sta()->clkInfoCount();
}

int
path_count()
{
  return Sta::sta()->pathCount();
}

int
endpoint_violation_count(const MinMax *min_max)
{
  return  Sta::sta()->endpointViolationCount(min_max);
}

void
report_loops()
{
  Sta *sta = Sta::sta();
  Report *report = sta->report();
  for (GraphLoop *loop : sta->graphLoops()) {
    loop->report(sta);
    report->reportLineString("");
  }
}

char
pin_sim_logic_value(const Pin *pin)
{
  return logicValueString(Sta::sta()->simLogicValue(pin));
}

InstanceSeq
slow_drivers(int count)
{
  return Sta::sta()->slowDrivers(count);
}

////////////////////////////////////////////////////////////////

PathEndSeq
find_path_ends(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       bool unconstrained,
	       Corner *corner,
	       const MinMaxAll *delay_min_max,
	       int group_path_count,
	       int endpoint_path_count,
	       bool unique_pins,
	       float slack_min,
	       float slack_max,
	       bool sort_by_slack,
	       PathGroupNameSet *groups,
	       bool setup,
	       bool hold,
	       bool recovery,
	       bool removal,
	       bool clk_gating_setup,
	       bool clk_gating_hold)
{
  Sta *sta = Sta::sta();
  PathEndSeq ends = sta->findPathEnds(from, thrus, to, unconstrained,
                                      corner, delay_min_max,
                                      group_path_count, endpoint_path_count,
				      unique_pins,
                                      slack_min, slack_max,
                                      sort_by_slack,
                                      groups->size() ? groups : nullptr,
                                      setup, hold,
                                      recovery, removal,
                                      clk_gating_setup, clk_gating_hold);
  delete groups;
  return ends;
}

////////////////////////////////////////////////////////////////

void
report_path_end_header()
{
  Sta::sta()->reportPathEndHeader();
}

void
report_path_end_footer()
{
  Sta::sta()->reportPathEndFooter();
}

void
report_path_end(PathEnd *end)
{
  Sta::sta()->reportPathEnd(end);
}

void
report_path_end2(PathEnd *end,
		 PathEnd *prev_end,
                 bool last)
{
  Sta::sta()->reportPathEnd(end, prev_end, last);
}

void
set_report_path_format(ReportPathFormat format)
{
  Sta::sta()->setReportPathFormat(format);
}
    
void
set_report_path_field_order(StringSeq *field_names)
{
  Sta::sta()->setReportPathFieldOrder(field_names);
  delete field_names;
}

void
set_report_path_fields(bool report_input_pin,
                       bool report_hier_pins,
		       bool report_net,
		       bool report_cap,
		       bool report_slew,
		       bool report_fanout,
		       bool report_src_attr)
{
  Sta::sta()->setReportPathFields(report_input_pin,
                                  report_hier_pins,
				  report_net,
				  report_cap,
				  report_slew,
				  report_fanout,
				  report_src_attr);
}

void
set_report_path_field_properties(const char *field_name,
				 const char *title,
				 int width,
				 bool left_justify)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setProperties(title, width, left_justify);
  else
    sta->report()->warn(1575, "unknown report path field %s", field_name);
}

void
set_report_path_field_width(const char *field_name,
			    int width)
{
  Sta *sta = Sta::sta();
  ReportField *field = sta->findReportPathField(field_name);
  if (field)
    field->setWidth(width);
  else
    sta->report()->warn(1576, "unknown report path field %s", field_name);
}

void
set_report_path_digits(int digits)
{
  Sta::sta()->setReportPathDigits(digits);
}

void
set_report_path_no_split(bool no_split)
{
  Sta::sta()->setReportPathNoSplit(no_split);
}

void
set_report_path_sigmas(bool report_sigmas)
{
  Sta::sta()->setReportPathSigmas(report_sigmas);
}

void
report_path_cmd(Path *path)
{
  Sta::sta()->reportPath(path);
}

void
report_path_ends(PathEndSeq *ends)
{
  Sta::sta()->reportPathEnds(ends);
  delete ends;
}

////////////////////////////////////////////////////////////////

void
report_clk_skew(ConstClockSeq clks,
		const Corner *corner,
		const SetupHold *setup_hold,
                bool include_internal_latency,
		int digits)
{
  Sta::sta()->reportClkSkew(clks, corner, setup_hold,
                            include_internal_latency, digits);
}

void
report_clk_latency(ConstClockSeq clks,
                   const Corner *corner,
                   bool include_internal_latency,
                   int digits)
{
  Sta::sta()->reportClkLatency(clks, corner, include_internal_latency, digits);
}

float
worst_clk_skew_cmd(const SetupHold *setup_hold,
                   bool include_internal_latency)
{
  return Sta::sta()->findWorstClkSkew(setup_hold, include_internal_latency);
}

////////////////////////////////////////////////////////////////

MinPulseWidthCheckSeq &
min_pulse_width_violations(const Corner *corner)
{
  return Sta::sta()->minPulseWidthViolations(corner);
}

MinPulseWidthCheckSeq &
min_pulse_width_check_pins(PinSeq *pins,
			   const Corner *corner)
{
  Sta *sta = Sta::sta();
  MinPulseWidthCheckSeq &checks = sta->minPulseWidthChecks(pins, corner);
  delete pins;
  return checks;
}

MinPulseWidthCheckSeq &
min_pulse_width_checks(const Corner *corner)
{
  return Sta::sta()->minPulseWidthChecks(corner);
}

MinPulseWidthCheck *
min_pulse_width_check_slack(const Corner *corner)
{
  return Sta::sta()->minPulseWidthSlack(corner);
}

void
report_mpw_checks(MinPulseWidthCheckSeq *checks,
		  bool verbose)
{
  Sta::sta()->reportMpwChecks(checks, verbose);
}

void
report_mpw_check(MinPulseWidthCheck *check,
		 bool verbose)
{
  Sta::sta()->reportMpwCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MinPeriodCheckSeq &
min_period_violations()
{
  return Sta::sta()->minPeriodViolations();
}

MinPeriodCheck *
min_period_check_slack()
{
  return Sta::sta()->minPeriodSlack();
}

void
report_min_period_checks(MinPeriodCheckSeq *checks,
			 bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_min_period_check(MinPeriodCheck *check,
			bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

MaxSkewCheckSeq &
max_skew_violations()
{
  return Sta::sta()->maxSkewViolations();
}

MaxSkewCheck *
max_skew_check_slack()
{
  return Sta::sta()->maxSkewSlack();
}

void
report_max_skew_checks(MaxSkewCheckSeq *checks,
		       bool verbose)
{
  Sta::sta()->reportChecks(checks, verbose);
}

void
report_max_skew_check(MaxSkewCheck *check,
		      bool verbose)
{
  Sta::sta()->reportCheck(check, verbose);
}

////////////////////////////////////////////////////////////////

Slack
find_clk_min_period(const Clock *clk,
                    bool ignore_port_paths)
{
  return Sta::sta()->findClkMinPeriod(clk, ignore_port_paths);
}

////////////////////////////////////////////////////////////////

PinSeq
check_slew_limits(Net *net,
                  bool violators,
                  const Corner *corner,
                  const MinMax *min_max)
{
  return Sta::sta()->checkSlewLimits(net, violators, corner, min_max);
}

size_t
max_slew_violation_count()
{
  return Sta::sta()->checkSlewLimits(nullptr, true, nullptr, MinMax::max()).size();
}

float
max_slew_check_slack()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(slack);
}

float
max_slew_check_limit()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  Slew slew;
  float slack;
  float limit;
  sta->maxSlewCheck(pin, slew, slack, limit);
  return sta->units()->timeUnit()->staToUser(limit);
}

void
report_slew_limit_short_header()
{
  Sta::sta()->reportSlewLimitShortHeader();
}

void
report_slew_limit_short(Pin *pin,
			const Corner *corner,
			const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitShort(pin, corner, min_max);
}

void
report_slew_limit_verbose(Pin *pin,
			  const Corner *corner,
			  const MinMax *min_max)
{
  Sta::sta()->reportSlewLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_fanout_limits(Net *net,
                    bool violators,
                    const MinMax *min_max)
{
  return Sta::sta()->checkFanoutLimits(net, violators, min_max);
}

size_t
max_fanout_violation_count()
{
  return Sta::sta()->checkFanoutLimits(nullptr, true, MinMax::max()).size();
}

float
max_fanout_check_slack()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return slack;;
}

float
max_fanout_check_limit()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  float fanout;
  float slack;
  float limit;
  sta->maxFanoutCheck(pin, fanout, slack, limit);
  return limit;;
}

void
report_fanout_limit_short_header()
{
  Sta::sta()->reportFanoutLimitShortHeader();
}

void
report_fanout_limit_short(Pin *pin,
			  const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitShort(pin, min_max);
}

void
report_fanout_limit_verbose(Pin *pin,
			    const MinMax *min_max)
{
  Sta::sta()->reportFanoutLimitVerbose(pin, min_max);
}

////////////////////////////////////////////////////////////////

PinSeq
check_capacitance_limits(Net *net,
                         bool violators,
                         const Corner *corner,
                         const MinMax *min_max)
{
  return Sta::sta()->checkCapacitanceLimits(net, violators, corner, min_max);
}

size_t
max_capacitance_violation_count()
{
  return Sta::sta()->checkCapacitanceLimits(nullptr, true,nullptr,MinMax::max()).size();
}

float
max_capacitance_check_slack()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(slack);
}

float
max_capacitance_check_limit()
{
  Sta *sta = Sta::sta();
  const Pin *pin;
  float capacitance;
  float slack;
  float limit;
  sta->maxCapacitanceCheck(pin, capacitance, slack, limit);
  return sta->units()->capacitanceUnit()->staToUser(limit);
}

void
report_capacitance_limit_short_header()
{
  Sta::sta()->reportCapacitanceLimitShortHeader();
}

void
report_capacitance_limit_short(Pin *pin,
			       const Corner *corner,
			       const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitShort(pin, corner, min_max);
}

void
report_capacitance_limit_verbose(Pin *pin,
				 const Corner *corner,
				 const MinMax *min_max)
{
  Sta::sta()->reportCapacitanceLimitVerbose(pin, corner, min_max);
}

////////////////////////////////////////////////////////////////

void
write_timing_model_cmd(const char *lib_name,
                       const char *cell_name,
                       const char *filename,
                       const Corner *corner)
{
  Sta::sta()->writeTimingModel(lib_name, cell_name, filename, corner);
}

////////////////////////////////////////////////////////////////

void
define_corners_cmd(StringSet *corner_names)
{
  Sta *sta = Sta::sta();
  sta->makeCorners(corner_names);
  delete corner_names;
}

Corner *
cmd_corner()
{
  return Sta::sta()->cmdCorner();
}

void
set_cmd_corner(Corner *corner)
{
  Sta::sta()->setCmdCorner(corner);
}

Corner *
find_corner(const char *corner_name)
{
  return Sta::sta()->findCorner(corner_name);
}

Corners *
corners()
{
  return Sta::sta()->corners();
}

bool
multi_corner()
{
  return Sta::sta()->multiCorner();
}

////////////////////////////////////////////////////////////////

CheckErrorSeq &
check_timing_cmd(bool no_input_delay,
		 bool no_output_delay,
		 bool reg_multiple_clks,
		 bool reg_no_clks,
		 bool unconstrained_endpoints,
		 bool loops,
		 bool generated_clks)
{
  return Sta::sta()->checkTiming(no_input_delay, no_output_delay,
				 reg_multiple_clks, reg_no_clks,
				 unconstrained_endpoints,
				 loops, generated_clks);
}

////////////////////////////////////////////////////////////////

PinSet
find_fanin_pins(PinSeq *to,
		bool flat,
		bool startpoints_only,
		int inst_levels,
		int pin_levels,
		bool thru_disabled,
		bool thru_constants)
{
  Sta *sta = Sta::sta();
  PinSet fanin = sta->findFaninPins(to, flat, startpoints_only,
                                    inst_levels, pin_levels,
                                    thru_disabled, thru_constants);
  delete to;
  return fanin;
}

InstanceSet
find_fanin_insts(PinSeq *to,
		 bool flat,
		 bool startpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  Sta *sta = Sta::sta();
  InstanceSet fanin = sta->findFaninInstances(to, flat, startpoints_only,
                                              inst_levels, pin_levels,
                                              thru_disabled, thru_constants);
  delete to;
  return fanin;
}

PinSet
find_fanout_pins(PinSeq *from,
		 bool flat,
		 bool endpoints_only,
		 int inst_levels,
		 int pin_levels,
		 bool thru_disabled,
		 bool thru_constants)
{
  Sta *sta = Sta::sta();
  PinSet fanout = sta->findFanoutPins(from, flat, endpoints_only,
                                      inst_levels, pin_levels,
                                      thru_disabled, thru_constants);
  delete from;
  return fanout;
}

InstanceSet
find_fanout_insts(PinSeq *from,
		  bool flat,
		  bool endpoints_only,
		  int inst_levels,
		  int pin_levels,
		  bool thru_disabled,
		  bool thru_constants)
{
  Sta *sta = Sta::sta();
  InstanceSet fanout = sta->findFanoutInstances(from, flat, endpoints_only,
                                                inst_levels, pin_levels,
                                                thru_disabled, thru_constants);
  delete from;
  return fanout;
}

////////////////////////////////////////////////////////////////
//
// Variables
//
////////////////////////////////////////////////////////////////

bool
crpr_enabled()
{
  return Sta::sta()->crprEnabled();
}

void
set_crpr_enabled(bool enabled)
{
  return Sta::sta()->setCrprEnabled(enabled);
}

const char *
crpr_mode()
{
  switch (Sta::sta()->crprMode()) {
  case CrprMode::same_transition:
    return "same_transition";
  case CrprMode::same_pin:
    return "same_pin";
  default:
    return "";
  }
}

void
set_crpr_mode(const char *mode)
{
  Sta *sta = Sta::sta();
  if (stringEq(mode, "same_pin"))
    Sta::sta()->setCrprMode(CrprMode::same_pin);
  else if (stringEq(mode, "same_transition"))
    Sta::sta()->setCrprMode(CrprMode::same_transition);
  else
    sta->report()->critical(1573, "unknown common clk pessimism mode.");
}

bool
pocv_enabled()
{
  return Sta::sta()->pocvEnabled();
}

void
set_pocv_enabled(bool enabled)
{
#if !SSTA
  if (enabled)
    Sta::sta()->report()->error(1574, "POCV support requires compilation with SSTA=1.");
#endif
  return Sta::sta()->setPocvEnabled(enabled);
}

float
pocv_sigma_factor()
{
  return Sta::sta()->sigmaFactor();
}

void
set_pocv_sigma_factor(float factor)
{
  Sta::sta()->setSigmaFactor(factor);
}

bool
propagate_gated_clock_enable()
{
  return Sta::sta()->propagateGatedClockEnable();
}

void
set_propagate_gated_clock_enable(bool enable)
{
  Sta::sta()->setPropagateGatedClockEnable(enable);
}

bool
preset_clr_arcs_enabled()
{
  return Sta::sta()->presetClrArcsEnabled();
}

void
set_preset_clr_arcs_enabled(bool enable)
{
  Sta::sta()->setPresetClrArcsEnabled(enable);
}

bool
cond_default_arcs_enabled()
{
  return Sta::sta()->condDefaultArcsEnabled();
}

void
set_cond_default_arcs_enabled(bool enabled)
{
  Sta::sta()->setCondDefaultArcsEnabled(enabled);
}

bool
bidirect_inst_paths_enabled()
{
  return Sta::sta()->bidirectInstPathsEnabled();
}

void
set_bidirect_inst_paths_enabled(bool enabled)
{
  Sta::sta()->setBidirectInstPathsEnabled(enabled);
}

bool
bidirect_net_paths_enabled()
{
  return Sta::sta()->bidirectNetPathsEnabled();
}

void
set_bidirect_net_paths_enabled(bool enabled)
{
  Sta::sta()->setBidirectNetPathsEnabled(enabled);
}

bool
recovery_removal_checks_enabled()
{
  return Sta::sta()->recoveryRemovalChecksEnabled();
}

void
set_recovery_removal_checks_enabled(bool enabled)
{
  Sta::sta()->setRecoveryRemovalChecksEnabled(enabled);
}

bool
gated_clk_checks_enabled()
{
  return Sta::sta()->gatedClkChecksEnabled();
}

void
set_gated_clk_checks_enabled(bool enabled)
{
  Sta::sta()->setGatedClkChecksEnabled(enabled);
}

bool
dynamic_loop_breaking()
{
  return Sta::sta()->dynamicLoopBreaking();
}

void
set_dynamic_loop_breaking(bool enable)
{
  Sta::sta()->setDynamicLoopBreaking(enable);
}

bool
use_default_arrival_clock()
{
  return Sta::sta()->useDefaultArrivalClock();
}

void
set_use_default_arrival_clock(bool enable)
{
  Sta::sta()->setUseDefaultArrivalClock(enable);
}

// For regression tests.
void
report_arrival_entries()
{
  Sta *sta = Sta::sta();
  Search *search = sta->search();
  search->arrivalIterator()->reportEntries();
}

// For regression tests.
void
report_required_entries()
{
  Sta *sta = Sta::sta();
  Search *search = sta->search();
  search->requiredIterator()->reportEntries();
}

// For regression tests.
void
levelize()
{
  Sta *sta = Sta::sta();
  sta->levelize()->levelize();
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend PathEnd {
bool is_unconstrained() { return self->isUnconstrained(); }
bool is_check() { return self->isCheck(); }
bool is_latch_check() { return self->isLatchCheck(); }
bool is_data_check() { return self->isDataCheck(); }
bool is_output_delay() { return self->isOutputDelay(); }
bool is_path_delay() { return self->isPathDelay(); }
bool is_gated_clock() { return self->isGatedClock(); }
Pin *pin() { return self->vertex(Sta::sta())->pin(); }
Vertex *vertex() { return self->vertex(Sta::sta()); }
Path *path() { return self->path(); }
RiseFall *end_transition()
{ return const_cast<RiseFall*>(self->path()->transition(Sta::sta())); }
Slack slack() { return self->slack(Sta::sta()); }
ArcDelay margin() { return self->margin(Sta::sta()); }
Required data_required_time() { return self->requiredTimeOffset(Sta::sta()); }
Arrival data_arrival_time() { return self->dataArrivalTimeOffset(Sta::sta()); }
const TimingRole *check_role() { return self->checkRole(Sta::sta()); }
MinMax *min_max() { return const_cast<MinMax*>(self->minMax(Sta::sta())); }
float source_clk_offset() { return self->sourceClkOffset(Sta::sta()); }
Arrival source_clk_latency() { return self->sourceClkLatency(Sta::sta()); }
Arrival source_clk_insertion_delay()
{ return self->sourceClkInsertionDelay(Sta::sta()); }
const Clock *target_clk() { return self->targetClk(Sta::sta()); }
const ClockEdge *target_clk_edge() { return self->targetClkEdge(Sta::sta()); }
Path *target_clk_path() { return self->targetClkPath(); }
float target_clk_time() { return self->targetClkTime(Sta::sta()); }
float target_clk_offset() { return self->targetClkOffset(Sta::sta()); }
float target_clk_mcp_adjustment()
{ return self->targetClkMcpAdjustment(Sta::sta()); }
Arrival target_clk_delay() { return self->targetClkDelay(Sta::sta()); }
Arrival target_clk_insertion_delay()
{ return self->targetClkInsertionDelay(Sta::sta()); }
float target_clk_uncertainty()
{ return self->targetNonInterClkUncertainty(Sta::sta()); }
float inter_clk_uncertainty()
{ return self->interClkUncertainty(Sta::sta()); }
Arrival target_clk_arrival() { return self->targetClkArrival(Sta::sta()); }
bool path_delay_margin_is_external()
{ return self->pathDelayMarginIsExternal();}
Crpr check_crpr() { return self->checkCrpr(Sta::sta()); }
RiseFall *target_clk_end_trans()
{ return const_cast<RiseFall*>(self->targetClkEndTrans(Sta::sta())); }
Delay clk_skew() { return self->clkSkew(Sta::sta()); }

}

%extend Path {
float
arrival()
{
  return delayAsFloat(self->arrival());
}

float
required()
{
  return delayAsFloat(self->required());
}

float
slack()
{
  Sta *sta = Sta::sta();
  return delayAsFloat(self->slack(sta));
}

const Pin *
pin()
{
  Sta *sta = Sta::sta();
  return self->pin(sta);
}

const RiseFall *
edge()
{
  return self->transition(Sta::sta());
}

string
tag()
{
  Sta *sta = Sta::sta();
  return self->tag(sta)->to_string(sta);
}

// mea_opt3
PinSeq
pins()
{
  Sta *sta = Sta::sta();
  PinSeq pins;
  Path *path1 = self;
  while (path1) {
    pins.push_back(path1->vertex(sta)->pin());
    path1 = path1->prevPath();
  }
  return pins;
}

const Path *
start_path()
{
  PathExpanded expanded(self, Sta::sta());
  return expanded.startPath();
}

}

%extend VertexPathIterator {
bool has_next() { return self->hasNext(); }
Path *
next()
{
  return self->next();
}

void finish() { delete self; }
}

%extend MinPulseWidthCheckSeqIterator {
bool has_next() { return self->hasNext(); }
MinPulseWidthCheck *next() { return self->next(); }
void finish() { delete self; }
} // MinPulseWidthCheckSeqIterator methods

%extend Corner {
const char *name() { return self->name(); }
}
