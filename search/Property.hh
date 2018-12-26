// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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

#ifndef STA_PROPERTY_H
#define STA_PROPERTY_H

#include "NetworkClass.hh"
#include "SearchClass.hh"

namespace sta {

class Sta;

class PropertyValue
{
public:
  enum Type { type_none, type_string, type_float,
	      type_instance, type_pin, type_pins, type_net,
	      type_clock, type_clocks, type_path_refs };
  PropertyValue();
  PropertyValue(const char *value);
  PropertyValue(float value);
  PropertyValue(Instance *value);
  PropertyValue(Pin *value);
  PropertyValue(PinSeq *value);
  PropertyValue(PinSet *value);
  PropertyValue(Net *value);
  PropertyValue(Clock *value);
  PropertyValue(ClockSeq *value);
  PropertyValue(ClockSet *value);
  PropertyValue(PathRefSeq *value);
  // Copy constructor.
  PropertyValue(const PropertyValue &props);
  ~PropertyValue();
  Type type() const { return type_; }
  const char *string() const { return string_; }
  float floatValue() const { return float_; }
  Instance *instance() const { return inst_; }
  Pin *pin() const { return pin_; }
  PinSeq *pins() const { return pins_; }
  Net *net() const { return net_; }
  Clock *clock() const { return clk_; }
  ClockSeq *clocks() const { return clks_; }
  PathRefSeq *pathRefs() const { return path_refs_; }
  void operator=(const PropertyValue &);

private:
  void init();

  Type type_;
  const char *string_;
  float float_;
  Instance *inst_;
  Pin *pin_;
  PinSeq *pins_;
  Net *net_;
  Clock *clk_;
  ClockSeq *clks_;
  PathRefSeq *path_refs_;
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

} // namespace
#endif
