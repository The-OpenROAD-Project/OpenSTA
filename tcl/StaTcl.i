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

////////////////////////////////////////////////////////////////
//
// Define TCL methods for each network object.  This works despite the
// fact that the underlying implementation does not have class methods
// corresponding to the TCL methods defined here.
//
// Note the function name changes from sta naming convention
// (lower/capitalize) to TCL naming convention (lower/underscore).
//
////////////////////////////////////////////////////////////////

%module sta

%{
#include "Machine.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "Stats.hh"
#include "Report.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "PatternMatch.hh"
#include "MinMax.hh"
#include "Fuzzy.hh"
#include "FuncExpr.hh"
#include "Units.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "DelayCalc.hh"
#include "DcalcAnalysisPt.hh"
#include "Corner.hh"
#include "PathVertex.hh"
#include "PathRef.hh"
#include "PathExpanded.hh"
#include "PathEnd.hh"
#include "PathGroup.hh"
#include "PathAnalysisPt.hh"
#include "Property.hh"
#include "Search.hh"
#include "Sta.hh"
#include "search/Tag.hh"
#include "search/CheckTiming.hh"
#include "search/CheckMinPulseWidths.hh"
#include "search/Levelize.hh"
#include "search/ReportPath.hh"

namespace sta {

////////////////////////////////////////////////////////////////
//
// C++ helper functions used by the interface functions.
// These are not visible in the TCL API.
//
////////////////////////////////////////////////////////////////

typedef MinPulseWidthCheckSeq::Iterator MinPulseWidthCheckSeqIterator;

// Get the network for commands.
Network *
cmdNetwork()
{
  return Sta::sta()->cmdNetwork();
}

// Make sure the network has been read and linked.
// Throwing an error means the caller doesn't have to check the result.
Network *
cmdLinkedNetwork()
{
  Network *network = cmdNetwork();
  if (network->isLinked())
    return network;
  else {
    Report *report = Sta::sta()->report();
    report->error(1570, "no network has been linked.");
    return nullptr;
  }
}

// Make sure an editable network has been read and linked.
NetworkEdit *
cmdEditNetwork()
{
  Network *network = cmdLinkedNetwork();
  if (network->isEditable())
    return dynamic_cast<NetworkEdit*>(network);
  else {
    Report *report = Sta::sta()->report();
    report->error(1571, "network does not support edits.");
    return nullptr;
  }
}

// Get the graph for commands.
// Throw to cmd level on failure.
Graph *
cmdGraph()
{
  cmdLinkedNetwork();
  return Sta::sta()->ensureGraph();
}

} // namespace

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// Empty class definitions to make swig happy.
// Private constructor/destructor so swig doesn't emit them.
//
////////////////////////////////////////////////////////////////

class Transition
{
private:
  Transition();
  ~Transition();
};

class Corner
{
private:
  Corner();
  ~Corner();
};

////////////////////////////////////////////////////////////////
//
// C++ functions visible as TCL functions.
//
////////////////////////////////////////////////////////////////

%inline %{

float float_inf = INF;
int group_count_max = PathGroup::group_count_max;

const char *
version()
{
  return STA_VERSION;
}

const char *
git_sha1()
{
  return STA_GIT_SHA1;
}

// Elapsed run time (in seconds).
double
elapsed_run_time()
{
  return elapsedRunTime();
}

// User run time (in seconds).
double
user_run_time()
{
  return userRunTime();
}

// User run time (in seconds).
unsigned long
cputime()
{
  return static_cast<unsigned long>(userRunTime() + .5);
}

// Peak memory usage in bytes.
unsigned long
memory_usage()
{
  return memoryUsage();
}

int
processor_count()
{
  return processorCount();
}

int
thread_count()
{
  return Sta::sta()->threadCount();
}

void
set_thread_count(int count)
{
  Sta::sta()->setThreadCount(count);
}

////////////////////////////////////////////////////////////////

void
report_error(int id,
             const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, "%s", msg);
}

void
report_file_error(int id,
                  const char *filename,
                  int line,
                  const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, filename, line, "%s", msg);
}

void
report_warn(int id,
            const char *msg)
{
  Report *report = Sta::sta()->report();
  report->warn(id, "%s", msg);
}

void
report_file_warn(int id,
                 const char *filename,
                 int line,
                 const char *msg)
{
  Report *report = Sta::sta()->report();
  report->fileWarn(id, filename, line, "%s", msg);
}

void
report_line(const char *msg)
{
  Sta *sta = Sta::sta();
  if (sta)
    sta->report()->reportLineString(msg);
  else
    // After sta::delete_all_memory souce -echo prints the cmd file line
    printf("%s\n", msg);
}

