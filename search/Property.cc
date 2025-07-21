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

#include "Property.hh"

#include "StringUtil.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Corner.hh"
#include "PathEnd.hh"
#include "PathExpanded.hh"
#include "Path.hh"
#include "power/Power.hh"
#include "Sta.hh"

namespace sta {

using std::string;
using std::max;

class PropertyUnknown : public Exception
{
public:
  PropertyUnknown(const char *type,
		  const char *property);
  PropertyUnknown(const char *type,
		  const string property);
  virtual ~PropertyUnknown() {}
  virtual const char *what() const noexcept;

private:
  const char *type_;
  const string property_;
};

PropertyUnknown::PropertyUnknown(const char *type,
				 const char *property) :
  Exception(),
  type_(type),
  property_(property)
{
}

PropertyUnknown::PropertyUnknown(const char *type,
				 const string property) :
  Exception(),
  type_(type),
  property_(property)
{
}

const char *
PropertyUnknown::what() const noexcept
{
  return stringPrint("%s objects do not have a %s property.",
		     type_, property_.c_str());
}

////////////////////////////////////////////////////////////////

class PropertyTypeWrong : public Exception
{
public:
  PropertyTypeWrong(const char *accessor,
                    const char *type);
  virtual ~PropertyTypeWrong() {}
  virtual const char *what() const noexcept;

private:
  const char *accessor_;
  const char *type_;
};

PropertyTypeWrong::PropertyTypeWrong(const char *accessor,
                                     const char *type) :
  Exception(),
  accessor_(accessor),
  type_(type)
{
}

const char *
PropertyTypeWrong::what() const noexcept
{
  return stringPrint("property accessor %s is only valid for %s properties.",
		     accessor_, type_);
}
////////////////////////////////////////////////////////////////

PropertyValue::PropertyValue() :
  type_(type_none),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const char *value) :
  type_(type_string),
  string_(stringCopy(value)),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(std::string &value) :
  type_(type_string),
  string_(stringCopy(value.c_str())),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(float value,
                             const Unit *unit) :
  type_(type_float),
  float_(value),
  unit_(unit)
{
}

PropertyValue::PropertyValue(bool value) :
  type_(type_bool),
  bool_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const LibertyLibrary *value) :
  type_(type_liberty_library),
  liberty_library_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const LibertyCell *value) :
  type_(type_liberty_cell),
  liberty_cell_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const LibertyPort *value) :
  type_(type_liberty_port),
  liberty_port_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Library *value) :
  type_(type_library),
  library_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Cell *value) :
  type_(type_cell),
  cell_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Port *value) :
  type_(type_port),
  port_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Instance *value) :
  type_(type_instance),
  inst_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Pin *value) :
  type_(type_pin),
  pin_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(PinSeq *value) :
  type_(type_pins),
  pins_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(PinSet *value) :
  type_(type_pins),
  pins_(new PinSeq),
  unit_(nullptr)
{
  PinSet::Iterator pin_iter(value);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    pins_->push_back( pin);
  }
}

PropertyValue::PropertyValue(const PinSet &value) :
  type_(type_pins),
  pins_(new PinSeq),
  unit_(nullptr)
{
  PinSet::ConstIterator pin_iter(value);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    pins_->push_back( pin);
  }
}

PropertyValue::PropertyValue(const Net *value) :
  type_(type_net),
  net_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const Clock *value) :
  type_(type_clk),
  clk_(value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(ClockSeq *value) :
  type_(type_clks),
  clks_(new ClockSeq(*value)),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(ClockSet *value) :
  type_(type_clks),
  clks_(new ClockSeq),
  unit_(nullptr)
{
  ClockSet::Iterator clk_iter(value);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    clks_->push_back(clk);
  }
}

PropertyValue::PropertyValue(ConstPathSeq *value) :
  type_(type_paths),
  paths_(new ConstPathSeq(*value)),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(PwrActivity *value) :
  type_(type_pwr_activity),
  pwr_activity_(*value),
  unit_(nullptr)
{
}

