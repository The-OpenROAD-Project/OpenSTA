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

#include "VcdReader.hh"

#include <inttypes.h>
#include <unordered_map>

#include "VcdParse.hh"
#include "Debug.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "VerilogNamespace.hh"
#include "ParseBus.hh"
#include "Sdc.hh"
#include "Power.hh"
#include "Sta.hh"

namespace sta {

using std::string;
using std::abs;
using std::min;
using std::to_string;
using std::vector;
using std::unordered_map;

// Transition count and high time for duty cycle for a group of pins
// for one bit of vcd ID.
class VcdCount
{
public:
  VcdCount();
  double transitionCount() const { return transition_count_; }
  VcdTime highTime(VcdTime time_max) const;
  void incrCounts(VcdTime time,
                  char value);
  void incrCounts(VcdTime time,
                  int64_t value);
  void addPin(const Pin *pin);
  const PinSeq &pins() const { return pins_; }

private:
  PinSeq pins_;
  VcdTime prev_time_;
  char prev_value_;
  VcdTime high_time_;
  double transition_count_;
};

VcdCount::VcdCount() :
  prev_time_(-1),
  prev_value_('\0'),
  high_time_(0),
  transition_count_(0)
{
}

void
VcdCount::addPin(const Pin *pin)
{
  pins_.push_back(pin);
}

void
VcdCount::incrCounts(VcdTime time,
                     char value)
{
  // Initial value does not coontribute to transitions or high time.
  if (prev_time_ != -1) {
    if (prev_value_ == '1')
      high_time_ += time - prev_time_;
    if (value != prev_value_)
      transition_count_ += (value == 'X'
                            || value == 'Z'
                            || prev_value_ == 'X'
                            || prev_value_ == 'Z')
        ? .5
        : 1.0;
  }
  prev_time_ = time;
  prev_value_ = value;
}

VcdTime
VcdCount::highTime(VcdTime time_max) const
{
  if (prev_value_ == '1')
    return high_time_ + time_max - prev_time_;
  else
    return high_time_;
}

////////////////////////////////////////////////////////////////

// VcdCount[bit]
typedef vector<VcdCount> VcdCounts;
// ID -> VcdCount[bit]
typedef unordered_map<string, VcdCounts> VcdIdCountsMap;

class VcdCountReader : public VcdReader
{
public:
  VcdCountReader(const char *scope,
                 Network *sdc_network,
                 Report *report,
                 Debug *debug);
  VcdTime timeMax() const { return time_max_; }
  VcdTime timeMin() const { return time_min_; }
  const VcdIdCountsMap &countMap() const { return vcd_count_map_; }
  double timeScale() const { return time_scale_; }

  // VcdParse callbacks.
  void setDate(const string &) override {}
  void setComment(const string &) override {}
  void setVersion(const string &) override {}
  void setTimeUnit(const string &time_unit,
                   double time_unit_scale,
                   double time_scale) override;
  void setTimeMin(VcdTime time) override;
  void setTimeMax(VcdTime time) override;
  void varMinDeltaTime(VcdTime) override {}
  bool varIdValid(const string &id) override;
  void makeVar(const VcdScope &scope,
               const string &name,
               VcdVarType type,
               size_t width,
               const string &id) override;
  void varAppendValue(const string &id,
                      VcdTime time,
                      char value) override;
  void varAppendBusValue(const string &id,
                         VcdTime time,
                         const string &bus_value) override;

private:
  void addVarPin(const string &pin_name,
                 const string &id,
                 size_t width,
                 size_t bit_idx);

  const char *scope_;
  Network *sdc_network_;
  Report *report_;
  Debug *debug_;

