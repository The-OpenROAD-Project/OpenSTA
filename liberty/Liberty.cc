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

#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "EnumNameMap.hh"
#include "Report.hh"
#include "Debug.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "StringSet.hh"
#include "PatternMatch.hh"
#include "Units.hh"
#include "PortDirection.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Sequential.hh"
#include "Wireload.hh"
#include "EquivCells.hh"
#include "Network.hh"
#include "Liberty.hh"

namespace sta {

typedef Set<TimingModel*> TimingModelSet;
typedef Set<FuncExpr*> FuncExprSet;
typedef Set<LatchEnable*> LatchEnableSet;

bool LibertyLibrary::found_rise_fall_caps_ = false;

void
initLiberty()
{
  TimingArcSet::init();
}

void
deleteLiberty()
{
  TimingArcSet::destroy();
}

LibertyLibrary::LibertyLibrary(const char *name,
			       const char *filename) :
  ConcreteLibrary(name, filename, true),
  units_(new Units()),
  delay_model_type_(DelayModelType::cmos_linear), // default
  nominal_process_(0.0),
  nominal_voltage_(0.0),
  nominal_temperature_(0.0),
  scale_factors_(nullptr),
  default_input_pin_cap_(0.0),
  default_output_pin_cap_(0.0),
  default_bidirect_pin_cap_(0.0),
  default_fanout_load_(0.0),
  default_max_cap_(0.0),
  default_max_cap_exists_(false),
  default_max_fanout_(0.0),
  default_max_fanout_exists_(false),
  default_max_slew_(0.0),
  default_max_slew_exists_(false),
  slew_derate_from_library_(1.0),
  default_wire_load_(nullptr),
  default_wire_load_mode_(WireloadMode::unknown),
  default_wire_load_selection_(nullptr),
  default_operating_conditions_(nullptr),
  ocv_arc_depth_(0.0),
  default_ocv_derate_(nullptr),
  buffers_(nullptr)
{
  // Scalar templates are builtin.
  for (int i = 0; i != int(TableTemplateType::count); i++) {
    TableTemplateType type = static_cast<TableTemplateType>(i);
    TableTemplate *scalar_template = new TableTemplate("scalar", nullptr,
						       nullptr, nullptr);
    addTableTemplate(scalar_template, type);
  }

  for (auto tr_index : RiseFall::rangeIndex()) {
    wire_slew_degradation_tbls_[tr_index] = nullptr;
    input_threshold_[tr_index] = input_threshold_default_;
    output_threshold_[tr_index] = output_threshold_default_;
    slew_lower_threshold_[tr_index] = slew_lower_threshold_default_;
    slew_upper_threshold_[tr_index] = slew_upper_threshold_default_;
  }
}

LibertyLibrary::~LibertyLibrary()
{
  bus_dcls_.deleteContents();
  for (int i = 0; i < int(TableTemplateType::count); i++)
    template_maps_[i].deleteContents();
  scale_factors_map_.deleteContents();
  delete scale_factors_;

  for (auto tr_index : RiseFall::rangeIndex()) {
    TableModel *model = wire_slew_degradation_tbls_[tr_index];
    delete model;
  }
  operating_conditions_.deleteContents();
  wireloads_.deleteContents();
  wire_load_selections_.deleteContents();
  delete units_;
  ocv_derate_map_.deleteContents();

  for (auto name_volt : supply_voltage_map_) {
    const char *supply_name = name_volt.first;
    stringDelete(supply_name);
  }
  delete buffers_;
}

LibertyCell *
LibertyLibrary::findLibertyCell(const char *name) const
{
  return static_cast<LibertyCell*>(findCell(name));
}

void
LibertyLibrary::findLibertyCellsMatching(PatternMatch *pattern,
					 LibertyCellSeq *cells)
{
  LibertyCellIterator cell_iter(this);
  while (cell_iter.hasNext()) {
    LibertyCell *cell = cell_iter.next();
    if (pattern->match(cell->name()))
      cells->push_back(cell);
  }
}

LibertyCellSeq *
LibertyLibrary::buffers()
{
  if (buffers_ == nullptr) {
    buffers_ = new LibertyCellSeq;
    LibertyCellIterator cell_iter(this);
    while (cell_iter.hasNext()) {
      LibertyCell *cell = cell_iter.next();
      if (!cell->dontUse()
	  && cell->isBuffer())
	buffers_->push_back(cell);
    }
  }
  return buffers_;
}

void
LibertyLibrary::setDelayModelType(DelayModelType type)
{
  delay_model_type_ = type;
}

void
LibertyLibrary::addBusDcl(BusDcl *bus_dcl)
{
  bus_dcls_[bus_dcl->name()] = bus_dcl;
}

BusDcl *
LibertyLibrary::findBusDcl(const char *name) const
{
  return bus_dcls_.findKey(name);
}

void
LibertyLibrary::addTableTemplate(TableTemplate *tbl_template,
				 TableTemplateType type)
{
  template_maps_[int(type)][tbl_template->name()] = tbl_template;
}

TableTemplate *
LibertyLibrary::findTableTemplate(const char *name,
				  TableTemplateType type)
{
  return template_maps_[int(type)][name];
}

void
LibertyLibrary::setNominalProcess(float process)
{
  nominal_process_ = process;
}

void
LibertyLibrary::setNominalVoltage(float voltage)
{
  nominal_voltage_ = voltage;
}

void
LibertyLibrary::setNominalTemperature(float temperature)
{
  nominal_temperature_ = temperature;
}

void
LibertyLibrary::setScaleFactors(ScaleFactors *scales)
{
  scale_factors_ = scales;
}

void
LibertyLibrary::addScaleFactors(ScaleFactors *scales)
{
  scale_factors_map_[scales->name()] = scales;
}

ScaleFactors *
LibertyLibrary::findScaleFactors(const char *name)
{
  return scale_factors_map_[name];
}

float
LibertyLibrary::scaleFactor(ScaleFactorType type,
			    const Pvt *pvt) const
{
  return scaleFactor(type, 0, nullptr, pvt);
}

float
LibertyLibrary::scaleFactor(ScaleFactorType type,
			    const LibertyCell *cell,
			    const Pvt *pvt) const
{
  return scaleFactor(type, 0, cell, pvt);
}

float
LibertyLibrary::scaleFactor(ScaleFactorType type,
			    int tr_index,
			    const LibertyCell *cell,
			    const Pvt *pvt) const
{
  if (pvt == nullptr)
    pvt = default_operating_conditions_;
  // If there is no operating condition, nominal pvt values are used.
  // All scale factors are unity for nominal pvt.
  if (pvt) {
    ScaleFactors *scale_factors = nullptr;
    // Cell level scale factors have precidence over library scale factors.
    if (cell)
      scale_factors = cell->scaleFactors();
    if (scale_factors == nullptr)
      scale_factors = scale_factors_;
    if (scale_factors) {
      float process_scale = 1.0F + (pvt->process() - nominal_process_)
	* scale_factors->scale(type, ScaleFactorPvt::process, tr_index);
      float temp_scale = 1.0F + (pvt->temperature() - nominal_temperature_)
	* scale_factors->scale(type, ScaleFactorPvt::temp, tr_index);
      float volt_scale = 1.0F + (pvt->voltage() - nominal_voltage_)
	* scale_factors->scale(type, ScaleFactorPvt::volt, tr_index);
      float scale = process_scale * temp_scale * volt_scale;
      return scale;
    }
  }
  return 1.0F;
}

void
LibertyLibrary::setWireSlewDegradationTable(TableModel *model,
			 		    RiseFall *rf)
{
  int tr_index = rf->index();
  if (wire_slew_degradation_tbls_[tr_index])
    delete wire_slew_degradation_tbls_[tr_index];
  wire_slew_degradation_tbls_[tr_index] = model;
}

TableModel *
LibertyLibrary::wireSlewDegradationTable(const RiseFall *rf) const
{
  return wire_slew_degradation_tbls_[rf->index()];
}

float
LibertyLibrary::degradeWireSlew(const LibertyCell *cell,
				const RiseFall *rf,
				const Pvt *pvt,
				float in_slew,
				float wire_delay) const
{
  const TableModel *model = wireSlewDegradationTable(rf);
  if (model)
    return degradeWireSlew(cell, pvt, model, in_slew, wire_delay);
  else
    return in_slew;
}

float
LibertyLibrary::degradeWireSlew(const LibertyCell *cell,
				const Pvt *pvt,
				const TableModel *model,
				float in_slew,
				float wire_delay) const
{
  switch (model->order()) {
  case 0:
    return model->findValue(this, cell, pvt, 0.0, 0.0, 0.0);
  case 1: {
    TableAxis *axis1 = model->axis1();
    TableAxisVariable var1 = axis1->variable();
    if (var1 == TableAxisVariable::output_pin_transition)
      return model->findValue(this, cell, pvt, in_slew, 0.0, 0.0);
    else if (var1 == TableAxisVariable::connect_delay)
      return model->findValue(this, cell, pvt, wire_delay, 0.0, 0.0);
    else {
      internalError("unsupported slew degradation table axes");
      return 0.0;
    }
  }
  case 2: {
    TableAxis *axis1 = model->axis1();
    TableAxis *axis2 = model->axis2();
    TableAxisVariable var1 = axis1->variable();
    TableAxisVariable var2 = axis2->variable();
    if (var1 == TableAxisVariable::output_pin_transition
	&& var2 == TableAxisVariable::connect_delay)
      return model->findValue(this, cell, pvt, in_slew, wire_delay, 0.0);
    else if (var1 == TableAxisVariable::connect_delay
	     && var2 == TableAxisVariable::output_pin_transition)
      return model->findValue(this, cell, pvt, wire_delay, in_slew, 0.0);
    else {
      internalError("unsupported slew degradation table axes");
      return 0.0;
    }
  }
  default:
    internalError("unsupported slew degradation table order");
    return 0.0;
  }
}

// Check for supported axis variables.
// Return true if axes are supported.
bool
LibertyLibrary::checkSlewDegradationAxes(Table *table)
{
  switch (table->order()) {
  case 0:
    return true;
  case 1: {
    TableAxis *axis1 = table->axis1();
    TableAxisVariable var1 = axis1->variable();
    return var1 == TableAxisVariable::output_pin_transition
      || var1 == TableAxisVariable::connect_delay;
  }
  case 2: {
    TableAxis *axis1 = table->axis1();
    TableAxis *axis2 = table->axis2();
    TableAxisVariable var1 = axis1->variable();
    TableAxisVariable var2 = axis2->variable();
    return (var1 == TableAxisVariable::output_pin_transition
	    && var2 == TableAxisVariable::connect_delay)
      || (var1 == TableAxisVariable::connect_delay
	  && var2 == TableAxisVariable::output_pin_transition);
  }
  default:
    internalError("unsupported slew degradation table axes");
    return 0.0;
  }
}

void
LibertyLibrary::defaultMaxFanout(float &fanout,
				 bool &exists) const
{
  fanout = default_max_fanout_;
  exists = default_max_fanout_exists_;
}

void
LibertyLibrary::setDefaultMaxFanout(float fanout)
{
  default_max_fanout_ = fanout;
  default_max_fanout_exists_ = true;
}

void
LibertyLibrary::defaultMaxSlew(float &slew,
			       bool &exists) const
{
  slew = default_max_slew_;
  exists = default_max_slew_exists_;
}

void
LibertyLibrary::setDefaultMaxSlew(float slew)
{
  default_max_slew_ = slew;
  default_max_slew_exists_ = true;
}

void
LibertyLibrary::defaultMaxCapacitance(float &cap,
				      bool &exists) const
{
  cap = default_max_cap_;
  exists = default_max_cap_exists_;
}

void
LibertyLibrary::setDefaultMaxCapacitance(float cap)
{
  default_max_cap_ = cap;
  default_max_cap_exists_ = true;
}

void
LibertyLibrary::setDefaultFanoutLoad(float load)
{
  default_fanout_load_ = load;
}

void
LibertyLibrary::setDefaultBidirectPinCap(float cap)
{
  default_bidirect_pin_cap_ = cap;
}

void
LibertyLibrary::setDefaultInputPinCap(float cap)
{
  default_input_pin_cap_ = cap;
}

void
LibertyLibrary::setDefaultOutputPinCap(float cap)
{
  default_output_pin_cap_ = cap;
}

void
LibertyLibrary::defaultIntrinsic(const RiseFall *rf,
				 // Return values.
				 float &intrinsic,
				 bool &exists) const
{
  default_intrinsic_.value(rf, intrinsic, exists);
}

void
LibertyLibrary::setDefaultIntrinsic(const RiseFall *rf,
				    float value)
{
  default_intrinsic_.setValue(rf, value);
}

void
LibertyLibrary::defaultPinResistance(const RiseFall *rf,
				     const PortDirection *dir,
				     // Return values.
				     float &res,
				     bool &exists) const
{
  if (dir->isAnyTristate())
    defaultBidirectPinRes(rf, res, exists);
  else
    defaultOutputPinRes(rf, res, exists);
}

void
LibertyLibrary::defaultBidirectPinRes(const RiseFall *rf,
				      // Return values.
				      float &res,
				      bool &exists) const
{
  return default_inout_pin_res_.value(rf, res, exists);
}

void
LibertyLibrary::setDefaultBidirectPinRes(const RiseFall *rf,
					 float value)
{
  default_inout_pin_res_.setValue(rf, value);
}

void
LibertyLibrary::defaultOutputPinRes(const RiseFall *rf,
				    // Return values.
				    float &res,
				    bool &exists) const
{
  default_output_pin_res_.value(rf, res, exists);
}

void
LibertyLibrary::setDefaultOutputPinRes(const RiseFall *rf,
				       float value)
{
  default_output_pin_res_.setValue(rf, value);
}

void
LibertyLibrary::addWireload(Wireload *wireload)
{
  wireloads_[wireload->name()] = wireload;
}

Wireload *
LibertyLibrary::findWireload(const char *name) const
{
  return wireloads_.findKey(name);
}

void
LibertyLibrary::setDefaultWireload(Wireload *wireload)
{
  default_wire_load_ = wireload;
}

Wireload *
LibertyLibrary::defaultWireload() const
{
  return default_wire_load_;
}

void
LibertyLibrary::addWireloadSelection(WireloadSelection *selection)
{
  wire_load_selections_[selection->name()] = selection;
}

WireloadSelection *
LibertyLibrary::findWireloadSelection(const char *name) const
{
  return wire_load_selections_.findKey(name);
}

WireloadSelection *
LibertyLibrary::defaultWireloadSelection() const
{
  return default_wire_load_selection_;
}

void
LibertyLibrary::setDefaultWireloadSelection(WireloadSelection *selection)
{
  default_wire_load_selection_ = selection;
}

WireloadMode
LibertyLibrary::defaultWireloadMode() const
{
  return default_wire_load_mode_;
}

void
LibertyLibrary::setDefaultWireloadMode(WireloadMode mode)
{
  default_wire_load_mode_ = mode;
}

void
LibertyLibrary::addOperatingConditions(OperatingConditions *op_cond)
{
  operating_conditions_[op_cond->name()] = op_cond;
}

OperatingConditions *
LibertyLibrary::findOperatingConditions(const char *name)
{
  return operating_conditions_.findKey(name);
}

OperatingConditions *
LibertyLibrary::defaultOperatingConditions() const
{
  return default_operating_conditions_;
}

void
LibertyLibrary::setDefaultOperatingConditions(OperatingConditions *op_cond)
{
  default_operating_conditions_ = op_cond;
}

float
LibertyLibrary::inputThreshold(const RiseFall *rf) const
{
  return input_threshold_[rf->index()];
}

void
LibertyLibrary::setInputThreshold(const RiseFall *rf,
				  float th)
{
  input_threshold_[rf->index()] = th;
}

float
LibertyLibrary::outputThreshold(const RiseFall *rf) const
{
  return output_threshold_[rf->index()];
}

void
LibertyLibrary::setOutputThreshold(const RiseFall *rf,
				   float th)
{
  output_threshold_[rf->index()] = th;
}

float
LibertyLibrary::slewLowerThreshold(const RiseFall *rf) const
{
  return slew_lower_threshold_[rf->index()];
}

void
LibertyLibrary::setSlewLowerThreshold(const RiseFall *rf,
				      float th)
{
  slew_lower_threshold_[rf->index()] = th;
}

float
LibertyLibrary::slewUpperThreshold(const RiseFall *rf) const
{
  return slew_upper_threshold_[rf->index()];
}

void
LibertyLibrary::setSlewUpperThreshold(const RiseFall *rf,
				      float th)
{
  slew_upper_threshold_[rf->index()] = th;
}

float
LibertyLibrary::slewDerateFromLibrary() const
{
  return slew_derate_from_library_;
}

void
LibertyLibrary::setSlewDerateFromLibrary(float derate)
{
  slew_derate_from_library_ = derate;
}

LibertyCell *
LibertyLibrary::makeScaledCell(const char *name,
			       const char *filename)
{
  LibertyCell *cell = new LibertyCell(this, name, filename);
  return cell;
}

////////////////////////////////////////////////////////////////

void
LibertyLibrary::makeCornerMap(LibertyLibrary *lib,
			      int ap_index,
			      Network *network,
			      Report *report)
{
  LibertyCellIterator cell_iter(lib);
  while (cell_iter.hasNext()) {
    LibertyCell *cell = cell_iter.next();
    const char *name = cell->name();
    LibertyCell *link_cell = network->findLibertyCell(name);
    if (link_cell)
      makeCornerMap(link_cell, cell, ap_index, report);
  }
}

// Map a cell linked in the network to the corresponding liberty cell
// to use for delay calculation at a corner.
void
LibertyLibrary::makeCornerMap(LibertyCell *link_cell,
			      LibertyCell *corner_cell,
			      int ap_index,
			      Report *report)
{
  link_cell->setCornerCell(corner_cell, ap_index);
  makeCornerMap(link_cell, corner_cell, true, ap_index, report);
  // Check for brain damage in the other direction.
  makeCornerMap(corner_cell, link_cell, false, ap_index, report);
}

void
LibertyLibrary::makeCornerMap(LibertyCell *cell1,
			      LibertyCell *cell2,
			      bool link,
			      int ap_index,
			      Report *report)
{
  LibertyCellPortBitIterator port_iter1(cell1);
  while (port_iter1.hasNext()) {
    LibertyPort *port1 = port_iter1.next();
    const char *port_name = port1->name();
    LibertyPort *port2 = cell2->findLibertyPort(port_name);
    if (port2) {
      if (link)
	port1->setCornerPort(port2, ap_index);
    }
    else
      report->warn("cell %s/%s port %s not found in cell %s/%s.\n",
		   cell1->library()->name(),
		   cell1->name(),
		   port_name,
		   cell2->library()->name(),
		   cell2->name());
  }

  for (auto arc_set1 : cell1->timing_arc_sets_) {
    auto arc_set2 = cell2->findTimingArcSet(arc_set1);
    if (arc_set2) {
      if (link) {
	TimingArcSetArcIterator arc_iter1(arc_set1);
	TimingArcSetArcIterator arc_iter2(arc_set2);
	while (arc_iter1.hasNext() && arc_iter2.hasNext()) {
	  TimingArc *arc1 = arc_iter1.next();
	  TimingArc *arc2 = arc_iter2.next();
	  if (TimingArc::equiv(arc1, arc2))
	    arc1->setCornerArc(arc2, ap_index);
	}
      }
    }
    else
      report->warn("cell %s/%s %s -> %s timing group %s not found in cell %s/%s.\n",
		   cell1->library()->name(),
		   cell1->name(),
		   arc_set1->from()->name(),
		   arc_set1->to()->name(),
		   arc_set1->role()->asString(),
		   cell2->library()->name(),
		   cell2->name());
  }
}

////////////////////////////////////////////////////////////////

float
LibertyLibrary::ocvArcDepth() const
{
  return ocv_arc_depth_;
}

void
LibertyLibrary::setOcvArcDepth(float depth)
{
  ocv_arc_depth_ = depth;
}

OcvDerate *
LibertyLibrary::defaultOcvDerate() const
{
  return default_ocv_derate_;
}

void
LibertyLibrary::setDefaultOcvDerate(OcvDerate *derate)
{
  default_ocv_derate_ = derate;
}

OcvDerate *
LibertyLibrary::findOcvDerate(const char *derate_name)
{
  return ocv_derate_map_.findKey(derate_name);
}

void
LibertyLibrary::addOcvDerate(OcvDerate *derate)
{
  ocv_derate_map_[derate->name()] = derate;
}

void
LibertyLibrary::addSupplyVoltage(const char *supply_name,
				 float voltage)
{
  supply_voltage_map_[stringCopy(supply_name)] = voltage;
}

void
LibertyLibrary::supplyVoltage(const char *supply_name,
			      // Return value.
			      float &voltage,
			      bool &exists) const
{
  supply_voltage_map_.findKey(supply_name, voltage, exists);
}

bool
LibertyLibrary::supplyExists(const char *supply_name) const
{
  return supply_voltage_map_.hasKey(supply_name);
}

////////////////////////////////////////////////////////////////

LibertyCellIterator::LibertyCellIterator(const LibertyLibrary *
						       library):
  iter_(library->cell_map_)
{
}

bool
LibertyCellIterator::hasNext()
{
  return iter_.hasNext();
}

LibertyCell *
LibertyCellIterator::next()
{
  return static_cast<LibertyCell*>(iter_.next());
}

////////////////////////////////////////////////////////////////

LibertyCell::LibertyCell(LibertyLibrary *library,
			 const char *name,
			 const char *filename) :
  ConcreteCell(library, name, true, filename),
  liberty_library_(library),
  area_(0.0),
  dont_use_(false),
  is_macro_(false),
  is_pad_(false),
  has_internal_ports_(false),
  interface_timing_(false),
  clock_gate_type_(ClockGateType::none),
  has_infered_reg_timing_arcs_(false),
  scale_factors_(nullptr),
  test_cell_(nullptr),
  ocv_arc_depth_(0.0),
  ocv_derate_(nullptr),
  is_disabled_constraint_(false),
  leakage_power_(0.0),
  leakage_power_exists_(false)
{
  liberty_cell_ = this;
}

LibertyCell::~LibertyCell()
{
  mode_defs_.deleteContents();
  latch_d_to_q_map_.deleteContents();

  deleteTimingArcAttrs();
  timing_arc_sets_.deleteContents();
  port_timing_arc_set_map_.deleteContents();
  timing_arc_set_from_map_.deleteContents();
  timing_arc_set_to_map_.deleteContents();

  deleteInternalPowerAttrs();
  internal_powers_.deleteContents();
  leakage_powers_.deleteContents();

  sequentials_.deleteContents();
  bus_dcls_.deleteContents();
  scaled_cells_.deleteContents();

  delete test_cell_;
  ocv_derate_map_.deleteContents();

  pg_port_map_.deleteContents();
}

void
LibertyCell::deleteTimingArcAttrs()
{
  for (auto attrs : timing_arc_attrs_) {
    attrs->deleteContents();
    delete attrs;
  }
}

LibertyPort *
LibertyCell::findLibertyPort(const char *name) const
{
  return static_cast<LibertyPort*>(findPort(name));
}

void
LibertyCell::findLibertyPortsMatching(PatternMatch *pattern,
				      LibertyPortSeq *ports) const
{
  LibertyCellPortIterator port_iter(this);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    if (pattern->match(port->name()))
      ports->push_back(port);
  }
}

