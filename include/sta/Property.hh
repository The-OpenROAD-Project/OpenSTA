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

#pragma once

#include <map>
#include <string>
#include <functional>

#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SearchClass.hh"
#include "SdcClass.hh"
#include "PowerClass.hh"

namespace sta {

class Sta;
class PropertyValue;

class Sta;
class PropertyValue;

template<class TYPE>
class PropertyRegistry
{
public:
  typedef std::function<PropertyValue (TYPE object, Sta *sta)> PropertyHandler;
  void defineProperty(const std::string &property,
                      PropertyHandler handler);
  PropertyValue getProperty(TYPE object,
                            const std::string &property,
                            const char *type_name,
                            Sta *sta);

private:
  std::map<std::string, PropertyHandler> registry_;
};

class Properties
{
public:
  Properties(Sta *sta);
  virtual ~Properties() {}

  PropertyValue getProperty(const Library *lib,
                            const std::string property);
  PropertyValue getProperty(const LibertyLibrary *lib,
                            const std::string property);
  PropertyValue getProperty(const Cell *cell,
                            const std::string property);
  PropertyValue getProperty(const LibertyCell *cell,
                            const std::string property);
  PropertyValue getProperty(const Port *port,
                            const std::string property);
  PropertyValue getProperty(const LibertyPort *port,
                            const std::string property);
  PropertyValue getProperty(const Instance *inst,
                            const std::string property);
  PropertyValue getProperty(const Pin *pin,
                            const std::string property);
  PropertyValue getProperty(const Net *net,
                            const std::string property);
  PropertyValue getProperty(Edge *edge,
                            const std::string property);
  PropertyValue getProperty(const Clock *clk,
                            const std::string property);
  PropertyValue getProperty(PathEnd *end,
                            const std::string property);
  PropertyValue getProperty(Path *path,
                            const std::string property);
  PropertyValue getProperty(TimingArcSet *arc_set,
                            const std::string property);

  // Define handler for external property.
  // properties->defineProperty("foo",
  //                            [] (const Instance *, Sta *) -> PropertyValue {
  //                              return PropertyValue("bar");
  //                            });
  void defineProperty(std::string &property,
                      PropertyRegistry<const Library *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const LibertyLibrary *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Cell *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const LibertyCell *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Port *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const LibertyPort *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Instance *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Pin *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Net *>::PropertyHandler handler);
  void defineProperty(std::string &property,
                      PropertyRegistry<const Clock *>::PropertyHandler handler);

protected:
  PropertyValue portSlew(const Port *port,
                         const MinMax *min_max);
  PropertyValue portSlew(const Port *port,
                         const RiseFall *rf,
                         const MinMax *min_max);
  PropertyValue portSlack(const Port *port,
                          const MinMax *min_max);
  PropertyValue portSlack(const Port *port,
                          const RiseFall *rf,
                          const MinMax *min_max);
  PropertyValue pinArrival(const Pin *pin,
                           const RiseFall *rf,
                           const MinMax *min_max);

  PropertyValue pinSlack(const Pin *pin,
                         const MinMax *min_max);
  PropertyValue pinSlack(const Pin *pin,
                         const RiseFall *rf,
                         const MinMax *min_max);
  PropertyValue pinSlew(const Pin *pin,
                        const MinMax *min_max);
  PropertyValue pinSlew(const Pin *pin,
                        const RiseFall *rf,
                        const MinMax *min_max);

  PropertyValue delayPropertyValue(Delay delay);
  PropertyValue resistancePropertyValue(float res);
  PropertyValue capacitancePropertyValue(float cap);
  PropertyValue edgeDelay(Edge *edge,
                          const RiseFall *rf,
                          const MinMax *min_max);

  PropertyRegistry<const Library*> registry_library_;
  PropertyRegistry<const LibertyLibrary*> registry_liberty_library_;
  PropertyRegistry<const Cell*> registry_cell_;
  PropertyRegistry<const LibertyCell*> registry_liberty_cell_;
  PropertyRegistry<const Port*> registry_port_;
  PropertyRegistry<const LibertyPort*> registry_liberty_port_;
  PropertyRegistry<const Instance*> registry_instance_;
  PropertyRegistry<const Pin*> registry_pin_;
  PropertyRegistry<const Net*> registry_net_;
  PropertyRegistry<const Clock*> registry_clock_;

  Sta *sta_;
};

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
	      type_clk, type_clks, type_paths, type_pwr_activity };
  PropertyValue();
  PropertyValue(const char *value);
  PropertyValue(std::string &value);
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
  PropertyValue(ConstPathSeq *value);
  PropertyValue(PwrActivity *value);
  // Copy constructor.
  PropertyValue(const PropertyValue &props);
  // Move constructor.
  PropertyValue(PropertyValue &&props);
  ~PropertyValue();
  Type type() const { return type_; }
  const Unit *unit() const { return unit_; }

  std::string to_string(const Network *network) const;
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
  ConstPathSeq *paths() const { return paths_; }
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
    ConstPathSeq *paths_;
    PwrActivity pwr_activity_;
  };
  const Unit *unit_;
};

} // namespace
