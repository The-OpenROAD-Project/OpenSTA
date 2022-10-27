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
using std::swap;
using std::to_string;

static void
setVcdActivities(Vcd &vcd,
                 Sta *sta);
static void
setPinActivity(const char *pin_name,
               int transition_count,
               float activity,
               float duty,
               Debug *debug,
               Network *network,
               Power *power);
void
readVcdActivities(const char *filename,
                  Sta *sta)
{
  Vcd vcd = readVcdFile(filename, sta);
  setVcdActivities(vcd, sta);
}

static void
setVcdActivities(Vcd &vcd,
                 Sta *sta)
{
  Debug *debug = sta->debug();
  Network *network = sta->network();
  Power *power = sta->power();

  float clk_period = 0.0;
  for (Clock *clk : *sta->sdc()->clocks())
    clk_period = min(clk->period(), clk_period);

  VarTime time_max = vcd.timeMax();
  for (VcdVar &var : vcd.vars()) {
    const VcdValues &var_values = vcd.values(var);
    if (!var_values.empty()) {
      int transition_count = 0;
      char prev_value = var_values[0].value();
      VarTime prev_time = var_values[0].time();
      VarTime high_time = 0;
      for (const VcdValue &var_value : var_values) {
        VarTime time = var_value.time();
        char value = var_value.value();
        if (prev_value == '1')
          high_time += time - prev_time;
        transition_count++;
        prev_time = time;
        prev_value = value;
      }
      if (prev_value == '1')
        high_time += time_max - prev_time;
      float duty = static_cast<float>(high_time) / time_max;
      float activity = transition_count
        / (time_max * vcd.timeUnitScale() / clk_period);

      string var_name = var.name();
      if (var_name[0] == '\\')
        var_name += ' ';
      const char *sta_name = verilogToSta(var_name.c_str());
      if (var.width() == 1) {
        setPinActivity(sta_name, transition_count, activity, duty,
                       debug, network, power);
      }
      else {
        char *bus_name;
        int from, to;
        parseBusRange(sta_name, '[', ']', '\\',
                      bus_name, from, to);
        if (from > to)
          swap(from, to);
        for (int bit = from; bit <= to; bit++) {
          string name = bus_name;
          name += '[';
          name += to_string(bit);
          name += ']';
          setPinActivity(name.c_str(), transition_count, activity, duty,
                       debug, network, power);
        }
      }
    }
  }
}

static void
setPinActivity(const char *pin_name,
               int transition_count,
               float activity,
               float duty,
               Debug *debug,
               Network *network,
               Power *power)
{
  Pin *pin = network->findPin(pin_name);
  if (pin) {
    debugPrint(debug, "vcd_activities", 1,
               "%s transitions %d activity %.2f duty %.2f",
               pin_name,
               transition_count,
               activity,
               duty);
    power->setUserActivity(pin, activity, duty,
                           PwrActivityOrigin::user);
  }
}

}