void
LibertyCell::addPort(ConcretePort *port)
{
  ConcreteCell::addPort(port);
  if (port->direction()->isInternal())
    has_internal_ports_ = true;
}


void
LibertyCell::setHasInternalPorts(bool has_internal)
{
  has_internal_ports_ = has_internal;
}

void
LibertyCell::addPgPort(LibertyPgPort *pg_port)
{
  pg_port_map_[pg_port->name()] = pg_port;
}

LibertyPgPort *
LibertyCell::findPgPort(const char *name)
{
  return pg_port_map_.findKey(name);
}

ModeDef *
LibertyCell::makeModeDef(const char *name)
{
  ModeDef *mode = new ModeDef(name);
  mode_defs_[mode->name()] = mode;
  return mode;
}

ModeDef *
LibertyCell::findModeDef(const char *name)
{
  return mode_defs_.findKey(name);
}

void
LibertyCell::setScaleFactors(ScaleFactors *scale_factors)
{
  scale_factors_ = scale_factors;
}

void
LibertyCell::addBusDcl(BusDcl *bus_dcl)
{
  bus_dcls_[bus_dcl->name()] = bus_dcl;
}

BusDcl *
LibertyCell::findBusDcl(const char *name) const
{
  return bus_dcls_.findKey(name);
}

