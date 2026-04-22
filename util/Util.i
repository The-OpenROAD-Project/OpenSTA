// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

%include <std_string.i>

%{

#include "Error.hh"
#include "Fuzzy.hh"
#include "Report.hh"
#include "Sta.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "Stats.hh"
#include "StringUtil.hh"
#include "Units.hh"

using namespace sta;

%}

%inline %{

const float float_inf = INF;

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

// See Sta::dispatchCallCount.
unsigned long long
dispatch_call_count()
{
  return Sta::sta()->dispatchCallCount();
}

// See Sta::arrivalVisitCount.
unsigned long long
arrival_visit_count()
{
  return Sta::sta()->arrivalVisitCount();
}

////////////////////////////////////////////////////////////////

void
report_error(int id,
             const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, "{}", msg);
}

void
report_file_error(int id,
                  const char *filename,
                  int line,
                  const char *msg)
{
  Report *report = Sta::sta()->report();
  report->error(id, filename, line, "{}", msg);
}

void
report_warn(int id,
            const char *msg)
{
  Report *report = Sta::sta()->report();
  report->warn(id, "{}", msg);
}

void
report_file_warn(int id,
                 const char *filename,
                 int line,
                 const char *msg)
{
  Report *report = Sta::sta()->report();
  report->fileWarn(id, filename, line, "{}", msg);
}

void
report_line(const char *msg)
{
  Sta *sta = Sta::sta();
  if (sta)
    sta->report()->reportLine(msg);
  else
    // After sta::delete_all_memory include -echo prints the cmd file line.
    printf("%s\n", msg);
}

void
suppress_msg_id(int id)
{
  Sta::sta()->report()->suppressMsgId(id);
}

void
unsuppress_msg_id(int id)
{
  Sta::sta()->report()->unsuppressMsgId(id);
}

int
is_suppressed(int id)
{
  return Sta::sta()->report()->isSuppressed(id);
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

std::string
unit_scale_abbreviation (const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaleAbbreviation();
  else
    return "";
}

std::string
unit_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->suffix();
  else
    return "";
}

std::string
unit_scale_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit) {
    return unit->scaleSuffix();
  }
  else
    return "";
}

std::string
unit_scale_abbrev_suffix(const char *unit_name)
{
  Unit *unit = Sta::sta()->units()->find(unit_name);
  if (unit)
    return unit->scaleAbbrevSuffix();
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
std::string
format_time(std::string value,
            int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->timeUnit()->asString(value1, digits);
}

std::string
format_capacitance(const char *value,
                   int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->capacitanceUnit()->asString(value1, digits);
}

std::string
format_resistance(const char *value,
                  int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->resistanceUnit()->asString(value1, digits);
}

std::string
format_voltage(const char *value,
               int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->voltageUnit()->asString(value1, digits);
}

std::string
format_current(const char *value,
               int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->currentUnit()->asString(value1, digits);
}

std::string
format_power(const char *value,
             int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->powerUnit()->asString(value1, digits);
}

std::string
format_distance(const char *value,
                int digits)
{
  auto [value1, valid] = stringFloat(value);
  return Sta::sta()->units()->distanceUnit()->asString(value1, digits);
}

std::string
format_area(const char *value,
            int digits)
{
  auto [value1, valid] = stringFloat(value);
  Unit *dist_unit = Sta::sta()->units()->distanceUnit();
  return dist_unit->asString(value1 / dist_unit->scale(), digits);
}

////////////////////////////////////////////////////////////////

std::string_view
rise_short_name()
{
  return RiseFall::rise()->shortName();
}

std::string_view
fall_short_name()
{
  return RiseFall::fall()->shortName();
}

////////////////////////////////////////////////////////////////

bool
fuzzy_equal(float value1,
            float value2)
{
  return fuzzyEqual(value1, value2);
}

%} // inline
