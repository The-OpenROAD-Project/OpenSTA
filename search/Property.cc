// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "Machine.hh"
#include "StringUtil.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "PortDirection.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Corner.hh"
#include "PathEnd.hh"
#include "PathExpanded.hh"
#include "PathRef.hh"
#include "Property.hh"
#include "Sta.hh"
#include "Property.hh"

namespace sta {

using std::string;

static PropertyValue
pinSlewProperty(const Pin *pin,
		const TransRiseFall *tr,
		const MinMax *min_max,
		Sta *sta);
static PropertyValue
pinSlackProperty(const Pin *pin,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlewProperty(const Port *port,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlackProperty(const Port *port,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta);
static PropertyValue
edgeDelayProperty(Edge *edge,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta);
static float
delayPropertyValue(Delay delay,
		   Sta *sta);

////////////////////////////////////////////////////////////////

class PropertyUnknown : public StaException
{
public:
  PropertyUnknown(const char *type,
		  const char *property);
  virtual ~PropertyUnknown() THROW_DCL {}
  virtual const char *what() const throw();

private:
  const char *type_;
  const char *property_;
};

PropertyUnknown::PropertyUnknown(const char *type,
				 const char *property) :
  type_(type),
  property_(property)
{
}

const char *
PropertyUnknown::what() const throw()
{
  return stringPrint("%s objects do not have a %s property.",
		     type_, property_);
}

////////////////////////////////////////////////////////////////

PropertyValue::PropertyValue() :
  type_(type_none)
{
}

PropertyValue::PropertyValue(const char *value) :
  type_(type_string),
  string_(stringCopy(value))
{
}

PropertyValue::PropertyValue(std::string &value) :
  type_(type_string),
  string_(stringCopy(value.c_str()))
{
}

PropertyValue::PropertyValue(float value) :
  type_(type_float),
  float_(value)
{
}

PropertyValue::PropertyValue(LibertyLibrary *value) :
  type_(type_liberty_library),
  liberty_library_(value)
{
}

PropertyValue::PropertyValue(LibertyCell *value) :
  type_(type_liberty_cell),
  liberty_cell_(value)
{
}

PropertyValue::PropertyValue(Library *value) :
  type_(type_library),
  library_(value)
{
}

PropertyValue::PropertyValue(Cell *value) :
  type_(type_cell),
  cell_(value)
{
}

PropertyValue::PropertyValue(Instance *value) :
  type_(type_instance),
  inst_(value)
{
}

PropertyValue::PropertyValue(Pin *value) :
  type_(type_pin),
  pin_(value)
{
}

PropertyValue::PropertyValue(PinSeq *value) :
  type_(type_pins),
  pins_(value)
{
}

PropertyValue::PropertyValue(PinSet *value) :
  type_(type_pins),
  pins_(new PinSeq)
{
  PinSet::Iterator pin_iter(value);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    pins_->push_back( pin);
  }
}

PropertyValue::PropertyValue(Net *value) :
  type_(type_net),
  net_(value)
{
}

PropertyValue::PropertyValue(Clock *value) :
  type_(type_clk),
  clk_(value)
{
}

PropertyValue::PropertyValue(ClockSeq *value) :
  type_(type_clks),
  clks_(new ClockSeq(*value))
{
}

PropertyValue::PropertyValue(ClockSet *value) :
  type_(type_clks),
  clks_(new ClockSeq)
{
  ClockSet::Iterator clk_iter(value);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    clks_->push_back(clk);
  }
}

PropertyValue::PropertyValue(PathRefSeq *value) :
  type_(type_path_refs),
  path_refs_(new PathRefSeq(*value))
{
}

PropertyValue::PropertyValue(const PropertyValue &value) :
  type_(value.type_)
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
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_library:
    library_ = value.library_;
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
  }
}

PropertyValue::PropertyValue(PropertyValue &&value) :
  type_(value.type_)
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
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_library:
    library_ = value.library_;
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
  switch (type_) {
  case Type::type_none:
    break;
  case Type::type_string:
    string_ = stringCopy(value.string_);
    break;
  case Type::type_float:
    float_ = value.float_;
    break;
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_library:
    library_ = value.library_;
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
  }
  return *this;
}

PropertyValue &
PropertyValue::operator=(PropertyValue &&value)
{
  type_ = value.type_;
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
  case Type::type_liberty_library:
    liberty_library_ = value.liberty_library_;
    break;
  case Type::type_liberty_cell:
    liberty_cell_ = value.liberty_cell_;
    break;
  case Type::type_cell:
    cell_ = value.cell_;
    break;
  case Type::type_library:
    library_ = value.library_;
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
#if 0
  else if (stringEqual(property, "filename"))
    return PropertyValue(network->filename(lib));
#endif
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

  else if (stringEqual(property, "actual_fall_transition_min"))
    return portSlewProperty(port, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_fall_transition_max"))
    return portSlewProperty(port, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_min"))
    return portSlewProperty(port, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_rise_transition_max"))
    return portSlewProperty(port, TransRiseFall::rise(), MinMax::max(), sta);

  else if (stringEqual(property, "min_fall_slack"))
    return portSlackProperty(port, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "max_fall_slack"))
    return portSlackProperty(port, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "min_rise_slack"))
    return portSlackProperty(port, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "max_rise_slack"))
    return portSlackProperty(port, TransRiseFall::rise(), MinMax::max(), sta);

  else
    throw PropertyUnknown("port", property);
}

static PropertyValue
portSlewProperty(const Port *port,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlewProperty(pin, tr, min_max, sta);
}

static PropertyValue
portSlackProperty(const Port *port,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlackProperty(pin, tr, min_max, sta);
}

PropertyValue
getProperty(const LibertyPort *port,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "full_name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "direction"))
    return PropertyValue(port->direction()->name());
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
  if (stringEqual(property, "direction"))
    return PropertyValue(network->direction(pin)->name());
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(pin));
  else if (stringEqual(property, "lib_pin_name"))
    return PropertyValue(network->portName(pin));
  else if (stringEqual(property, "clocks")) {
    ClockSet clks;
    sta->clocks(pin, clks);
    return PropertyValue(&clks);
  }

  else if (stringEqual(property, "max_fall_slack"))
    return pinSlackProperty(pin, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "max_rise_slack"))
    return pinSlackProperty(pin, TransRiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "min_fall_slack"))
    return pinSlackProperty(pin, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "min_rise_slack"))
    return pinSlackProperty(pin, TransRiseFall::rise(), MinMax::min(), sta);

  else if (stringEqual(property, "actual_fall_transition_max"))
    return pinSlewProperty(pin, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_max"))
    return pinSlewProperty(pin, TransRiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_min"))
    return pinSlewProperty(pin, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_fall_transition_min"))
    return pinSlewProperty(pin, TransRiseFall::fall(), MinMax::min(), sta);

  else
    throw PropertyUnknown("pin", property);
}

static PropertyValue
pinSlackProperty(const Pin *pin,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta)
{
  Slack slack = sta->pinSlack(pin, tr, min_max);
  return PropertyValue(delayPropertyValue(slack, sta));
}

static PropertyValue
pinSlewProperty(const Pin *pin,
		const TransRiseFall *tr,
		const MinMax *min_max,
		Sta *sta)
{
  auto graph = sta->graph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta->vertexSlew(vertex, tr, min_max);
    if (fuzzyGreater(vertex_slew, slew, min_max))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta->vertexSlew(bidirect_drvr_vertex, tr, min_max);
    if (fuzzyGreater(vertex_slew, slew, min_max))
      slew = vertex_slew;
  }
  return PropertyValue(delayPropertyValue(slew, sta));
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Net *net,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "full_name"))
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
    return stringPrintTmp("%s -> %s", from, to);
  }
  if (stringEqual(property, "delay_min_fall"))
    return edgeDelayProperty(edge, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_fall"))
    return edgeDelayProperty(edge, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "delay_min_rise"))
    return edgeDelayProperty(edge, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_rise"))
    return edgeDelayProperty(edge, TransRiseFall::rise(), MinMax::max(), sta);
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
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta)
{
  ArcDelay delay = 0.0;
  bool delay_exists = false;
  TimingArcSet *arc_set = edge->timingArcSet();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    TransRiseFall *to_tr = arc->toTrans()->asRiseFall();
    if (to_tr == tr) {
      CornerIterator corner_iter(sta);
      while (corner_iter.hasNext()) {
	Corner *corner = corner_iter.next();
	DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
	ArcDelay arc_delay = sta->arcDelay(edge, arc, dcalc_ap);
	if (!delay_exists
	    || ((min_max == MinMax::max()
		 && arc_delay > delay)
		|| (min_max == MinMax::min()
		    && arc_delay < delay)))
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
    auto from = arc_set->from()->name();
    auto to = arc_set->to()->name();
    auto cell_name = arc_set->libertyCell()->name();
    string name;
    stringPrint(name, "%s %s -> %s", cell_name, from, to);
    return PropertyValue(name);
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
    return PropertyValue(sta->units()->timeUnit()->asString(clk->period(), 8));
  else if (stringEqual(property, "sources"))
    return PropertyValue(clk->pins());
  else if (stringEqual(property, "propagated"))
    return PropertyValue(clk->isPropagated() ? "1" : "0");
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

static float
delayPropertyValue(Delay delay,
		   Sta *sta)
{
  return delayAsFloat(delay) / sta->units()->timeUnit()->scale();
}

} // namespace
