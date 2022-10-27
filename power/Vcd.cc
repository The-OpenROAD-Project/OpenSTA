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

#include "Vcd.hh"

#include "Report.hh"

namespace sta {

Vcd::Vcd(StaState *sta) :
  StaState(sta),
  time_unit_scale_(0.0),
  max_var_name_length_(0),
  max_var_width_(0),
  time_max_(0)
{
}

////////////////////////////////////////////////////////////////

void
Vcd::setTimeUnit(const string &time_unit,
                 double time_unit_scale)
{
  time_unit_ = time_unit;
  time_unit_scale_ = time_unit_scale;
}

void
Vcd::setDate(const string &date)
{
  date_ = date;
}

void
Vcd::setComment(const string &comment)
{
  comment_ = comment;
}

void
Vcd::setVersion(const string &version)
{
  version_ = version;
}

void
Vcd::setTimeScale(double time_scale)
{
  time_scale_ = time_scale;
}

void
Vcd::setMinDeltaTime(VarTime min_delta_time)
{
  min_delta_time_ = min_delta_time;
}

void
Vcd::setTimeMax(VarTime time_max)
{
  time_max_ = time_max;
}

void
Vcd::makeVar(string &name,
             VcdVarType type,
             int width,
             string &id)
{
  vars_.push_back(VcdVar(name, type, width, id));
  max_var_name_length_ = std::max(max_var_name_length_, name.size());
  max_var_width_ = std::max(max_var_width_, width);
  // Make entry for var ID.
  id_values_map_[id].clear();
}

bool
Vcd::varIdValid(string &id)
{
  return id_values_map_.find(id) != id_values_map_.end();
}

void
Vcd::varAppendValue(string &id,
                    VarTime time,
                    char value)
{
  VcdValues &values = id_values_map_[id];
  values.push_back(VcdValue(time, value, 0));
}

void
Vcd::varAppendBusValue(string &id,
                       VarTime time,
                       int64_t bus_value)
{
  VcdValues &values = id_values_map_[id];
  values.push_back(VcdValue(time, '\0', bus_value));
}

VcdValues &
Vcd::values(VcdVar &var)
{
  if (id_values_map_.find(var.id()) ==  id_values_map_.end()) {
    report_->error(805, "Unknown variable %s ID %s",
                   var.name().c_str(),
                   var.id().c_str());
    static VcdValues empty;
    return empty;
  }
  else
    return id_values_map_[var.id()];
}

////////////////////////////////////////////////////////////////

VcdVar::VcdVar(string name,
               VcdVarType type,
               int width,
               string id) :
  name_(name),
  type_(type),
  width_(width),
  id_(id)
{
}

VcdValue::VcdValue(VarTime time,
                   char value,
                   uint64_t bus_value) :
  time_(time),
  value_(value),
  bus_value_(bus_value)
{
}

}