void
LibertyCell::setArea(float area)
{
  area_ = area;
}

void
LibertyCell::setDontUse(bool dont_use)
{
  dont_use_ = dont_use;
}

void
LibertyCell::setIsMacro(bool is_macro)
{
  is_macro_ = is_macro;
}

void
LibertyCell::LibertyCell::setIsPad(bool is_pad)
{
  is_pad_ = is_pad;
}

void
LibertyCell::setInterfaceTiming(bool value)
{
  interface_timing_ = value;
}

bool
LibertyCell::isClockGateLatchPosedge() const
{
  return clock_gate_type_ == ClockGateType::latch_posedge;
}

bool
LibertyCell::isClockGateLatchNegedge() const
{
  return clock_gate_type_ == ClockGateType::latch_posedge;
}

bool
LibertyCell::isClockGateOther() const
{
  return clock_gate_type_ == ClockGateType::other;
}

bool
LibertyCell::isClockGate() const
{
  return clock_gate_type_ != ClockGateType::none;
}

void
LibertyCell::setClockGateType(ClockGateType type)
{
  clock_gate_type_ = type;
}

bool
LibertyCell::isBuffer() const
{
  LibertyPort *input;
  LibertyPort *output;
  bufferPorts(input, output);
  return input && output
    && hasBufferFunc(input, output);
}

bool
LibertyCell::hasBufferFunc(const LibertyPort *input,
			   const LibertyPort *output) const
{
  FuncExpr *func = output->function();
  return func
    && func->op() == FuncExpr::op_port
    && func->port() == input;
}

void
LibertyCell::bufferPorts(// Return values.
			 LibertyPort *&input,
			 LibertyPort *&output) const
{
  input = nullptr;
  output = nullptr;
  for (ConcretePort *cport : ports_) {
    LibertyPort *port = static_cast<LibertyPort*>(cport);
    PortDirection *dir = port->direction();
    if (dir->isInput()) {
      if (input) {
	// More than one input.
	input = nullptr;
	output = nullptr;
	break;
      }
      input = port;
    }
    else if (dir->isOutput()) {
      if (output) {
	// More than one output.
	input = nullptr;
	output = nullptr;
	break;
      }
      output = port;
    }
    else if (!dir->isPowerGround()) {
	input = nullptr;
	output = nullptr;
	break;
    }
  }
}

unsigned
LibertyCell::addTimingArcSet(TimingArcSet *arc_set)
{
  int set_index = timing_arc_sets_.size();
  if (set_index > timing_arc_set_index_max)
    internalError("timing arc set max index exceeded");
  timing_arc_sets_.push_back(arc_set);

  LibertyPort *from = arc_set->from();
  TimingRole *role = arc_set->role();
  if (role == TimingRole::regClkToQ()
      || role == TimingRole::latchEnToQ())
    from->setIsRegClk(true);
  if (role->isTimingCheck())
    from->setIsCheckClk(true);
  return set_index;
}

void
LibertyCell::addTimingArcAttrs(TimingArcAttrs *attrs)
{
  timing_arc_attrs_.push_back(attrs);
}

void
LibertyCell::addInternalPower(InternalPower *power)
{
  internal_powers_.push_back(power);
}

void
LibertyCell::addInternalPowerAttrs(InternalPowerAttrs *attrs)
{
  internal_power_attrs_.push_back(attrs);
}

void
LibertyCell::deleteInternalPowerAttrs()
{
  for (auto attrs : internal_power_attrs_) {
    attrs->deleteContents();
    delete attrs;
  }
}