void
fflush()
{
  fflush(stdout);
  fflush(stderr);
}

void
redirect_file_begin(const char *filename)
{
  Sta::sta()->report()->redirectFileBegin(filename);
}

void
redirect_file_append_begin(const char *filename)
{
  Sta::sta()->report()->redirectFileAppendBegin(filename);
}

void
redirect_file_end()
{
  Sta::sta()->report()->redirectFileEnd();
}

void
redirect_string_begin()
{
  Sta::sta()->report()->redirectStringBegin();
}

const char *
redirect_string_end()
{
  return Sta::sta()->report()->redirectStringEnd();
}

void
log_begin_cmd(const char *filename)
{
  Sta::sta()->report()->logBegin(filename);
}

void
log_end()
{
  Sta::sta()->report()->logEnd();
}

void
set_debug(const char *what,
	  int level)
{
  Sta::sta()->setDebugLevel(what, level);
}

////////////////////////////////////////////////////////////////

bool
is_object(const char *obj)
{
  // _hexaddress_p_type
  const char *s = obj;
  char ch = *s++;
  if (ch != '_')
    return false;
  while (*s && isxdigit(*s))
    s++;
  if ((s - obj - 1) == sizeof(void*) * 2
      && *s && *s++ == '_'
      && *s && *s++ == 'p'
      && *s && *s++ == '_') {
    while (*s && *s != ' ')
      s++;
    return *s == '\0';
  }
  else
    return false;
}

// Assumes is_object is true.
const char *
object_type(const char *obj)
{
  return &obj[1 + sizeof(void*) * 2 + 3];
}

bool
is_object_list(const char *list,
	       const char *type)
{
  const char *s = list;
  while (s) {
    bool type_match;
    const char *next;
    objectListNext(s, type, type_match, next);
    if (type_match)
      s = next;
    else
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////

// Initialize sta after delete_all_memory.
void
init_sta()
{
  initSta();
}

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

// format_unit functions print with fixed digits and suffix.
// Pass value arg as string to support NaNs.
const char *
format_time(const char *value,
	    int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->timeUnit()->asString(value1, digits);
}

const char *
format_capacitance(const char *value,
		   int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->capacitanceUnit()->asString(value1, digits);
}

const char *
format_resistance(const char *value,
		  int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->resistanceUnit()->asString(value1, digits);
}

const char *
format_voltage(const char *value,
	       int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->voltageUnit()->asString(value1, digits);
}

const char *
format_current(const char *value,
	       int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->currentUnit()->asString(value1, digits);
}

const char *
format_power(const char *value,
	     int digits)
{
  float value1 = strtof(value, nullptr);
  return Sta::sta()->units()->powerUnit()->asString(value1, digits);
}

const char *
format_distance(const char *value,
		int digits)
{
  float value1 = strtof(value, nullptr);
  Unit *dist_unit = Sta::sta()->units()->distanceUnit();
  return dist_unit->asString(value1, digits);
}

const char *
format_area(const char *value,
	    int digits)
{
  float value1 = strtof(value, nullptr);
  Unit *dist_unit = Sta::sta()->units()->distanceUnit();
  return dist_unit->asString(value1 / dist_unit->scale(), digits);
}

////////////////////////////////////////////////////////////////

// <unit>_sta_ui conversion from sta units to user interface units.
// <unit>_ui_sta conversion from user interface units to sta units.

double
time_ui_sta(double value)
{
  return Sta::sta()->units()->timeUnit()->userToSta(value);
}

double
time_sta_ui(double value)
{
  return Sta::sta()->units()->timeUnit()->staToUser(value);
}

double
capacitance_ui_sta(double value)
{
  return Sta::sta()->units()->capacitanceUnit()->userToSta(value);
}

double
capacitance_sta_ui(double value)
{
  return Sta::sta()->units()->capacitanceUnit()->staToUser(value);
}

double
resistance_ui_sta(double value)
{
  return Sta::sta()->units()->resistanceUnit()->userToSta(value);
}

double
resistance_sta_ui(double value)
{
  return Sta::sta()->units()->resistanceUnit()->staToUser(value);
}

double
voltage_ui_sta(double value)
{
  return Sta::sta()->units()->voltageUnit()->userToSta(value);
}

double
voltage_sta_ui(double value)
{
  return Sta::sta()->units()->voltageUnit()->staToUser(value);
}

double
current_ui_sta(double value)
{
  return Sta::sta()->units()->currentUnit()->userToSta(value);
}

double
current_sta_ui(double value)
{
  return Sta::sta()->units()->currentUnit()->staToUser(value);
}

double
power_ui_sta(double value)
{
  return Sta::sta()->units()->powerUnit()->userToSta(value);
}

double
power_sta_ui(double value)
{
  return Sta::sta()->units()->powerUnit()->staToUser(value);
}

double
distance_ui_sta(double value)
{
  return Sta::sta()->units()->distanceUnit()->userToSta(value);
}

double
distance_sta_ui(double value)
{
  return Sta::sta()->units()->distanceUnit()->staToUser(value);
}

double
area_ui_sta(double value)
{
  double scale = Sta::sta()->units()->distanceUnit()->scale();
  return value * scale * scale;
}

double
area_sta_ui(double value)
{
  double scale = Sta::sta()->units()->distanceUnit()->scale();
  return value / (scale * scale);
}

////////////////////////////////////////////////////////////////

void
set_cmd_unit_scale(const char *unit_name,
		   float scale)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    unit->setScale(scale);
}

void
set_cmd_unit_digits(const char *unit_name,
		    int digits)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    unit->setDigits(digits);
}