  double time_scale_;
  VcdTime time_min_;
  VcdTime time_max_;
  VcdIdCountsMap vcd_count_map_;
};

VcdCountReader::VcdCountReader(const char *scope,
                               Network *sdc_network,
                               Report *report,
                               Debug *debug) :
  scope_(scope),
  sdc_network_(sdc_network),
  report_(report),
  debug_(debug),
  time_scale_(1.0),
  time_min_(0),
  time_max_(0)
{
}

void
VcdCountReader::setTimeUnit(const string &,
                            double time_unit_scale,
                            double time_scale)
{
  time_scale_ = time_scale * time_unit_scale;
}

void
VcdCountReader::setTimeMin(VcdTime time)
{
  time_min_ = time;
}

void
VcdCountReader::setTimeMax(VcdTime time)
{
  time_max_ = time;
}

bool
VcdCountReader::varIdValid(const string &)
{
  return true;
}

void
VcdCountReader::makeVar(const VcdScope &scope,
                        const string &name,
                        VcdVarType type,
                        size_t width,
                        const string &id)
{
  if (type == VcdVarType::wire
      || type == VcdVarType::reg) {
    string path_name;
    bool first = true;
    for (const string &context : scope) {
      if (!first)
        path_name += '/';
      path_name += context;
      first = false;
    }
    size_t scope_length = strlen(scope_);
    // string::starts_with in c++20
    if (scope_length == 0
        || path_name.substr(0, scope_length) == scope_) {
      path_name += '/';
      path_name += name;
      // Strip the scope from the name.
      string var_scoped = path_name.substr(scope_length + 1);
      if (width == 1) {
        string pin_name = netVerilogToSta(&var_scoped);
        addVarPin(pin_name, id, width, 0);
      }
      else {
        bool is_bus, is_range, subscript_wild;
        string bus_name;
        int from, to;
        parseBusName(var_scoped.c_str(), '[', ']', '\\',
                     is_bus, is_range, bus_name, from, to, subscript_wild);
        if (is_bus) {
          string sta_bus_name = netVerilogToSta(&bus_name);
          int bit_idx = 0;
          if (to < from) {
            for (int bus_bit = to; bus_bit <= from; bus_bit++) {
              string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += to_string(bus_bit);
              pin_name += ']';
              addVarPin(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
          else {
            for (int bus_bit = to; bus_bit >= from; bus_bit--) {
              string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += to_string(bus_bit);
              pin_name += ']';
              addVarPin(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
        }
        else
          report_->warn(1451, "problem parsing bus %s.", var_scoped.c_str());
      }
    }
  }
}

void
VcdCountReader::addVarPin(const string &pin_name,
                          const string &id,
                          size_t width,
                          size_t bit_idx)
{
  const Pin *pin = sdc_network_->findPin(pin_name.c_str());
  LibertyPort *liberty_port = pin ? sdc_network_->libertyPort(pin) : nullptr;
  if (pin
      && !sdc_network_->isHierarchical(pin)
      && !sdc_network_->direction(pin)->isInternal()
      && !sdc_network_->direction(pin)->isPowerGround()
      && !(liberty_port && liberty_port->isPwrGnd())) {
    VcdCounts &vcd_counts = vcd_count_map_[id];
    vcd_counts.resize(width);
    vcd_counts[bit_idx].addPin(pin);
    debugPrint(debug_, "read_vcd", 2, "id %s pin %s",
               id.c_str(),
               pin_name.c_str());
  }
}

void
VcdCountReader::varAppendValue(const string &id,
                               VcdTime time,
                               char value)
{
  const auto &itr = vcd_count_map_.find(id);
  if (itr != vcd_count_map_.end()) {
    VcdCounts &vcd_counts = itr->second;
    if (debug_->check("read_vcd", 3)) {
      for (size_t bit_idx = 0; bit_idx < vcd_counts.size(); bit_idx++) {
        VcdCount &vcd_count = vcd_counts[bit_idx];
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 3, "%s time %" PRIu64 " value %c",
                     sdc_network_->pathName(pin),
                     time,
                     value);
        }
      }
    }
    for (size_t bit_idx = 0; bit_idx < vcd_counts.size(); bit_idx++) {
      VcdCount &vcd_count = vcd_counts[bit_idx];
      vcd_count.incrCounts(time, value);
    }
  }
}

void
VcdCountReader::varAppendBusValue(const string &id,
                                  VcdTime time,
                                  const string &bus_value)
{
  const auto &itr = vcd_count_map_.find(id);
  if (itr != vcd_count_map_.end()) {
    VcdCounts &vcd_counts = itr->second;
    for (size_t bit_idx = 0; bit_idx < vcd_counts.size(); bit_idx++) {
      char bit_value;
      if (bus_value.size() == 1)
        bit_value = bus_value[0];
      else if  (bit_idx < bus_value.size())
        bit_value = bus_value[bit_idx];
      else
        bit_value = '0';
      VcdCount &vcd_count = vcd_counts[bit_idx];
      vcd_count.incrCounts(time, bit_value);
      if (debug_->check("read_vcd", 3)) {
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 3, "%s time %" PRIu64 " value %c",
                     sdc_network_->pathName(pin),
                     time,
                     bit_value);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

class ReadVcdActivities : public StaState
{
public:
  ReadVcdActivities(const char *filename,
                    const char *scope,
                    Sta *sta);
  void readActivities();

private:
  void setActivities();
  void checkClkPeriod(const Pin *pin,
                      double transition_count);

  const char *filename_;
  VcdCountReader vcd_reader_;
  VcdParse vcd_parse_;

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
  vcd_reader_(scope, sdc_network_, report_, debug_),
  vcd_parse_(report_, debug_),
  power_(sta->power())
{
}

void
ReadVcdActivities::readActivities()
{
  ClockSeq *clks = sdc_->clocks();
  if (clks->empty())
    report_->error(805, "No clocks have been defined.");

  vcd_parse_.read(filename_, &vcd_reader_);

  if (vcd_reader_.timeMax() > 0)
    setActivities();
  else
    report_->warn(1450, "VCD max time is zero.");
  report_->reportLine("Annotated %zu pin activities.", annotated_pins_.size());
}

void
ReadVcdActivities::setActivities()
{
  VcdTime time_min = vcd_reader_.timeMin();
  VcdTime time_max = vcd_reader_.timeMax();
  VcdTime time_delta = time_max - time_min;
  double time_scale = vcd_reader_.timeScale();
  for (auto& [id, vcd_counts] : vcd_reader_.countMap()) {
    for (const VcdCount &vcd_count : vcd_counts) {
      double transition_count = vcd_count.transitionCount();
      VcdTime high_time = vcd_count.highTime(time_max);
      float duty = static_cast<double>(high_time) / time_delta;
      float density = transition_count / (time_delta * time_scale);
      if (debug_->check("read_vcd", 1)) {
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 1,
                     "%s transitions %.1f activity %.2f duty %.2f",
                     sdc_network_->pathName(pin),
                     transition_count,
                     density,
                     duty);
        }
      }
      for (const Pin *pin : vcd_count.pins()) {
        power_->setUserActivity(pin, density, duty, PwrActivityOrigin::vcd);
        if (sdc_->isLeafPinClock(pin))
          checkClkPeriod(pin, transition_count);
        annotated_pins_.insert(pin);
      }
    }
  }
}

void
ReadVcdActivities::checkClkPeriod(const Pin *pin,
                                  double transition_count)
{
  VcdTime time_max = vcd_reader_.timeMax();
  VcdTime time_min = vcd_reader_.timeMin();
  double time_scale = vcd_reader_.timeScale();
  double sim_period = (time_max - time_min) * time_scale / (transition_count / 2.0);
  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  if (clks) {
    for (Clock *clk : *clks) {
      double clk_period = clk->period();
      if (abs((clk_period - sim_period) / clk_period) > sim_clk_period_tolerance_)
        // Warn if sim clock period differs from SDC by more than 10%.
        report_->warn(1452, "clock %s vcd period %s differs from SDC clock period %s",
                      clk->name(),
                      delayAsString(sim_period, this),
                      delayAsString(clk_period, this));
    }
  }
}

} // namespace
