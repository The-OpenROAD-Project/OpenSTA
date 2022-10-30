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
findVarActivity(const char *pin_name,
                int value_bit,
                const VcdValues &var_values,
                Vcd &vcd,
                float clk_period,
                Debug *debug,
                Network *network,
                Power *power);

////////////////////////////////////////////////////////////////

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

  float clk_period = INF;
  for (Clock *clk : *sta->sdc()->clocks())
    clk_period = min(clk->period(), clk_period);

  for (VcdVar &var : vcd.vars()) {
    const VcdValues &var_values = vcd.values(var);
    if (!var_values.empty()) {
      string var_name = var.name();
      if (var_name[0] == '\\')
        var_name += ' ';
      const char *sta_name = verilogToSta(var_name.c_str());

      if (var.width() == 1)
        findVarActivity(sta_name, 0, var_values, vcd,
                  clk_period, debug, network, power);
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
            findVarActivity(pin_name.c_str(), value_bit, var_values, vcd,
                            clk_period, debug, network, power);
            value_bit++;
          }
        }
        else {
          for (int bus_bit = to; bus_bit >= from; bus_bit--) {
            string pin_name = bus_name;
            pin_name += '[';
            pin_name += to_string(bus_bit);
            pin_name += ']';
            findVarActivity(pin_name.c_str(), value_bit, var_values, vcd,
                            clk_period, debug, network, power);
            value_bit++;
          }
        }
      }
    }
  }
}

static void
findVarActivity(const char *pin_name,
                int value_bit,
                const VcdValues &var_values,
                Vcd &vcd,
                float clk_period,
                Debug *debug,
                Network *network,
                Power *power)
{
  int transition_count = 0;
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
    transition_count++;
    prev_time = time;
    prev_value = value;
  }
  VcdTime time_max = vcd.timeMax();
  if (prev_value == '1')
    high_time += time_max - prev_time;
  float duty = static_cast<float>(high_time) / time_max;
  float activity = transition_count
    / (time_max * vcd.timeUnitScale() / clk_period);

  Pin *pin = network->findPin(pin_name);
  if (pin) {
    debugPrint(debug, "read_vcd_activities", 1,
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