void
set_cmd_unit_suffix(const char *unit_name,
		    const char *suffix)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit) {
    unit->setSuffix(suffix);
  }
}

const char *
unit_scale_abbreviation (const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaleAbbreviation();
  else
    return "";
}

const char *
unit_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->suffix();
  else
    return "";
}

const char *
unit_scaled_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaledSuffix();
  else
    return "";
}

float
unit_scale(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scale();
  else
    return 1.0F;
}

////////////////////////////////////////////////////////////////

const char *
rise_short_name()
{
  return RiseFall::rise()->shortName();
}

const char *
fall_short_name()
{
  return RiseFall::fall()->shortName();
}

////////////////////////////////////////////////////////////////

PropertyValue
pin_property(const Pin *pin,
	     const char *property)
{
  cmdLinkedNetwork();
  return getProperty(pin, property, Sta::sta());
}

PropertyValue
instance_property(const Instance *inst,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(inst, property, Sta::sta());
}

PropertyValue
net_property(const Net *net,
	     const char *property)
{
  cmdLinkedNetwork();
  return getProperty(net, property, Sta::sta());
}

PropertyValue
port_property(const Port *port,
	      const char *property)
{
  return getProperty(port, property, Sta::sta());
}


PropertyValue
liberty_cell_property(const LibertyCell *cell,
		      const char *property)
{
  return getProperty(cell, property, Sta::sta());
}

PropertyValue
cell_property(const Cell *cell,
	      const char *property)
{
  return getProperty(cell, property, Sta::sta());
}

PropertyValue
liberty_port_property(const LibertyPort *port,
		      const char *property)
{
  return getProperty(port, property, Sta::sta());
}

PropertyValue
library_property(const Library *lib,
		 const char *property)
{
  return getProperty(lib, property, Sta::sta());
}

PropertyValue
liberty_library_property(const LibertyLibrary *lib,
			 const char *property)
{
  return getProperty(lib, property, Sta::sta());
}

PropertyValue
edge_property(Edge *edge,
	      const char *property)
{
  cmdGraph();
  return getProperty(edge, property, Sta::sta());
}

PropertyValue
clock_property(Clock *clk,
	       const char *property)
{
  cmdLinkedNetwork();
  return getProperty(clk, property, Sta::sta());
}

PropertyValue
path_end_property(PathEnd *end,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(end, property, Sta::sta());
}

PropertyValue
path_ref_property(PathRef *path,
		  const char *property)
{
  cmdLinkedNetwork();
  return getProperty(path, property, Sta::sta());
}

PropertyValue
timing_arc_set_property(TimingArcSet *arc_set,
			const char *property)
{
  cmdLinkedNetwork();
  return getProperty(arc_set, property, Sta::sta());
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
  cmdLinkedNetwork();
  return Sta::sta()->checkTiming(no_input_delay, no_output_delay,
				 reg_multiple_clks, reg_no_clks,
				 unconstrained_endpoints,
				 loops, generated_clks);
}

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
  return Sta::sta()->setUseDefaultArrivalClock(enable);
}

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

////////////////////////////////////////////////////////////////