PropertyValue::PropertyValue(const PropertyValue &value) :
  type_(value.type_),
  unit_(value.unit_)
{
  switch (type_) {
  case Type::type_none:
    break;
  case Type::type_string:
    string_ = stringCopy(value.string_);
    break;
  case Type::type_float:
    float_ = value.float_;
    break;
  case Type::type_bool:
    bool_ = value.bool_;
    break;
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_liberty_port:
    liberty_port_ = value.liberty_port_;
    break;
  case Type::type_library:
    library_ = value.library_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_port:
    port_ = value.port_;
    break;
  case Type::type_instance:
    inst_ = value.inst_;
    break;
  case Type::type_pin:
    pin_ = value.pin_;
    break;
  case Type::type_pins:
    pins_ = value.pins_ ? new PinSeq(*value.pins_) : nullptr;
    break;
  case Type::type_net:
    net_ = value.net_;
    break;
  case Type::type_clk:
    clk_ = value.clk_;
    break;
  case Type::type_clks:
    clks_ = value.clks_ ? new ClockSeq(*value.clks_) : nullptr;
    break;
  case Type::type_paths:
    paths_ = value.paths_ ? new ConstPathSeq(*value.paths_) : nullptr;
    break;
  case Type::type_pwr_activity:
    pwr_activity_ = value.pwr_activity_;
    break;
  }
}

PropertyValue::PropertyValue(PropertyValue &&value) :
  type_(value.type_),
  unit_(value.unit_)

{
  switch (type_) {
  case Type::type_none:
    break;
  case Type::type_string:
    string_ = value.string_;
    value.string_ = nullptr;
    break;
  case Type::type_float:
    float_ = value.float_;
    break;
  case Type::type_bool:
    bool_ = value.bool_;
    break;
  case Type::type_library:
    library_ = value.library_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_port:
    port_ = value.port_;
    break;
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_liberty_port:
    liberty_port_ = value.liberty_port_;
    break;
  case Type::type_instance:
    inst_ = value.inst_;
    break;
  case Type::type_pin:
    pin_ = value.pin_;
    break;
  case Type::type_pins:
    pins_ = value.pins_;
    value.pins_ = nullptr;
    break;
  case Type::type_net:
    net_ = value.net_;
    break;
  case Type::type_clk:
    clk_ = value.clk_;
    break;
  case Type::type_clks:
    clks_ = value.clks_;
    // Steal the value.
    value.clks_ = nullptr;
    break;
  case Type::type_paths:
    paths_ = value.paths_;
    // Steal the value.
    value.clks_ = nullptr;
    break;
  case Type::type_pwr_activity:
    pwr_activity_ = value.pwr_activity_;
    break;
  }
}

PropertyValue::~PropertyValue()
{  
  switch (type_) {
  case Type::type_string:
    stringDelete(string_);
    break;
  case Type::type_clks:
    delete clks_;
    break;
  case Type::type_pins:
    delete pins_;
    break;
  case Type::type_paths:
    delete paths_;
    break;
  default:
    break;
  }
}

PropertyValue &
PropertyValue::operator=(const PropertyValue &value)
{
  type_ = value.type_;
  unit_ = value.unit_;

  switch (type_) {
  case Type::type_none:
    break;
  case Type::type_string:
    string_ = stringCopy(value.string_);
    break;
  case Type::type_float:
    float_ = value.float_;
    break;
  case Type::type_bool:
    bool_ = value.bool_;
    break;
  case Type::type_library:
    library_ = value.library_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_port:
    port_ = value.port_;
    break;
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_liberty_port:
    liberty_port_ = value.liberty_port_;
    break;
  case Type::type_instance:
    inst_ = value.inst_;
    break;
  case Type::type_pin:
    pin_ = value.pin_;
    break;
  case Type::type_pins:
    pins_ = value.pins_ ? new PinSeq(*value.pins_) : nullptr;
    break;
  case Type::type_net:
    net_ = value.net_;
    break;
  case Type::type_clk:
    clk_ = value.clk_;
    break;
  case Type::type_clks:
    clks_ = value.clks_ ? new ClockSeq(*value.clks_) : nullptr;
    break;
  case Type::type_paths:
    paths_ = value.paths_ ? new ConstPathSeq(*value.paths_) : nullptr;
    break;
  case Type::type_pwr_activity:
    pwr_activity_ = value.pwr_activity_;
    break;
  }
  return *this;
}

