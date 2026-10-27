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

#include "VcdReader.hh"

#include <cmath>
#include <cinttypes>
#include <unordered_map>
#include <vector>

#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Liberty.hh"
#include "Mode.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "ParseBus.hh"
#include "PortDirection.hh"
#include "Power.hh"
#include "Sdc.hh"
#include "Sta.hh"
#include "VcdParse.hh"
#include "VerilogNamespace.hh"

namespace sta {

// Transition count and high time for duty cycle for a group of pins
// for one bit of vcd ID.
class VcdCount
{
public:
  double transitionCount() const { return transition_count_; }
  VcdTime highTime(VcdTime time_max) const;
  void incrCounts(VcdTime time,
                  char value,
                  double x_value);
  void addPin(const Pin *pin);
  const PinSeq &pins() const { return pins_; }

  static void setFilter(VcdTime begin,
                        VcdTime end);

private:
  VcdTime clippedIntervalStart() const;

  PinSeq pins_;
  VcdTime prev_time_ = vcd_null_time;
  char prev_value_ = '\0';
  VcdTime high_time_ = 0;
  double transition_count_ = 0;

  static VcdTime begin_time_;
  static VcdTime end_time_;
};

// Define static members
VcdTime VcdCount::begin_time_ = vcd_null_time;
VcdTime VcdCount::end_time_ = vcd_null_time;

void
VcdCount::addPin(const Pin *pin)
{
  pins_.push_back(pin);
}

VcdTime
VcdCount::clippedIntervalStart() const
{
  // Clip prev_time_ to begin_time if signal went high before the window.
  return (begin_time_ != vcd_null_time && prev_time_ < begin_time_)
          ? begin_time_ : prev_time_;
}

void
VcdCount::setFilter(VcdTime begin,
                    VcdTime end)
{
  begin_time_ = begin;
  end_time_ = end;
}

void
VcdCount::incrCounts(VcdTime time,
                     char value,
                     double x_value)
{
  // Initial value does not contribute to transitions or high time.
  if (prev_time_ != vcd_null_time
      // Determine if this time point is within the filter window
      && (begin_time_ == vcd_null_time || time >= begin_time_)
      && (end_time_ == vcd_null_time || time <= end_time_)) {
    value = std::toupper(value);
    if (prev_value_ == '1') {
      VcdTime interval_start = clippedIntervalStart();
      if (time > interval_start)
        high_time_ += time - interval_start;
    }
    if (value != prev_value_
        // Filter X->Z and Z->X transitions.
        && !((value == 'X' && prev_value_ == 'Z')
             || (value == 'Z' && prev_value_ == 'X'))) {
      if ((value == '0' || value == '1')
          && (prev_value_ == '0' || prev_value_ == '1'))
        transition_count_ += 1.0;
      else
        transition_count_ += x_value;
    }
  }
  // Update state for transitions before or within the window.
  // This prevents values after window boundaries corrupting high time.
  if (end_time_ == vcd_null_time || time <= end_time_) {
    prev_time_ = time;
    prev_value_ = value;
  }
}

VcdTime
VcdCount::highTime(VcdTime time_max) const
{
  if (prev_value_ == '1') {
    VcdTime interval_start = clippedIntervalStart();
    if (time_max > interval_start)
      return high_time_ + time_max - interval_start;
    else
      return high_time_;
  }
  else
    return high_time_;
}

////////////////////////////////////////////////////////////////

// VcdCount[bit]
using VcdCounts = std::vector<VcdCount>;
// ID -> VcdCount[bit]
using VcdIdCountsMap = std::unordered_map<std::string, VcdCounts>;

class VcdCountReader : public VcdReader
{
public:
  VcdCountReader(std::string_view scope,
                 const Network *sdc_network,
                 Report *report,
                 Debug *debug);
  VcdTime timeMax() const { return time_max_; }
  VcdTime timeMin() const { return time_min_; }
  const VcdIdCountsMap &countMap() const { return vcd_count_map_; }
  double timeScale() const { return time_scale_; }

  // VcdParse callbacks.
  void setDate(std::string_view ) override {}
  void setComment(std::string_view ) override {}
  void setVersion(std::string_view ) override {}
  void setTimeUnit(std::string_view time_unit,
                   double time_unit_scale,
                   double time_scale) override;
  void setTimeMin(VcdTime time) override;
  void setTimeMax(VcdTime time) override;
  void varMinDeltaTime(VcdTime) override {}
  bool varIdValid(std::string_view id) override;
  void makeVar(const VcdScope &scope,
               std::string_view name,
               VcdVarType type,
               size_t width,
               std::string_view id) override;
  void varAppendValue(const std::string &id,
                      VcdTime time,
                      char value) override;
  void varAppendBusValue(const std::string &id,
                         VcdTime time,
                         std::string_view bus_value) override;
  void endDefinitions() override;

private:
  struct Var
  {
    std::string name;
    std::string id;
    size_t width;
    size_t bit_idx;
  };

  void recordVar(const std::string &name,
                 std::string_view id,
                 size_t width,
                 size_t bit_idx);
  void resolveVars();
  void addVarPin(const Var &var,
                 const PinUnorderedSet &named_pins);
  bool annotatable(const Pin *pin) const;

  const std::string scope_;

  double time_scale_ = 1.0;
  VcdTime time_min_ = 0;
  VcdTime time_max_ = 0;
  std::vector<Var> vars_;
  bool vars_resolved_ = false;
  VcdIdCountsMap vcd_count_map_;
  // Pins already added from a VCD name.
  PinUnorderedSet added_pins_;

  const Network *sdc_network_;
  Report *report_;
  Debug *debug_;
};

VcdCountReader::VcdCountReader(std::string_view scope,
                               const Network *sdc_network,
                               Report *report,
                               Debug *debug) :
  scope_(scope),
  sdc_network_(sdc_network),
  report_(report),
  debug_(debug)
{
}

void
VcdCountReader::setTimeUnit(std::string_view ,
                            double time_unit_scale,
                            double time_scale)
{
  time_scale_ = time_scale * time_unit_scale;
}

void
VcdCountReader::setTimeMin(VcdTime time)
{
  debugPrint(debug_, "read_vcd", 1, "setTimeMin called with time {}", time);
  time_min_ = time;
}

void
VcdCountReader::setTimeMax(VcdTime time)
{
  debugPrint(debug_, "read_vcd", 1, "setTimeMax called with time {}", time);
  time_max_ = time;
}

bool
VcdCountReader::varIdValid(std::string_view )
{
  return true;
}

void
VcdCountReader::makeVar(const VcdScope &scope,
                        std::string_view name,
                        VcdVarType type,
                        size_t width,
                        std::string_view id)
{
  if (type == VcdVarType::wire || type == VcdVarType::reg) {
    std::string path_name;
    bool first = true;
    for (std::string_view context : scope) {
      if (!first)
        path_name += '/';
      path_name += context;
      first = false;
    }
    size_t scope_length = scope_.size();
    // string::starts_with in c++20
    if (scope_length == 0 || path_name.substr(0, scope_length) == scope_) {
      path_name += '/';
      path_name += name;
      // Strip the scope from the name.
      std::string var_scoped = path_name.substr(scope_length + 1);
      if (width == 1) {
        std::string pin_name = netVerilogToSta(var_scoped);
        recordVar(pin_name, id, width, 0);
      }
      else {
        bool is_bus, is_range, subscript_wild;
        std::string bus_name;
        int from, to;
        parseBusName(var_scoped, '[', ']', '\\', is_bus, is_range, bus_name,
                     from, to, subscript_wild);
        if (is_bus) {
          std::string sta_bus_name = netVerilogToSta(bus_name);
          int bit_idx = 0;
          if (to < from) {
            for (int bus_bit = to; bus_bit <= from; bus_bit++) {
              std::string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += std::to_string(bus_bit);
              pin_name += ']';
              recordVar(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
          else {
            for (int bus_bit = to; bus_bit >= from; bus_bit--) {
              std::string pin_name = sta_bus_name;
              pin_name += '[';
              pin_name += std::to_string(bus_bit);
              pin_name += ']';
              recordVar(pin_name, id, width, bit_idx);
              bit_idx++;
            }
          }
        }
        else
          report_->warn(1451, "problem parsing bus {}.", var_scoped);
      }
    }
  }
}

void
VcdCountReader::recordVar(const std::string &name,
                          std::string_view id,
                          size_t width,
                          size_t bit_idx)
{
  vars_.push_back(Var{name, std::string(id), width, bit_idx});
}

void
VcdCountReader::endDefinitions()
{
  resolveVars();
}

void
VcdCountReader::resolveVars()
{
  if (!vars_resolved_) {
    vars_resolved_ = true;
    // Pins named by a VCD variable. A net variable does not override these.
    PinUnorderedSet named_pins;
    for (const Var &var : vars_) {
      const Pin *pin = sdc_network_->findPin(var.name);
      if (pin && annotatable(pin))
        named_pins.insert(pin);
    }
    for (const Var &var : vars_)
      addVarPin(var, named_pins);
  }
}

void
VcdCountReader::addVarPin(const Var &var,
                          const PinUnorderedSet &named_pins)
{
  // A net name annotates every pin on the net that has no VCD variable of
  // its own. A pin name that is not a net annotates that pin.
  PinSeq pins;
  Net *net = sdc_network_->findNet(var.name);
  const Pin *named_pin = sdc_network_->findPin(var.name);
  if (net) {
    NetConnectedPinIterator *pin_iter = sdc_network_->connectedPinIterator(net);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      if (pin == named_pin || !named_pins.contains(pin))
        pins.push_back(pin);
    }
    delete pin_iter;
    sort(pins, PinPathNameLess(sdc_network_));
  }
  else if (named_pin)
    pins.push_back(named_pin);
  for (const Pin *pin : pins) {
    if (annotatable(pin) && !added_pins_.contains(pin)) {
      added_pins_.insert(pin);
      VcdCounts &vcd_counts = vcd_count_map_[var.id];
      vcd_counts.resize(var.width);
      vcd_counts[var.bit_idx].addPin(pin);
      debugPrint(debug_, "read_vcd", 2, "id {} pin {}", var.id,
                 sdc_network_->pathName(pin));
    }
  }
}

bool
VcdCountReader::annotatable(const Pin *pin) const
{
  LibertyPort *liberty_port = sdc_network_->libertyPort(pin);
  return !sdc_network_->isHierarchical(pin)
      && !sdc_network_->direction(pin)->isInternal()
      && !sdc_network_->direction(pin)->isPowerGround()
      && !(liberty_port && liberty_port->isPwrGnd());
}

void
VcdCountReader::varAppendValue(const std::string &id,
                               VcdTime time,
                               char value)
{
  resolveVars();
  const auto &itr = vcd_count_map_.find(id);
  if (itr != vcd_count_map_.end()) {
    VcdCounts &vcd_counts = itr->second;
    if (debug_->check("read_vcd", 3)) {
      for (auto &vcd_count : vcd_counts) {
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 3, "{} time {} value {}",
                     sdc_network_->pathName(pin), time, value);
        }
      }
    }
    for (auto &vcd_count : vcd_counts)
      vcd_count.incrCounts(time, value, 0.5);
  }
}

void
VcdCountReader::varAppendBusValue(const std::string &id,
                                  VcdTime time,
                                  std::string_view bus_value)
{
  resolveVars();
  const auto &itr = vcd_count_map_.find(id);
  if (itr != vcd_count_map_.end()) {
    VcdCounts &vcd_counts = itr->second;
    char extend_value = '0';
    if (bus_value.size() < vcd_counts.size()) {
      char left_value = std::toupper(bus_value[bus_value.size() - 1]);
      if (left_value == 'X' || left_value == 'Z')
        extend_value = left_value;
    }
    for (size_t bit_idx = 0; bit_idx < vcd_counts.size(); bit_idx++) {
      char bit_value = (bit_idx < bus_value.size())
        ? std::toupper(bus_value[bit_idx])
        : extend_value;
      VcdCount &vcd_count = vcd_counts[bit_idx];
      vcd_count.incrCounts(time, bit_value, 1.0);
      if (debug_->check("read_vcd", 3)) {
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 3, "{} time {} value {}",
                     sdc_network_->pathName(pin), time, bit_value);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////

class ReadVcdActivities : public StaState
{
public:
  ReadVcdActivities(std::string_view filename,
                    std::string_view scope,
                    VcdTime begin_time,
                    VcdTime end_time,
                    const Sdc *sdc,
                    StaState *sta);
  void readActivities();

private:
  void setActivities();
  void checkClkPeriod(const Pin *pin,
                      double transition_count);

  const std::string filename_;
  VcdTime begin_time_;
  VcdTime end_time_;

  std::set<const Pin *> annotated_pins_;
  VcdCountReader vcd_reader_;
  VcdParse vcd_parse_;
  const Sdc *sdc_;

  static constexpr double sim_clk_period_tolerance_ = .1;
};

void
readVcdActivities(std::string_view filename,
                  std::string_view scope,
                  const Mode *mode,
                  VcdTime begin_time,
                  VcdTime end_time,
                  StaState *sta)
{
  const Sdc *sdc = mode->sdc();
  ReadVcdActivities reader(filename, scope, begin_time, end_time, sdc, sta);
  reader.readActivities();
}

ReadVcdActivities::ReadVcdActivities(std::string_view filename,
                                     std::string_view scope,
                                     VcdTime begin_time,
                                     VcdTime end_time,
                                     const Sdc *sdc,
                                     StaState *sta) :
  StaState(sta),
  filename_(filename),
  begin_time_(begin_time),
  end_time_(end_time),
  vcd_reader_(scope, sdc_network_, report_, debug_),
  vcd_parse_(report_, debug_),
  sdc_(sdc)
{
}

void
ReadVcdActivities::readActivities()
{
  const ClockSeq &clks = sdc_->clocks();
  if (clks.empty())
    report_->error(820, "No clocks have been defined.");

  // Set the time window filter once globally
  VcdCount::setFilter(begin_time_, end_time_);
  vcd_parse_.read(filename_.c_str(), &vcd_reader_, begin_time_, end_time_);

  if (vcd_reader_.timeMax() > 0)
    setActivities();
  else
    report_->warn(1450, "VCD max time is zero.");
  report_->report("Annotated {} pin activities.", annotated_pins_.size());
}

void
ReadVcdActivities::setActivities()
{
  VcdTime time_min = vcd_reader_.timeMin();
  VcdTime time_max = vcd_reader_.timeMax();
  VcdTime time_delta = time_max - time_min;
  double time_scale = vcd_reader_.timeScale();
  for (auto &[id, vcd_counts] : vcd_reader_.countMap()) {
    for (const VcdCount &vcd_count : vcd_counts) {
      double transition_count = vcd_count.transitionCount();
      VcdTime high_time = vcd_count.highTime(time_max);
      float duty = static_cast<double>(high_time) / time_delta;
      float density = transition_count / (time_delta * time_scale);
      if (debug_->check("read_vcd", 1)) {
        for (const Pin *pin : vcd_count.pins()) {
          debugPrint(debug_, "read_vcd", 1,
                     "{} transitions {:.1f} activity {:.2f} duty {:.2f}",
                     sdc_network_->pathName(pin), transition_count, density, duty);
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
  ClockSet *clks = sdc_->findLeafPinClocks(pin);
  if (clks) {
    VcdTime time_max = vcd_reader_.timeMax();
    VcdTime time_min = vcd_reader_.timeMin();
    double time_scale = vcd_reader_.timeScale();
    double sim_period =
        (time_max - time_min) * time_scale / (transition_count / 2.0);
    for (Clock *clk : *clks) {
      if (transition_count == 0)
        report_->warn(1453, "clock {} pin {} has no vcd transitions.", clk->name(),
                      sdc_network_->pathName(pin));
      else {
        double clk_period = clk->period();
        if (std::abs((clk_period - sim_period) / clk_period)
            > sim_clk_period_tolerance_)
          // Warn if sim clock period differs from SDC by more than 10%.
          report_->warn(1452,
                        "clock {} vcd period {} differs from SDC clock period {}",
                        clk->name(), delayAsString(sim_period, this),
                        delayAsString(clk_period, this));
      }
    }
  }
}

}  // namespace sta