void
LibertyCell::addLeakagePower(LeakagePower *power)
{
  leakage_powers_.push_back(power);
}

void
LibertyCell::setLeakagePower(float leakage)
{
  leakage_power_ = leakage;
  leakage_power_exists_ = true;
}

void
LibertyCell::leakagePower(// Return values.
			  float &leakage,
			  bool &exists) const
{
  leakage = leakage_power_;
  exists = leakage_power_exists_;
}

void
LibertyCell::finish(bool infer_latches,
		    Report *report,
		    Debug *debug)
{
  translatePresetClrCheckRoles();
  makeTimingArcMap(report);
  makeTimingArcPortMaps();
  findDefaultCondArcs();
  makeLatchEnables(report, debug);
  if (infer_latches
      && !interface_timing_)
    inferLatchRoles(debug);
}

void
LibertyCell::findDefaultCondArcs()
{
  for (auto port_pair_set : port_timing_arc_set_map_) {
    TimingArcSetSeq *sets = port_pair_set.second;
    bool has_cond_arcs = false;
    for (auto set : *sets) {
      if (set->cond()) {
	has_cond_arcs = true;
	break;
      }
    }
    if (has_cond_arcs) {
      for (auto set : *sets) {
	if (!set->cond())
	  set->setIsCondDefault(true);
      }
    }
  }
}

// Timing checks for set/clear pins use setup/hold times.  This
// changes their roles to recovery/removal by finding the set/clear
// pins and then translating the timing check roles.
void
LibertyCell::translatePresetClrCheckRoles()
{
  LibertyPortSet pre_clr_ports;
  for (auto arc_set : timing_arc_sets_) {
    if (arc_set->role() == TimingRole::regSetClr())
      pre_clr_ports.insert(arc_set->from());
  }

  if (!pre_clr_ports.empty()) {
    for (auto arc_set : timing_arc_sets_) {
      if (pre_clr_ports.findKey(arc_set->to())) {
	if (arc_set->role() == TimingRole::setup())
	  arc_set->setRole(TimingRole::recovery());
	else if (arc_set->role() == TimingRole::hold())
	  arc_set->setRole(TimingRole::removal());
      }
    }
  }
}

void
LibertyCell::makeTimingArcMap(Report *)
{
  // Filter duplicate timing arcs, keeping the later definition.
  for (auto arc_set : timing_arc_sets_)
    // The last definition will be left in the set.
    timing_arc_set_map_.insert(arc_set);

  // Prune the arc sets not in the map.
  int j = 0;
  for (size_t i = 0; i < timing_arc_sets_.size(); i++) {
    TimingArcSet *arc_set = timing_arc_sets_[i];
    TimingArcSet *match = timing_arc_set_map_.findKey(arc_set);
    if (match != arc_set) {
      // Unfortunately these errors are common in some brain damaged
      // libraries.
      // report->warn("cell %s/%s has duplicate %s -> %s %s timing groups.\n",
      // 		   library_->name(),
      // 		   name_,
      // 		   match->from()->name(),
      // 		   match->to()->name(),
      // 		   match->role()->asString());
      delete arc_set;
    }
    else
      // Shift arc sets down to fill holes left by removed duplicates.
      timing_arc_sets_[j++] = arc_set;
  }
  timing_arc_sets_.resize(j);

  if (timing_arc_set_map_.size() != timing_arc_sets_.size())
    internalError("timing arc count mismatch\n");
}

void
LibertyCell::makeTimingArcPortMaps()
{
  for (auto arc_set : timing_arc_sets_) {
    LibertyPort *from = arc_set->from();
    LibertyPort *to = arc_set->to();
    LibertyPortPair port_pair(from, to);
    TimingArcSetSeq *sets = port_timing_arc_set_map_.findKey(port_pair);
    if (sets == nullptr) {
      // First arc set for from/to ports.
      sets = new TimingArcSetSeq;
      port_timing_arc_set_map_[port_pair] = sets;
    }
    sets->push_back(arc_set);

    sets = timing_arc_set_from_map_.findKey(from);
    if (sets == nullptr) {
      sets = new TimingArcSetSeq;
      timing_arc_set_from_map_[from] = sets;
    }
    sets->push_back(arc_set);

    sets = timing_arc_set_to_map_.findKey(to);
    if (sets == nullptr) {
      sets = new TimingArcSetSeq;
      timing_arc_set_to_map_[to] = sets;
    }
    sets->push_back(arc_set);
  }
}

TimingArcSetSeq *
LibertyCell::timingArcSets(const LibertyPort *from,
			   const LibertyPort *to) const
{
  if (from && to) {
    LibertyPortPair port_pair(from, to);
    return port_timing_arc_set_map_.findKey(port_pair);
  }
  else if (from)
    return timing_arc_set_from_map_.findKey(from);
  else if (to)
    return timing_arc_set_to_map_.findKey(to);
  else
    return nullptr;
}

TimingArcSet *
LibertyCell::findTimingArcSet(TimingArcSet *key) const
{
  return timing_arc_set_map_.findKey(key);
}

TimingArcSet *
LibertyCell::findTimingArcSet(unsigned arc_set_index) const
{
  return timing_arc_sets_[arc_set_index];
}

size_t
LibertyCell::timingArcSetCount() const
{
  return timing_arc_sets_.size();
}

bool
LibertyCell::hasTimingArcs(LibertyPort *port) const
{
  return timing_arc_set_from_map_.findKey(port)
    || timing_arc_set_to_map_.findKey(port);
}

void
LibertyCell::makeSequential(int size,
			    bool is_register,
			    FuncExpr *clk,
			    FuncExpr *data,
			    FuncExpr *clear,
			    FuncExpr *preset,
			    LogicValue clr_preset_out,
			    LogicValue clr_preset_out_inv,
			    LibertyPort *output,
			    LibertyPort *output_inv)
{
  for (int bit = 0; bit < size; bit++) {
    FuncExpr *clk_bit = nullptr;
    if (clk)
      clk_bit = clk->bitSubExpr(bit);
    FuncExpr *data_bit = nullptr;
    if (data)
      data_bit = data->bitSubExpr(bit);
    FuncExpr *clear_bit = nullptr;
    if (clear)
      clear_bit = clear->bitSubExpr(bit);
    FuncExpr *preset_bit = nullptr;
    if (preset)
      preset_bit = preset->bitSubExpr(bit);
    LibertyPort *out_bit = output;
    if (output && output->hasMembers())
      out_bit = output->findLibertyMember(bit);
    LibertyPort *out_inv_bit = output_inv;
    if (output_inv && output_inv->hasMembers())
      out_inv_bit = output_inv->findLibertyMember(bit);
    Sequential *seq = new Sequential(is_register, clk_bit, data_bit,
				     clear_bit,preset_bit,
				     clr_preset_out, clr_preset_out_inv,
				     out_bit, out_inv_bit);
    sequentials_.push_back(seq);
    port_to_seq_map_[seq->output()] = seq;
    port_to_seq_map_[seq->outputInv()] = seq;
  }
}

Sequential *
LibertyCell::outputPortSequential(LibertyPort *port)
{
  return port_to_seq_map_.findKey(port);
}

bool
LibertyCell::hasSequentials() const
{
  return !sequentials_.empty();
}

void
LibertyCell::addScaledCell(OperatingConditions *op_cond,
			   LibertyCell *scaled_cell)
{
  scaled_cells_[op_cond] = scaled_cell;

  LibertyCellPortBitIterator port_iter1(this);
  LibertyCellPortBitIterator port_iter2(scaled_cell);
  while (port_iter1.hasNext() && port_iter2.hasNext()) {
    LibertyPort *port = port_iter1.next();
    LibertyPort *scaled_port = port_iter2.next();
    port->addScaledPort(op_cond, scaled_port);
  }

  LibertyCellTimingArcSetIterator set_iter1(this);
  LibertyCellTimingArcSetIterator set_iter2(scaled_cell);
  while (set_iter1.hasNext() && set_iter2.hasNext()) {
    TimingArcSet *arc_set1 = set_iter1.next();
    TimingArcSet *arc_set2 = set_iter2.next();
    TimingArcSetArcIterator arc_iter1(arc_set1);
    TimingArcSetArcIterator arc_iter2(arc_set2);
    while (arc_iter1.hasNext() && arc_iter2.hasNext()) {
      TimingArc *arc = arc_iter1.next();
      TimingArc *scaled_arc = arc_iter2.next();
      if (TimingArc::equiv(arc, scaled_arc)) {
	TimingModel *model = scaled_arc->model();
	model->setIsScaled(true);
	arc->addScaledModel(op_cond, model);
      }
    }
  }
}

void
LibertyCell::setLibertyLibrary(LibertyLibrary *library)
{
  liberty_library_ = library;
  library_ = library;
}

void
LibertyCell::setHasInferedRegTimingArcs(bool infered)
{
  has_infered_reg_timing_arcs_ = infered;
}

void
LibertyCell::setTestCell(TestCell *test)
{
  test_cell_ = test;
}

void
LibertyCell::setIsDisabledConstraint(bool is_disabled)
{
  is_disabled_constraint_ = is_disabled;
}

LibertyCell *
LibertyCell::cornerCell(int ap_index)
{
  if (ap_index < static_cast<int>(corner_cells_.size()))
    return corner_cells_[ap_index];
  else
    return nullptr;
}

void
LibertyCell::setCornerCell(LibertyCell *corner_cell,
			   int ap_index)
{
  if (ap_index >= static_cast<int>(corner_cells_.size()))
    corner_cells_.resize(ap_index + 1);
  corner_cells_[ap_index] = corner_cell;
}

////////////////////////////////////////////////////////////////

float
LibertyCell::ocvArcDepth() const
{
  return ocv_arc_depth_;
}

void
LibertyCell::setOcvArcDepth(float depth)
{
  ocv_arc_depth_ = depth;
}

OcvDerate *
LibertyCell::ocvDerate() const
{
  if (ocv_derate_)
    return ocv_derate_;
  else
    return liberty_library_->defaultOcvDerate();
}

void
LibertyCell::setOcvDerate(OcvDerate *derate)
{
  ocv_derate_ = derate;
}

OcvDerate *
LibertyCell::findOcvDerate(const char *derate_name)
{
  return ocv_derate_map_.findKey(derate_name);
}

void
LibertyCell::addOcvDerate(OcvDerate *derate)
{
  ocv_derate_map_[derate->name()] = derate;
}

////////////////////////////////////////////////////////////////

LibertyCellTimingArcSetIterator::LibertyCellTimingArcSetIterator(const LibertyCell *cell) :
  TimingArcSetSeq::ConstIterator(&cell->timing_arc_sets_)
{
}

LibertyCellTimingArcSetIterator::LibertyCellTimingArcSetIterator(const LibertyCell *cell,
								 const LibertyPort *from,
								 const LibertyPort *to):
  TimingArcSetSeq::ConstIterator(cell->timingArcSets(from, to))
{
}
  
////////////////////////////////////////////////////////////////

// Latch enable port/function for a latch D->Q timing arc set.
class LatchEnable
{
public:
  LatchEnable(LibertyPort *data,
	      LibertyPort *enable,
	      RiseFall *enable_rf,
	      FuncExpr *enable_func,
	      LibertyPort *output,
	      TimingArcSet *d_to_q,
	      TimingArcSet *en_to_q,
	      TimingArcSet *setup_check);
  LibertyPort *data() const { return data_; }
  LibertyPort *output() const { return output_; }
  LibertyPort *enable() const { return enable_; }
  FuncExpr *enableFunc() const { return enable_func_; }
  RiseFall *enableTransition() const { return enable_rf_; }
  TimingArcSet *dToQ() const { return d_to_q_; }
  TimingArcSet *enToQ() const { return en_to_q_; }
  TimingArcSet *setupCheck() const { return setup_check_; }

private:
  DISALLOW_COPY_AND_ASSIGN(LatchEnable);

  LibertyPort *data_;
  LibertyPort *enable_;
  RiseFall *enable_rf_;
  FuncExpr *enable_func_;
  LibertyPort *output_;
  TimingArcSet *d_to_q_;
  TimingArcSet *en_to_q_;
  TimingArcSet *setup_check_;
};

LatchEnable::LatchEnable(LibertyPort *data,
			 LibertyPort *enable,
			 RiseFall *enable_rf,
			 FuncExpr *enable_func,
			 LibertyPort *output,
			 TimingArcSet *d_to_q,
			 TimingArcSet *en_to_q,
			 TimingArcSet *setup_check) :
  data_(data),
  enable_(enable),
  enable_rf_(enable_rf),
  enable_func_(enable_func),
  output_(output),
  d_to_q_(d_to_q),
  en_to_q_(en_to_q),
  setup_check_(setup_check)
{
}