PropertyValue &
PropertyValue::operator=(PropertyValue &&value)
{
  type_ = value.type_;
  unit_ = value.unit_;

  switch (type_) {
  case Type::type_none:
    break;
  case Type::type_string:
    string_ = value.string_;
    value.string_ = nullptr;
    break;
  case Type::type_float:
    float_ = value.float_;
    break;
  case Type::type_bool:
    bool_ = value.bool_;
    break;
  case Type::type_library:
    library_ = value.library_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_port:
    port_ = value.port_;
    break;
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_liberty_port:
    liberty_port_ = value.liberty_port_;
    break;
  case Type::type_instance:
    inst_ = value.inst_;
    break;
  case Type::type_pin:
    pin_ = value.pin_;
    break;
  case Type::type_pins:
    pins_ = value.pins_;
    value.pins_ = nullptr;
    break;
  case Type::type_net:
    net_ = value.net_;
    break;
  case Type::type_clk:
    clk_ = value.clk_;
    break;
  case Type::type_clks:
    clks_ = value.clks_;
    value.clks_ = nullptr;
    break;
  case Type::type_paths:
    paths_ = value.paths_;
    value.clks_ = nullptr;
    break;
  case Type::type_pwr_activity:
    pwr_activity_ = value.pwr_activity_;
    break;
  }
  return *this;
}

string
PropertyValue::to_string(const Network *network) const
{
  switch (type_) {
  case Type::type_string:
    return string_;
  case Type::type_float:
    return unit_->asString(float_, 6);
  case Type::type_bool:
    // true/false would be better but these are TCL true/false values.
    if (bool_)
      return "1";
    else
      return "0";
  case Type::type_liberty_library:
    return liberty_library_->name();
  case Type::type_liberty_cell:
    return liberty_cell_->name();
  case Type::type_liberty_port:
    return liberty_port_->name();
  case Type::type_library:
    return network->name(library_);
  case Type::type_cell:
    return network->name(cell_);
  case Type::type_port:
    return network->name(port_);
  case Type::type_instance:
    return network->pathName(inst_);
  case Type::type_pin:
    return network->pathName(pin_);
  case Type::type_net:
    return network->pathName(net_);
  case Type::type_clk:
    return clk_->name();
  case Type::type_none:
  case Type::type_pins:
  case Type::type_clks:
  case Type::type_paths:
  case Type::type_pwr_activity:
    return "";
  }
  return "";
}

const char *
PropertyValue::stringValue() const
{
  if (type_ != Type::type_string)
    throw PropertyTypeWrong("stringValue", "string");
  return string_;
}

float
PropertyValue::floatValue() const
{
  if (type_ != Type::type_float)
    throw PropertyTypeWrong("floatValue", "float");
  return float_;
}

bool
PropertyValue::boolValue() const
{
  if (type_ != Type::type_bool)
    throw PropertyTypeWrong("boolValue", "boolt");
  return bool_;
}

////////////////////////////////////////////////////////////////

Properties::Properties(Sta *sta) :
  sta_(sta)
{
}

