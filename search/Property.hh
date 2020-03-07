// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <string>
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SearchClass.hh"
#include "SdcClass.hh"

namespace sta {

using std::string;

class Sta;
class PwrActivity;

// Adding a new property type
//  value union
//  enum Type
//  constructor
//  copy constructor switch clause
//  move constructor switch clause
//  operator= &  switch clause
//  operator= && switch clause
//  StaTcl.i swig %typemap(out) PropertyValue switch clause

class PropertyValue
{
public:
  enum Type { type_none, type_string, type_float, type_bool,
	      type_library, type_cell, type_port,
	      type_liberty_library, type_liberty_cell, type_liberty_port,
	      type_instance, type_pin, type_pins, type_net,
	      type_clk, type_clks, type_path_refs, type_pwr_activity };
  PropertyValue();
  PropertyValue(const char *value);
  PropertyValue(string &value);
  PropertyValue(float value);
  PropertyValue(bool value);
  PropertyValue(Library *value);
  PropertyValue(Cell *value);
  PropertyValue(Port *value);
  PropertyValue(LibertyLibrary *value);
  PropertyValue(LibertyCell *value);
  PropertyValue(LibertyPort *value);
  PropertyValue(Instance *value);
  PropertyValue(Pin *value);
  PropertyValue(PinSeq *value);
  PropertyValue(PinSet *value);
  PropertyValue(Net *value);
  PropertyValue(Clock *value);
  PropertyValue(ClockSeq *value);
  PropertyValue(ClockSet *value);
  PropertyValue(PathRefSeq *value);
  PropertyValue(PwrActivity *value);
  // Copy constructor.
  PropertyValue(const PropertyValue &props);
  // Move constructor.
  PropertyValue(PropertyValue &&props);
  ~PropertyValue();
  Type type() const { return type_; }
  const char *stringValue() const { return string_; }
  float floatValue() const { return float_; }
  bool boolValue() const { return bool_; }
  LibertyLibrary *libertyLibrary() const { return liberty_library_; }
  LibertyCell *libertyCell() const { return liberty_cell_; }
  LibertyPort *libertyPort() const { return liberty_port_; }
  Library *library() const { return library_; }
  Cell *cell() const { return cell_; }
  Port *port() const { return port_; }
  Instance *instance() const { return inst_; }
  Pin *pin() const { return pin_; }
  PinSeq *pins() const { return pins_; }
  Net *net() const { return net_; }
  Clock *clock() const { return clk_; }
  ClockSeq *clocks() const { return clks_; }
  PathRefSeq *pathRefs() const { return path_refs_; }
  PwrActivity pwrActivity() const { return pwr_activity_; }
  // Copy assignment.
  PropertyValue &operator=(const PropertyValue &);
  // Move assignment.
  PropertyValue &operator=(PropertyValue &&);

private:
  Type type_;
  union {
    const char *string_;
    float float_;
    bool bool_;
    Library *library_;
    Cell *cell_;
    Port *port_;
    LibertyLibrary *liberty_library_;
    LibertyCell *liberty_cell_;
    LibertyPort *liberty_port_;
    Instance *inst_;
    Pin *pin_;
    PinSeq *pins_;
    Net *net_;
    Clock *clk_;
    ClockSeq *clks_;
    PathRefSeq *path_refs_;
    PwrActivity pwr_activity_;
  };
};

PropertyValue
getProperty(const Instance *inst,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const Pin *pin,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const Net *net,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const Port *port,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const Cell *cell,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const LibertyCell *cell,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const LibertyPort *port,
	    const char *property,
	    Sta *);

PropertyValue
getProperty(const LibertyLibrary *lib,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(const Library *lib,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(Edge *edge,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(Clock *clk,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(PathEnd *end,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(PathRef *end,
	    const char *property,
	    Sta *sta);

PropertyValue
getProperty(TimingArcSet *arc_set,
	    const char *property,
	    Sta *sta);

} // namespace
