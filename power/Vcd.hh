// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "StaState.hh"

namespace sta {

using std::string;
using std::vector;
using std::map;
using std::max;
using std::min;

class VcdVar;
class VcdValue;
typedef vector<VcdValue> VcdValues;
typedef int64_t VcdTime;
typedef vector<string> VcdScope;
typedef map<string, VcdVar*> VcdNameMap;

enum class VcdVarType {
  wire,
  reg,
  parameter,
  integer,
  real,
  supply0,
  supply1,
  tri,
  triand,
  trior,
  trireg,
  tri0,
  tri1,
  wand,
  wor,
  unknown
};

class Vcd : public StaState
{
public:
  Vcd(StaState *sta);
  Vcd(const Vcd &vcd);
  // Move copy assignment.
  Vcd& operator=(Vcd &&vcd1);
  ~Vcd();
  VcdVar *var(const string name);
  VcdValues &values(VcdVar *var);

  const string &date() const { return date_; }
  void setDate(const string &date);
  const string &comment() const { return comment_; }
  void setComment(const string &comment);
  const string &version() const { return version_; }
  void setVersion(const string &version); 
  double timeScale() const { return time_scale_; }
  void setTimeScale(double time_scale);
  const string &timeUnit() const { return time_unit_; }
  double timeUnitScale() const { return time_unit_scale_; }
  void setTimeUnit(const string &time_unit,
                   double time_unit_scale);
  VcdTime timeMax() const { return time_max_; }
  void setTimeMax(VcdTime time_max);
  VcdTime minDeltaTime() const { return min_delta_time_; }
  void setMinDeltaTime(VcdTime min_delta_time);
  vector<VcdVar*> vars() { return vars_; }
  void makeVar(string &name,
               VcdVarType type,
               int width,
               string &id);
  int maxVarWidth() const { return max_var_width_; }
  int maxVarNameLength() const { return max_var_name_length_; }
  bool varIdValid(string &id);
  void varAppendValue(string &id,
                      VcdTime time,
                      char value);
  void varAppendBusValue(string &id,
                         VcdTime time,
                         int64_t bus_value);

private:
  string date_;
  string comment_;
  string version_;
  double time_scale_;
  string time_unit_;
  double time_unit_scale_;

  vector<VcdVar*> vars_;
  VcdNameMap var_name_map_;
  size_t max_var_name_length_;
  int max_var_width_;
  map<string, VcdValues> id_values_map_;
  VcdTime min_delta_time_;
  VcdTime time_max_;
};

class VcdVar
{
public:
  VcdVar(string name,
         VcdVarType type,
         int width,
         string id);
  const string& name() const { return name_; }
  VcdVarType type() const { return type_; }
  int width() const { return width_; }
  const string& id() const { return id_; }

private:
  string name_;
  VcdVarType type_;
  int width_;
  string id_;
};

class VcdValue
{
public:
  VcdValue(VcdTime time,
           char value,
           uint64_t bus_value);
  VcdTime time() const { return time_; }
  char value() const { return value_; }
  uint64_t busValue() const { return bus_value_; }

private:
  VcdTime time_;
  // 01XUZ or '\0' when width > 1 to use bus_value_.
  char value_;
  uint64_t bus_value_;
};

} // namespace