PropertyValue
Properties::getProperty(const Library *lib,
                        const std::string property)
{
  Network *network = sta_->cmdNetwork();
  if (property == "name"
      || property == "full_name")
    return PropertyValue(network->name(lib));
  else {
    PropertyValue value = registry_library_.getProperty(lib, property,
                                                        "library", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("library", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const LibertyLibrary *lib,
                        const std::string property)
{
  if (property == "name"
      || property == "full_name")
    return PropertyValue(lib->name());
  else if (property == "filename")
    return PropertyValue(lib->filename());
  else {
    PropertyValue value = registry_liberty_library_.getProperty(lib, property,
                                                                "liberty_library",
                                                                sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("liberty library", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Cell *cell,
                        const std::string property)
{
  Network *network = sta_->cmdNetwork();
  if (property == "name"
      || property == "base_name")
    return PropertyValue(network->name(cell));
  else if (property == "full_name") {
    Library *lib = network->library(cell);
    string lib_name = network->name(lib);
    string cell_name = network->name(cell);
    string full_name = lib_name + network->pathDivider() + cell_name;
    return PropertyValue(full_name);
  }
  else if (property == "library")
    return PropertyValue(network->library(cell));
  else if (property == "filename")
    return PropertyValue(network->filename(cell));
  else {
    PropertyValue value = registry_cell_.getProperty(cell, property,
                                                     "cell", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("cell", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const LibertyCell *cell,
                        const std::string property)
{
  if (property == "name"
      || property == "base_name")
    return PropertyValue(cell->name());
  else if (property == "full_name") {
    Network *network = sta_->cmdNetwork();
    LibertyLibrary *lib = cell->libertyLibrary();
    string lib_name = lib->name();
    string cell_name = cell->name();
    string full_name = lib_name + network->pathDivider() + cell_name;
    return PropertyValue(full_name);
  }
  else if (property == "filename")
    return PropertyValue(cell->filename());
  else if (property == "library")
    return PropertyValue(cell->libertyLibrary());
  else if (property == "is_buffer")
    return PropertyValue(cell->isBuffer());
  else if (property =="is_inverter")
    return PropertyValue(cell->isInverter());
  else if (property == "is_memory")
    return PropertyValue(cell->isMemory());
  else if (property == "dont_use")
    return PropertyValue(cell->dontUse());
  else if (property == "area")
    return PropertyValue(cell->area(), sta_->units()->scalarUnit());
  else {
    PropertyValue value = registry_liberty_cell_.getProperty(cell, property,
                                                             "liberty_cell", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("liberty cell", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Port *port,
                        const std::string property)
{
  Network *network = sta_->cmdNetwork();
  if (property == "name"
	   || property == "full_name")
    return PropertyValue(network->name(port));
  else if (property == "direction"
	   || property == "port_direction")
    return PropertyValue(network->direction(port)->name());
  else if (property == "liberty_port")
    return PropertyValue(network->libertyPort(port));

  else if (property == "activity") {
    const Instance *top_inst = network->topInstance();
    const Pin *pin = network->findPin(top_inst, port);
    PwrActivity activity = sta_->activity(pin);
    return PropertyValue(&activity);
  }

  else if (property == "slack_max")
    return portSlack(port, MinMax::max());
  else if (property == "slack_max_fall")
    return portSlack(port, RiseFall::fall(), MinMax::max());
  else if (property == "slack_max_rise")
    return portSlack(port, RiseFall::rise(), MinMax::max());
  else if (property == "slack_min")
    return portSlack(port, MinMax::min());
  else if (property == "slack_min_fall")
    return portSlack(port, RiseFall::fall(), MinMax::min());
  else if (property == "slack_min_rise")
    return portSlack(port, RiseFall::rise(), MinMax::min());

  else if (property == "slew_max")
    return portSlew(port, MinMax::max());
  else if (property == "slew_max_fall")
    return portSlew(port, RiseFall::fall(), MinMax::max());
  else if (property == "slew_max_rise")
    return portSlew(port, RiseFall::rise(), MinMax::max());
  else if (property == "slew_min")
    return portSlew(port, MinMax::min());
  else if (property == "slew_min_rise")
    return portSlew(port, RiseFall::rise(), MinMax::min());
  else if (property == "slew_min_fall")
    return portSlew(port, RiseFall::fall(), MinMax::min());

  else {
    PropertyValue value = registry_port_.getProperty(port, property,
                                                     "port", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("port", property);
  }
}

PropertyValue
Properties::portSlew(const Port *port,
                     const MinMax *min_max)
{
  Network *network = sta_->ensureLibLinked();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlew(pin, min_max);
}

PropertyValue
Properties::portSlew(const Port *port,
                     const RiseFall *rf,
                     const MinMax *min_max)
{
  Network *network = sta_->ensureLibLinked();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlew(pin, rf, min_max);
}

PropertyValue
Properties::portSlack(const Port *port,
                      const MinMax *min_max)
{
  Network *network = sta_->ensureLibLinked();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlack(pin, min_max);
}

PropertyValue
Properties::portSlack(const Port *port,
                      const RiseFall *rf,
                      const MinMax *min_max)
{
  Network *network = sta_->ensureLibLinked();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlack(pin, rf, min_max);
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const LibertyPort *port,
                        const std::string property)
{
  if (property == "name")
    return PropertyValue(port->name());
  else if (property == "full_name")
    return PropertyValue(port->name());
  else if (property == "lib_cell")
    return PropertyValue(port->libertyCell());
  else if (property == "direction"
	   || property == "port_direction")
    return PropertyValue(port->direction()->name());
  else if (property == "capacitance") {
    float cap = port->capacitance(RiseFall::rise(), MinMax::max());
    return capacitancePropertyValue(cap);
  }
  else if (property == "is_clock")
    return PropertyValue(port->isClock());
  else if (property == "is_register_clock")
    return PropertyValue(port->isRegClk());

  else if (property == "drive_resistance") {
    float res = port->driveResistance();
    return resistancePropertyValue(res);
  }
  else if (property == "drive_resistance_min_rise") {
    float res = port->driveResistance(RiseFall::rise(), MinMax::min());
    return resistancePropertyValue(res);
  }
  else if (property == "drive_resistance_max_rise") {
    float res = port->driveResistance(RiseFall::rise(), MinMax::max());
    return resistancePropertyValue(res);
  }
  else if (property == "drive_resistance_min_fall") {
    float res = port->driveResistance(RiseFall::fall(), MinMax::min());
    return resistancePropertyValue(res);
  }
  else if (property == "drive_resistance_max_fall") {
    float res = port->driveResistance(RiseFall::fall(), MinMax::max());
    return resistancePropertyValue(res);
  }

  else if (property == "intrinsic_delay") {
    ArcDelay delay = port->intrinsicDelay(sta_);
    return delayPropertyValue(delay);
  }
  else if (property == "intrinsic_delay_min_rise") {
    ArcDelay delay = port->intrinsicDelay(RiseFall::rise(),
                                          MinMax::min(), sta_);
    return delayPropertyValue(delay);
  }
  else if (property == "intrinsic_delay_max_rise") {
    ArcDelay delay = port->intrinsicDelay(RiseFall::rise(),
                                          MinMax::max(), sta_);
    return delayPropertyValue(delay);
  }
  else if (property == "intrinsic_delay_min_fall") {
    ArcDelay delay = port->intrinsicDelay(RiseFall::fall(),
                                          MinMax::min(), sta_);
    return delayPropertyValue(delay);
  }
  else if (property == "intrinsic_delay_max_fall") {
    ArcDelay delay = port->intrinsicDelay(RiseFall::fall(),
                                          MinMax::max(), sta_);
    return delayPropertyValue(delay);
  }
   else {
    PropertyValue value = registry_liberty_port_.getProperty(port, property,
                                                             "liberty_port", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("liberty port", property);
   }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Instance *inst,
                        const string property)
{
  Network *network = sta_->ensureLinked();
  LibertyCell *liberty_cell = network->libertyCell(inst);
  if (property == "name")
    return PropertyValue(network->name(inst));
  else if (property == "full_name")
    return PropertyValue(network->pathName(inst));
  else if (property == "ref_name")
    return PropertyValue(network->name(network->cell(inst)));
  else if (property ==  "liberty_cell")
    return PropertyValue(network->libertyCell(inst));
  else if (property == "cell")
    return PropertyValue(network->cell(inst));
  else if (property == "is_hierarchical")
    return PropertyValue(network->isHierarchical(inst));
  else if (property == "is_buffer")
    return PropertyValue(liberty_cell && liberty_cell->isBuffer());
  else if (property == "is_clock_gate")
    return PropertyValue(liberty_cell && liberty_cell->isClockGate());
  else if (property == "is_inverter")
    return PropertyValue(liberty_cell && liberty_cell->isInverter());
  else if (property == "is_macro")
    return PropertyValue(liberty_cell && liberty_cell->isMacro());
  else if (property == "is_memory")
    return PropertyValue(liberty_cell && liberty_cell->isMemory());
  else {
    PropertyValue value = registry_instance_.getProperty(inst, property,
                                                         "instance", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("instance", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Pin *pin,
                        const std::string property)
{
  Network *network = sta_->ensureLinked();
  if (property == "name"
      || property == "lib_pin_name")
    return PropertyValue(network->portName(pin));
  else if (property == "full_name")
    return PropertyValue(network->pathName(pin));
  else if (property == "direction"
  	   || property == "pin_direction")
    return PropertyValue(network->direction(pin)->name());
  else if (property == "is_hierarchical")
    return PropertyValue(network->isHierarchical(pin));
  else if (property == "is_port")
    return PropertyValue(network->isTopLevelPort(pin));
  else if (property == "is_clock") {
    const LibertyPort *port = network->libertyPort(pin);
    return PropertyValue(port->isClock());
  }
  else if (property == "is_register_clock") {
    const LibertyPort *port = network->libertyPort(pin);
    return PropertyValue(port && port->isRegClk());
  }
  else if (property == "clocks") {
    ClockSet clks = sta_->clocks(pin);
    return PropertyValue(&clks);
  }
  else if (property == "clock_domains") {
    ClockSet clks = sta_->clockDomains(pin);
    return PropertyValue(&clks);
  }
  else if (property == "activity") {
    PwrActivity activity = sta_->activity(pin);
    return PropertyValue(&activity);
  }

  else if (property == "arrival_max_rise")
    return pinArrival(pin, RiseFall::rise(), MinMax::max());
  else if (property == "arrival_max_fall")
    return pinArrival(pin, RiseFall::fall(), MinMax::max());
  else if (property == "arrival_min_rise")
    return pinArrival(pin, RiseFall::rise(), MinMax::min());
  else if (property == "arrival_min_fall")
    return pinArrival(pin, RiseFall::fall(), MinMax::min());

  else if (property == "slack_max")
    return pinSlack(pin, MinMax::max());
  else if (property == "slack_max_fall")
    return pinSlack(pin, RiseFall::fall(), MinMax::max());
  else if (property == "slack_max_rise")
    return pinSlack(pin, RiseFall::rise(), MinMax::max());
  else if (property == "slack_min")
    return pinSlack(pin, MinMax::min());
  else if (property == "slack_min_fall")
    return pinSlack(pin, RiseFall::fall(), MinMax::min());
  else if (property == "slack_min_rise")
    return pinSlack(pin, RiseFall::rise(), MinMax::min());

  else if (property == "slew_max")
    return pinSlew(pin, MinMax::max());
  else if (property == "slew_max_fall")
    return pinSlew(pin, RiseFall::fall(), MinMax::max());
  else if (property == "slew_max_rise")
    return pinSlew(pin, RiseFall::rise(), MinMax::max());
  else if (property == "slew_min")
    return pinSlew(pin, MinMax::min());
  else if (property == "slew_min_rise")
    return pinSlew(pin, RiseFall::rise(), MinMax::min());
  else if (property == "slew_min_fall")
    return pinSlew(pin, RiseFall::fall(), MinMax::min());

  else {
    PropertyValue value = registry_pin_.getProperty(pin, property, "pin", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("pin", property);
  }
}

PropertyValue
Properties::pinArrival(const Pin *pin,
                       const RiseFall *rf,
                       const MinMax *min_max)
{
  Arrival arrival = sta_->pinArrival(pin, rf, min_max);;
  return PropertyValue(delayPropertyValue(arrival));
}

PropertyValue
Properties::pinSlack(const Pin *pin,
                     const MinMax *min_max)
{
  Slack slack = sta_->pinSlack(pin, min_max);
  return PropertyValue(delayPropertyValue(slack));
}

PropertyValue
Properties::pinSlack(const Pin *pin,
                     const RiseFall *rf,
                     const MinMax *min_max)
{
  Slack slack = sta_->pinSlack(pin, rf, min_max);
  return PropertyValue(delayPropertyValue(slack));
}

PropertyValue
Properties::pinSlew(const Pin *pin,
                    const MinMax *min_max)
{
  Graph *graph = sta_->ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta_->vertexSlew(vertex, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta_))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta_->vertexSlew(bidirect_drvr_vertex, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta_))
      slew = vertex_slew;
  }
  return delayPropertyValue(slew);
}

PropertyValue
Properties::pinSlew(const Pin *pin,
                    const RiseFall *rf,
                    const MinMax *min_max)
{
  Graph *graph = sta_->ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta_->vertexSlew(vertex, rf, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta_))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta_->vertexSlew(bidirect_drvr_vertex, rf, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta_))
      slew = vertex_slew;
  }
  return delayPropertyValue(slew);
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Net *net,
                        const std::string property)
{
  Network *network = sta_->ensureLinked();
  if (property == "name")
    return PropertyValue(network->name(net));
  else if (property == "full_name")
    return PropertyValue(network->pathName(net));
  else {
    PropertyValue value = registry_net_.getProperty(net, property, "net", sta_);
    if (value.type() != PropertyValue::Type::type_none)
      return value;
    else
      throw PropertyUnknown("net", property);
  }
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(Edge *edge,
                        const std::string property)
{
  if (property == "full_name") {
    string full_name = edge->to_string(sta_);
    return PropertyValue(full_name);
  }
  if (property == "delay_min_fall")
    return edgeDelay(edge, RiseFall::fall(), MinMax::min());
  else if (property == "delay_max_fall")
    return edgeDelay(edge, RiseFall::fall(), MinMax::max());
  else if (property == "delay_min_rise")
    return edgeDelay(edge, RiseFall::rise(), MinMax::min());
  else if (property == "delay_max_rise")
    return edgeDelay(edge, RiseFall::rise(), MinMax::max());
  else if (property == "sense")
    return PropertyValue(to_string(edge->sense()));
  else if (property == "from_pin")
    return PropertyValue(edge->from(sta_->graph())->pin());
  else if (property == "to_pin")
    return PropertyValue(edge->to(sta_->graph())->pin());
  else
      throw PropertyUnknown("edge", property);
}

PropertyValue
Properties::edgeDelay(Edge *edge,
                      const RiseFall *rf,
                      const MinMax *min_max)
{
  ArcDelay delay = 0.0;
  bool delay_exists = false;
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    const RiseFall *to_rf = arc->toEdge()->asRiseFall();
    if (to_rf == rf) {
      for (const Corner *corner : *sta_->corners()) {
	DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
	ArcDelay arc_delay = sta_->arcDelay(edge, arc, dcalc_ap);
	if (!delay_exists
	    || delayGreater(arc_delay, delay, min_max, sta_)) {
	  delay = arc_delay;
          delay_exists = true;
        }
      }
    }
  }
  return delayPropertyValue(delay);
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(TimingArcSet *arc_set,
                        const std::string property)
{
  if (property == "name"
      || property == "full_name") {
    if (arc_set->isWire())
      return PropertyValue("wire");
    else {
      const char *from = arc_set->from()->name();
      const char *to = arc_set->to()->name();
      const char *cell_name = arc_set->libertyCell()->name();
      string name;
      stringPrint(name, "%s %s -> %s", cell_name, from, to);
      return PropertyValue(name);
    }
  }
  else
    throw PropertyUnknown("timing arc", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(const Clock *clk,
                        const std::string property)
{
  if (property == "name"
      || property == "full_name")
    return PropertyValue(clk->name());
  else if (property == "period")
    return PropertyValue(clk->period(), sta_->units()->timeUnit());
  else if (property == "sources")
    return PropertyValue(clk->pins());
  else if (property == "is_generated")
    return PropertyValue(clk->isGenerated());
  else if (property == "is_virtual")
    return PropertyValue(clk->isVirtual());
  else if (property == "is_propagated")
    return PropertyValue(clk->isPropagated());
  else
    throw PropertyUnknown("clock", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
Properties::getProperty(PathEnd *end,
                        const std::string property)
{
  if (property == "startpoint") {
    PathExpanded expanded(end->path(), sta_);
    return PropertyValue(expanded.startPath()->pin(sta_));
  }
  else if (property == "startpoint_clock")
    return PropertyValue(end->path()->clock(sta_));
  else if (property == "endpoint")
    return PropertyValue(end->path()->pin(sta_));
  else if (property == "endpoint_clock")
    return PropertyValue(end->targetClk(sta_));
  else if (property == "endpoint_clock_pin")
    return PropertyValue(end->targetClkPath()->pin(sta_));
  else if (property == "slack")
    return PropertyValue(delayPropertyValue(end->slack(sta_)));
  else if (property == "points") {
    PathExpanded expanded(end->path(), sta_);
    ConstPathSeq paths;
    for (size_t i = expanded.startIndex(); i < expanded.size(); i++) {
      const Path *path = expanded.path(i);
      paths.push_back(path);
    }
    return PropertyValue(&paths);
  }
  else
    throw PropertyUnknown("path end", property);
}

PropertyValue
Properties::getProperty(Path *path,
                        const std::string property)
{
  if (property == "pin")
    return PropertyValue(path->pin(sta_));
  else if (property == "arrival")
    return PropertyValue(delayPropertyValue(path->arrival()));
  else if (property == "required")
    return PropertyValue(delayPropertyValue(path->required()));
  else if (property == "slack")
    return PropertyValue(delayPropertyValue(path->slack(sta_)));
  else
    throw PropertyUnknown("path", property);
}

PropertyValue
Properties::delayPropertyValue(Delay delay)
{
  return PropertyValue(delayAsFloat(delay), sta_->units()->timeUnit());
}

PropertyValue
Properties::resistancePropertyValue(float res)
{
  return PropertyValue(res, sta_->units()->resistanceUnit());
}

PropertyValue
Properties::capacitancePropertyValue(float cap)
{
  return PropertyValue(cap, sta_->units()->capacitanceUnit());
}

////////////////////////////////////////////////////////////////

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Library *>::PropertyHandler handler)
{
  registry_library_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const LibertyLibrary *>::PropertyHandler handler)
{
  registry_liberty_library_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Cell *>::PropertyHandler handler)
{
  registry_cell_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const LibertyCell *>::PropertyHandler handler)
{
  registry_liberty_cell_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Port *>::PropertyHandler handler)
{
  registry_port_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const LibertyPort *>::PropertyHandler handler)
{
  registry_liberty_port_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Instance *>::PropertyHandler handler)
{
  registry_instance_.defineProperty(property, handler);
}  

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Pin *>::PropertyHandler handler)
{
  registry_pin_.defineProperty(property, handler);
}

void
Properties::defineProperty(std::string &property,
                           PropertyRegistry<const Net *>::PropertyHandler handler)
{
  registry_net_.defineProperty(property, handler);
}

////////////////////////////////////////////////////////////////

template<class TYPE>
PropertyValue
PropertyRegistry<TYPE>::getProperty(TYPE object,
                                    const std::string &property,
                                    const char *type_name,
                                    Sta *sta)

{
  auto itr = registry_.find({property});
  if (itr != registry_.end())
    return itr->second(object, sta);
  else
    throw PropertyUnknown(type_name, property);
}

template<class TYPE>
void
PropertyRegistry<TYPE>::defineProperty(const std::string &property,
                                       PropertyHandler handler)
{
  registry_[property] = handler;
}

} // namespace
