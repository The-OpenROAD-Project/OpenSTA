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

#include "Vcd.hh"

#include "Report.hh"

namespace sta {

Vcd::Vcd(StaState *sta) :
  StaState(sta),
  time_scale_(1.0),
  time_unit_scale_(1.0),
  max_var_name_length_(0),
  max_var_width_(0),
  min_delta_time_(0),
  time_max_(0)
{
}

Vcd::Vcd(const Vcd &vcd) :
  StaState(vcd),
  date_(vcd.date_),
  comment_(vcd.comment_),
  version_(vcd.version_),
  time_scale_(vcd.time_scale_),
  time_unit_(vcd.time_unit_),
  time_unit_scale_(vcd.time_unit_scale_),
  vars_(vcd.vars_),
  var_name_map_(vcd.var_name_map_),
  max_var_name_length_(vcd.max_var_name_length_),
  max_var_width_(vcd.max_var_width_),
  id_values_map_(vcd.id_values_map_),
  min_delta_time_(vcd.min_delta_time_),
  time_max_(vcd.time_max_)
{
}

Vcd&
Vcd::operator=(Vcd &&vcd1)
{
  date_ = vcd1.date_;
  comment_ = vcd1.comment_;
  version_ = vcd1.version_;
  time_scale_ = vcd1.time_scale_;
  time_unit_ = vcd1.time_unit_;
  time_unit_scale_ = vcd1.time_unit_scale_;
  vars_ = vcd1.vars_;
  var_name_map_ = vcd1.var_name_map_;
  max_var_name_length_ = vcd1.max_var_name_length_;
  max_var_width_ = vcd1.max_var_width_;
  id_values_map_ = vcd1.id_values_map_;
  min_delta_time_ = vcd1.min_delta_time_;
  time_max_ = vcd1.time_max_;

  vcd1.vars_.clear();
  return *this;
}

Vcd::~Vcd()
{
  for (VcdVar *var : vars_)
    delete var;
}

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
Vcd::setMinDeltaTime(VcdTime min_delta_time)
{
  min_delta_time_ = min_delta_time;
}

void
Vcd::setTimeMax(VcdTime time_max)
{
 time_max_ = time_max;
}

void
Vcd::makeVar(string &name,
             VcdVarType type,
             int width,
             string &id)
{
  VcdVar *var = new VcdVar(name, type, width, id);
  vars_.push_back(var);
  var_name_map_[name] = var;
  max_var_name_length_ = std::max(max_var_name_length_, name.size());
  max_var_width_ = std::max(max_var_width_, width);
  // Make entry for var ID.
  id_values_map_[id].clear();
}

VcdVar *
Vcd::var(const string name)
{
  return var_name_map_[name];
}

bool
Vcd::varIdValid(string &id)
{
  return id_values_map_.find(id) != id_values_map_.end();
}

void
Vcd::varAppendValue(string &id,
                    VcdTime time,
                    char value)
{
  VcdValues &values = id_values_map_[id];
  values.emplace_back(time, value, 0);
}

void
Vcd::varAppendBusValue(string &id,
                       VcdTime time,
                       int64_t bus_value)
{
  VcdValues &values = id_values_map_[id];
  values.emplace_back(time, '\0', bus_value);
}

VcdValues &
Vcd::values(VcdVar *var)
{
  if (id_values_map_.find(var->id()) ==  id_values_map_.end()) {
    report_->error(1360, "Unknown variable %s ID %s",
                   var->name().c_str(),
                   var->id().c_str());
    static VcdValues empty;
    return empty;
  }
  else
    return id_values_map_[var->id()];
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

VcdValue::VcdValue(VcdTime time,
                   char value,
                   uint64_t bus_value) :
  time_(time),
  value_(value),
  bus_value_(bus_value)
{
}

char
VcdValue::value(int value_bit) const
{
  if (value_ == '\0')
    return ((bus_value_ >> value_bit) & 0x1) ? '1' : '0';
  else
    return value_;
}

}
