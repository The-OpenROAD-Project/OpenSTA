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

#pragma once

#include <string>

#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SearchClass.hh"
#include "SdcClass.hh"
#include "PowerClass.hh"

namespace sta {

using std::string;

class Sta;

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
  PropertyValue(float value,
                const Unit *unit);
  explicit PropertyValue(bool value);
  PropertyValue(const Library *value);
  PropertyValue(const Cell *value);
  PropertyValue(const Port *value);
  PropertyValue(const LibertyLibrary *value);
  PropertyValue(const LibertyCell *value);
  PropertyValue(const LibertyPort *value);
  PropertyValue(const Instance *value);
  PropertyValue(const Pin *value);
  PropertyValue(PinSeq *value);
  PropertyValue(PinSet *value);
  PropertyValue(const PinSet &value);
  PropertyValue(const Net *value);
  PropertyValue(const Clock *value);
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
  const Unit *unit() const { return unit_; }

  const char *asString(const Network *network) const;
  const char *stringValue() const; // valid for type string
  float floatValue() const;        // valid for type float
  bool boolValue() const;          // valid for type bool
  const LibertyLibrary *libertyLibrary() const { return liberty_library_; }
  const LibertyCell *libertyCell() const { return liberty_cell_; }
  const LibertyPort *libertyPort() const { return liberty_port_; }
  const Library *library() const { return library_; }
  const Cell *cell() const { return cell_; }
  const Port *port() const { return port_; }
  const Instance *instance() const { return inst_; }
  const Pin *pin() const { return pin_; }
  PinSeq *pins() const { return pins_; }
  const Net *net() const { return net_; }
  const Clock *clock() const { return clk_; }
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
    const Library *library_;
    const Cell *cell_;
    const Port *port_;
    const LibertyLibrary *liberty_library_;
    const LibertyCell *liberty_cell_;
    const LibertyPort *liberty_port_;
    const Instance *inst_;
    const Pin *pin_;
    PinSeq *pins_;
    const Net *net_;
    const Clock *clk_;
    ClockSeq *clks_;
    PathRefSeq *path_refs_;
    PwrActivity pwr_activity_;
  };
  const Unit *unit_;
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
