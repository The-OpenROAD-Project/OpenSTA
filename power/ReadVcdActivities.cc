// OpenSTA, Static Timing Analyzer
// Copyright (c) 2022, Parallax Software, Inc.
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

#include "VcdReader.hh"
#include "Debug.hh"
#include "Network.hh"
#include "VerilogNamespace.hh"
#include "ParseBus.hh"
#include "Sdc.hh"
#include "Power.hh"
#include "Sta.hh"

namespace sta {

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
  void setVarActivity(const char *pin_name,
                      const VcdValues &var_values,
                      int value_bit);
  void findVarActivity(const VcdValues &var_values,
                       int value_bit,
                       // Return values.
                       int &transition_count,
                       float &activity,
                       float &duty);
  void checkClkPeriod(const Pin *pin,
                      int transition_count);

  const char *filename_;
  const char *scope_;
  Vcd vcd_;
  float clk_period_;
  Sta *sta_;
  Power *power_;
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
    clk_period_ = min(clk->period(), clk_period_);

  setActivities();
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
      if (scope_length
          && var_name.substr(0, scope_length) == scope_)
        var_name = var_name.substr(scope_length + 1);
      if (var_name[0] == '\\')
        var_name += ' ';
      const char *sta_name = verilogToSta(var_name.c_str());

      if (var->width() == 1)
        setVarActivity(sta_name, var_values, 0);
      else {
        char *bus_name;
        int from, to;
        parseBusRange(sta_name, '[', ']', '\\',
                      bus_name, from, to);
        int value_bit = 0;
        if (to < from) {
          for (int bus_bit = to; bus_bit <= from; bus_bit++) {
            string pin_name = bus_name;
            pin_name += '[';
            pin_name += to_string(bus_bit);
            pin_name += ']';
            setVarActivity(pin_name.c_str(), var_values, value_bit);
            value_bit++;
          }
        }
        else {
          for (int bus_bit = to; bus_bit >= from; bus_bit--) {
            string pin_name = bus_name;
            pin_name += '[';
            pin_name += to_string(bus_bit);
            pin_name += ']';
            setVarActivity(pin_name.c_str(), var_values, value_bit);
            value_bit++;
          }
        }
      }
    }
  }
}

void
ReadVcdActivities::setVarActivity(const char *pin_name,
                                  const VcdValues &var_values,
                                  int value_bit)
{
  const Pin *pin = network_->findPin(pin_name);
  if (pin) {
    int transition_count;
    float activity, duty;
    findVarActivity(var_values, value_bit,
                    transition_count, activity, duty);
    debugPrint(debug_, "read_vcd_activities", 1,
               "%s transitions %d activity %.2f duty %.2f",
               pin_name,
               transition_count,
               activity,
               duty);
    if (sdc_->isLeafPinClock(pin))
      checkClkPeriod(pin, transition_count);
    else
      power_->setUserActivity(pin, activity, duty,
                              PwrActivityOrigin::user);
  }
}

void
ReadVcdActivities::findVarActivity(const VcdValues &var_values,
                                   int value_bit,
                                   // Return values.
                                   int &transition_count,
                                   float &activity,
                                   float &duty)
{
  transition_count = 0;
  char prev_value = var_values[0].value();
  VcdTime prev_time = var_values[0].time();
  VcdTime high_time = 0;
  for (const VcdValue &var_value : var_values) {
    VcdTime time = var_value.time();
    char value = var_value.value();
    if (value == '\0') {
      uint64_t bus_value = var_value.busValue();
      value = ((bus_value >> value_bit) & 0x1) ? '1' : '0';
    }
    if (prev_value == '1')
      high_time += time - prev_time;
    if (value != prev_value)
      transition_count++;
    prev_time = time;
    prev_value = value;
  }
  VcdTime time_max = vcd_.timeMax();
  if (prev_value == '1')
    high_time += time_max - prev_time;
  duty = static_cast<float>(high_time) / time_max;
  activity = transition_count
    / (time_max * vcd_.timeUnitScale() / clk_period_);
}

void
ReadVcdActivities::checkClkPeriod(const Pin *pin,
                                  int transition_count)
{
  VcdTime time_max = vcd_.timeMax();
  float sim_period = time_max * vcd_.timeUnitScale() / ((transition_count - 1) / 2.0);

  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  if (clks) {
    for (Clock *clk : *clks) {
      //printf("%.2e %.2e\n", clk->period(), sim_period);
    }
  }
}

}