// Latch enable port/function for a latch D->Q timing arc set.
// This augments cell timing data by linking enables to D->Q arcs.
// Use timing arcs rather than sequentials (because they are optional).
void
LibertyCell::makeLatchEnables(Report *report,
			      Debug *debug)
{
  if (hasSequentials()
      || hasInferedRegTimingArcs()) {
    for (auto en_to_q : timing_arc_sets_) {
      if (en_to_q->role() == TimingRole::latchEnToQ()) {
	LibertyPort *en = en_to_q->from();
	LibertyPort *q = en_to_q->to();
	LibertyCellTimingArcSetIterator to_iter(this, nullptr, q);
	while (to_iter.hasNext()) {
	  TimingArcSet *d_to_q = to_iter.next();
	  if (d_to_q->role() == TimingRole::latchDtoQ()) {
	    LibertyPort *d = d_to_q->from();
	    LibertyCellTimingArcSetIterator check_iter(this, en, d);
	    while (check_iter.hasNext()) {
	      TimingArcSet *setup_check = check_iter.next();
	      if (setup_check->role() == TimingRole::setup()) {
		LatchEnable *latch_enable = makeLatchEnable(d, en, q, d_to_q,
							    en_to_q,
							    setup_check,
							    debug);
		TimingArcSetArcIterator check_arc_iter(setup_check);
		if (check_arc_iter.hasNext()) {
		  TimingArc *check_arc = check_arc_iter.next();
		  RiseFall *en_rf = latch_enable->enableTransition();
		  RiseFall *check_rf = check_arc->fromTrans()->asRiseFall();
		  if (check_rf == en_rf) {
		    report->warn("cell %s/%s %s -> %s latch enable %s_edge timing arc is inconsistent with %s -> %s setup_%s check.\n",
				 library_->name(),
				 name_,
				 en->name(),
				 q->name(),
				 en_rf == RiseFall::rise()?"rising":"falling",
				 en->name(),
				 d->name(),
				 check_rf==RiseFall::rise()?"rising":"falling");
		  }
		  FuncExpr *en_func = latch_enable->enableFunc();
		  if (en_func) {
		    TimingSense en_sense = en_func->portTimingSense(en);
		    if (en_sense == TimingSense::positive_unate
			&& en_rf != RiseFall::rise())
		      report->warn("cell %s/%s %s -> %s latch enable %s_edge is inconsistent with latch group enable function positive sense.\n",
				   library_->name(),
				   name_,
				   en->name(),
				   q->name(),
				   en_rf == RiseFall::rise()?"rising":"falling");
		    else if (en_sense == TimingSense::negative_unate
			     && en_rf != RiseFall::fall())
		      report->warn("cell %s/%s %s -> %s latch enable %s_edge is inconsistent with latch group enable function negative sense.\n",
				   library_->name(),
				   name_,
				   en->name(),
				   q->name(),
				   en_rf == RiseFall::rise()?"rising":"falling");
		  }
		  break;
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

FuncExpr *
LibertyCell::findLatchEnableFunc(LibertyPort *data,
				 LibertyPort *enable) const
{
  for (auto seq : sequentials_) {
    if (seq->isLatch()
	&& seq->data()
	&& seq->data()->hasPort(data)
	&& seq->clock()
	&& seq->clock()->hasPort(enable))
      return seq->clock();
  }
  return nullptr;
}

LatchEnable *
LibertyCell::makeLatchEnable(LibertyPort *d,
			     LibertyPort *en,
			     LibertyPort *q,
			     TimingArcSet *d_to_q,
			     TimingArcSet *en_to_q,
			     TimingArcSet *setup_check,
			     Debug *debug)
{
  RiseFall *en_rf = en_to_q->isRisingFallingEdge();
  FuncExpr *en_func = findLatchEnableFunc(d, en);
  LatchEnable *latch_enable = new LatchEnable(d, en, en_rf, en_func, q,
					      d_to_q, en_to_q, setup_check);
  // Multiple enables for D->Q pairs are not supported.
  if (latch_d_to_q_map_[d_to_q])
    delete latch_d_to_q_map_[d_to_q];
  latch_d_to_q_map_[d_to_q] = latch_enable;
  latch_check_map_[setup_check] = latch_enable;
  latch_data_ports_.insert(d);
  debugPrint3(debug, "liberty", 2, "latch d=%s en=%s q=%s\n",
	      d->name(), en->name(), q->name());
  return latch_enable;
}

void
LibertyCell::inferLatchRoles(Debug *debug)
{
  if (hasInferedRegTimingArcs()) {
    // Hunt down potential latch D/EN/Q triples.
    LatchEnableSet latch_enables;
    LibertyCellTimingArcSetIterator set_iter(this);
    while (set_iter.hasNext()) {
      TimingArcSet *en_to_q = set_iter.next();
      // Locate potential d->q arcs from reg clk->q arcs.
      if (en_to_q->role() == TimingRole::regClkToQ()) {
	LibertyPort *en = en_to_q->from();
	LibertyPort *q = en_to_q->to();
	LibertyCellTimingArcSetIterator to_iter(this, nullptr, q);
	while (to_iter.hasNext()) {
	  TimingArcSet *d_to_q = to_iter.next();
	  // Look for combinational d->q arcs.
	  TimingRole *d_to_q_role = d_to_q->role();
	  if ((d_to_q_role == TimingRole::combinational()
	       && ((d_to_q->arcCount() == 2
		    && (d_to_q->sense() == TimingSense::positive_unate
			|| d_to_q->sense() == TimingSense::negative_unate))
		   || (d_to_q->arcCount() == 4)))
	      // Previously identified as D->Q arc.
	      || d_to_q_role == TimingRole::latchDtoQ()) {
	    LibertyPort *d = d_to_q->from();
	    // Check for setup check from en -> d.
	    LibertyCellTimingArcSetIterator check_iter(this, en, d);
	    while (check_iter.hasNext()) {
	      TimingArcSet *setup_check = check_iter.next();
	      if (setup_check->role() == TimingRole::setup()) {
		makeLatchEnable(d, en, q, d_to_q, en_to_q, setup_check, debug);
		d_to_q->setRole(TimingRole::latchDtoQ());
		en_to_q->setRole(TimingRole::latchEnToQ());
	      }
	    }
	  }
	}
      }
    }
  }
}

bool
LibertyCell::isLatchData(LibertyPort *port)
{
  return latch_data_ports_.hasKey(port);
}

void
LibertyCell::latchEnable(TimingArcSet *d_to_q_set,
			 // Return values.
			 LibertyPort *&enable_port,
			 FuncExpr *&enable_func,
			 RiseFall *&enable_rf) const
{
  enable_port = nullptr;
  LatchEnable *latch_enable = latch_d_to_q_map_.findKey(d_to_q_set);
  if (latch_enable) {
    enable_port = latch_enable->enable();
    enable_func = latch_enable->enableFunc();
    enable_rf = latch_enable->enableTransition();
  }
}

RiseFall *
LibertyCell::latchCheckEnableTrans(TimingArcSet *check_set)
{
  LatchEnable *latch_enable = latch_check_map_.findKey(check_set);
  if (latch_enable)
    return latch_enable->enableTransition();
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

LibertyCellPortIterator::LibertyCellPortIterator(const LibertyCell *cell) :
  iter_(cell->ports_)
{
}

bool
LibertyCellPortIterator::hasNext()
{
  return iter_.hasNext();
}

LibertyPort *
LibertyCellPortIterator::next()
{
  return static_cast<LibertyPort*>(iter_.next());
}

////////////////////////////////////////////////////////////////

LibertyCellPortBitIterator::LibertyCellPortBitIterator(const LibertyCell *cell):
  iter_(cell->portBitIterator())
{
}

LibertyCellPortBitIterator::~LibertyCellPortBitIterator()
{
  delete iter_;
}

bool
LibertyCellPortBitIterator::hasNext()
{
  return iter_->hasNext();
}

LibertyPort *
LibertyCellPortBitIterator::next()
{
  return static_cast<LibertyPort*>(iter_->next());
}

////////////////////////////////////////////////////////////////

LibertyPort::LibertyPort(LibertyCell *cell,
			 const char *name,
			 bool is_bus,
			 int from_index,
			 int to_index,
			 bool is_bundle,
			 ConcretePortSeq *members) :
  ConcretePort(cell, name, is_bus, from_index, to_index, is_bundle, members),
  liberty_cell_(cell),
  function_(nullptr),
  tristate_enable_(nullptr),
  scaled_ports_(nullptr),
  // capacitance_ intentionally not initialized so
  // liberty reader can apply default capacitance.
  min_period_(0.0),
  pulse_clk_trigger_(nullptr),
  pulse_clk_sense_(nullptr),
  related_ground_pin_(nullptr),
  related_power_pin_(nullptr),
  min_pulse_width_exists_(false),
  min_period_exists_(false),
  is_clk_(false),
  is_reg_clk_(false),
  is_check_clk_(false),
  is_clk_gate_clk_pin_(false),
  is_clk_gate_enable_pin_(false),
  is_clk_gate_out_pin_(false),
  is_pll_feedback_pin_(false),
  is_disabled_constraint_(false)
{
  liberty_port_ = this;
  min_pulse_width_[RiseFall::riseIndex()] = 0.0;
  min_pulse_width_[RiseFall::fallIndex()] = 0.0;
}

LibertyPort::~LibertyPort()
{
  if (function_)
    function_->deleteSubexprs();
  if (tristate_enable_)
    tristate_enable_->deleteSubexprs();
  delete scaled_ports_;
  stringDelete(related_ground_pin_);
  stringDelete(related_power_pin_);
}

void
LibertyPort::setDirection(PortDirection *dir)
{
  ConcretePort::setDirection(dir);
  if (dir->isInternal())
    liberty_cell_->setHasInternalPorts(true);
}

LibertyPort *
LibertyPort::findLibertyMember(int index) const
{
  return static_cast<LibertyPort*>(findMember(index));
}

LibertyPort *
LibertyPort::findLibertyBusBit(int index) const
{
  return static_cast<LibertyPort*>(findBusBit(index));
}

void
LibertyPort::setCapacitance(float cap)
{
  setCapacitance(RiseFall::rise(), MinMax::min(), cap);
  setCapacitance(RiseFall::fall(), MinMax::min(), cap);
  setCapacitance(RiseFall::rise(), MinMax::max(), cap);
  setCapacitance(RiseFall::fall(), MinMax::max(), cap);
}

void
LibertyPort::setCapacitance(const RiseFall *rf,
			    const MinMax *min_max,
			    float cap)
{
  capacitance_.setValue(rf, min_max, cap);
  if (hasMembers()) {
    LibertyPortMemberIterator member_iter(this);
    while (member_iter.hasNext()) {
      LibertyPort *port_bit = member_iter.next();
      port_bit->setCapacitance(rf, min_max, cap);
    }
  }
}

float
LibertyPort::capacitance(const RiseFall *rf,
			 const MinMax *min_max) const
{
  float cap;
  bool exists;
  capacitance_.value(rf, min_max, cap, exists);
  if (exists)
    return cap;
  else
    return 0.0;
}

void
LibertyPort::capacitance(const RiseFall *rf,
			 const MinMax *min_max,
			 // Return values.
			 float &cap,
			 bool &exists) const
{
  capacitance_.value(rf, min_max, cap, exists);
}

float
LibertyPort::capacitance(const RiseFall *rf,
			 const MinMax *min_max,
			 const OperatingConditions *op_cond,
			 const Pvt *pvt) const
{
  if (scaled_ports_) {
    LibertyPort *scaled_port = (*scaled_ports_)[op_cond];
    // Scaled capacitance is not derated because scale factors are wrt
    // nominal pvt.
    if (scaled_port)
      return scaled_port->capacitance(rf, min_max);
  }
  LibertyLibrary *lib = liberty_cell_->libertyLibrary();
  float cap = capacitance(rf, min_max);
  return cap * lib->scaleFactor(ScaleFactorType::pin_cap, liberty_cell_, pvt);
}

bool
LibertyPort::capacitanceIsOneValue() const
{
  return capacitance_.isOneValue();
}

////////////////////////////////////////////////////////////////

// Use the min/max "drive" for all the timing arcs in the cell.
float
LibertyPort::driveResistance(const RiseFall *rf,
			     const MinMax *min_max) const
{
  float max_drive = min_max->initValue();
  bool found_drive = false;
  LibertyCellTimingArcSetIterator set_iter(liberty_cell_, nullptr, this);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    if (!set->role()->isTimingCheck()) {
      TimingArcSetArcIterator arc_iter(set);
      while (arc_iter.hasNext()) {
	TimingArc *arc = arc_iter.next();
	if (rf == nullptr
	    || arc->toTrans()->asRiseFall() == rf) {
	  GateTimingModel *model = dynamic_cast<GateTimingModel*>(arc->model());
	  if (model) {
	    float drive = model->driveResistance(liberty_cell_, nullptr);
	    if (min_max->compare(drive, max_drive))
	      max_drive = drive;
	    found_drive = true;
	  }
	}
      }
    }
  }
  if (found_drive)
    return max_drive;
  else
    return 0.0;
}

float
LibertyPort::driveResistance() const
{
  return driveResistance(nullptr, MinMax::max());
}

void
LibertyPort::setFunction(FuncExpr *func)
{
  function_ = func;
  if (is_bus_ || is_bundle_) {
    LibertyPortMemberIterator member_iter(this);
    int bit_offset = 0;
    while (member_iter.hasNext()) {
      LibertyPort *port_bit = member_iter.next();
      FuncExpr *sub_expr = (func) ? func->bitSubExpr(bit_offset) : nullptr;
      port_bit->setFunction(sub_expr);
      bit_offset++;
    }
  }
}

void
LibertyPort::setTristateEnable(FuncExpr *enable)
{
  tristate_enable_ = enable;
  if (hasMembers()) {
    LibertyPortMemberIterator member_iter(this);
    while (member_iter.hasNext()) {
      LibertyPort *port_bit = member_iter.next();
      FuncExpr *sub_expr =
	(enable) ? enable->bitSubExpr(port_bit->busBitIndex()) : nullptr;
      port_bit->setTristateEnable(sub_expr);
    }
  }
}

void
LibertyPort::slewLimit(const MinMax *min_max,
		       // Return values.
		       float &limit,
		       bool &exists) const
{
  slew_limit_.value(min_max, limit, exists);
}

void
LibertyPort::setSlewLimit(float slew, const MinMax *min_max)
{
  slew_limit_.setValue(min_max, slew);
}

void
LibertyPort::capacitanceLimit(const MinMax *min_max,
			      // Return values.
			      float &limit,
			      bool &exists) const
{
  return cap_limit_.value(min_max, limit, exists);
}

void
LibertyPort::setCapacitanceLimit(float cap,
				 const MinMax *min_max)
{
  cap_limit_.setValue(min_max, cap);
}

void
LibertyPort::fanoutLimit(const MinMax *min_max,
			 // Return values.
			 float &limit,
			 bool &exists) const
{
  return fanout_limit_.value(min_max, limit, exists);
}

void
LibertyPort::setFanoutLimit(float fanout,
			    const MinMax *min_max)
{
  fanout_limit_.setValue(min_max, fanout);
}

void
LibertyPort::minPeriod(const OperatingConditions *op_cond,
		       const Pvt *pvt,
		       float &min_period,
		       bool &exists) const
{
  if (scaled_ports_) {
    LibertyPort *scaled_port = (*scaled_ports_)[op_cond];
    if (scaled_port) {
      scaled_port->minPeriod(min_period, exists);
      return;
    }
  }
  LibertyLibrary *lib = liberty_cell_->libertyLibrary();
  min_period = min_period_ * lib->scaleFactor(ScaleFactorType::min_period,
					      liberty_cell_, pvt);
  exists = min_period_exists_;
}

void
LibertyPort::minPeriod(float &min_period,
		       bool &exists) const
{
  min_period = min_period_;
  exists = min_period_exists_;
}

void
LibertyPort::setMinPeriod(float min_period)
{
  min_period_ = min_period;
  min_period_exists_ = true;
}

void
LibertyPort::minPulseWidth(const RiseFall *hi_low,
			   const OperatingConditions *op_cond,
			   const Pvt *pvt,
			   float &min_width,
			   bool &exists) const
{
  if (scaled_ports_) {
    LibertyPort *scaled_port = (*scaled_ports_)[op_cond];
    if (scaled_port) {
      scaled_port->minPulseWidth(hi_low, min_width, exists);
      return;
    }
  }
  int hi_low_index = hi_low->index();
  LibertyLibrary *lib = liberty_cell_->libertyLibrary();
  min_width = min_pulse_width_[hi_low_index]
    * lib->scaleFactor(ScaleFactorType::min_pulse_width, hi_low_index,
		       liberty_cell_, pvt);
  exists = min_pulse_width_exists_ & (1 << hi_low_index);
}

void
LibertyPort::minPulseWidth(const RiseFall *hi_low,
			   float &min_width,
			   bool &exists) const
{
  int hi_low_index = hi_low->index();
  min_width = min_pulse_width_[hi_low_index];
  exists = min_pulse_width_exists_ & (1 << hi_low_index);
}

void
LibertyPort::setMinPulseWidth(RiseFall *hi_low,
			      float min_width)
{
  int hi_low_index = hi_low->index();
  min_pulse_width_[hi_low_index] = min_width;
  min_pulse_width_exists_ |= (1 << hi_low_index);
}

bool
LibertyPort::equiv(const LibertyPort *port1,
		   const LibertyPort *port2)
{
  return (port1 == nullptr && port2 == nullptr)
    || (port1 != nullptr && port2 != nullptr
	&& stringEq(port1->name(), port2->name())
	&& port1->direction() == port2->direction());
}

bool
LibertyPort::less(const LibertyPort *port1,
		  const LibertyPort *port2)
{
  const char *name1 = port1->name();
  const char *name2 = port2->name();
  if (stringEq(name1, name2)) {
    PortDirection *dir1 = port1->direction();
    PortDirection *dir2 = port2->direction();
    if (dir1 == dir2) {
    }
    else
      return dir1->index() < dir2->index();
  }
  return stringLess(name1, name2);
}

void
LibertyPort::addScaledPort(OperatingConditions *op_cond,
			   LibertyPort *scaled_port)
{
  if (scaled_ports_ == nullptr)
    scaled_ports_ = new ScaledPortMap;
  (*scaled_ports_)[op_cond] = scaled_port;
}

bool
LibertyPort::isClock() const
{
  return is_clk_;
}

void
LibertyPort::setIsClock(bool is_clk)
{
  is_clk_ = is_clk;
}

void
LibertyPort::setIsRegClk(bool is_clk)
{
  is_reg_clk_ = is_clk;
}

void
LibertyPort::setIsCheckClk(bool is_clk)
{
  is_check_clk_ = is_clk;
}

void
LibertyPort::setIsClockGateClockPin(bool is_clk_gate_clk)
{
  is_clk_gate_clk_pin_ = is_clk_gate_clk;
}

void
LibertyPort::setIsClockGateEnablePin(bool is_clk_gate_enable)
{
  is_clk_gate_enable_pin_ = is_clk_gate_enable;
}

void
LibertyPort::setIsClockGateOutPin(bool is_clk_gate_out)
{
  is_clk_gate_out_pin_ = is_clk_gate_out;
}

void
LibertyPort::setIsPllFeedbackPin(bool is_pll_feedback_pin)
{
  is_pll_feedback_pin_ = is_pll_feedback_pin;
}

void
LibertyPort::setPulseClk(RiseFall *trigger,
			 RiseFall *sense)
{
  pulse_clk_trigger_ = trigger;
  pulse_clk_sense_ = sense;
}

void
LibertyPort::setIsDisabledConstraint(bool is_disabled)
{
  is_disabled_constraint_ = is_disabled;
}

LibertyPort *
LibertyPort::cornerPort(int ap_index)
{
  if (ap_index < static_cast<int>(corner_ports_.size())) {
    LibertyPort *corner_port = corner_ports_[ap_index];
    if (corner_port)
      return corner_port;
  }
  return this;
}

void
LibertyPort::setCornerPort(LibertyPort *corner_port,
			   int ap_index)
{
  if (ap_index >= static_cast<int>(corner_ports_.size()))
    corner_ports_.resize(ap_index + 1);
  corner_ports_[ap_index] = corner_port;
}

void
LibertyPort::setRelatedGroundPin(const char *related_ground_pin)
{
  related_ground_pin_ = stringCopy(related_ground_pin);
}

void
LibertyPort::setRelatedPowerPin(const char *related_power_pin)
{
  related_power_pin_ = stringCopy(related_power_pin);
}

////////////////////////////////////////////////////////////////

void
sortLibertyPortSet(LibertyPortSet *set,
		   LibertyPortSeq &ports)
{
  LibertyPortSet::Iterator port_iter(set);
  while (port_iter.hasNext())
    ports.push_back(port_iter.next());
  sort(ports, LibertyPortNameLess());
}

bool
LibertyPortNameLess::operator()(const LibertyPort *port1,
				const LibertyPort *port2) const
{
  return stringLess(port1->name(), port2->name());
}

bool
LibertyPortPairLess::operator()(const LibertyPortPair *pair1,
				const LibertyPortPair *pair2) const
{
  return pair1->first < pair2->first
    || (pair1->first == pair2->first
	&& pair1->second < pair2->second);
}

bool
LibertyPortPairLess::operator()(const LibertyPortPair &pair1,
				const LibertyPortPair &pair2) const
{
  return pair1.first < pair2.first
    || (pair1.first == pair2.first
	&& pair1.second < pair2.second);
}

////////////////////////////////////////////////////////////////

LibertyPortMemberIterator::LibertyPortMemberIterator(const LibertyPort *port) :
  iter_(port->memberIterator())
{
}

LibertyPortMemberIterator::~LibertyPortMemberIterator()
{
  delete iter_;
}

bool
LibertyPortMemberIterator::hasNext()
{
  return iter_->hasNext();
}

LibertyPort *
LibertyPortMemberIterator::next()
{
  return static_cast<LibertyPort*>(iter_->next());
}

////////////////////////////////////////////////////////////////

BusDcl::BusDcl(const char *name,
	       int from,
	       int to) :
  name_(stringCopy(name)),
  from_(from),
  to_(to)
{
}

BusDcl::~BusDcl()
{
  stringDelete(name_);
}

////////////////////////////////////////////////////////////////

ModeDef::ModeDef(const char *name) :
  name_(stringCopy(name))
{
}

ModeDef::~ModeDef()
{
  values_.deleteContents();
  stringDelete(name_);
}

ModeValueDef *
ModeDef::defineValue(const char *value,
		     FuncExpr *cond,
		     const char *sdf_cond)
{
  ModeValueDef *val_def = new ModeValueDef(value, cond, sdf_cond);
  values_[val_def->value()] = val_def;
  return val_def;
}

ModeValueDef *
ModeDef::findValueDef(const char *value)
{
  return values_[value];
}

////////////////////////////////////////////////////////////////

ModeValueDef::ModeValueDef(const char *value,
			   FuncExpr *cond,
			   const char *sdf_cond) :
  value_(stringCopy(value)),
  cond_(cond),
  sdf_cond_(stringCopy(sdf_cond))
{
}

ModeValueDef::~ModeValueDef()
{
  stringDelete(value_);
  if (cond_)
    cond_->deleteSubexprs();
  if (sdf_cond_)
    stringDelete(sdf_cond_);
}

void
ModeValueDef::setSdfCond(const char *sdf_cond)
{
  sdf_cond_ = stringCopy(sdf_cond);
}

////////////////////////////////////////////////////////////////

TableTemplate::TableTemplate(const char *name) :
  name_(stringCopy(name)),
  axis1_(nullptr),
  axis2_(nullptr),
  axis3_(nullptr)
{
}

TableTemplate::TableTemplate(const char *name,
			     TableAxis *axis1,
			     TableAxis *axis2,
			     TableAxis *axis3) :
  name_(stringCopy(name)),
  axis1_(axis1),
  axis2_(axis2),
  axis3_(axis3)
{
}

TableTemplate::~TableTemplate()
{
  stringDelete(name_);
  delete axis1_;
  delete axis2_;
  delete axis3_;
}

void
TableTemplate::setName(const char *name)
{
  stringDelete(name_);
  name_ = stringCopy(name);
}

void
TableTemplate::setAxis1(TableAxis *axis)
{
  axis1_ = axis;
}

void
TableTemplate::setAxis2(TableAxis *axis)
{
  axis2_ = axis;
}

void
TableTemplate::setAxis3(TableAxis *axis)
{
  axis3_ = axis;
}

////////////////////////////////////////////////////////////////

Pvt::Pvt(float process,
	 float voltage,
	 float temperature) :
  process_(process),
  voltage_(voltage),
  temperature_(temperature)
{
}

void
Pvt::setProcess(float process)
{
  process_ = process;
}

void
Pvt::setVoltage(float voltage)
{
  voltage_ = voltage;
}

void
Pvt::setTemperature(float temp)
{
  temperature_ = temp;
}

OperatingConditions::OperatingConditions(const char *name) :
  Pvt(0.0, 0.0, 0.0),
  name_(stringCopy(name)),
  // Default wireload tree.
  wire_load_tree_(WireloadTree::balanced)
{
}

OperatingConditions::OperatingConditions(const char *name,
					 float process,
					 float voltage,
					 float temperature,
					 WireloadTree wire_load_tree) :
  Pvt(process, voltage, temperature),
  name_(stringCopy(name)),
  wire_load_tree_(wire_load_tree)
{
}

OperatingConditions::~OperatingConditions()
{
  stringDelete(name_);
}

void
OperatingConditions::setWireloadTree(WireloadTree tree)
{
  wire_load_tree_ = tree;
}

////////////////////////////////////////////////////////////////

static EnumNameMap<ScaleFactorType> scale_factor_type_map =
  {{ScaleFactorType::pin_cap, "pin_cap"},
   {ScaleFactorType::wire_cap, "wire_res"},
   {ScaleFactorType::min_period, "min_period"},
   {ScaleFactorType::cell, "cell"},
   {ScaleFactorType::hold, "hold"},
   {ScaleFactorType::setup, "setup"},
   {ScaleFactorType::recovery, "recovery"},
   {ScaleFactorType::removal, "removal"},
   {ScaleFactorType::nochange, "nochange"},
   {ScaleFactorType::skew, "skew"},
   {ScaleFactorType::leakage_power, "leakage_power"},
   {ScaleFactorType::internal_power, "internal_power"},
   {ScaleFactorType::transition, "transition"},
   {ScaleFactorType::min_pulse_width, "min_pulse_width"},
   {ScaleFactorType::unknown, "unknown"}
  };

const char *
scaleFactorTypeName(ScaleFactorType type)
{
  return scale_factor_type_map.find(type);
}

ScaleFactorType
findScaleFactorType(const char *name)
{
  return scale_factor_type_map.find(name, ScaleFactorType::unknown);
}

bool
scaleFactorTypeRiseFallSuffix(ScaleFactorType type)
{
  return type == ScaleFactorType::cell
    || type == ScaleFactorType::hold
    || type == ScaleFactorType::setup
    || type == ScaleFactorType::recovery
    || type == ScaleFactorType::removal
    || type == ScaleFactorType::nochange
    || type == ScaleFactorType::skew;
}

bool
scaleFactorTypeRiseFallPrefix(ScaleFactorType type)
{
  return type == ScaleFactorType::transition;
}

bool
scaleFactorTypeLowHighSuffix(ScaleFactorType type)
{
  return type == ScaleFactorType::min_pulse_width;
}

////////////////////////////////////////////////////////////////

EnumNameMap<ScaleFactorPvt> scale_factor_pvt_names =
  {{ScaleFactorPvt::process, "process"},
   {ScaleFactorPvt::volt, "volt"},
   {ScaleFactorPvt::temp, "temp"}
  };

ScaleFactorPvt
findScaleFactorPvt(const char *name)
{
  return scale_factor_pvt_names.find(name, ScaleFactorPvt::unknown);
}

const char *
scaleFactorPvtName(ScaleFactorPvt pvt)
{
  return scale_factor_pvt_names.find(pvt);
}

////////////////////////////////////////////////////////////////

ScaleFactors::ScaleFactors(const char *name) :
  name_(stringCopy(name))
{
  for (int type = 0; type < scale_factor_type_count; type++) {
    for (int pvt = 0; pvt < int(ScaleFactorPvt::count); pvt++) {
      for (auto tr_index : RiseFall::rangeIndex()) {
	scales_[type][pvt][tr_index] = 0.0;
      }
    }
  }
}

ScaleFactors::~ScaleFactors()
{
  stringDelete(name_);
}

void
ScaleFactors::setScale(ScaleFactorType type,
		       ScaleFactorPvt pvt,
		       RiseFall *rf,
		       float scale)
{
  scales_[int(type)][int(pvt)][rf->index()] = scale;
}

void
ScaleFactors::setScale(ScaleFactorType type,
		       ScaleFactorPvt pvt,
		       float scale)
{
  scales_[int(type)][int(pvt)][0] = scale;
}

float
ScaleFactors::scale(ScaleFactorType type,
		    ScaleFactorPvt pvt,
		    RiseFall *rf)
{
  return scales_[int(type)][int(pvt)][rf->index()];
}

float
ScaleFactors::scale(ScaleFactorType type,
		    ScaleFactorPvt pvt,
		    int tr_index)
{
  return scales_[int(type)][int(pvt)][tr_index];
}

float
ScaleFactors::scale(ScaleFactorType type,
		    ScaleFactorPvt pvt)
{
  return scales_[int(type)][int(pvt)][0];
}

void
ScaleFactors::print()
{
  printf("%10s", " ");
  for (int pvt_index = 0; pvt_index < int(ScaleFactorPvt::count); pvt_index++) {
    ScaleFactorPvt pvt = (ScaleFactorPvt) pvt_index;
    printf("%10s", scaleFactorPvtName(pvt));
  }
  printf("\n");
  for (int type_index = 0; type_index < scale_factor_type_count; type_index++) {
    ScaleFactorType type = (ScaleFactorType) type_index;
    printf("%10s ", scaleFactorTypeName(type));
    for (int pvt_index = 0; pvt_index < int(ScaleFactorPvt::count); pvt_index++) {
      if (scaleFactorTypeRiseFallSuffix(type)
	  || scaleFactorTypeRiseFallPrefix(type)
	  || scaleFactorTypeLowHighSuffix(type)) {
	printf(" %.3f,%.3f",
	       scales_[type_index][pvt_index][RiseFall::riseIndex()],
	       scales_[type_index][pvt_index][RiseFall::fallIndex()]);
      }
      else {
	printf(" %.3f",
	       scales_[type_index][pvt_index][0]);
      }
    }
    printf("\n");
  }
}

TestCell::TestCell(LibertyPort *data_in,
		   LibertyPort *scan_in,
		   LibertyPort *scan_enable,
		   LibertyPort *scan_out,
		   LibertyPort *scan_out_inv) :
  data_in_(data_in),
  scan_in_(scan_in),
  scan_enable_(scan_enable),
  scan_out_(scan_out),
  scan_out_inv_(scan_out_inv)
{
}

TestCell::TestCell() :
  data_in_(nullptr),
  scan_in_(nullptr),
  scan_enable_(nullptr),
  scan_out_(nullptr),
  scan_out_inv_(nullptr)
{
}

void
TestCell::setDataIn(LibertyPort *port)
{
  data_in_ = port;
}

void
TestCell::setScanIn(LibertyPort *port)
{
  scan_in_ = port;
}

void
TestCell::setScanEnable(LibertyPort *port)
{
  scan_enable_ = port;
}

void
TestCell::setScanOut(LibertyPort *port)
{
  scan_out_ = port;
}

void
TestCell::setScanOutInv(LibertyPort *port)
{
  scan_out_inv_ = port;
}

////////////////////////////////////////////////////////////////

OcvDerate::OcvDerate(const char *name) :
  name_(name)
{
  for (auto el_index : EarlyLate::rangeIndex()) {
    for (auto tr_index : RiseFall::rangeIndex()) {
      derate_[tr_index][el_index][int(PathType::clk)] = nullptr;
      derate_[tr_index][el_index][int(PathType::data)] = nullptr;
    }
  }
}

OcvDerate::~OcvDerate()
{
  stringDelete(name_);
  // Derating table models can be shared in multiple places in derate_;
  // Collect them in a set to avoid duplicate deletes.
  Set<Table*> models;
  for (auto el_index : EarlyLate::rangeIndex()) {
    for (auto tr_index : RiseFall::rangeIndex()) {
      Table *derate;
      derate = derate_[tr_index][el_index][int(PathType::clk)];
      if (derate)
	models.insert(derate);
      derate = derate_[tr_index][el_index][int(PathType::data)];
      if (derate)
	models.insert(derate);
    }
  }
  Set<Table*>::Iterator model_iter(models);
  while (model_iter.hasNext()) {
    Table *model = model_iter.next();
    delete model;
  }
}

Table *
OcvDerate::derateTable(const RiseFall *rf,
		       const EarlyLate *early_late,
		       PathType path_type)
{
  return derate_[rf->index()][early_late->index()][int(path_type)];
}

void
OcvDerate::setDerateTable(const RiseFall *rf,
			  const EarlyLate *early_late,
			  const PathType path_type,
			  Table *derate)
{
  derate_[rf->index()][early_late->index()][int(path_type)] = derate;
}

////////////////////////////////////////////////////////////////

LibertyPgPort::LibertyPgPort(const char *name,
			     LibertyCell *cell) :
  name_(stringCopy(name)),
  pg_type_(unknown),
  voltage_name_(nullptr),
  cell_(cell)
{
}

LibertyPgPort::~LibertyPgPort()
{
  stringDelete(name_);
  stringDelete(voltage_name_);
}

void
LibertyPgPort::setPgType(PgType type)
{
  pg_type_ = type;
}

void
LibertyPgPort::setVoltageName(const char *voltage_name)
{
  voltage_name_ = stringCopy(voltage_name);
}

} // namespace