PathEndSeq
find_path_ends(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       bool unconstrained,
	       Corner *corner,
	       const MinMaxAll *delay_min_max,
	       int group_count,
	       int endpoint_count,
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
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  PathEndSeq ends = sta->findPathEnds(from, thrus, to, unconstrained,
                                      corner, delay_min_max,
                                      group_count, endpoint_count, unique_pins,
                                      slack_min, slack_max,
                                      sort_by_slack,
                                      groups->size() ? groups : nullptr,
                                      setup, hold,
                                      recovery, removal,
                                      clk_gating_setup, clk_gating_hold);
  delete groups;
  return ends;
}

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
		 PathEnd *prev_end)
{
  Sta::sta()->reportPathEnd(end, prev_end);
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
		       bool report_net,
		       bool report_cap,
		       bool report_slew,
                       bool report_fanout)
{
  Sta::sta()->setReportPathFields(report_input_pin,
				  report_net,
				  report_cap,
				  report_slew,
                                  report_fanout);
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
    sta->report()->error(1575, "unknown report path field %s", field_name);
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
    sta->report()->error(1576, "unknown report path field %s", field_name);
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
delete_path_ref(PathRef *path)
{
  delete path;
}

void
report_path_cmd(PathRef *path)
{
  Sta::sta()->reportPath(path);
}

////////////////////////////////////////////////////////////////

void
report_clk_skew(ConstClockSeq clks,
		const Corner *corner,
		const SetupHold *setup_hold,
                bool include_internal_latency,
		int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkSkew(clks, corner, setup_hold,
                            include_internal_latency, digits);
}

void
report_clk_latency(ConstClockSeq clks,
                   const Corner *corner,
                   bool include_internal_latency,
                   int digits)
{
  cmdLinkedNetwork();
  Sta::sta()->reportClkLatency(clks, corner, include_internal_latency, digits);
}

float
worst_clk_skew_cmd(const SetupHold *setup_hold,
                   bool include_internal_latency)
{
  cmdLinkedNetwork();
  return Sta::sta()->findWorstClkSkew(setup_hold, include_internal_latency);
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

MinPulseWidthCheckSeq &
min_pulse_width_violations(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthViolations(corner);
}

MinPulseWidthCheckSeq &
min_pulse_width_check_pins(PinSeq *pins,
			   const Corner *corner)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  MinPulseWidthCheckSeq &checks = sta->minPulseWidthChecks(pins, corner);
  delete pins;
  return checks;
}

MinPulseWidthCheckSeq &
min_pulse_width_checks(const Corner *corner)
{
  cmdLinkedNetwork();
  return Sta::sta()->minPulseWidthChecks(corner);
}

MinPulseWidthCheck *
min_pulse_width_check_slack(const Corner *corner)
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  return Sta::sta()->minPeriodViolations();
}

MinPeriodCheck *
min_period_check_slack()
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  return Sta::sta()->maxSkewViolations();
}

MaxSkewCheck *
max_skew_check_slack()
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  return sta->findClkMinPeriod(clk, ignore_port_paths);
}

////////////////////////////////////////////////////////////////

PinSeq
check_slew_limits(Net *net,
                  bool violators,
                  const Corner *corner,
                  const MinMax *min_max)
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(net, violators, corner, min_max);
}

size_t
max_slew_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkSlewLimits(nullptr, true, nullptr, MinMax::max()).size();
}

float
max_slew_check_slack()
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(net, violators, min_max);
}

size_t
max_fanout_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkFanoutLimits(nullptr, true, MinMax::max()).size();
}

float
max_fanout_check_slack()
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(net, violators, corner, min_max);
}

size_t
max_capacitance_violation_count()
{
  cmdLinkedNetwork();
  return Sta::sta()->checkCapacitanceLimits(nullptr, true,nullptr,MinMax::max()).size();
}

float
max_capacitance_check_slack()
{
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
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

bool
fuzzy_equal(float value1,
	    float value2)
{
  return fuzzyEqual(value1, value2);
}

char
pin_sim_logic_value(const Pin *pin)
{
  return logicValueString(Sta::sta()->simLogicValue(pin));
}

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

InstanceSeq
slow_drivers(int count)
{
  return Sta::sta()->slowDrivers(count);
}

bool
timing_role_is_check(TimingRole *role)
{
  return role->isTimingCheck();
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
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
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
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  InstanceSet fanout = sta->findFanoutInstances(from, flat, endpoints_only,
                                                inst_levels, pin_levels,
                                                thru_disabled, thru_constants);
  delete from;
  return fanout;
}

////////////////////////////////////////////////////////////////

void
report_loops()
{
  Sta *sta = Sta::sta();
  Network *network = cmdLinkedNetwork();
  Graph *graph = cmdGraph();
  Report *report = sta->report();
  for (GraphLoop *loop : *sta->graphLoops()) {
    loop->report(report, network, graph);
    report->reportLineString("");
  }
}

%} // inline

////////////////////////////////////////////////////////////////
//
// Object Methods
//
////////////////////////////////////////////////////////////////

%extend Corner {
const char *name() { return self->name(); }
}

// Local Variables:
// mode:c++
// End:
