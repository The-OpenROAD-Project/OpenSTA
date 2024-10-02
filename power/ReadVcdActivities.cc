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

#include "ReadVcdActivities.hh"

#include <inttypes.h>
#include <set>

#include "VcdReader.hh"
#include "Debug.hh"
#include "Network.hh"
#include "VerilogNamespace.hh"
#include "ParseBus.hh"
#include "Sdc.hh"
#include "Power.hh"
#include "Sta.hh"

namespace sta {

using std::abs;
using std::min;
using std::to_string;

class ReadVcdActivities : public StaState
{
public:
  ReadVcdActivities(const char *filename,
                    const char *scope,
                    Sta *sta);
  void readActivities();

private:
  void setActivities();
  void setVarActivity(VcdVar *var,
                      string &var_name,
                      const VcdValues &var_value);
  void setVarActivity(const char *pin_name,
                      const VcdValues &var_values,
                      int value_bit);
  void findVarActivity(const VcdValues &var_values,
                       int value_bit,
                       // Return values.
                       double &transition_count,
                       double &activity,
                       double &duty);
  void checkClkPeriod(const Pin *pin,
                      double transition_count);

  const char *filename_;
  const char *scope_;
  Vcd vcd_;
  double clk_period_;
  Sta *sta_;
  Power *power_;
  std::set<const Pin*> annotated_pins_;

  static constexpr double sim_clk_period_tolerance_ = .1;
};

void
readVcdActivities(const char *filename,
                  const char *scope,
                  Sta *sta)
{
  ReadVcdActivities reader(filename, scope, sta);
  reader.readActivities();
}

ReadVcdActivities::ReadVcdActivities(const char *filename,
                                     const char *scope,
                                     Sta *sta) :
  StaState(sta),
  filename_(filename),
  scope_(scope),
  vcd_(sta),
  clk_period_(0.0),
  sta_(sta),
  power_(sta->power())
{
}

void
ReadVcdActivities::readActivities()
{
  vcd_ = readVcdFile(filename_, sta_);

  clk_period_ = INF;
  for (Clock *clk : *sta_->sdc()->clocks())
    clk_period_ = min(static_cast<double>(clk->period()), clk_period_);

  if (vcd_.timeMax() > 0)
    setActivities();
  else
    report_->warn(1450, "VCD max time is zero.");
  report_->reportLine("Annotated %zu pin activities.", annotated_pins_.size());
}

void
ReadVcdActivities::setActivities()
{
  size_t scope_length = strlen(scope_);
  for (VcdVar *var : vcd_.vars()) {
    const VcdValues &var_values = vcd_.values(var);
    if (!var_values.empty()
        && (var->type() == VcdVarType::wire
            || var->type() == VcdVarType::reg)) {
      string var_name = var->name();
      // string::starts_with in c++20
      if (scope_length) {
        if (var_name.substr(0, scope_length) == scope_) {
          var_name = var_name.substr(scope_length + 1);
          setVarActivity(var, var_name, var_values);
        }
      }
      else
        setVarActivity(var, var_name, var_values);
    }
  }
}

void
ReadVcdActivities::setVarActivity(VcdVar *var,
                                  string &var_name,
                                  const VcdValues &var_values)
{
  if (var->width() == 1) {
    string sta_name = netVerilogToSta(var_name.c_str());
    setVarActivity(sta_name.c_str(), var_values, 0);
  }
  else {
    bool is_bus, is_range, subscript_wild;
    string bus_name;
    int from, to;
    parseBusName(var_name.c_str(), '[', ']', '\\',
                 is_bus, is_range, bus_name, from, to, subscript_wild);
    if (is_bus) {
      string sta_bus_name = netVerilogToSta(bus_name.c_str());
      int value_bit = 0;
      if (to < from) {
        for (int bus_bit = to; bus_bit <= from; bus_bit++) {
          string pin_name = sta_bus_name;
          pin_name += '[';
          pin_name += to_string(bus_bit);
          pin_name += ']';
          setVarActivity(pin_name.c_str(), var_values, value_bit);
          value_bit++;
        }
      }
      else {
        for (int bus_bit = to; bus_bit >= from; bus_bit--) {
          string pin_name = sta_bus_name;
          pin_name += '[';
          pin_name += to_string(bus_bit);
          pin_name += ']';
          setVarActivity(pin_name.c_str(), var_values, value_bit);
          value_bit++;
        }
      }
    }
    else
      report_->warn(1451, "problem parsing bus %s.", var_name.c_str());
  }
}

void
ReadVcdActivities::setVarActivity(const char *pin_name,
                                  const VcdValues &var_values,
                                  int value_bit)
{
  const Pin *pin = sdc_network_->findPin(pin_name);
  if (pin) {
    debugPrint(debug_, "read_vcd_activities", 3, "%s values", pin_name);
    double transition_count, activity, duty;
    findVarActivity(var_values, value_bit,
                    transition_count, activity, duty);
    debugPrint(debug_, "read_vcd_activities", 1,
               "%s transitions %.1f activity %.2f duty %.2f",
               pin_name,
               transition_count,
               activity,
               duty);
    if (sdc_->isLeafPinClock(pin))
      checkClkPeriod(pin, transition_count);
    power_->setUserActivity(pin, activity, duty, PwrActivityOrigin::vcd);
    annotated_pins_.insert(pin);
  }
}

void
ReadVcdActivities::findVarActivity(const VcdValues &var_values,
                                   int value_bit,
                                   // Return values.
                                   double &transition_count,
                                   double &activity,
                                   double &duty)
{
  transition_count = 0.0;
  char prev_value = var_values[0].value(value_bit);
  VcdTime prev_time = var_values[0].time();
  VcdTime high_time = 0;
  for (const VcdValue &var_value : var_values) {
    VcdTime time = var_value.time();
    char value = var_value.value(value_bit);
    debugPrint(debug_, "read_vcd_activities", 3, " %" PRId64 " %c", time, value);
    if (prev_value == '1')
      high_time += time - prev_time;
    if (value != prev_value)
      transition_count += (value == 'X'
                           || value == 'Z'
                           || prev_value == 'X'
                           || prev_value == 'Z')
        ? .5
        : 1.0;
    prev_time = time;
    prev_value = value;
  }
  VcdTime time_max = vcd_.timeMax();
  if (prev_value == '1')
    high_time += time_max - prev_time;
  duty = static_cast<double>(high_time) / time_max;
  activity = transition_count / (time_max * vcd_.timeScale() / clk_period_);
}

void
ReadVcdActivities::checkClkPeriod(const Pin *pin,
                                  double transition_count)
{
  VcdTime time_max = vcd_.timeMax();
  double sim_period = time_max * vcd_.timeScale() / (transition_count / 2.0);

  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  if (clks) {
    for (Clock *clk : *clks) {
      double clk_period = clk->period();
      if (abs((clk_period - sim_period) / clk_period) > .1)
        // Warn if sim clock period differs from SDC by 10%.
        report_->warn(1452, "clock %s vcd period %s differs from SDC clock period %s",
                      clk->name(),
                      delayAsString(sim_period, this),
                      delayAsString(clk_period, this));
    }
  }
}

}
