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
#include "PathRef.hh"
#include "power/Power.hh"
#include "Sta.hh"

namespace sta {

using std::string;
using std::max;

static PropertyValue
pinSlewProperty(const Pin *pin,
		const MinMax *min_max,
		Sta *sta);
static PropertyValue
pinSlewProperty(const Pin *pin,
		const RiseFall *rf,
		const MinMax *min_max,
		Sta *sta);
static PropertyValue
pinArrivalProperty(const Pin *pin,
                   const RiseFall *rf,
                   const MinMax *min_max,
                   Sta *sta);
static PropertyValue
pinSlackProperty(const Pin *pin,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
pinSlackProperty(const Pin *pin,
		 const RiseFall *rf,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlewProperty(const Port *port,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlewProperty(const Port *port,
		 const RiseFall *rf,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlackProperty(const Port *port,
		  const MinMax *min_max,
		  Sta *sta);
static PropertyValue
portSlackProperty(const Port *port,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  Sta *sta);
static PropertyValue
edgeDelayProperty(Edge *edge,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  Sta *sta);
static PropertyValue
delayPropertyValue(Delay delay,
		   Sta *sta);
static PropertyValue
resistancePropertyValue(float res,
                        Sta *sta);
static PropertyValue
capacitancePropertyValue(float cap,
                         Sta *sta);

////////////////////////////////////////////////////////////////

class PropertyUnknown : public Exception
{
public:
  PropertyUnknown(const char *type,
		  const char *property);
  virtual ~PropertyUnknown() {}
  virtual const char *what() const noexcept;

private:
  const char *type_;
  const char *property_;
};

PropertyUnknown::PropertyUnknown(const char *type,
				 const char *property) :
  Exception(),
  type_(type),
  property_(property)
{
}

const char *
PropertyUnknown::what() const noexcept
{
  return stringPrint("%s objects do not have a %s property.",
		     type_, property_);
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

PropertyValue::PropertyValue(PathRefSeq *value) :
  type_(type_path_refs),
  path_refs_(new PathRefSeq(*value)),
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
  case Type::type_path_refs:
    path_refs_ = value.path_refs_ ? new PathRefSeq(*value.path_refs_) : nullptr;
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
  case Type::type_path_refs:
    path_refs_ = value.path_refs_;
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
  case Type::type_path_refs:
    delete path_refs_;
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
  case Type::type_path_refs:
    path_refs_ = value.path_refs_ ? new PathRefSeq(*value.path_refs_) : nullptr;
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
  case Type::type_path_refs:
    path_refs_ = value.path_refs_;
    value.clks_ = nullptr;
    break;
  case Type::type_pwr_activity:
    pwr_activity_ = value.pwr_activity_;
    break;
  }
  return *this;
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Library *lib,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(network->name(lib));
  else
    throw PropertyUnknown("library", property);
}

PropertyValue
getProperty(const LibertyLibrary *lib,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(lib->name());
  else if (stringEqual(property, "filename"))
    return PropertyValue(lib->filename());
  else
    throw PropertyUnknown("liberty library", property);
}

PropertyValue
getProperty(const LibertyCell *cell,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "base_name"))
    return PropertyValue(cell->name());
  else if (stringEqual(property, "full_name")) {
    auto network = sta->cmdNetwork();
    auto lib = cell->libertyLibrary();
    const char *lib_name = lib->name();
    const char *cell_name = cell->name();
    string full_name;
    stringPrint(full_name, "%s%c%s",
		lib_name,
		network->pathDivider(),
		cell_name);
    return PropertyValue(full_name);
  }
  else if (stringEqual(property, "filename"))
    return PropertyValue(cell->filename());
  else if (stringEqual(property, "library"))
    return PropertyValue(cell->libertyLibrary());
  else if (stringEqual(property, "is_buffer"))
    return PropertyValue(cell->isBuffer());
  else if (stringEqual(property, "is_inverter"))
    return PropertyValue(cell->isInverter());
  else if (stringEqual(property, "dont_use"))
    return PropertyValue(cell->dontUse());
  else if (stringEqual(property, "area"))
    return PropertyValue(cell->area());
  else
    throw PropertyUnknown("liberty cell", property);
}

PropertyValue
getProperty(const Cell *cell,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
      || stringEqual(property, "base_name"))
    return PropertyValue(network->name(cell));
  else if (stringEqual(property, "full_name")) {
    auto lib = network->library(cell);
    const char *lib_name = network->name(lib);
    const char *cell_name = network->name(cell);
    string full_name;
    stringPrint(full_name, "%s%c%s",
		lib_name,
		network->pathDivider(),
		cell_name);
    return PropertyValue(full_name);
  }
  else if (stringEqual(property, "library"))
    return PropertyValue(network->library(cell));
  else if (stringEqual(property, "filename"))
    return PropertyValue(network->filename(cell));
  else
    throw PropertyUnknown("cell", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Port *port,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
	   || stringEqual(property, "full_name"))
    return PropertyValue(network->name(port));
  else if (stringEqual(property, "direction"))
    return PropertyValue(network->direction(port)->name());
  else if (stringEqual(property, "liberty_port"))
    return PropertyValue(network->libertyPort(port));

  else if (stringEqual(property, "activity")) {
    const Instance *top_inst = network->topInstance();
    const Pin *pin = network->findPin(top_inst, port);
    PwrActivity activity = sta->findClkedActivity(pin);
    return PropertyValue(&activity);
  }

  else if (stringEqual(property, "slack_max"))
    return portSlackProperty(port, MinMax::max(), sta);
  else if (stringEqual(property, "slack_max_fall"))
    return portSlackProperty(port, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "slack_max_rise"))
    return portSlackProperty(port, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "slack_min"))
    return portSlackProperty(port, MinMax::min(), sta);
  else if (stringEqual(property, "slack_min_fall"))
    return portSlackProperty(port, RiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "slack_min_rise"))
    return portSlackProperty(port, RiseFall::rise(), MinMax::min(), sta);

  else if (stringEqual(property, "slew_max"))
    return portSlewProperty(port, MinMax::max(), sta);
  else if (stringEqual(property, "slew_max_fall"))
    return portSlewProperty(port, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "slew_max_rise"))
    return portSlewProperty(port, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "slew_min"))
    return portSlewProperty(port, MinMax::min(), sta);
  else if (stringEqual(property, "slew_min_rise"))
    return portSlewProperty(port, RiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "slew_min_fall"))
    return portSlewProperty(port, RiseFall::fall(), MinMax::min(), sta);

  else
    throw PropertyUnknown("port", property);
}

static PropertyValue
portSlewProperty(const Port *port,
		 const MinMax *min_max,
		 Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlewProperty(pin, min_max, sta);
}

static PropertyValue
portSlewProperty(const Port *port,
		 const RiseFall *rf,
		 const MinMax *min_max,
		 Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlewProperty(pin, rf, min_max, sta);
}

static PropertyValue
portSlackProperty(const Port *port,
		  const MinMax *min_max,
		  Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlackProperty(pin, min_max, sta);
}

static PropertyValue
portSlackProperty(const Port *port,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlackProperty(pin, rf, min_max, sta);
}

PropertyValue
getProperty(const LibertyPort *port,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "full_name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "lib_cell"))
    return PropertyValue(port->libertyCell());
  else if (stringEqual(property, "direction"))
    return PropertyValue(port->direction()->name());
  else if (stringEqual(property, "capacitance")) {
    float cap = port->capacitance(RiseFall::rise(), MinMax::max());
    return capacitancePropertyValue(cap, sta);
  }
  else if (stringEqual(property, "is_register_clock"))
    return PropertyValue(port->isRegClk());

  else if (stringEqual(property, "drive_resistance")) {
    float res = port->driveResistance();
    return resistancePropertyValue(res, sta);
  }
  else if (stringEqual(property, "drive_resistance_min_rise")) {
    float res = port->driveResistance(RiseFall::rise(), MinMax::min());
    return resistancePropertyValue(res, sta);
  }
  else if (stringEqual(property, "drive_resistance_max_rise")) {
    float res = port->driveResistance(RiseFall::rise(), MinMax::max());
    return resistancePropertyValue(res, sta);
  }
  else if (stringEqual(property, "drive_resistance_min_fall")) {
    float res = port->driveResistance(RiseFall::fall(), MinMax::min());
    return resistancePropertyValue(res, sta);
  }
  else if (stringEqual(property, "drive_resistance_max_fall")) {
    float res = port->driveResistance(RiseFall::fall(), MinMax::max());
    return resistancePropertyValue(res, sta);
  }

  else if (stringEqual(property, "intrinsic_delay")) {
    ArcDelay delay = port->intrinsicDelay(sta);
    return delayPropertyValue(delay, sta);
  }
  else if (stringEqual(property, "intrinsic_delay_min_rise")) {
    ArcDelay delay = port->intrinsicDelay(RiseFall::rise(),
                                          MinMax::min(), sta);
    return delayPropertyValue(delay, sta);
  }
  else if (stringEqual(property, "intrinsic_delay_max_rise")) {
    ArcDelay delay = port->intrinsicDelay(RiseFall::rise(),
                                          MinMax::max(), sta);
    return delayPropertyValue(delay, sta);
  }
  else if (stringEqual(property, "intrinsic_delay_min_fall")) {
    ArcDelay delay = port->intrinsicDelay(RiseFall::fall(),
                                          MinMax::min(), sta);
    return delayPropertyValue(delay, sta);
  }
  else if (stringEqual(property, "intrinsic_delay_max_fall")) {
    ArcDelay delay = port->intrinsicDelay(RiseFall::fall(),
                                          MinMax::max(), sta);
    return delayPropertyValue(delay, sta);
  }
  else
    throw PropertyUnknown("liberty port", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Instance *inst,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name"))
    return PropertyValue(network->name(inst));
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(inst));
  else if (stringEqual(property, "ref_name"))
    return PropertyValue(network->name(network->cell(inst)));
  else if (stringEqual(property, "liberty_cell"))
    return PropertyValue(network->libertyCell(inst));
  else if (stringEqual(property, "cell"))
    return PropertyValue(network->cell(inst));
  else
    throw PropertyUnknown("instance", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Pin *pin,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
      || stringEqual(property, "lib_pin_name"))
    return PropertyValue(network->portName(pin));
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(pin));
  else if (stringEqual(property, "direction"))
    return PropertyValue(network->direction(pin)->name());
  else if (stringEqual(property, "is_register_clock")) {
    const LibertyPort *port = network->libertyPort(pin);
    return PropertyValue(port && port->isRegClk());
  }
  else if (stringEqual(property, "clocks")) {
    ClockSet clks = sta->clocks(pin);
    return PropertyValue(&clks);
  }
  else if (stringEqual(property, "clock_domains")) {
    ClockSet clks = sta->clockDomains(pin);
    return PropertyValue(&clks);
  }
  else if (stringEqual(property, "activity")) {
    PwrActivity activity = sta->findClkedActivity(pin);
    return PropertyValue(&activity);
  }

  else if (stringEqual(property, "arrival_max_rise"))
    return pinArrivalProperty(pin, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "arrival_max_fall"))
    return pinArrivalProperty(pin, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "arrival_min_rise"))
    return pinArrivalProperty(pin, RiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "arrival_min_fall"))
    return pinArrivalProperty(pin, RiseFall::fall(), MinMax::min(), sta);

  else if (stringEqual(property, "slack_max"))
    return pinSlackProperty(pin, MinMax::max(), sta);
  else if (stringEqual(property, "slack_max_fall"))
    return pinSlackProperty(pin, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "slack_max_rise"))
    return pinSlackProperty(pin, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "slack_min"))
    return pinSlackProperty(pin, MinMax::min(), sta);
  else if (stringEqual(property, "slack_min_fall"))
    return pinSlackProperty(pin, RiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "slack_min_rise"))
    return pinSlackProperty(pin, RiseFall::rise(), MinMax::min(), sta);

  else if (stringEqual(property, "slew_max"))
    return pinSlewProperty(pin, MinMax::max(), sta);
  else if (stringEqual(property, "slew_max_fall"))
    return pinSlewProperty(pin, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "slew_max_rise"))
    return pinSlewProperty(pin, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "slew_min"))
    return pinSlewProperty(pin, MinMax::min(), sta);
  else if (stringEqual(property, "slew_min_rise"))
    return pinSlewProperty(pin, RiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "slew_min_fall"))
    return pinSlewProperty(pin, RiseFall::fall(), MinMax::min(), sta);

  else
    throw PropertyUnknown("pin", property);
}

static PropertyValue
pinArrivalProperty(const Pin *pin,
                   const RiseFall *rf,
                   const MinMax *min_max,
                   Sta *sta)
{
  Arrival arrival = sta->pinArrival(pin, rf, min_max);;
  return PropertyValue(delayPropertyValue(arrival, sta));
}

static PropertyValue
pinSlackProperty(const Pin *pin,
		 const MinMax *min_max,
		 Sta *sta)
{
  Slack slack = sta->pinSlack(pin, min_max);
  return PropertyValue(delayPropertyValue(slack, sta));
}

static PropertyValue
pinSlackProperty(const Pin *pin,
		 const RiseFall *rf,
		 const MinMax *min_max,
		 Sta *sta)
{
  Slack slack = sta->pinSlack(pin, rf, min_max);
  return PropertyValue(delayPropertyValue(slack, sta));
}

static PropertyValue
pinSlewProperty(const Pin *pin,
		const MinMax *min_max,
		Sta *sta)
{
  auto graph = sta->ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta->vertexSlew(vertex, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta->vertexSlew(bidirect_drvr_vertex, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta))
      slew = vertex_slew;
  }
  return delayPropertyValue(slew, sta);
}

static PropertyValue
pinSlewProperty(const Pin *pin,
		const RiseFall *rf,
		const MinMax *min_max,
		Sta *sta)
{
  auto graph = sta->ensureGraph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta->vertexSlew(vertex, rf, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta->vertexSlew(bidirect_drvr_vertex, rf, min_max);
    if (delayGreater(vertex_slew, slew, min_max, sta))
      slew = vertex_slew;
  }
  return delayPropertyValue(slew, sta);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Net *net,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name"))
    return PropertyValue(network->name(net));
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(net));
  else
    throw PropertyUnknown("net", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(Edge *edge,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "full_name")) {
    auto network = sta->cmdNetwork();
    auto graph = sta->graph();
    const char *from = edge->from(graph)->name(network);
    const char *to = edge->to(graph)->name(network);
    string full_name;
    stringPrint(full_name, "%s -> %s", from, to);
    return PropertyValue(full_name);
  }
  if (stringEqual(property, "delay_min_fall"))
    return edgeDelayProperty(edge, RiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_fall"))
    return edgeDelayProperty(edge, RiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "delay_min_rise"))
    return edgeDelayProperty(edge, RiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_rise"))
    return edgeDelayProperty(edge, RiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "sense"))
    return PropertyValue(timingSenseString(edge->sense()));
  else if (stringEqual(property, "from_pin"))
    return PropertyValue(edge->from(sta->graph())->pin());
  else if (stringEqual(property, "to_pin"))
    return PropertyValue(edge->to(sta->graph())->pin());
  else
    throw PropertyUnknown("edge", property);
}

static PropertyValue
edgeDelayProperty(Edge *edge,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  Sta *sta)
{
  ArcDelay delay = 0.0;
  bool delay_exists = false;
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    RiseFall *to_rf = arc->toEdge()->asRiseFall();
    if (to_rf == rf) {
      for (auto corner : *sta->corners()) {
	DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
	ArcDelay arc_delay = sta->arcDelay(edge, arc, dcalc_ap);
	if (!delay_exists
	    || ((min_max == MinMax::max()
		 && delayGreater(arc_delay, delay, sta))
		|| (min_max == MinMax::min()
		    && delayLess(arc_delay, delay, sta))))
	  delay = arc_delay;
      }
    }
  }
  return PropertyValue(delayPropertyValue(delay, sta));
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(TimingArcSet *arc_set,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name")) {
    if (arc_set->isWire())
      return PropertyValue("wire");
    else {
      auto from = arc_set->from()->name();
      auto to = arc_set->to()->name();
      auto cell_name = arc_set->libertyCell()->name();
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
getProperty(Clock *clk,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(clk->name());
  else if (stringEqual(property, "period"))
    return PropertyValue(clk->period(), sta->units()->timeUnit());
  else if (stringEqual(property, "sources"))
    return PropertyValue(clk->pins());
  else if (stringEqual(property, "propagated"))
    return PropertyValue(clk->isPropagated());
  else if (stringEqual(property, "is_generated"))
    return PropertyValue(clk->isGenerated());
  else
    throw PropertyUnknown("clock", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(PathEnd *end,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "startpoint")) {
    PathExpanded expanded(end->path(), sta);
    return PropertyValue(expanded.startPath()->pin(sta));
  }
  else if (stringEqual(property, "startpoint_clock"))
    return PropertyValue(end->path()->clock(sta));
  else if (stringEqual(property, "endpoint"))
    return PropertyValue(end->path()->pin(sta));
  else if (stringEqual(property, "endpoint_clock"))
    return PropertyValue(end->targetClk(sta));
  else if (stringEqual(property, "endpoint_clock_pin"))
    return PropertyValue(end->targetClkPath()->pin(sta));
  else if (stringEqual(property, "slack"))
    return PropertyValue(delayPropertyValue(end->slack(sta), sta));
  else if (stringEqual(property, "points")) {
    PathExpanded expanded(end->path(), sta);
    PathRefSeq paths;
    for (auto i = expanded.startIndex(); i < expanded.size(); i++) {
      PathRef *path = expanded.path(i);
      paths.push_back(*path);
    }
    return PropertyValue(&paths);
  }
  else
    throw PropertyUnknown("path end", property);
}

PropertyValue
getProperty(PathRef *path,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "pin"))
    return PropertyValue(path->pin(sta));
  else if (stringEqual(property, "arrival"))
    return PropertyValue(delayPropertyValue(path->arrival(sta), sta));
  else if (stringEqual(property, "required"))
    return PropertyValue(delayPropertyValue(path->required(sta), sta));
  else if (stringEqual(property, "slack"))
    return PropertyValue(delayPropertyValue(path->slack(sta), sta));
  else
    throw PropertyUnknown("path", property);
}

static PropertyValue
delayPropertyValue(Delay delay,
		   Sta *sta)
{
  return PropertyValue(delayAsFloat(delay), sta->units()->timeUnit());
}

static PropertyValue
resistancePropertyValue(float res,
                        Sta *sta)
{
  return PropertyValue(res, sta->units()->resistanceUnit());
}

static PropertyValue
capacitancePropertyValue(float cap,
                         Sta *sta)
{
  return PropertyValue(cap, sta->units()->capacitanceUnit());
}

} // namespace
