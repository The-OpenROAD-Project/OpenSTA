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
#include "Fuzzy.hh"
#include "Units.hh"
#include "Property.hh"
#include "Sta.hh"

namespace sta {

////////////////////////////////////////////////////////////////
//
// C++ helper functions used by the interface functions.
// These are not visible in the TCL API.
//
////////////////////////////////////////////////////////////////

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
// C++ functions visible as TCL functions.
//
////////////////////////////////////////////////////////////////

%inline %{

float float_inf = INF;

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
//
// Units
//
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
//
// Properties
//
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

bool
fuzzy_equal(float value1,
	    float value2)
{
  return fuzzyEqual(value1, value2);
}

%} // inline

// Local Variables:
// mode:c++
// End:
