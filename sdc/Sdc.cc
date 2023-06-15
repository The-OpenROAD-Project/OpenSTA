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

#include "Sdc.hh"

#include <algorithm>

#include "Stats.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "Report.hh"
#include "PatternMatch.hh"
#include "MinMax.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Transition.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "RiseFallMinMax.hh"
#include "Clock.hh"
#include "ClockLatency.hh"
#include "ClockInsertion.hh"
#include "CycleAccting.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "PortExtCap.hh"
#include "DisabledPorts.hh"
#include "InputDrive.hh"
#include "DataCheck.hh"
#include "ClockGatingCheck.hh"
#include "ClockGroups.hh"
#include "DeratingFactors.hh"
#include "HpinDrvrLoad.hh"
#include "search/Levelize.hh"
#include "Corner.hh"
#include "Graph.hh"

namespace sta {

bool
ClockPairLess::operator()(const ClockPair &pair1,
			  const ClockPair &pair2) const
{
  int first1 = pair1.first->index();
  int second1 = pair1.second->index();
  if (first1 > second1)
    std::swap(first1, second1);
  int first2 = pair2.first->index();
  int second2 = pair2.second->index();
  if (first2 > second2)
    std::swap(first2, second2);
  return (first1 < first2)
    || (first1 == first2
	&& second1 < second2);
}

////////////////////////////////////////////////////////////////

typedef Vector<ClockPair> ClockPairSeq;
typedef Set<Pvt*> PvtSet;

static ExceptionThruSeq *
clone(ExceptionThruSeq *thrus,
      Network *network);

////////////////////////////////////////////////////////////////

Sdc::Sdc(StaState *sta) :
  StaState(sta),
  derating_factors_(nullptr),
  clk_index_(0),
  clock_pin_map_(PinIdHash(network_)),
  clock_leaf_pin_map_(PinIdHash(network_)),
  clk_hpin_disables_(network_),
  propagated_clk_pins_(network_),
  clk_latencies_(network_),
  clk_insertions_(network_),
  clk_sense_map_(network_),
  clk_gating_check_(nullptr),
  cycle_acctings_(this),

  input_delay_pin_map_(PinIdLess(network_)),
  input_delay_ref_pin_map_(PinIdLess(network_)),
  input_delay_leaf_pin_map_(PinIdLess(network_)),
  input_delay_internal_pin_map_(PinIdLess(network_)),
  input_delay_index_(0),

  output_delay_pin_map_(PinIdLess(network_)),
  output_delay_ref_pin_map_(PinIdLess(network_)),
  output_delay_leaf_pin_map_(PinIdLess(network_)),

  disabled_pins_(network_),
  disabled_ports_(network_),
  disabled_wire_edges_(network_),
  disabled_clk_gating_checks_inst_(network_),
  disabled_clk_gating_checks_pin_(network_),
  have_thru_hpin_exceptions_(false),
  first_thru_edge_exceptions_(0, PinPairHash(network_), PinPairEqual()),
  path_delay_internal_startpoints_(network_),
  path_delay_internal_endpoints_(network_)
{
  sdc_ = this;
  initVariables();
  if (corners_)
    makeCornersAfter(corners_);
  setWireload(nullptr, MinMaxAll::all());
  setWireloadSelection(nullptr, MinMaxAll::all());
  setOperatingConditions(nullptr, MinMaxAll::all());
  makeDefaultArrivalClock();
}

void
Sdc::makeDefaultArrivalClock()
{
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(0.0);
  default_arrival_clk_ = new Clock("input port clock", clk_index_++, network_);
  default_arrival_clk_->initClk(0, false, 0.0, waveform, nullptr, network_);
}

Sdc::~Sdc()
{
  deleteConstraints();
}

// This does NOT call initVariables() because those variable values
// survive linking a new design.
void
Sdc::clear()
{
  removeLibertyAnnotations();
  deleteConstraints();
  propagated_clk_pins_.clear();
  clocks_.clear();
  clock_name_map_.clear();
  clock_pin_map_.clear();
  clock_leaf_pin_map_.clear();
  clk_latencies_.clear();
  edge_clk_latency_.clear();
  clk_insertions_.clear();

  pin_clk_uncertainty_map_.clear();
  inter_clk_uncertainties_.clear();

  clk_groups_name_map_.clear();
  clearClkGroupExclusions();

  clk_gating_check_map_.clear();
  inst_clk_gating_check_map_.clear();
  pin_clk_gating_check_map_.clear();
  data_checks_from_map_.clear();
  data_checks_to_map_.clear();

  input_delays_.clear();
  input_delay_pin_map_.clear();
  input_delay_index_ = 0;
  input_delay_ref_pin_map_.clear();
  input_delay_leaf_pin_map_.clear();
  input_delay_internal_pin_map_.clear();

  output_delays_.clear();
  output_delay_pin_map_.clear();
  output_delay_leaf_pin_map_.clear();

  port_slew_limit_map_.clear();
  cell_slew_limit_map_.clear();
  have_clk_slew_limits_ = false;

  cell_cap_limit_map_.clear();
  port_cap_limit_map_.clear();
  pin_cap_limit_map_.clear();

  port_fanout_limit_map_.clear();
  cell_fanout_limit_map_.clear();

  disabled_pins_.clear();
  disabled_ports_.clear();
  disabled_lib_ports_.clear();
  disabled_edges_.clear();
  disabled_cell_ports_.clear();
  disabled_inst_ports_.clear();

  disabled_clk_gating_checks_inst_.clear();
  disabled_clk_gating_checks_pin_.clear();

  input_drive_map_.clear();
  logic_value_map_.clear();
  case_value_map_.clear();

  pin_latch_borrow_limit_map_.clear();
  inst_latch_borrow_limit_map_.clear();
  clk_latch_borrow_limit_map_.clear();

  min_pulse_width_.clear();

  setWireload(nullptr, MinMaxAll::all());
  setWireloadSelection(nullptr, MinMaxAll::all());
  // Operating conditions are owned by Liberty libraries.
  setOperatingConditions(nullptr, MinMaxAll::all());
  clk_index_ = 0;
  makeDefaultArrivalClock();

  unsetTimingDerate();
}

void
Sdc::initVariables()
{
  analysis_type_ = AnalysisType::ocv;
  use_default_arrival_clock_ = false;
  crpr_enabled_ = true;
  crpr_mode_ = CrprMode::same_pin;
  propagate_gated_clock_enable_ = true;
  preset_clr_arcs_enabled_ = false;
  cond_default_arcs_enabled_ = true;
  bidirect_net_paths_enabled_ = false;
  bidirect_inst_paths_enabled_ = false;
  recovery_removal_checks_enabled_ = true;
  gated_clk_checks_enabled_ = true;
  clk_thru_tristate_enabled_ = false;
  dynamic_loop_breaking_ = false;
  propagate_all_clks_ = false;
  wireload_mode_ = WireloadMode::unknown;
  max_area_ = 0.0;
  path_delays_without_to_ = false;
  clk_hpin_disables_valid_ = false;
  have_clk_slew_limits_ = false;
}

void
Sdc::deleteConstraints()
{
  clocks_.deleteContents();
  delete default_arrival_clk_;
  clock_pin_map_.deleteContents();
  clock_leaf_pin_map_.deleteContents();
  clk_latencies_.deleteContents();
  clk_insertions_.deleteContents();

  clk_groups_name_map_.deleteContents();
  clearClkGroupExclusions();

  pin_clk_uncertainty_map_.deleteContents();
  inter_clk_uncertainties_.deleteContents();
  delete clk_gating_check_;
  clk_gating_check_ = nullptr;
  clk_gating_check_map_.deleteContents();
  inst_clk_gating_check_map_.deleteContents();
  pin_clk_gating_check_map_.deleteContents();
  input_drive_map_.deleteContents();
  disabled_cell_ports_.deleteContents();
  disabled_inst_ports_.deleteContents();
  pin_min_pulse_width_map_.deleteContentsClear();
  inst_min_pulse_width_map_.deleteContentsClear();
  clk_min_pulse_width_map_.deleteContentsClear();

  for (auto pin_data_check : data_checks_from_map_) {
    DataCheckSet *checks = pin_data_check.second;
    checks->deleteContents();
    delete checks;
  }
  for (auto pin_data_check : data_checks_to_map_) {
    DataCheckSet *checks = pin_data_check.second;
    delete checks;
  }

  input_delays_.deleteContents();
  input_delay_pin_map_.deleteContents();
  input_delay_leaf_pin_map_.deleteContents();
  input_delay_ref_pin_map_.deleteContents();
  input_delay_internal_pin_map_.deleteContents();

  output_delays_.deleteContents();
  output_delay_pin_map_.deleteContents();
  output_delay_ref_pin_map_.deleteContents();
  output_delay_leaf_pin_map_.deleteContents();

  clk_hpin_disables_.deleteContentsClear();
  clk_hpin_disables_valid_ = false;

  clearCycleAcctings();
  deleteExceptions();
  clearGroupPathMap();
  deleteDeratingFactors();

  removeNetLoadCaps();

  clk_sense_map_.clear();

  for (int mm_index : MinMax::rangeIndex())
    instance_pvt_maps_[mm_index].deleteContentsClear();
}

void
Sdc::removeNetLoadCaps()
{
  if (!net_wire_cap_maps_.empty()) {
    for (int corner_index = 0; corner_index < corners_->count(); corner_index++) {
      net_wire_cap_maps_[corner_index].clear();
      drvr_pin_wire_cap_maps_[corner_index].clear();
      port_ext_cap_maps_[corner_index].deleteContentsClear();
    }
  }
}

void
Sdc::removeLibertyAnnotations()
{
  for (auto cell_port : disabled_cell_ports_) {
    DisabledCellPorts *disable = cell_port.second;
    LibertyCell *cell = disable->cell();
    if (disable->all())
      cell->setIsDisabledConstraint(false);

    if (disable->from()) {
      for (LibertyPort *from : *disable->from())
        from->setIsDisabledConstraint(false);
    }

    if (disable->to()) {
      for (LibertyPort *to : *disable->to())
        to->setIsDisabledConstraint(false);
    }

    if (disable->timingArcSets()) {
      for (TimingArcSet *arc_set : *disable->timingArcSets()) 
	arc_set->setIsDisabledConstraint(false);
    }

    
    if (disable->fromTo()) {
      for (const LibertyPortPair &pair : *disable->fromTo()) {
        const LibertyPort *from = pair.first;
        const LibertyPort *to = pair.second;
        for (TimingArcSet *arc_set : cell->timingArcSets(from, to))
          arc_set->setIsDisabledConstraint(false);
      }
    }
  }

  for (LibertyPort *port : disabled_lib_ports_)
    port->setIsDisabledConstraint(false);
}

void
Sdc::deleteNetBefore(const Net *net)
{
  for (int corner_index = 0; corner_index < corners_->count(); corner_index++) {
    net_wire_cap_maps_[corner_index].erase(net);
    for (const Pin *pin : *network_->drivers(net))
      drvr_pin_wire_cap_maps_[corner_index].erase(pin);
  }
}

void
Sdc::makeCornersBefore()
{
  removeNetLoadCaps();
}

void
Sdc::makeCornersAfter(Corners *corners)
{
  corners_ = corners;
  port_ext_cap_maps_.resize(corners_->count(), PortExtCapMap(PortIdLess(network_)));
  net_wire_cap_maps_.resize(corners_->count(), NetWireCapMap(NetIdLess(network_)));
  drvr_pin_wire_cap_maps_.resize(corners_->count(), PinWireCapMap(PinIdLess(network_)));
}

////////////////////////////////////////////////////////////////

bool
Sdc::isConstrained(const Pin *pin) const
{
  Port *port = network_->isTopLevelPort(pin) ? network_->port(pin) : nullptr;
  return clock_pin_map_.hasKey(pin)
    || propagated_clk_pins_.hasKey(pin)
    || hasClockLatency(pin)
    || hasClockInsertion(pin)
    || pin_clk_uncertainty_map_.hasKey(pin)
    || pin_clk_gating_check_map_.hasKey(pin)
    || data_checks_from_map_.hasKey(pin)
    || data_checks_to_map_.hasKey(pin)
    || input_delay_pin_map_.hasKey(pin)
    || output_delay_pin_map_.hasKey(pin)
    || pin_cap_limit_map_.hasKey(pin)
    || disabled_pins_.hasKey(pin)
    || disabled_ports_.hasKey(port)
    || disabled_clk_gating_checks_pin_.hasKey(pin)
    || first_from_pin_exceptions_.hasKey(pin)
    || first_thru_pin_exceptions_.hasKey(pin)
    || first_to_pin_exceptions_.hasKey(pin)
    || input_drive_map_.hasKey(port)
    || logic_value_map_.hasKey(pin)
    || case_value_map_.hasKey(pin)
    || pin_latch_borrow_limit_map_.hasKey(pin)
    || pin_min_pulse_width_map_.hasKey(pin)
    || (port && (port_slew_limit_map_.hasKey(port)
                 || port_cap_limit_map_.hasKey(port)
                 || port_fanout_limit_map_.hasKey(port)
                 || hasPortExtCap(port)));
}

bool
Sdc::isConstrained(const Instance *inst) const
{
  return instance_pvt_maps_[MinMax::minIndex()].hasKey(inst)
    || instance_pvt_maps_[MinMax::maxIndex()].hasKey(inst)
    || inst_derating_factors_.hasKey(inst)
    || inst_clk_gating_check_map_.hasKey(inst)
    || disabled_inst_ports_.hasKey(inst)
    || first_from_inst_exceptions_.hasKey(inst)
    || first_thru_inst_exceptions_.hasKey(inst)
    || first_to_inst_exceptions_.hasKey(inst)
    || inst_latch_borrow_limit_map_.hasKey(inst)
    || inst_min_pulse_width_map_.hasKey(inst);
}

bool
Sdc::isConstrained(const Net *net) const
{
  return net_derating_factors_.hasKey(net)
    || hasNetWireCap(net)
    || net_res_map_.hasKey(net)
    || first_thru_net_exceptions_.hasKey(net);
}

////////////////////////////////////////////////////////////////

void
Sdc::setAnalysisType(AnalysisType analysis_type)
{
  analysis_type_ = analysis_type;
}

void
Sdc::setOperatingConditions(OperatingConditions *op_cond,
			    const MinMaxAll *min_max)
{
  for (auto mm_index : min_max->rangeIndex())
    operating_conditions_[mm_index] = op_cond;
}

void
Sdc::setOperatingConditions(OperatingConditions *op_cond,
			    const MinMax *min_max)
{
  int mm_index = min_max->index();
  operating_conditions_[mm_index] = op_cond;
}

OperatingConditions *
Sdc::operatingConditions(const MinMax *min_max)
{
  int mm_index = min_max->index();
  return operating_conditions_[mm_index];
}

const Pvt *
Sdc::pvt(Instance *inst,
	 const MinMax *min_max) const
{
  const InstancePvtMap &pvt_map = instance_pvt_maps_[min_max->index()];
  return pvt_map.findKey(inst);
}

void
Sdc::setPvt(const Instance *inst,
            const MinMaxAll *min_max,
	    const Pvt &pvt)
{
  for (auto mm_index : min_max->rangeIndex()) {
    InstancePvtMap &pvt_map = instance_pvt_maps_[mm_index];
    pvt_map[inst] = new Pvt(pvt);
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::setTimingDerate(TimingDerateType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  if (derating_factors_ == nullptr)
    derating_factors_ = new DeratingFactorsGlobal;
  derating_factors_->setFactor(type, clk_data, rf, early_late, derate);
}

void
Sdc::setTimingDerate(const Net *net,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  DeratingFactorsNet *factors = net_derating_factors_.findKey(net);
  if (factors == nullptr) { 
    factors = new DeratingFactorsNet;
    net_derating_factors_[net] = factors;
  }
  factors->setFactor(clk_data, rf, early_late, derate);
}

void
Sdc::setTimingDerate(const Instance *inst,
		     TimingDerateCellType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  DeratingFactorsCell *factors = inst_derating_factors_.findKey(inst);
  if (factors == nullptr) {
    factors = new DeratingFactorsCell;
    inst_derating_factors_[inst] = factors;
  }
  factors->setFactor(type, clk_data, rf, early_late, derate);
}

void
Sdc::setTimingDerate(const LibertyCell *cell,
		     TimingDerateCellType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  DeratingFactorsCell *factors = cell_derating_factors_.findKey(cell);
  if (factors == nullptr) {
    factors = new DeratingFactorsCell;
    cell_derating_factors_[cell] = factors;
  }
  factors->setFactor(type, clk_data, rf, early_late, derate);
}

float
Sdc::timingDerateInstance(const Pin *pin,
			  TimingDerateCellType type,
			  PathClkOrData clk_data,
			  const RiseFall *rf,
			  const EarlyLate *early_late) const
{
  const Instance *inst = network_->instance(pin);
  DeratingFactorsCell *factors = inst_derating_factors_.findKey(inst);
  if (factors) {
    float factor;
    bool exists;
    factors->factor(type, clk_data, rf, early_late, factor, exists);
    if (exists)
      return factor;
  }

  const LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    DeratingFactorsCell *factors = cell_derating_factors_.findKey(cell);
    float factor;
    bool exists;
    if (factors) {
      factors->factor(type, clk_data, rf, early_late, factor, exists);
      if (exists)
        return factor;
    }
  }

  if (derating_factors_) {
    float factor;
    bool exists;
    derating_factors_->factor(type, clk_data, rf, early_late, factor, exists);
    if (exists)
      return factor;
  }
  return 1.0;
}

float
Sdc::timingDerateNet(const Pin *pin,
		     PathClkOrData clk_data,
		     const RiseFall *rf,
		     const EarlyLate *early_late) const
{
  const Net *net = network_->net(pin);
  DeratingFactorsNet *factors = net_derating_factors_.findKey(net);
  if (factors) {
    float factor;
    bool exists;
    factors->factor(clk_data, rf, early_late, factor, exists);
    if (exists)
      return factor;
  }

  if (derating_factors_) {
    float factor;
    bool exists;
    derating_factors_->factor(TimingDerateType::net_delay, clk_data, rf,
			      early_late, factor, exists);
    if (exists)
      return factor;
  }
  return 1.0;
}

void
Sdc::unsetTimingDerate()
{
  deleteDeratingFactors();
}

void
Sdc::moveDeratingFactors(Sdc *from,
                         Sdc *to)
{
  if (from->derating_factors_) {
    to->derating_factors_ = from->derating_factors_;
    from->derating_factors_ = nullptr;
  }

  to->net_derating_factors_.deleteContents();
  to->net_derating_factors_ = from->net_derating_factors_;
  from->net_derating_factors_.clear();

  to->inst_derating_factors_.deleteContents();
  to->inst_derating_factors_ = from->inst_derating_factors_;
  from->inst_derating_factors_.clear();

  to->cell_derating_factors_.deleteContents();
  to->cell_derating_factors_ = from->cell_derating_factors_;
  from->cell_derating_factors_.clear();
}

void
Sdc::deleteDeratingFactors()
{
  net_derating_factors_.deleteContents();
  inst_derating_factors_.deleteContents();
  cell_derating_factors_.deleteContents();

  delete derating_factors_;
  derating_factors_ = nullptr;
}

////////////////////////////////////////////////////////////////

void
Sdc::setDriveCell(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Port *port,
		  const LibertyPort *from_port,
		  float *from_slews,
		  const LibertyPort *to_port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max)
{
  ensureInputDrive(port)->setDriveCell(library, cell, from_port, from_slews,
				       to_port, rf, min_max);
}

void
Sdc::setInputSlew(const Port *port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float slew)
{
  ensureInputDrive(port)->setSlew(rf, min_max, slew);
}

void
Sdc::setDriveResistance(const Port *port,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			float res)
{
  ensureInputDrive(port)->setDriveResistance(rf, min_max, res);
}

InputDrive *
Sdc::ensureInputDrive(const Port *port)
{
  InputDrive *drive = input_drive_map_.findKey(port);
  if (drive == nullptr) {
    drive = new InputDrive;
    input_drive_map_[port] = drive;
  }
  return drive;
}

////////////////////////////////////////////////////////////////

void
Sdc::setSlewLimit(Clock *clk,
		  const RiseFallBoth *rf,
		  const PathClkOrData clk_data,
		  const MinMax *min_max,
		  float slew)
{
  clk->setSlewLimit(rf, clk_data, min_max, slew);
  have_clk_slew_limits_ = true;
}

bool
Sdc::haveClkSlewLimits() const
{
  return have_clk_slew_limits_;
}

void
Sdc::slewLimit(Clock *clk,
               const RiseFall *rf,
	       const PathClkOrData clk_data,
	       const MinMax *min_max,
	       float &slew,
	       bool &exists)
{
  clk->slewLimit(rf, clk_data, min_max, slew, exists);
}

void
Sdc::slewLimit(Port *port,
	       const MinMax *min_max,
	       float &slew,
	       bool &exists)
{
  slew = INF;
  MinMaxFloatValues values;
  port_slew_limit_map_.findKey(port, values, exists);
  if (exists)
    values.value(min_max, slew, exists);
}

void
Sdc::setSlewLimit(Port *port,
		  const MinMax *min_max,
		  float slew)
{
  MinMaxFloatValues &values = port_slew_limit_map_[port];
  values.setValue(min_max, slew);
}

void
Sdc::slewLimit(Cell *cell,
	       const MinMax *min_max,
	       float &slew,
	       bool &exists)
{
  slew = INF;
  MinMaxFloatValues values;
  cell_slew_limit_map_.findKey(cell, values, exists);
  if (exists)
    values.value(min_max, slew, exists);
}

void
Sdc::setSlewLimit(Cell *cell,
		  const MinMax *min_max,
		  float slew)
{
  MinMaxFloatValues &values = cell_slew_limit_map_[cell];
  values.setValue(min_max, slew);
}

void
Sdc::capacitanceLimit(Cell *cell,
		      const MinMax *min_max,
		      float &cap,
		      bool &exists)
{
  cap = 0.0;
  exists = false;
  MinMaxFloatValues values;
  cell_cap_limit_map_.findKey(cell, values, exists);
  if (exists)
    values.value(min_max, cap, exists);
}

void
Sdc::setCapacitanceLimit(Cell *cell,
			 const MinMax *min_max,
			 float cap)
{
  MinMaxFloatValues &values = cell_cap_limit_map_[cell];
  values.setValue(min_max, cap);
}

void
Sdc::capacitanceLimit(Port *port,
		      const MinMax *min_max,
		      float &cap,
		      bool &exists)
{
  cap = 0.0;
  exists = false;
  MinMaxFloatValues values;
  port_cap_limit_map_.findKey(port, values, exists);
  if (exists)
    values.value(min_max, cap, exists);
}

void
Sdc::setCapacitanceLimit(Port *port,
			 const MinMax *min_max,
			 float cap)
{
  MinMaxFloatValues &values = port_cap_limit_map_[port];
  values.setValue(min_max, cap);
}

void
Sdc::capacitanceLimit(Pin *pin,
		      const MinMax *min_max,
		      float &cap,
		      bool &exists)
{
  cap = 0.0;
  exists = false;
  MinMaxFloatValues values;
  pin_cap_limit_map_.findKey(pin, values, exists);
  if (exists)
    values.value(min_max, cap, exists);
}

void
Sdc::setCapacitanceLimit(Pin *pin,
			 const MinMax *min_max,
			 float cap)
{
  MinMaxFloatValues &values = pin_cap_limit_map_[pin];
  values.setValue(min_max, cap);
}

void
Sdc::fanoutLimit(Cell *cell,
		 const MinMax *min_max,
		 float &fanout,
		 bool &exists)
{
  fanout = min_max->initValue();
  MinMaxFloatValues values;
  cell_fanout_limit_map_.findKey(cell, values, exists);
  if (exists)
    values.value(min_max, fanout, exists);
}

void
Sdc::setFanoutLimit(Cell *cell,
		    const MinMax *min_max,
		    float fanout)
{
  MinMaxFloatValues &values = cell_fanout_limit_map_[cell];
  values.setValue(min_max, fanout);
}

void
Sdc::fanoutLimit(Port *port,
		 const MinMax *min_max,
		 float &fanout,
		 bool &exists)
{
  fanout = 0.0;
  MinMaxFloatValues values;
  port_fanout_limit_map_.findKey(port, values, exists);
  if (exists)
    values.value(min_max, fanout, exists);
}

void
Sdc::setFanoutLimit(Port *port,
		    const MinMax *min_max,
		    float fanout)
{
  MinMaxFloatValues &values = port_fanout_limit_map_[port];
  values.setValue(min_max, fanout);
}

void
Sdc::setMaxArea(float area)
{
  max_area_ = area;
}

float
Sdc::maxArea() const
{
  return max_area_;
}

////////////////////////////////////////////////////////////////

Clock *
Sdc::makeClock(const char *name,
	       PinSet *pins,
	       bool add_to_pins,
	       float period,
	       FloatSeq *waveform,
	       const char *comment)
{
  Clock *clk = clock_name_map_.findKey(name);
  if (!add_to_pins)
    deletePinClocks(clk, pins);
  if (clk)
    // Named clock redefinition.
    deleteClkPinMappings(clk);
  else {
    // Fresh clock definition.
    clk = new Clock(name, clk_index_++, network_);
    clk->setIsPropagated(propagate_all_clks_);
    clocks_.push_back(clk);
    // Use the copied name in the map.
    clock_name_map_[clk->name()] = clk;
  }
  clk->initClk(pins, add_to_pins, period, waveform, comment, network_);
  makeClkPinMappings(clk);
  clearCycleAcctings();
  invalidateGeneratedClks();
  clkHpinDisablesInvalid();
  return clk;
}

Clock *
Sdc::makeGeneratedClock(const char *name,
			PinSet *pins,
			bool add_to_pins,
			Pin *src_pin,
			Clock *master_clk,
			int divide_by,
			int multiply_by,
			float duty_cycle,
			bool invert,
			bool combinational,
			IntSeq *edges,
			FloatSeq *edge_shifts,
			const char *comment)
{
  Clock *clk = clock_name_map_.findKey(name);
  if (!add_to_pins)
    deletePinClocks(clk, pins);
  if (clk)
    deleteClkPinMappings(clk);
  else {
    clk = new Clock(name, clk_index_++, network_);
    clocks_.push_back(clk);
    clock_name_map_[clk->name()] = clk;
  }
  clk->initGeneratedClk(pins, add_to_pins, src_pin, master_clk,
			divide_by, multiply_by, duty_cycle,
			invert, combinational,
			edges, edge_shifts, propagate_all_clks_,
			comment, network_);
  makeClkPinMappings(clk);
  clearCycleAcctings();
  invalidateGeneratedClks();
  clkHpinDisablesInvalid();
  return clk;
}

void
Sdc::invalidateGeneratedClks() const
{
  for (auto clk : clocks_) {
    if (clk->isGenerated())
      clk->waveformInvalid();
  }
}

// If the clock is not defined with the -add option, any pins that already
// have a clock attached to them are removed from the pin.  If the clock
// is not the clock being defined and has no pins it is removed.
void
Sdc::deletePinClocks(Clock *defining_clk,
		     PinSet *pins)
{
  // Find all the clocks defined on pins to avoid finding the clock's
  // vertex pins multiple times.
  ClockSet clks;
  if (pins) {
    for (const Pin *pin : *pins) {
      ClockSet *pin_clks = clock_pin_map_.findKey(pin);
      if (pin_clks) {
        for (Clock *clk : *pin_clks) 
          clks.insert(clk);
      }
    }
  }
  for (Clock *clk : clks) {
    deleteClkPinMappings(clk);
    for (const Pin *pin : *pins)
      clk->deletePin(pin);
    if (clk != defining_clk) {
      if (clk->pins().empty())
	removeClock(clk);
      else {
	clk->makeLeafPins(network_);
	// One of the remaining clock pins may use a vertex pin that
	// was deleted above.
	makeClkPinMappings(clk);
      }
    }
  }
}

void
Sdc::deleteClkPinMappings(Clock *clk)
{
  for (const Pin *pin : clk->pins()) {
    ClockSet *pin_clks = clock_pin_map_.findKey(pin);
    if (pin_clks) {
      pin_clks->erase(clk);
      if (pin_clks->empty()) {
	clock_pin_map_.erase(pin);
	delete pin_clks;
      }
    }
  }

  for (const Pin *pin : clk->leafPins()) {
    ClockSet *pin_clks = clock_leaf_pin_map_.findKey(pin);
    if (pin_clks) {
      pin_clks->erase(clk);
      if (pin_clks->empty()) {
	clock_leaf_pin_map_.erase(pin);
	delete pin_clks;
      }
    }
  }
}

void
Sdc::makeClkPinMappings(Clock *clk)
{
  for (const Pin *pin : clk->pins()) {
    ClockSet *pin_clks = clock_pin_map_.findKey(pin);
    if (pin_clks == nullptr) {
      pin_clks = new ClockSet;
      clock_pin_map_.insert(pin, pin_clks);
    }
    pin_clks->insert(clk);
  }

  for (const Pin *pin : clk->leafPins()) {
    ClockSet *pin_clks = clock_leaf_pin_map_.findKey(pin);
    if (pin_clks == nullptr) {
      pin_clks = new ClockSet;
      clock_leaf_pin_map_.insert(pin, pin_clks);
    }
    pin_clks->insert(clk);
  }
}

void
Sdc::removeClock(Clock *clk)
{
  deleteExceptionsReferencing(clk);
  deleteInputDelaysReferencing(clk);
  deleteOutputDelaysReferencing(clk);
  deleteClockLatenciesReferencing(clk);
  deleteClockInsertionsReferencing(clk);
  deleteInterClockUncertaintiesReferencing(clk);
  deleteLatchBorrowLimitsReferencing(clk);
  deleteMinPulseWidthReferencing(clk);
  deleteMasterClkRefs(clk);
  clockGroupsDeleteClkRefs(clk);
  clearCycleAcctings();

  deleteClkPinMappings(clk);
  clocks_.eraseObject(clk);
  clock_name_map_.erase(clk->name());
  delete clk;
}

// Delete references to clk as a master clock.
void
Sdc::deleteMasterClkRefs(Clock *clk)
{
  for (auto gclk : clocks_) {
    if (gclk->isGenerated()
	&& gclk->masterClk() == clk) {
      gclk->setMasterClk(nullptr);
    }
  }
}

void
Sdc::clockDeletePin(Clock *clk,
		    Pin *pin)
{
  ClockSet *pin_clks = clock_pin_map_.findKey(pin);
  pin_clks->erase(clk);
  if (pin_clks->empty())
    clock_pin_map_.erase(pin);
  clk->deletePin(pin);
  clk->makeLeafPins(network_);
  makeClkPinMappings(clk);
}

Clock *
Sdc::findClock(const char *name) const
{
  return clock_name_map_.findKey(name);
}

bool
Sdc::isClock(const Pin *pin) const
{
  ClockSet *clks = findClocks(pin);
  return clks && !clks->empty();
}

bool
Sdc::isLeafPinClock(const Pin *pin) const
{
  ClockSet *clks = findLeafPinClocks(pin);
  return clks && !clks->empty();
}

bool
Sdc::isLeafPinNonGeneratedClock(const Pin *pin) const
{
  ClockSet *clks = findLeafPinClocks(pin);
  if (clks) {
    for (Clock *clk : *clks) {
      if (!clk->isGenerated())
	return true;
    }
    return false;
  }
  else
    return false;
}

ClockSet *
Sdc::findLeafPinClocks(const Pin *pin) const
{
  return clock_leaf_pin_map_.findKey(pin);
}

ClockSet *
Sdc::findClocks(const Pin *pin) const
{
  return clock_pin_map_.findKey(pin);
}

ClockSeq
Sdc::findClocksMatching(PatternMatch *pattern) const
{
  ClockSeq matches;
  if (!pattern->hasWildcards()) {
    Clock *clk = findClock(pattern->pattern());
    if (clk)
      matches.push_back(clk);
  }
  else {
    for (auto clk : clocks_) {
      if (pattern->match(clk->name()))
	matches.push_back(clk);
    }
  }
  return matches;
}

void
Sdc::sortedClocks(ClockSeq &clks)
{
  for (auto clk : clocks_)
    clks.push_back(clk);
  sort(clks, ClkNameLess());
}

////////////////////////////////////////////////////////////////

class ClkHpinDisable
{
public:
  ClkHpinDisable(const Clock *clk,
		 const Pin *from_pin,
		 const Pin *to_pin);
  const Clock *clk() const { return clk_; }
  const Pin *fromPin() const { return from_pin_; }
  const Pin *toPin() const { return to_pin_; }

private:
  const Clock *clk_;
  const Pin *from_pin_;
  const Pin *to_pin_;
};

ClkHpinDisable::ClkHpinDisable(const Clock *clk,
			       const Pin *from_pin,
			       const Pin *to_pin) :
  clk_(clk),
  from_pin_(from_pin),
  to_pin_(to_pin)
{
}

ClkHpinDisableLess::ClkHpinDisableLess(const Network *network) :
  network_(network)
{
}

bool
ClkHpinDisableLess::operator()(const ClkHpinDisable *disable1,
			       const ClkHpinDisable *disable2) const
{
  int clk_index1 = disable1->clk()->index();
  int clk_index2 = disable2->clk()->index();
  const Pin *from_pin1 = disable1->fromPin();
  const Pin *from_pin2 = disable2->fromPin();
  const Pin *to_pin1 = disable1->toPin();
  const Pin *to_pin2 = disable2->toPin();
  return clk_index1 < clk_index2
    || (clk_index1 == clk_index2
        && (network_->id(to_pin1) < network_->id(to_pin2)
            || (from_pin1 == from_pin2
                && network_->id(from_pin1) < network_->id(from_pin2))));
}

class FindClkHpinDisables : public HpinDrvrLoadVisitor
{
public:
  FindClkHpinDisables(Clock *clk,
		      const Network *network,
		      Sdc *sdc);
  bool drvrLoadExists(const Pin *drvr,
		      const Pin *load);

protected:
  virtual void visit(HpinDrvrLoad *drvr_load);
  void makeClkHpinDisables(const Pin *clk_src,
                           const Pin *drvr,
                           const Pin *load);

  Clock *clk_;
  PinPairSet drvr_loads_;
  const Network *network_;
  Sdc *sdc_;
};

FindClkHpinDisables::FindClkHpinDisables(Clock *clk,
					 const Network *network,
					 Sdc *sdc) :
  HpinDrvrLoadVisitor(),
  clk_(clk),
  drvr_loads_(network),
  network_(network),
  sdc_(sdc)
{
}

void
FindClkHpinDisables::visit(HpinDrvrLoad *drvr_load)
{
  const Pin *drvr = drvr_load->drvr();
  const Pin *load = drvr_load->load();

  makeClkHpinDisables(drvr, drvr, load);

  PinSet *hpins_from_drvr = drvr_load->hpinsFromDrvr();
  for (const Pin *hpin : *hpins_from_drvr)
    makeClkHpinDisables(hpin, drvr, load);
  drvr_loads_.insert(PinPair(drvr, load));
}

void
FindClkHpinDisables::makeClkHpinDisables(const Pin *clk_src,
					 const Pin *drvr,
					 const Pin *load)
{
  ClockSet *clks = sdc_->findClocks(clk_src);
  if (clks) {
    for (Clock *clk : *clks) {
      if (clk != clk_)
        // Do not propagate clock from source pin if another
        // clock is defined on a hierarchical pin between the
        // driver and load.
        sdc_->makeClkHpinDisable(clk, drvr, load);
    }
  }
}

bool
FindClkHpinDisables::drvrLoadExists(const Pin *drvr,
				    const Pin *load)
{
  PinPair probe(drvr, load);
  return drvr_loads_.hasKey(probe);
}

void
Sdc::ensureClkHpinDisables()
{
  if (!clk_hpin_disables_valid_) {
    clk_hpin_disables_.deleteContentsClear();
    for (auto clk : clocks_) {
      for (const Pin *src : clk->pins()) {
	if (network_->isHierarchical(src)) {
	  FindClkHpinDisables visitor1(clk, network_, this);
	  visitHpinDrvrLoads(src, network_, &visitor1);
	  PinSeq loads, drvrs;
	  PinSet visited_drvrs(network_);
	  FindNetDrvrLoads visitor2(nullptr, visited_drvrs, loads, drvrs, network_);
	  network_->visitConnectedPins(src, visitor2);

	  // Disable fanouts from the src driver pins that do
	  // not go thru the hierarchical src pin.
	  for (const Pin *drvr : drvrs) {
	    for (const Pin *load : loads) {
	      if (!visitor1.drvrLoadExists(drvr, load))
		makeClkHpinDisable(clk, drvr, load);
	    }
	  }
	}
      }
    }
    clk_hpin_disables_valid_ = true;
  }
}

void
Sdc::makeClkHpinDisable(const Clock *clk,
			const Pin *drvr,
			const Pin *load)
{
  ClkHpinDisable probe(clk, drvr, load);
  if (!clk_hpin_disables_.hasKey(&probe)) {
    ClkHpinDisable *disable = new ClkHpinDisable(clk, drvr, load);
    clk_hpin_disables_.insert(disable);
  }
}

void
Sdc::clkHpinDisablesInvalid()
{
  clk_hpin_disables_valid_ = false;
  for (auto clk : clocks_)
    clk->makeLeafPins(network_);
}

// Check that driver/load edge goes thru clock hpin.
// Check for disable by hierarchical clock pin between driver and load.
bool
Sdc::clkDisabledByHpinThru(const Clock *clk,
			   const Pin *from_pin,
			   const Pin *to_pin)
{
  if (clk->leafPins().hasKey(from_pin)) {
    ClkHpinDisable probe(clk, from_pin, to_pin);
    return clk_hpin_disables_.hasKey(&probe);
  }
  else
    return false;
}

////////////////////////////////////////////////////////////////

void
Sdc::setPropagatedClock(Clock *clk)
{
  clk->setIsPropagated(true);
  removeClockLatency(clk, nullptr);
}

void
Sdc::removePropagatedClock(Clock *clk)
{
  clk->setIsPropagated(false);
}

void
Sdc::setPropagatedClock(Pin *pin)
{
  propagated_clk_pins_.insert(pin);
  removeClockLatency(nullptr, pin);
}

void
Sdc::removePropagatedClock(Pin *pin)
{
  propagated_clk_pins_.erase(pin);
}

bool
Sdc::isPropagatedClock(const Pin *pin)
{
  return propagated_clk_pins_.hasKey(pin);
}

void
Sdc::setClockSlew(Clock *clk,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float slew)
{
  clk->setSlew(rf, min_max, slew);
}

void
Sdc::removeClockSlew(Clock *clk)
{
  clk->removeSlew();
}

void
Sdc::setClockLatency(Clock *clk,
		     Pin *pin,
		     const RiseFallBoth *rf,
		     const MinMaxAll *min_max,
		     float delay)
{
  ClockLatency probe(clk, pin);
  ClockLatency *latency = clk_latencies_.findKey(&probe);
  if (latency == nullptr) {
    latency = new ClockLatency(clk, pin);
    clk_latencies_.insert(latency);
  }
  latency->setDelay(rf, min_max, delay);

  // set_clock_latency removes set_propagated_clock on the same object.
  if (clk && pin == nullptr)
    removePropagatedClock(clk);
  if (pin)
    removePropagatedClock(pin);
}

void
Sdc::removeClockLatency(const Clock *clk,
			const Pin *pin)
{
  ClockLatency probe(clk, pin);
  ClockLatency *latency = clk_latencies_.findKey(&probe);
  if (latency)
    deleteClockLatency(latency);
}

void
Sdc::deleteClockLatency(ClockLatency *latency)
{
  clk_latencies_.erase(latency);
  delete latency;
}

void
Sdc::deleteClockLatenciesReferencing(Clock *clk)
{
  for (auto iter = clk_latencies_.cbegin();
       iter != clk_latencies_.cend(); ) {
    ClockLatency *latency = *iter;
    if (latency->clock() == clk) {
      iter = clk_latencies_.erase(iter);
      delete latency;
    }
    else
      iter++;
  }
}

bool
Sdc::hasClockLatency(const Pin *pin) const
{
  ClockLatency probe(nullptr, pin);
  return clk_latencies_.hasKey(&probe);
}

void
Sdc::clockLatency(const Clock *clk,
		  const Pin *pin,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  // Return values.
		  float &latency,
		  bool &exists) const
{
  latency = 0.0;
  exists = false;
  if (pin && clk) {
    ClockLatency probe(clk, pin);
    ClockLatency *latencies = clk_latencies_.findKey(&probe);
    if (latencies)
      latencies->delay(rf, min_max, latency, exists);
  }
  if (!exists) {
    ClockLatency probe(nullptr, pin);
    ClockLatency *latencies = clk_latencies_.findKey(&probe);
    if (latencies)
      latencies->delay(rf, min_max, latency, exists);
  }
}

void
Sdc::clockLatency(const Clock *clk,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  // Return values.
		  float &latency,
		  bool &exists) const
{
  latency = 0.0;
  exists = false;
  ClockLatency probe(clk, nullptr);
  ClockLatency *latencies = clk_latencies_.findKey(&probe);
  if (latencies)
    latencies->delay(rf, min_max, latency, exists);
}

float
Sdc::clockLatency(const Clock *clk,
		  const RiseFall *rf,
		  const MinMax *min_max) const
{
  float latency;
  bool exists;
  clockLatency(clk, rf, min_max,
	       latency, exists);
  return latency;
}

void
Sdc::setClockUncertainty(Pin *pin,
			 const SetupHoldAll *setup_hold,
			 float uncertainty)
{
  ClockUncertainties *uncertainties = pin_clk_uncertainty_map_.findKey(pin);
  if (uncertainties == nullptr) {
    uncertainties = new ClockUncertainties;
    pin_clk_uncertainty_map_[pin] = uncertainties;
  }
  uncertainties->setValue(setup_hold, uncertainty);
}

void
Sdc::removeClockUncertainty(Pin *pin,
			    const SetupHoldAll *setup_hold)
{
  ClockUncertainties *uncertainties = pin_clk_uncertainty_map_.findKey(pin);
  if (uncertainties) {
    uncertainties->removeValue(setup_hold);
    if (uncertainties->empty()) {
      delete uncertainties;
      pin_clk_uncertainty_map_.erase(pin);
    }
  }
}

ClockUncertainties *
Sdc::clockUncertainties(const Pin *pin)
{
  return pin_clk_uncertainty_map_.findKey(pin);
}

void
Sdc::clockUncertainty(const Pin *pin,
		      const SetupHold *setup_hold,
		      float &uncertainty,
		      bool &exists)
{
  ClockUncertainties *uncertainties = clockUncertainties(pin);
  if (uncertainties)
    uncertainties->value(setup_hold, uncertainty, exists);
  else {
    uncertainty = 0.0;
    exists = false;
  }
}

void
Sdc::clockUncertainty(const Clock *src_clk,
		      const RiseFall *src_rf,
		      const Clock *tgt_clk,
		      const RiseFall *tgt_rf,
		      const SetupHold *setup_hold,
		      float &uncertainty,
		      bool &exists)
{
  InterClockUncertainty probe(src_clk, tgt_clk);
  InterClockUncertainty *uncertainties =
    inter_clk_uncertainties_.findKey(&probe);
  if (uncertainties)
    uncertainties->uncertainty(src_rf, tgt_rf, setup_hold,
			       uncertainty, exists);
  else {
    uncertainty = 0.0;
    exists = false;
  }
}

void
Sdc::setClockUncertainty(Clock *from_clk,
			 const RiseFallBoth *from_rf,
			 Clock *to_clk,
			 const RiseFallBoth *to_rf,
			 const SetupHoldAll *setup_hold,
			 float uncertainty)
{
  InterClockUncertainty probe(from_clk, to_clk);
  InterClockUncertainty *uncertainties =
    inter_clk_uncertainties_.findKey(&probe);
  if (uncertainties == nullptr) {
    uncertainties = new InterClockUncertainty(from_clk, to_clk);
    inter_clk_uncertainties_.insert(uncertainties);
  }
  uncertainties->setUncertainty(from_rf, to_rf, setup_hold, uncertainty);
}

void
Sdc::removeClockUncertainty(Clock *from_clk,
			    const RiseFallBoth *from_rf,
			    Clock *to_clk,
			    const RiseFallBoth *to_rf,
			    const SetupHoldAll *setup_hold)
{
  InterClockUncertainty probe(from_clk, to_clk);
  InterClockUncertainty *uncertainties =
    inter_clk_uncertainties_.findKey(&probe);
  if (uncertainties) {
    uncertainties->removeUncertainty(from_rf, to_rf, setup_hold);
    if (uncertainties->empty()) {
      inter_clk_uncertainties_.erase(uncertainties);
      delete uncertainties;
    }
  }
}

void
Sdc::deleteInterClockUncertainty(InterClockUncertainty *uncertainties)
{
  inter_clk_uncertainties_.erase(uncertainties);
  delete uncertainties;
}

void
Sdc::deleteInterClockUncertaintiesReferencing(Clock *clk)
{
  for (auto iter = inter_clk_uncertainties_.cbegin();
       iter != inter_clk_uncertainties_.cend(); ) {
    InterClockUncertainty *uncertainties = *iter;
    if (uncertainties->src() == clk
	|| uncertainties->target() == clk) {
      iter = inter_clk_uncertainties_.erase(iter);
      delete uncertainties;
    }
    else
      iter++;
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::setClockInsertion(const Clock *clk,
		       const Pin *pin,
		       const RiseFallBoth *rf,
		       const MinMaxAll *min_max,
		       const EarlyLateAll *early_late,
		       float delay)
{
  ClockInsertion probe(clk, pin);
  ClockInsertion *insertion = clk_insertions_.findKey(&probe);
  if (insertion == nullptr) {
    insertion = new ClockInsertion(clk, pin);
    clk_insertions_.insert(insertion);
  }
  insertion->setDelay(rf, min_max, early_late, delay);
}

void
Sdc::setClockInsertion(const Clock *clk,
		       const Pin *pin,
		       const RiseFall *rf,
		       const MinMax *min_max,
		       const EarlyLate *early_late,
		       float delay)
{
  ClockInsertion probe(clk, pin);
  ClockInsertion *insertion = clk_insertions_.findKey(&probe);
  if (insertion == nullptr) {
    insertion = new ClockInsertion(clk, pin);
    clk_insertions_.insert(insertion);
  }
  insertion->setDelay(rf, min_max, early_late, delay);
}

void
Sdc::removeClockInsertion(const Clock *clk,
			  const Pin *pin)
{
  ClockInsertion probe(clk, pin);
  ClockInsertion *insertion = clk_insertions_.findKey(&probe);
  if (insertion != nullptr)
    deleteClockInsertion(insertion);
}

void
Sdc::deleteClockInsertion(ClockInsertion *insertion)
{
  clk_insertions_.erase(insertion);
  delete insertion;
}

void
Sdc::deleteClockInsertionsReferencing(Clock *clk)
{
  for (auto iter = clk_insertions_.cbegin();
       iter != clk_insertions_.cend(); ) {
    ClockInsertion *insertion = *iter;
    if (insertion->clock() == clk) {
      iter = clk_insertions_.erase(iter);
      delete insertion;
    }
    else
      iter++;
  }
}

float
Sdc::clockInsertion(const Clock *clk,
		    const RiseFall *rf,
		    const MinMax *min_max,
		    const EarlyLate *early_late) const
{
  float insertion;
  bool exists;
  clockInsertion(clk, nullptr, rf, min_max, early_late, insertion, exists);
  return insertion;
}

bool
Sdc::hasClockInsertion(const Pin *pin) const
{
  ClockInsertion probe(nullptr, pin);
  return clk_insertions_.hasKey(&probe);
}

void
Sdc::clockInsertion(const Clock *clk,
		    const Pin *pin,
		    const RiseFall *rf,
		    const MinMax *min_max,
		    const EarlyLate *early_late,
		    // Return values.
		    float &insertion,
		    bool &exists) const
{
  ClockInsertion *insert = nullptr;
  if (clk && pin) {
    ClockInsertion probe(clk, pin);
    insert = clk_insertions_.findKey(&probe);
  }
  if (insert == nullptr && pin) {
    ClockInsertion probe(nullptr, pin);
    insert = clk_insertions_.findKey(&probe);
  }
  if (insert == nullptr && clk) {
    ClockInsertion probe(clk, nullptr);
    insert = clk_insertions_.findKey(&probe);
  }

  if (insert)
    insert->delay(rf, min_max, early_late, insertion, exists);
  else {
    insertion = 0.0;
    exists = false;
  }
}

////////////////////////////////////////////////////////////////

ClockLatencyLess::ClockLatencyLess(const Network *network) :
  network_(network)
{
}

bool
ClockLatencyLess::operator()(const ClockLatency *latency1,
                             const ClockLatency *latency2) const
{
  const Clock *clk1 = latency1->clock();
  const Clock *clk2 = latency2->clock();
  const Pin *pin1 = latency1->pin();
  const Pin *pin2 = latency2->pin();
  return (clk1 == nullptr && clk2)
    || ((clk1 && clk2 && clk1->index() < clk2->index())
        || (clk1 == clk2
            && ((pin1 == nullptr && pin2)
                || (pin1 && pin2 && network_->id(pin1) < network_->id(pin2)))));
}

////////////////////////////////////////////////////////////////

ClockInsertionkLess::ClockInsertionkLess(const Network *network) :
  network_(network)
{
}

bool
ClockInsertionkLess::operator()(const ClockInsertion *insert1,
                                const ClockInsertion *insert2) const
{
  const Clock *clk1 = insert1->clock();
  const Clock *clk2 = insert2->clock();
  const Pin *pin1 = insert1->pin();
  const Pin *pin2 = insert2->pin();
  return (clk1 == nullptr && clk2)
    || ((clk1 && clk2 && clk1->index() < clk2->index())
        || (clk1 == clk2
            && ((pin1 == nullptr && pin2)
                || (pin1 && pin2 && network_->id(pin1) < network_->id(pin2)))));
}

////////////////////////////////////////////////////////////////

ClockGroups *
Sdc::makeClockGroups(const char *name,
		     bool logically_exclusive,
		     bool physically_exclusive,
		     bool asynchronous,
		     bool allow_paths,
		     const char *comment)
{
  char *gen_name = nullptr;
  if (name == nullptr
      || name[0] == '\0')
    name = gen_name = makeClockGroupsName();
  else {
    ClockGroups *groups = clk_groups_name_map_.findKey(name);
    if (groups)
      removeClockGroups(groups);
  }
  ClockGroups *groups = new ClockGroups(name, logically_exclusive,
					physically_exclusive,
					asynchronous, allow_paths, comment);
  clk_groups_name_map_[groups->name()] = groups;
  stringDelete(gen_name);
  return groups;
}

// Generate a name for the clock group.
char *
Sdc::makeClockGroupsName()
{
  char *name = nullptr;
  int i = 0;
  do {
    i++;
    stringDelete(name);
    name = stringPrint("group%d", i);
  } while (clk_groups_name_map_.hasKey(name));
  return name;
}

void
Sdc::makeClockGroup(ClockGroups *clk_groups,
		    ClockSet *clks)
{
  clk_groups->makeClockGroup(clks);
}

void
Sdc::ensureClkGroupExclusions()
{
  for (auto name_clk_groups : clk_groups_name_map_)
    makeClkGroupExclusions(name_clk_groups.second);
}
   
void
Sdc::makeClkGroupExclusions(ClockGroups *clk_groups)
{
  if (!(clk_groups->asynchronous()
	&& clk_groups->allowPaths())) {
    ClockGroupSet *groups = clk_groups->groups();
    if (groups->size() == 1)
      makeClkGroupExclusions1(groups);
    else
      makeClkGroupExclusions(groups);
  }
}

// If there is only one group all clocks not in the group
// are excluded.
void
Sdc::makeClkGroupExclusions1(ClockGroupSet *groups)
{
  ClockGroupSet::Iterator group_iter1(groups);
  ClockGroup *group1 = group_iter1.next();
  for (auto clk1 : *group1) {
    for (Clock *clk2 : clocks_) {
      if (clk2 != clk1
	  && !group1->hasKey(clk2))
	clk_group_exclusions_.insert(ClockPair(clk1, clk2));
    }
  }
  makeClkGroupSame(group1);
}

void
Sdc::makeClkGroupExclusions(ClockGroupSet *groups)
{
  for (auto group1 : *groups) {
    for (auto group2 : *groups) {
      if (group1 != group2) {
	for (auto clk1 : *group1) {
	  for (auto clk2 : *group2) {
	    // ClockPair is symmetric so only add one clk1/clk2 pair.
	    if (clk1->index() < clk2->index()) {
	      clk_group_exclusions_.insert(ClockPair(clk1, clk2));
	    }
	  }
	}
      }
    }
    makeClkGroupSame(group1);
  }
}

void
Sdc::makeClkGroupSame(ClockGroup *group)
{
  for (auto clk1 : *group) {
    for (auto clk2 : *group) {
      if (clk1->index() <= clk2->index()) {
	ClockPair clk_pair(clk1, clk2);
	if (!clk_group_same_.hasKey(clk_pair))
	  clk_group_same_.insert(clk_pair);
      }
    }
  }
}

void
Sdc::clearClkGroupExclusions()
{
  clk_group_exclusions_.clear();
  clk_group_same_.clear();
}

bool
Sdc::sameClockGroup(const Clock *clk1,
		    const Clock *clk2)
{
  if (clk1 && clk2) {
    ClockPair clk_pair(clk1, clk2);
    bool excluded = clk_group_exclusions_.hasKey(clk_pair);
    return !excluded;
  }
  else
    return true;
}

bool
Sdc::sameClockGroupExplicit(const Clock *clk1,
			    const Clock *clk2)
{
  ClockPair clk_pair(clk1, clk2);
  return clk_group_same_.hasKey(clk_pair);
}

void
Sdc::removeClockGroups(const char *name)
{
  ClockGroups *clk_groups = clk_groups_name_map_.findKey(name);
  if (clk_groups)
    removeClockGroups(clk_groups);
}

void
Sdc::removeClockGroupsLogicallyExclusive(const char *name)
{
  if (name) {
    ClockGroups *groups = clk_groups_name_map_.findKey(name);
    if (groups && groups->logicallyExclusive())
      removeClockGroups(groups);
  }
  else {
    for (auto name_group : clk_groups_name_map_) {
      ClockGroups *groups = name_group.second;
      if (groups->logicallyExclusive())
	removeClockGroups(groups);
    }
  }
}

void
Sdc::removeClockGroupsPhysicallyExclusive(const char *name)
{
  if (name) {
    ClockGroups *groups = clk_groups_name_map_.findKey(name);
    if (groups && groups->physicallyExclusive())
      removeClockGroups(groups);
  }
  else {
    for (auto name_group : clk_groups_name_map_) {
      ClockGroups *groups = name_group.second;
      if (groups->physicallyExclusive())
	removeClockGroups(groups);
    }
  }
}

void
Sdc::removeClockGroupsAsynchronous(const char *name)
{
  if (name) {
    ClockGroups *groups = clk_groups_name_map_.findKey(name);
    if (groups && groups->asynchronous())
      removeClockGroups(groups);
  }
  else {
    for (auto name_group : clk_groups_name_map_) {
      ClockGroups *groups = name_group.second;
      if (groups->asynchronous())
	removeClockGroups(groups);
    }
  }
}

void
Sdc::removeClockGroups(ClockGroups *groups)
{
  clk_groups_name_map_.erase(groups->name());
  delete groups;
  // Can't delete excluded clock pairs for deleted clock groups because
  // some other clock groups may exclude the same clock pair.
  clearClkGroupExclusions();
}

void
Sdc::clockGroupsDeleteClkRefs(Clock *clk)
{
  for (auto name_group : clk_groups_name_map_) {
    ClockGroups *groups = name_group.second;
    groups->removeClock(clk);
  }
  clearClkGroupExclusions();
}

////////////////////////////////////////////////////////////////

void
Sdc::setClockSense(PinSet *pins,
		   ClockSet *clks,
		   ClockSense sense)
{
  if (clks && clks->empty()) {
    delete clks;
    clks = nullptr;
  }
  for (const Pin *pin : *pins) {
    if (clks) {
      for (const Clock *clk : *clks)
	setClockSense(pin, clk, sense);
    }
    else
      setClockSense(pin, nullptr, sense);
  }
  delete pins;
  delete clks;
}

void
Sdc::setClockSense(const Pin *pin,
                   const Clock *clk,
                   ClockSense sense)
{
  PinClockPair probe(pin, clk);
  if (clk_sense_map_.hasKey(probe))
    clk_sense_map_[probe] = sense;
  else {
    PinClockPair pin_clk(pin, clk);
    clk_sense_map_[pin_clk] = sense;
  }
}

bool
Sdc::clkStopPropagation(const Pin *pin,
			const Clock *clk) const
{
  PinClockPair pin_clk(pin, clk);
  ClockSense sense;
  bool exists;
  clk_sense_map_.findKey(pin_clk, sense, exists);
  if (!exists) {
    PinClockPair pin_clk1(pin, nullptr);
    clk_sense_map_.findKey(pin_clk1, sense, exists);
  }
  return exists
    && sense == ClockSense::stop;
}

bool
Sdc::clkStopSense(const Pin *to_pin,
		  const Clock *clk,
		  const RiseFall *from_rf,
		  const RiseFall *to_rf) const
{
  PinClockPair pin_clk(to_pin, clk);
  ClockSense sense;
  bool exists;
  clk_sense_map_.findKey(pin_clk, sense, exists);
  if (!exists) {
    PinClockPair pin(to_pin, nullptr);
    clk_sense_map_.findKey(pin, sense, exists);
  }
  return exists
    && (sense == ClockSense::stop
	|| (sense == ClockSense::positive
	    && from_rf != to_rf)
	|| (sense == ClockSense::negative
	    && from_rf == to_rf));
}

bool
Sdc::clkStopPropagation(const Clock *clk,
			const Pin *from_pin,
			const RiseFall *from_rf,
			const Pin *to_pin,
			const RiseFall *to_rf) const
{
  return clkStopPropagation(from_pin, clk)
    || clkStopSense(to_pin, clk, from_rf, to_rf);
}

PinClockPairLess::PinClockPairLess(const Network *network) :
  network_(network)
{
}

bool
PinClockPairLess::operator()(const PinClockPair &pin_clk1,
			     const PinClockPair &pin_clk2) const
{
  const Pin *pin1 = pin_clk1.first;
  const Pin *pin2 = pin_clk2.first;
  const Clock *clk1 = pin_clk1.second;
  const Clock *clk2 = pin_clk2.second;
  return pin1 < pin2
    || (pin1 == pin2
	&& ((clk1 == nullptr && clk2)
	    || (clk1 && clk2
		&& clk1->index() < clk2->index())));
}

////////////////////////////////////////////////////////////////

void
Sdc::setClockGatingCheck(const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin)
{
  if (clk_gating_check_ == nullptr)
    clk_gating_check_ = new ClockGatingCheck;
  clk_gating_check_->margins()->setValue(rf, setup_hold, margin);
}

void
Sdc::setClockGatingCheck(Clock *clk,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin)
{
  ClockGatingCheck *check = clk_gating_check_map_.findKey(clk);
  if (check == nullptr) {
    check = new ClockGatingCheck();
    clk_gating_check_map_[clk] = check;
  }
  check->margins()->setValue(rf, setup_hold, margin);
}

void
Sdc::setClockGatingCheck(Instance *inst,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
			 LogicValue active_value)
{
  ClockGatingCheck *check = inst_clk_gating_check_map_.findKey(inst);
  if (check == nullptr) {
    check = new ClockGatingCheck();
    inst_clk_gating_check_map_[inst] = check;
  }
  check->margins()->setValue(rf, setup_hold, margin);
  check->setActiveValue(active_value);
}

void
Sdc::setClockGatingCheck(const Pin *pin,
			 const RiseFallBoth *rf,
			 const SetupHold *setup_hold,
			 float margin,
			 LogicValue active_value)
{
  ClockGatingCheck *check = pin_clk_gating_check_map_.findKey(pin);
  if (check == nullptr) {
    check = new ClockGatingCheck();
    pin_clk_gating_check_map_[pin] = check;
  }
  check->margins()->setValue(rf, setup_hold, margin);
  check->setActiveValue(active_value);
}

void
Sdc::clockGatingMarginEnablePin(const Pin *enable_pin,
				const RiseFall *enable_rf,
				const SetupHold *setup_hold,
				bool &exists, float &margin)
{
  ClockGatingCheck *check = pin_clk_gating_check_map_.findKey(enable_pin);
  if (check)
    check->margins()->value(enable_rf, setup_hold, margin, exists);
  else
    exists = false;
}

void
Sdc::clockGatingMarginInstance(Instance *inst,
			       const RiseFall *enable_rf,
			       const SetupHold *setup_hold,
			       bool &exists,
			       float &margin)
{
  ClockGatingCheck *check = inst_clk_gating_check_map_.findKey(inst);
  if (check)
    check->margins()->value(enable_rf, setup_hold, margin, exists);
  else
    exists = false;
}

void
Sdc::clockGatingMarginClkPin(const Pin *clk_pin,
			     const RiseFall *enable_rf,
			     const SetupHold *setup_hold,
			     bool &exists,
			     float &margin)
{
  ClockGatingCheck *check = pin_clk_gating_check_map_.findKey(clk_pin);
  if (check)
    check->margins()->value(enable_rf, setup_hold, margin, exists);
  else
    exists = false;
}

void
Sdc::clockGatingMarginClk(const Clock *clk,
			  const RiseFall *enable_rf,
			  const SetupHold *setup_hold,
			  bool &exists,
			  float &margin)
{
  ClockGatingCheck *check = clk_gating_check_map_.findKey(clk);
  if (check)
    check->margins()->value(enable_rf, setup_hold, margin, exists);
  else
    exists = false;
}

void
Sdc::clockGatingMargin(const RiseFall *enable_rf,
		       const SetupHold *setup_hold,
		       bool &exists,
		       float &margin)
{
  if (clk_gating_check_)
    clk_gating_check_->margins()->value(enable_rf, setup_hold, margin, exists);
  else
    exists = false;
}

LogicValue
Sdc::clockGatingActiveValue(const Pin *clk_pin,
			    const Pin *enable_pin)
{
  ClockGatingCheck *check;
  check = pin_clk_gating_check_map_.findKey(enable_pin);
  if (check)
    return check->activeValue();
  Instance *inst = network_->instance(enable_pin);
  check = inst_clk_gating_check_map_.findKey(inst);
  if (check)
    return check->activeValue();
  check = pin_clk_gating_check_map_.findKey(clk_pin);
  if (check)
    return check->activeValue();
  return LogicValue::unknown;
}

////////////////////////////////////////////////////////////////

// Determine cycle accounting "on demand".
CycleAccting *
Sdc::cycleAccting(const ClockEdge *src,
		  const ClockEdge *tgt)
{
  UniqueLock lock(cycle_acctings_lock_);
  return cycle_acctings_.cycleAccting(src, tgt);
}

void
Sdc::reportClkToClkMaxCycleWarnings()
{
  cycle_acctings_.reportClkToClkMaxCycleWarnings(report_);
}

void
Sdc::clearCycleAcctings()
{
  cycle_acctings_.clear();
}

////////////////////////////////////////////////////////////////

void
Sdc::setDataCheck(Pin *from,
		  const RiseFallBoth *from_rf,
		  Pin *to,
		  const RiseFallBoth *to_rf,
		  Clock *clk,
		  const SetupHoldAll *setup_hold,
		  float margin)
{
  DataCheck *check = nullptr;
  DataCheckSet *checks = data_checks_from_map_.findKey(from);
  if (checks == nullptr) {
    checks = new DataCheckSet(DataCheckLess(network_));
    data_checks_from_map_[from] = checks;
  }
  else {
    DataCheck probe(from, to, clk);
    check = checks->findKey(&probe);
  }
  if (check == nullptr)
    check = new DataCheck(from, to, clk);
  check->setMargin(from_rf, to_rf, setup_hold, margin);
  checks->insert(check);

  checks = data_checks_to_map_.findKey(to);
  if (checks == nullptr) {
    checks = new DataCheckSet(DataCheckLess(network_));
    data_checks_to_map_[to] = checks;
  }
  checks->insert(check);
}

void
Sdc::removeDataCheck(Pin *from,
		     const RiseFallBoth *from_rf,
		     Pin *to,
		     const RiseFallBoth *to_rf,
		     Clock *clk,
		     const SetupHoldAll *setup_hold)
{
  DataCheck probe(from, to, clk);
  DataCheckSet *checks = data_checks_from_map_.findKey(from);
  if (checks) {
    DataCheck *check = checks->findKey(&probe);
    if (check) {
      check->removeMargin(from_rf, to_rf, setup_hold);
      if (check->empty()) {
	checks->erase(check);
	checks = data_checks_to_map_.findKey(to);
	if (checks)
	  checks->erase(check);
	delete check;
      }
    }
  }
}

DataCheckSet *
Sdc::dataChecksFrom(const Pin *from) const
{
  return data_checks_from_map_.findKey(from);
}

DataCheckSet *
Sdc::dataChecksTo(const Pin *to) const
{
  return data_checks_to_map_.findKey(to);
}

////////////////////////////////////////////////////////////////

void
Sdc::setLatchBorrowLimit(const Pin *pin,
			 float limit)
{
  pin_latch_borrow_limit_map_[pin] = limit;
}

void
Sdc::setLatchBorrowLimit(const Instance *inst,
			 float limit)
{
  inst_latch_borrow_limit_map_[inst] = limit;
}

void
Sdc::setLatchBorrowLimit(const Clock *clk,
                         float limit)
{
  clk_latch_borrow_limit_map_[clk] = limit;
}

void
Sdc::deleteLatchBorrowLimitsReferencing(Clock *clk)
{
  clk_latch_borrow_limit_map_.erase(clk);
}

void
Sdc::latchBorrowLimit(const Pin *data_pin,
		      const Pin *enable_pin,
		      const Clock *clk,
		      // Return values.
		      float &limit,
		      bool &exists)
{
  pin_latch_borrow_limit_map_.findKey(data_pin, limit, exists);
  if (!exists) {
    pin_latch_borrow_limit_map_.findKey(enable_pin, limit, exists);
    if (!exists) {
      Instance *inst = network_->instance(data_pin);
      inst_latch_borrow_limit_map_.findKey(inst, limit, exists);
      if (!exists)
	clk_latch_borrow_limit_map_.findKey(clk, limit, exists);
    }
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::setMinPulseWidth(const RiseFallBoth *rf,
		      float min_width)
{
  for (auto rf1 : rf->range())
    min_pulse_width_.setValue(rf1, min_width);
}

void
Sdc::setMinPulseWidth(const Pin *pin,
		      const RiseFallBoth *rf,
		      float min_width)
{
  RiseFallValues *widths = pin_min_pulse_width_map_.findKey(pin);
  if (widths == nullptr) {
    widths = new RiseFallValues;
    pin_min_pulse_width_map_[pin] = widths;
  }
  for (auto rf1 : rf->range())
    widths->setValue(rf1, min_width);
}

void
Sdc::setMinPulseWidth(const Instance *inst,
		      const RiseFallBoth *rf,
		      float min_width)
{
  RiseFallValues *widths = inst_min_pulse_width_map_.findKey(inst);
  if (widths == nullptr) {
    widths = new RiseFallValues;
    inst_min_pulse_width_map_[inst] = widths;
  }
  for (auto rf1 : rf->range())
    widths->setValue(rf1, min_width);
}

void
Sdc::setMinPulseWidth(const Clock *clk,
		      const RiseFallBoth *rf,
		      float min_width)
{
  RiseFallValues *widths = clk_min_pulse_width_map_.findKey(clk);
  if (widths == nullptr) {
    widths = new RiseFallValues;
    clk_min_pulse_width_map_[clk] = widths;
  }
  for (auto rf1 : rf->range())
    widths->setValue(rf1, min_width);
}

void
Sdc::minPulseWidth(const Pin *pin,
		   const Clock *clk,
		   const RiseFall *hi_low,
		   float &min_width,
		   bool &exists) const
{
  RiseFallValues *widths = pin_min_pulse_width_map_.findKey(pin);
  if (widths)
    widths->value(hi_low, min_width, exists);
  else {
    if (pin) {
      const Instance *inst = network_->instance(pin);
      widths = inst_min_pulse_width_map_.findKey(inst);
    }
    if (widths == nullptr)
      widths = clk_min_pulse_width_map_.findKey(clk);
    if (widths)
      widths->value(hi_low, min_width, exists);
    else
      min_pulse_width_.value(hi_low, min_width, exists);
  }
}

void
Sdc::deleteMinPulseWidthReferencing(Clock *clk)
{
  RiseFallValues *widths = clk_min_pulse_width_map_.findKey(clk);
  if (widths) {
    delete widths;
    clk_min_pulse_width_map_.erase(clk);
  }
}

////////////////////////////////////////////////////////////////

InputDrive *
Sdc::findInputDrive(Port *port)
{
  return input_drive_map_.findKey(port);
}

void
Sdc::setInputDelay(const Pin *pin,
		   const RiseFallBoth *rf,
		   const Clock *clk,
		   const RiseFall *clk_rf,
		   const Pin *ref_pin,
		   bool source_latency_included,
		   bool network_latency_included,
		   const MinMaxAll *min_max,
		   bool add,
		   float delay)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  InputDelay *input_delay = findInputDelay(pin, clk_edge);
  if (input_delay == nullptr)
    input_delay = makeInputDelay(pin, clk_edge);
  if (add) {
    RiseFallMinMax *delays = input_delay->delays();
    delays->mergeValue(rf, min_max, delay);
  }
  else {
    deleteInputDelays(pin, input_delay);
    RiseFallMinMax *delays = input_delay->delays();
    delays->setValue(rf, min_max, delay);
  }

  if (ref_pin) {
    InputDelaySet *ref_inputs = input_delay_ref_pin_map_.findKey(ref_pin);
    if (ref_inputs == nullptr) {
      ref_inputs = new InputDelaySet;
      input_delay_ref_pin_map_[ref_pin] = ref_inputs;
    }
    ref_inputs->insert(input_delay);
  }
  input_delay->setRefPin(ref_pin);

  input_delay->setSourceLatencyIncluded(source_latency_included);
  input_delay->setNetworkLatencyIncluded(network_latency_included);
}

InputDelay *
Sdc::makeInputDelay(const Pin *pin,
		    const ClockEdge *clk_edge)
{
  InputDelay *input_delay = new InputDelay(pin, clk_edge, input_delay_index_++,
					   network_);
  input_delays_.insert(input_delay);
  InputDelaySet *inputs = input_delay_pin_map_.findKey(pin);
  if (inputs == nullptr) {
    inputs = new InputDelaySet;
    input_delay_pin_map_[pin] = inputs;
  }
  inputs->insert(input_delay);

  for (const Pin *lpin : input_delay->leafPins()) {
    InputDelaySet *leaf_inputs = input_delay_leaf_pin_map_[lpin];
    if (leaf_inputs == nullptr) {
      leaf_inputs = new InputDelaySet;
      input_delay_leaf_pin_map_[lpin] = leaf_inputs;
    }
    leaf_inputs->insert(input_delay);

    if (!network_->isTopLevelPort(lpin)) {
      InputDelaySet *internal_inputs = input_delay_internal_pin_map_[lpin];
      if (internal_inputs == nullptr) {
	internal_inputs = new InputDelaySet;
	input_delay_internal_pin_map_[pin] = internal_inputs;
      }
      internal_inputs->insert(input_delay);
    }
  }
  return input_delay;
}

InputDelay *
Sdc::findInputDelay(const Pin *pin,
		    const ClockEdge *clk_edge)
{
  InputDelaySet *inputs = input_delay_pin_map_.findKey(pin);
  if (inputs) {
    for (InputDelay *input_delay : *inputs) {
      if (input_delay->clkEdge() == clk_edge)
	return input_delay;
    }
  }
  return nullptr;
}

void
Sdc::removeInputDelay(const Pin *pin,
		      const RiseFallBoth *rf,
		      const Clock *clk,
		      const RiseFall *clk_rf,
		      const MinMaxAll *min_max)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  InputDelay *input_delay = findInputDelay(pin, clk_edge);
  if (input_delay) {
    RiseFallMinMax *delays = input_delay->delays();
    delays->removeValue(rf, min_max);
    if (delays->empty())
      deleteInputDelay(input_delay);
  }
}

void
Sdc::deleteInputDelays(const Pin *pin,
		       InputDelay *except)
{
  InputDelaySet *input_delays = input_delay_pin_map_[pin];
  InputDelaySet::Iterator iter(input_delays);
  while (iter.hasNext()) {
    InputDelay *input_delay = iter.next();
    if (input_delay != except)
      deleteInputDelay(input_delay);
  }
}

InputDelaySet *
Sdc::refPinInputDelays(const Pin *ref_pin) const
{
  return input_delay_ref_pin_map_.findKey(ref_pin);
}

InputDelaySet *
Sdc::inputDelaysLeafPin(const Pin *leaf_pin)
{
  return input_delay_leaf_pin_map_.findKey(leaf_pin);
}

bool
Sdc::hasInputDelay(const Pin *leaf_pin) const
{
  InputDelaySet *input_delays = input_delay_leaf_pin_map_.findKey(leaf_pin);
  return input_delays && !input_delays->empty();
}

bool
Sdc::isInputDelayInternal(const Pin *pin) const
{
  return input_delay_internal_pin_map_.hasKey(pin);
}

void
Sdc::deleteInputDelaysReferencing(const Clock *clk)
{
  InputDelaySet::Iterator iter(input_delays_);
  while (iter.hasNext()) {
    InputDelay *input_delay = iter.next();
    if (input_delay->clock() == clk)
      deleteInputDelay(input_delay);
  }
}

void
Sdc::deleteInputDelay(InputDelay *input_delay)
{
  input_delays_.erase(input_delay);

  const Pin *pin = input_delay->pin();
  InputDelaySet *inputs = input_delay_pin_map_[pin];
  inputs->erase(input_delay);

  for (const Pin *lpin : input_delay->leafPins()) {
    InputDelaySet *inputs = input_delay_leaf_pin_map_[lpin];
    inputs->erase(input_delay);
  }

  delete input_delay;
}

void
Sdc::movePortDelays(Sdc *from,
                    Sdc *to)
{
  to->input_delays_.deleteContents();
  to->input_delays_ = from->input_delays_;
  from->input_delays_.clear();

  to->input_delay_pin_map_.deleteContents();
  to->input_delay_pin_map_ = from->input_delay_pin_map_;
  from->input_delay_pin_map_.clear();

  to->input_delay_ref_pin_map_.deleteContents();
  to->input_delay_ref_pin_map_ = from->input_delay_ref_pin_map_;
  from->input_delay_ref_pin_map_.clear();

  to->input_delay_leaf_pin_map_.deleteContents();
  to->input_delay_leaf_pin_map_ = from->input_delay_leaf_pin_map_;
  from->input_delay_leaf_pin_map_.clear();

  to->input_delay_internal_pin_map_.deleteContents();
  to->input_delay_internal_pin_map_ = from->input_delay_internal_pin_map_;
  from->input_delay_internal_pin_map_.clear();

  to->input_delay_index_ = from->input_delay_index_;
  from->input_delay_index_ = 0;

  ////////////////

  to->output_delays_.deleteContents();
  to->output_delays_ = from->output_delays_;
  from->output_delays_.clear();

  to->output_delay_pin_map_.deleteContents();
  to->output_delay_pin_map_ = from->output_delay_pin_map_;
  from->output_delay_pin_map_.clear();

  to->output_delay_ref_pin_map_.deleteContents();
  to->output_delay_ref_pin_map_ = from->output_delay_ref_pin_map_;
  from->output_delay_ref_pin_map_.clear();

  to->output_delay_leaf_pin_map_.deleteContents();
  to->output_delay_leaf_pin_map_ = from->output_delay_leaf_pin_map_;
  from->output_delay_leaf_pin_map_.clear();
}

////////////////////////////////////////////////////////////////

void
Sdc::setOutputDelay(const Pin *pin,
		    const RiseFallBoth *rf,
		    const Clock *clk,
		    const RiseFall *clk_rf,
		    const Pin *ref_pin,
		    bool source_latency_included,
		    bool network_latency_included,
		    const MinMaxAll *min_max,
		    bool add,
		    float delay)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  OutputDelay *output_delay = findOutputDelay(pin, clk_edge);
  if (output_delay == nullptr)
    output_delay = makeOutputDelay(pin, clk_edge);
  if (add) {
    RiseFallMinMax *delays = output_delay->delays();
    delays->mergeValue(rf, min_max, delay);
  }
  else {
    deleteOutputDelays(pin, output_delay);
    RiseFallMinMax *delays = output_delay->delays();
    delays->setValue(rf, min_max, delay);
  }

  if (ref_pin) {
    OutputDelaySet *ref_outputs = output_delay_ref_pin_map_.findKey(ref_pin);
    if (ref_outputs == nullptr) {
      ref_outputs = new OutputDelaySet;
      output_delay_ref_pin_map_[ref_pin] = ref_outputs;
    }
    ref_outputs->insert(output_delay);
  }
  output_delay->setRefPin(ref_pin);

  output_delay->setSourceLatencyIncluded(source_latency_included);
  output_delay->setNetworkLatencyIncluded(network_latency_included);
}

OutputDelay *
Sdc::findOutputDelay(const Pin *pin,
		     const ClockEdge *clk_edge)
{
  OutputDelaySet *outputs = output_delay_pin_map_.findKey(pin);
  if (outputs) {
    for (OutputDelay *output_delay : *outputs) {
      if (output_delay->clkEdge() == clk_edge)
	return output_delay;
    }
  }
  return nullptr;
}

OutputDelay *
Sdc::makeOutputDelay(const Pin *pin,
		     const ClockEdge *clk_edge)
{
  OutputDelay *output_delay = new OutputDelay(pin, clk_edge, network_);
  output_delays_.insert(output_delay);
  OutputDelaySet *outputs = output_delay_pin_map_.findKey(pin);
  if (outputs == nullptr) {
    outputs = new OutputDelaySet;
    output_delay_pin_map_[pin] = outputs;
  }
  outputs->insert(output_delay);

  for (const Pin *lpin : output_delay->leafPins()) {
    OutputDelaySet *leaf_outputs = output_delay_leaf_pin_map_[lpin];
    if (leaf_outputs == nullptr) {
      leaf_outputs = new OutputDelaySet;
      output_delay_leaf_pin_map_[lpin] = leaf_outputs;
    }
    leaf_outputs->insert(output_delay);
  }
  return output_delay;
}

void
Sdc::removeOutputDelay(const Pin *pin,
		       const RiseFallBoth *rf,
		       const Clock *clk,
		       const RiseFall *clk_rf,
		       const MinMaxAll *min_max)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  OutputDelay *output_delay = findOutputDelay(pin, clk_edge);
  if (output_delay) {
    RiseFallMinMax *delays = output_delay->delays();
    delays->removeValue(rf, min_max);
  }
}

void
Sdc::deleteOutputDelays(const Pin *pin,
			OutputDelay *except)
{
  OutputDelaySet *output_delays = output_delay_pin_map_[pin];
  OutputDelaySet::Iterator iter(output_delays);
  while (iter.hasNext()) {
    OutputDelay *output_delay = iter.next();
    if (output_delay != except)
      deleteOutputDelay(output_delay);
  }
}

OutputDelaySet *
Sdc::outputDelaysLeafPin(const Pin *leaf_pin)
{
  return output_delay_leaf_pin_map_.findKey(leaf_pin);
}

bool
Sdc::hasOutputDelay(const Pin *leaf_pin) const
{
  return output_delay_leaf_pin_map_.hasKey(leaf_pin);
}

void
Sdc::deleteOutputDelaysReferencing(const Clock *clk)
{
  OutputDelaySet::Iterator iter(output_delays_);
  while (iter.hasNext()) {
    OutputDelay *output_delay = iter.next();
    if (output_delay->clock() == clk)
      deleteOutputDelay(output_delay);
  }
}

void
Sdc::deleteOutputDelay(OutputDelay *output_delay)
{
  output_delays_.erase(output_delay);

  const Pin *pin = output_delay->pin();
  OutputDelaySet *outputs = output_delay_pin_map_[pin];
  outputs->erase(output_delay);

  for (const Pin *lpin : output_delay->leafPins()) {
    OutputDelaySet *outputs = output_delay_leaf_pin_map_[lpin];
    outputs->erase(output_delay);
  }

  delete output_delay;
}

////////////////////////////////////////////////////////////////

void
Sdc::setPortExtPinCap(const Port *port,
		      const RiseFall *rf,
                      const Corner *corner,
		      const MinMax *min_max,
		      float cap)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port, corner);
  port_cap->setPinCap(cap, rf, min_max);
}

void
Sdc::setPortExtWireCap(const Port *port,
		       bool subtract_pin_cap,
		       const RiseFall *rf,
		       const Corner *corner,
		       const MinMax *min_max,
		       float cap)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port, corner);
  if (subtract_pin_cap) {
    Pin *pin = network_->findPin(network_->name(port));
    const OperatingConditions *op_cond = operatingConditions(min_max);
    cap -= connectedPinCap(pin, rf, op_cond, corner, min_max);
    if (cap < 0.0)
      cap = 0.0;
  }
  port_cap->setWireCap(cap, rf, min_max);
}

PortExtCap *
Sdc::portExtCap(const Port *port,
                const Corner *corner) const
{
  return port_ext_cap_maps_[corner->index()].findKey(port);
}

bool
Sdc::hasPortExtCap(const Port *port) const
{
  for (int corner_index = 0; corner_index < corners_->count(); corner_index++) {
    if (port_ext_cap_maps_[corner_index].hasKey(port))
      return true;
  }
  return false;
}

void
Sdc::portExtCap(const Port *port,
		const RiseFall *rf,
                const Corner *corner,
		const MinMax *min_max,
		// Return values.
		float &pin_cap,
		bool &has_pin_cap,
		float &wire_cap,
		bool &has_wire_cap,
		int &fanout,
		bool &has_fanout) const
{
  PortExtCap *port_cap = port_ext_cap_maps_[corner->index()].findKey(port);
  if (port_cap) {
    port_cap->pinCap(rf, min_max, pin_cap, has_pin_cap);
    port_cap->wireCap(rf, min_max, wire_cap, has_wire_cap);
    port_cap->fanout(min_max, fanout, has_fanout);
  }
  else {
    pin_cap = 0.0F;
    has_pin_cap = false;
    wire_cap = 0.0F;
    has_wire_cap = false;
    fanout = 0.0F;
    has_fanout = false;
  }
}

float
Sdc::portExtCap(const Port *port,
		const RiseFall *rf,
                const Corner *corner,
		const MinMax *min_max) const
{
  float pin_cap, wire_cap;
  int fanout;
  bool has_pin_cap, has_wire_cap, has_fanout;
  portExtCap(port, rf, corner, min_max,
	     pin_cap, has_pin_cap,
	     wire_cap, has_wire_cap,
	     fanout, has_fanout);
  float cap = 0.0;
  if (has_pin_cap)
    cap += pin_cap;
  if (has_wire_cap)
    cap += wire_cap;
  return cap;
}

bool
Sdc::drvrPinHasWireCap(const Pin *pin,
                       const Corner *corner)
{
  return drvr_pin_wire_cap_maps_[corner->index()].hasKey(pin);
}

void
Sdc::drvrPinWireCap(const Pin *pin,
		    const Corner *corner,
		    const MinMax *min_max,
		    // Return values.
		    float &cap,
		    bool &exists) const
{
  MinMaxFloatValues *values = drvr_pin_wire_cap_maps_[corner->index()].findKey(pin);
  if (values)
    return values->value(min_max, cap, exists);
  cap = 0.0;
  exists = false;
}

void
Sdc::setNetWireCap(const Net *net,
		   bool subtract_pin_cap,
		   const Corner *corner,
		   const MinMax *min_max,
		   float cap)
{
  float wire_cap = cap;
  if (subtract_pin_cap) {
    OperatingConditions *op_cond = operatingConditions(min_max);
    NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
    if (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      float pin_cap_rise = connectedPinCap(pin, RiseFall::rise(),
					   op_cond, corner, min_max);
      float pin_cap_fall = connectedPinCap(pin, RiseFall::fall(),
					   op_cond, corner, min_max);
      float pin_cap = (pin_cap_rise + pin_cap_fall) / 2.0F;
      wire_cap -= pin_cap;
      if ((wire_cap + pin_cap) < 0.0)
	wire_cap = -pin_cap;
      delete pin_iter;
    }
  }
  bool make_drvr_entry = !net_wire_cap_maps_[corner->index()].hasKey(net);
  MinMaxFloatValues &values = net_wire_cap_maps_[corner->index()][net];
  values.setValue(min_max, wire_cap);

  // Only need to do this when there is new net_wire_cap_maps_ entry.
  if (make_drvr_entry) {
    for (const Pin *pin : *network_->drivers(net)) {
      drvr_pin_wire_cap_maps_[corner->index()][pin] = &values;
    }
  }
}

bool
Sdc::hasNetWireCap(const Net *net) const
{
  for (int i = 0; i < corners_->count(); i++) {
    if (net_wire_cap_maps_[i].hasKey(net))
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////

void
Sdc::connectedCap(const Pin *pin,
		  const RiseFall *rf,
		  const OperatingConditions *op_cond,
		  const Corner *corner,
		  const MinMax *min_max,
		  // Return values.
		  float &pin_cap,
		  float &wire_cap,
		  float &fanout,
                  bool &has_net_load) const
{
  netCaps(pin, rf, op_cond, corner, min_max,
	  pin_cap, wire_cap, fanout, has_net_load);
  float net_wire_cap;
  drvrPinWireCap(pin, corner, min_max, net_wire_cap, has_net_load);
  if (has_net_load) {
    wire_cap += net_wire_cap;
  }
}

float
Sdc::connectedPinCap(const Pin *pin,
		     const RiseFall *rf,
		     const OperatingConditions *op_cond,
		     const Corner *corner,
		     const MinMax *min_max)
{
  float pin_cap, wire_cap, fanout;
  bool has_net_load;
  connectedCap(pin, rf, op_cond, corner, min_max,
	       pin_cap, wire_cap, fanout, has_net_load);
  return pin_cap;
}

class FindNetCaps : public PinVisitor
{
public:
  FindNetCaps(const RiseFall *rf,
	      const OperatingConditions *op_cond,
	      const Corner *corner,
	      const MinMax *min_max,
	      float &pin_cap,
	      float &wire_cap,
	      float &fanout,
	      bool &has_net_load,
	      const Sdc *sdc);
  virtual void operator()(const Pin *pin);

protected:
  const RiseFall *rf_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *min_max_;
  float &pin_cap_;
  float &wire_cap_;
  float &fanout_;
  bool &has_net_load_;
  const Sdc *sdc_;
};

FindNetCaps::FindNetCaps(const RiseFall *rf,
			 const OperatingConditions *op_cond,
			 const Corner *corner,
			 const MinMax *min_max,
			 float &pin_cap,
			 float &wire_cap,
			 float &fanout,
			 bool &has_net_load,
			 const Sdc *sdc) :
  PinVisitor(),
  rf_(rf),
  op_cond_(op_cond),
  corner_(corner),
  min_max_(min_max),
  pin_cap_(pin_cap),
  wire_cap_(wire_cap),
  fanout_(fanout),
  has_net_load_(has_net_load),
  sdc_(sdc)
{
}

void
FindNetCaps::operator()(const Pin *pin)
{
  sdc_->pinCaps(pin, rf_, op_cond_, corner_, min_max_,
		pin_cap_, wire_cap_, fanout_);
}

// Capacitances for all pins connected to drvr_pin's net.
void
Sdc::netCaps(const Pin *drvr_pin,
	     const RiseFall *rf,
	     const OperatingConditions *op_cond,
	     const Corner *corner,
	     const MinMax *min_max,
	     // Return values.
	     float &pin_cap,
	     float &wire_cap,
	     float &fanout,
             bool &has_net_load) const
{
  pin_cap = 0.0;
  wire_cap = 0.0;
  fanout = 0.0;
  has_net_load = false;
  FindNetCaps visitor(rf, op_cond, corner, min_max, pin_cap,
		      wire_cap, fanout, has_net_load, this);
  network_->visitConnectedPins(drvr_pin, visitor);
}

void
Sdc::pinCaps(const Pin *pin,
	     const RiseFall *rf,
	     const OperatingConditions *op_cond,
	     const Corner *corner,
	     const MinMax *min_max,
	     // Return values.
	     float &pin_cap,
	     float &wire_cap,
	     float &fanout) const
{
  if (network_->isTopLevelPort(pin)) {
    Port *port = network_->port(pin);
    bool is_output = network_->direction(port)->isAnyOutput();
    float port_pin_cap, port_wire_cap;
    int port_fanout;
    bool has_pin_cap, has_wire_cap, has_fanout;
    portExtCap(port, rf, corner, min_max,
	       port_pin_cap, has_pin_cap,
	       port_wire_cap, has_wire_cap,
	       port_fanout, has_fanout);
    if (has_pin_cap)
      pin_cap += port_pin_cap;
    if (has_wire_cap)
      wire_cap += port_wire_cap;
    if (is_output) {
      if (has_fanout)
	fanout += port_fanout;
      // Output port counts as a fanout.
      fanout++;
    }
  }
  else {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      Instance *inst = network_->instance(pin);
      pin_cap += portCapacitance(inst, port, rf, op_cond, corner, min_max);
      if (port->direction()->isAnyInput())
	fanout++;
    }
  }
}

float
Sdc::portCapacitance(Instance *inst,
		     LibertyPort *port,
		     const RiseFall *rf,
		     const OperatingConditions *op_cond,
		     const Corner *corner,
		     const MinMax *min_max) const
{
  const Pvt *inst_pvt = nullptr;
  if (inst)
    inst_pvt = pvt(inst, min_max);
  LibertyPort *corner_port = port->cornerPort(corner, min_max);
  return corner_port->capacitance(rf, min_max, op_cond, inst_pvt);
}

float
Sdc::pinCapacitance(const Pin *pin,
		    const RiseFall *rf,
		    const OperatingConditions *op_cond,
		    const Corner *corner,
		    const MinMax *min_max)
{
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    Instance *inst = network_->instance(pin);
    return portCapacitance(inst, port, rf, op_cond, corner, min_max);
  }
  else
    return 0.0;
}

////////////////////////////////////////////////////////////////

void
Sdc::setResistance(const Net *net,
		   const MinMaxAll *min_max,
		   float res)
{
  MinMaxFloatValues &values = net_res_map_[net];
  values.setValue(min_max, res);
}

void
Sdc::resistance(const Net *net,
		const MinMax *min_max,
		float &res,
		bool &exists)
{
  res = 0.0;
  MinMaxFloatValues values;
  net_res_map_.findKey(net, values, exists);
  if (exists)
    values.value(min_max, res, exists);
}

void
Sdc::setPortExtFanout(const Port *port,
                      const Corner *corner,
		      const MinMax *min_max,
		      int fanout)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port, corner);
  port_cap->setFanout(fanout, min_max);
}

void
Sdc::portExtFanout(const Port *port,
                   const Corner *corner,
		   const MinMax *min_max,
		   // Return values.
		   int &fanout,
		   bool &exists)
{
  PortExtCap *port_cap = portExtCap(port, corner);
  if (port_cap)
    port_cap->fanout(min_max, fanout, exists);
  else {
    fanout = 0.0;
    exists = false;
  }
}

int
Sdc::portExtFanout(Port *port,
                   const Corner *corner,
		   const MinMax *min_max)
{
  int fanout;
  bool exists;
  portExtFanout(port, corner, min_max, fanout, exists);
  if (exists)
    return fanout;
  else
    return 0.0;
}

PortExtCap *
Sdc::ensurePortExtPinCap(const Port *port,
                         const Corner *corner)
{
  PortExtCap *port_cap = port_ext_cap_maps_[corner->index()].findKey(port);
  if (port_cap == nullptr) {
    port_cap = new PortExtCap(port);
    port_ext_cap_maps_[corner->index()][port] = port_cap;
  }
  return port_cap;
}

void
Sdc::movePortExtCaps(Sdc *from,
                     Sdc *to)
{
  for (int corner_index = 0; corner_index < from->corners()->count(); corner_index++) {
    to->port_ext_cap_maps_[corner_index] = from->port_ext_cap_maps_[corner_index];
    from->port_ext_cap_maps_[corner_index].clear();

    to->net_wire_cap_maps_[corner_index] = from->net_wire_cap_maps_[corner_index];
    from->net_wire_cap_maps_[corner_index].clear();
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::disable(LibertyCell *cell,
	     LibertyPort *from,
	     LibertyPort *to)
{
  DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
  if (disabled_cell == nullptr) {
    disabled_cell = new DisabledCellPorts(cell);
    disabled_cell_ports_[cell] = disabled_cell;
  }
  if (from && to) {
    disabled_cell->setDisabledFromTo(from, to);
    for (TimingArcSet *arc_set : cell->timingArcSets(from, to))
      arc_set->setIsDisabledConstraint(true);
  }
  else if (from) {
    disabled_cell->setDisabledFrom(from);
    from->setIsDisabledConstraint(true);
  }
  else if (to) {
    disabled_cell->setDisabledTo(to);
    to->setIsDisabledConstraint(true);
  }
  else {
    disabled_cell->setDisabledAll();
    cell->setIsDisabledConstraint(true);
  }
}

void
Sdc::removeDisable(LibertyCell *cell,
		   LibertyPort *from,
		   LibertyPort *to)
{
  DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
  if (disabled_cell) {
    if (from && to) {
      disabled_cell->removeDisabledFromTo(from, to);
      for (TimingArcSet *arc_set : cell->timingArcSets(from, to))
        arc_set->setIsDisabledConstraint(false);
    }
    else if (from) {
      disabled_cell->removeDisabledFrom(from);
      from->setIsDisabledConstraint(false);
    }
    else if (to) {
      disabled_cell->removeDisabledTo(to);
      to->setIsDisabledConstraint(false);
    }
    else {
      disabled_cell->removeDisabledAll();
      cell->setIsDisabledConstraint(false);
    }
  }
}

void
Sdc::disable(TimingArcSet *arc_set)
{
  LibertyCell *cell = arc_set->libertyCell();
  DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
  if (disabled_cell == nullptr) {
    disabled_cell = new DisabledCellPorts(cell);
    disabled_cell_ports_[cell] = disabled_cell;
  }
  disabled_cell->setDisabled(arc_set);
  arc_set->setIsDisabledConstraint(true);
}

void
Sdc::removeDisable(TimingArcSet *arc_set)
{
  LibertyCell *cell = arc_set->libertyCell();
  DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
  if (disabled_cell) {
    disabled_cell->removeDisabled(arc_set);
    arc_set->setIsDisabledConstraint(false);
  }
}

void
Sdc::disable(LibertyPort *port)
{
  disabled_lib_ports_.insert(port);
  port->setIsDisabledConstraint(true);
}

void
Sdc::removeDisable(LibertyPort *port)
{
  disabled_lib_ports_.erase(port);
  port->setIsDisabledConstraint(false);
}

void
Sdc::disable(Port *port)
{
  disabled_ports_.insert(port);
}

void
Sdc::removeDisable(Port *port)
{
  disabled_ports_.erase(port);
}

void
Sdc::disable(Instance *inst,
	     LibertyPort *from,
	     LibertyPort *to)
{
  DisabledInstancePorts *disabled_inst = disabled_inst_ports_.findKey(inst);
  if (disabled_inst == nullptr) {
    disabled_inst = new DisabledInstancePorts(inst);
    disabled_inst_ports_[inst] = disabled_inst;
  }
  if (from && to)
    disabled_inst->setDisabledFromTo(from, to);
  else if (from)
    disabled_inst->setDisabledFrom(from);
  else if (to)
    disabled_inst->setDisabledTo(to);
  else
    disabled_inst->setDisabledAll();
}

void
Sdc::removeDisable(Instance *inst,
		   LibertyPort *from,
		   LibertyPort *to)
{
  DisabledInstancePorts *disabled_inst = disabled_inst_ports_.findKey(inst);
  if (disabled_inst) {
    if (from && to)
      disabled_inst->removeDisabledFromTo(from, to);
    else if (from)
      disabled_inst->removeDisabledFrom(from);
    else if (to)
      disabled_inst->removeDisabledTo(to);
    else
      disabled_inst->removeDisabledAll();
  }
}

void
Sdc::disable(Pin *from,
	     Pin *to)
{
  PinPair pair(from, to);
  disabled_wire_edges_.insert(pair);
}

void
Sdc::removeDisable(Pin *from,
		   Pin *to)
{
  PinPair probe(from, to);
  disabled_wire_edges_.erase(probe);
}

void
Sdc::disable(Edge *edge)
{
  disabled_edges_.insert(edge);
  edge->setIsDisabledConstraint(true);
}

void
Sdc::removeDisable(Edge *edge)
{
  disabled_edges_.erase(edge);
  edge->setIsDisabledConstraint(false);
}

bool
Sdc::isDisabled(Edge *edge)
{
  return disabled_edges_.hasKey(edge);
}

class DisableEdgesThruHierPin : public HierPinThruVisitor
{
public:
  DisableEdgesThruHierPin(PinPairSet *pairs,
			  Graph *graph);

protected:
  virtual void visit(const Pin *drvr,
		     const Pin *load);

  PinPairSet *pairs_;
  Graph *graph_;
};

DisableEdgesThruHierPin::DisableEdgesThruHierPin(PinPairSet *pairs,
						 Graph *graph) :
  HierPinThruVisitor(),
  pairs_(pairs),
  graph_(graph)
{
}

void
DisableEdgesThruHierPin::visit(const Pin *drvr,
			       const Pin *load)
{
  PinPair pair(drvr, load);
  pairs_->insert(pair);
}

void
Sdc::disable(const Pin *pin)
{
  if (network_->isHierarchical(pin)) {
    // Add leaf pins thru hierarchical pin to disabled_edges_.
    DisableEdgesThruHierPin visitor(&disabled_wire_edges_, graph_);
    visitDrvrLoadsThruHierPin(pin, network_, &visitor);
  }
  else
    disabled_pins_.insert(pin);
}

class RemoveDisableEdgesThruHierPin : public HierPinThruVisitor
{
public:
  RemoveDisableEdgesThruHierPin(PinPairSet *pairs,
				Graph *graph);

protected:
  virtual void visit(const Pin *drvr,
                     const Pin *load);

  PinPairSet *pairs_;
  Graph *graph_;
};

RemoveDisableEdgesThruHierPin::RemoveDisableEdgesThruHierPin(PinPairSet *pairs,
							     Graph *graph) :
  HierPinThruVisitor(),
  pairs_(pairs),
  graph_(graph)
{
}

void
RemoveDisableEdgesThruHierPin::visit(const Pin *drvr,
				     const Pin *load)
{
  PinPair pair(drvr, load);
  pairs_->erase(pair);
}

void
Sdc::removeDisable(Pin *pin)
{
  if (network_->isHierarchical(pin)) {
    // Remove leaf pins thru hierarchical pin from disabled_edges_.
    RemoveDisableEdgesThruHierPin visitor(&disabled_wire_edges_, graph_);
    visitDrvrLoadsThruHierPin(pin, network_, &visitor);
  }
  else
    disabled_pins_.erase(pin);
}

bool
Sdc::isDisabled(const Pin *pin) const
{
  Port *port = network_->port(pin);
  LibertyPort *lib_port = network_->libertyPort(pin);
  return disabled_pins_.hasKey(pin)
    || disabled_ports_.hasKey(port)
    || disabled_lib_ports_.hasKey(lib_port);
}

bool
Sdc::isDisabled(const Instance *inst,
		const Pin *from_pin,
		const Pin *to_pin,
		const TimingRole *role) const
{
  if (role == TimingRole::wire()) {
    // Hierarchical thru pin disables.
    PinPair pair(from_pin, to_pin);
    return disabled_wire_edges_.hasKey(pair);
  }
  else {
    LibertyCell *cell = network_->libertyCell(inst);
    LibertyPort *from_port = network_->libertyPort(from_pin);
    LibertyPort *to_port = network_->libertyPort(to_pin);
    DisabledInstancePorts *disabled_inst = disabled_inst_ports_.findKey(inst);
    DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
    return (disabled_inst
	    && disabled_inst->isDisabled(from_port, to_port, role))
      || (disabled_cell
	  && disabled_cell->isDisabled(from_port, to_port, role));
  }
}

bool
Sdc::isDisabled(TimingArcSet *arc_set) const
{
  LibertyCell *cell = arc_set->libertyCell();
  if (cell) {
    DisabledCellPorts *disabled_cell = disabled_cell_ports_.findKey(cell);
    return disabled_cell
      && disabled_cell->isDisabled(arc_set);
  }
  else
    return false;
}

const DisabledInstancePortsMap *
Sdc::disabledInstancePorts() const
{
  return &disabled_inst_ports_;
}

DisabledCellPortsMap *
Sdc::disabledCellPorts()
{
  return &disabled_cell_ports_;
}

void
Sdc::disableClockGatingCheck(Instance *inst)
{
  disabled_clk_gating_checks_inst_.insert(inst);
}

void
Sdc::disableClockGatingCheck(Pin *pin)
{
  disabled_clk_gating_checks_pin_.insert(pin);
}

void
Sdc::removeDisableClockGatingCheck(Instance *inst)
{
  disabled_clk_gating_checks_inst_.erase(inst);
}

void
Sdc::removeDisableClockGatingCheck(Pin *pin)
{
  disabled_clk_gating_checks_pin_.erase(pin);
}

bool
Sdc::isDisableClockGatingCheck(const Instance *inst)
{
  return disabled_clk_gating_checks_inst_.hasKey(inst);
}

bool
Sdc::isDisableClockGatingCheck(const Pin *pin)
{
  return disabled_clk_gating_checks_pin_.hasKey(pin);
}

////////////////////////////////////////////////////////////////

void
Sdc::setLogicValue(const Pin *pin,
                   LogicValue value)
{
  logic_value_map_[pin] = value;
}

void
Sdc::logicValue(const Pin *pin,
		LogicValue &value,
		bool &exists)
{
  logic_value_map_.findKey(pin, value, exists);
}

void
Sdc::setCaseAnalysis(const Pin *pin,
		     LogicValue value)
{
  case_value_map_[pin] = value;
}

void
Sdc::removeCaseAnalysis(const Pin *pin)
{
  case_value_map_.erase(pin);
}

void
Sdc::caseLogicValue(const Pin *pin,
		    LogicValue &value,
		    bool &exists)
{
  case_value_map_.findKey(pin, value, exists);
}

bool
Sdc::hasLogicValue(const Pin *pin)
{
  return case_value_map_.hasKey(pin)
    || logic_value_map_.hasKey(pin);
}

////////////////////////////////////////////////////////////////

ExceptionFrom *
Sdc::makeExceptionFrom(PinSet *from_pins,
		       ClockSet *from_clks,
		       InstanceSet *from_insts,
		       const RiseFallBoth *from_rf)
{
  if ((from_pins && !from_pins->empty())
      || (from_clks && !from_clks->empty())
      || (from_insts && !from_insts->empty()))
    return new ExceptionFrom(from_pins, from_clks, from_insts, from_rf,
                             true, network_);
  else
    return nullptr;
}

ExceptionThru *
Sdc::makeExceptionThru(PinSet *pins,
		       NetSet *nets,
		       InstanceSet *insts,
		       const RiseFallBoth *rf)
{
  if ((pins && !pins->empty())
      || (nets && !nets->empty())
      || (insts && !insts->empty()))
    return new ExceptionThru(pins, nets, insts, rf, true, network_);
  else
    return nullptr;
}

ExceptionTo *
Sdc::makeExceptionTo(PinSet *pins,
		     ClockSet *clks,
		     InstanceSet *insts,
		     const RiseFallBoth *rf,
		     const RiseFallBoth *end_rf)
{
  if ((pins && !pins->empty())
      || (clks && !clks->empty())
      || (insts && !insts->empty())
      || (rf != RiseFallBoth::riseFall())
      || (end_rf != RiseFallBoth::riseFall()))
    return new ExceptionTo(pins, clks, insts, rf, end_rf, true, network_);
  else
    return nullptr;
}

// Valid endpoints include gated clock enables which are not
// known until clock arrivals are determined.
bool
Sdc::exceptionToInvalid(const Pin *pin)
{
  Net *net = network_->net(pin);
  // Floating pins are invalid.
  if ((net == nullptr
       && !(network_->isTopLevelPort(pin)
            || network_->direction(pin)->isInternal()))
      || (net
	  // Pins connected to power/ground are invalid.
	  && (network_->isPower(net)
	      || network_->isGround(net)))
      // Hierarchical pins are invalid.
      || network_->isHierarchical(pin))
    return true;
  // Register/latch Q pins are invalid.
  LibertyPort *port = network_->libertyPort(pin);
  if (port) {
    LibertyCell *cell = port->libertyCell();
    for (TimingArcSet *arc_set : cell->timingArcSets(nullptr, port)) {
      TimingRole *role = arc_set->role();
      if (role->genericRole() == TimingRole::regClkToQ())
	return true;
    }
  }
  return false;
}

void
Sdc::makeFalsePath(ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const MinMaxAll *min_max,
		   const char *comment)
{
  checkFromThrusTo(from, thrus, to);
  FalsePath *exception = new FalsePath(from, thrus, to, min_max, true,
				       comment);
  addException(exception);
}

void
Sdc::makeMulticyclePath(ExceptionFrom *from,
			ExceptionThruSeq *thrus,
			ExceptionTo *to,
			const MinMaxAll *min_max,
			bool use_end_clk,
			int path_multiplier,
			const char *comment)
{
  checkFromThrusTo(from, thrus, to);
  MultiCyclePath *exception = new MultiCyclePath(from, thrus, to,
						 min_max, use_end_clk,
						 path_multiplier, true,
						 comment);
  addException(exception);
}

void
Sdc::makePathDelay(ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const MinMax *min_max,
		   bool ignore_clk_latency,
		   float delay,
		   const char *comment)
{
  checkFromThrusTo(from, thrus, to);
  PathDelay *exception = new PathDelay(from, thrus, to, min_max, 
				       ignore_clk_latency, delay, true,
				       comment);
  addException(exception);
}

void
Sdc::recordPathDelayInternalStartpoints(ExceptionPath *exception)
{
  ExceptionFrom *from = exception->from();
  if (from
      && from->hasPins()) {
    for (const Pin *pin : *from->pins()) {
      if (!(network_->isRegClkPin(pin)
	    || network_->isTopLevelPort(pin))) {
	path_delay_internal_startpoints_.insert(pin);
      }
    }
  }
}

void
Sdc::unrecordPathDelayInternalStartpoints(ExceptionFrom *from)
{
  if (from
      && from->hasPins()
      && !path_delay_internal_startpoints_.empty()) {
    for (const Pin *pin : *from->pins()) {
      if (!(network_->isRegClkPin(pin)
	    || network_->isTopLevelPort(pin))
	  && !pathDelayFrom(pin))
	path_delay_internal_startpoints_.erase(pin);
    }
  }
}

bool
Sdc::pathDelayFrom(const Pin *pin)
{
  ExceptionPathSet *exceptions = first_from_pin_exceptions_.findKey(pin);
  for (ExceptionPath *exception : *exceptions) {
    if (exception->isPathDelay())
      return true;
  }
  return false;
}

bool
Sdc::isPathDelayInternalStartpoint(const Pin *pin) const
{
  return path_delay_internal_startpoints_.hasKey(pin);
}

const PinSet &
Sdc::pathDelayInternalStartpoints() const
{
  return path_delay_internal_startpoints_;
}

void
Sdc::recordPathDelayInternalEndpoints(ExceptionPath *exception)
{
  ExceptionTo *to = exception->to();
  if (to
      && to->hasPins()) {
    for (const Pin *pin : *to->pins()) {
      if (!(hasLibertyChecks(pin)
	    || network_->isTopLevelPort(pin))) {
	path_delay_internal_endpoints_.insert(pin);
      }
    }
  }
}

void
Sdc::unrecordPathDelayInternalEndpoints(ExceptionPath *exception)
{
  ExceptionTo *to = exception->to();
  if (to
      && to->hasPins()
      && !path_delay_internal_endpoints_.empty()) {
    for (const Pin *pin : *to->pins()) {
      if (!(hasLibertyChecks(pin)
	    || network_->isTopLevelPort(pin))
	  && !pathDelayTo(pin))
	path_delay_internal_endpoints_.erase(pin);
    }
  }
}

bool
Sdc::hasLibertyChecks(const Pin *pin)
{
  const Instance *inst = network_->instance(pin);
  LibertyCell *cell = network_->libertyCell(inst);
  if (cell) {
    LibertyPort *port = network_->libertyPort(pin);
    if (port) {
      for (TimingArcSet *arc_set : cell->timingArcSets(nullptr, port)) {
	if (arc_set->role()->isTimingCheck())
	  return true;
      }
    }
  }
  return false;
}

bool
Sdc::pathDelayTo(const Pin *pin)
{
  ExceptionPathSet *exceptions = first_to_pin_exceptions_.findKey(pin);
  for (ExceptionPath *exception : *exceptions) {
    if (exception->isPathDelay())
      return true;
  }
  return false;
}

bool
Sdc::isPathDelayInternalEndpoint(const Pin *pin) const
{
  return path_delay_internal_endpoints_.hasKey(pin);
}

////////////////////////////////////////////////////////////////

void
Sdc::clearGroupPathMap()
{
  // GroupPath exceptions are deleted with other exceptions.
  // Delete group_path name strings.
  for (auto name_groups : group_path_map_) {
    const char *name = name_groups.first;
    GroupPathSet *groups = name_groups.second;
    stringDelete(name);
    groups->deleteContents();
    delete groups;
  }
  group_path_map_.clear();
}

void
Sdc::makeGroupPath(const char *name,
		   bool is_default,
		   ExceptionFrom *from,
		   ExceptionThruSeq *thrus,
		   ExceptionTo *to,
		   const char *comment)
{
  checkFromThrusTo(from, thrus, to);
  if (name && is_default)
    report_->critical(213, "group path name and is_default are mutually exclusive.");
  else if (name) {
    GroupPath *group_path = new GroupPath(name, is_default, from, thrus, to,
					  true, comment);
    // Clone the group_path because it may get merged and hence deleted
    // by addException.
    ExceptionFrom *from1 = group_path->from()
      ? group_path->from()->clone(network_):nullptr;
    ExceptionThruSeq *thrus1 = exceptionThrusClone(group_path->thrus(), network_);
    ExceptionTo *to1 = group_path->to() ? group_path->to()->clone(network_) : nullptr;
    ExceptionPath *clone = group_path->clone(from1, thrus1, to1, true);
    addException(clone);
    // A named group path can have multiple exceptions.
    GroupPathSet *groups = group_path_map_.findKey(name);
    if (groups == nullptr) {
      groups = new GroupPathSet(network_);
      group_path_map_[stringCopy(name)] = groups;
    }
    if (groups->hasKey(group_path))
      // Exact copy of existing group path.
      delete group_path;
    else
      groups->insert(group_path);
  }
  else {
    // is_default
    GroupPath *group_path = new GroupPath(name, is_default, from, thrus, to,
					  true, comment);
    addException(group_path);
  }
}

bool
Sdc::isGroupPathName(const char *group_name)
{
  return group_path_map_.hasKey(group_name);
}

////////////////////////////////////////////////////////////////

FilterPath *
Sdc::makeFilterPath(ExceptionFrom *from,
		    ExceptionThruSeq *thrus,
		    ExceptionTo *to)
{
  checkFromThrusTo(from, thrus, to);
  FilterPath *exception = new FilterPath(from, thrus, to, true);
  addException(exception);
  // This is the only type of exception that can be returned.
  // There is only one of them, so it shouldn't merge.
  return exception;
}

////////////////////////////////////////////////////////////////

void
Sdc::makeLoopExceptions()
{
  for (GraphLoop *loop : *levelize_->loops()) 
    makeLoopExceptions(loop);
}

// Make a -thru pin false path from every edge entering the loop
// around the loop and back.
void
Sdc::makeLoopExceptions(GraphLoop *loop)
{
  debugPrint(debug_, "loop", 2, "Loop false path");
  for (Edge *edge : *loop->edges()) {
    Vertex *from_vertex = edge->from(graph_);
    Vertex *to_vertex = edge->to(graph_);
    Pin *from_pin = from_vertex->pin();
    Pin *to_pin = to_vertex->pin();
    // Find edges entering the loop.
    VertexInEdgeIterator in_edge_iter(to_vertex, graph_);
    while (in_edge_iter.hasNext()) {
      Edge *in_edge = in_edge_iter.next();
      if (in_edge != edge) {
	Pin *loop_input_pin = in_edge->from(graph_)->pin();
	makeLoopException(loop_input_pin, to_pin, from_pin);
	// Prevent sub-loops by blocking paths on the main loop also.
	makeLoopException(from_pin, to_pin, loop_input_pin);
      }
    }
  }
}

void
Sdc::makeLoopException(const Pin *loop_input_pin,
		       const Pin *loop_pin,
		       const Pin *loop_prev_pin)
{
  ExceptionThruSeq *thrus = new ExceptionThruSeq;
  makeLoopExceptionThru(loop_input_pin, thrus);
  makeLoopExceptionThru(loop_pin, thrus);
  makeLoopExceptionThru(loop_prev_pin, thrus);
  makeLoopExceptionThru(loop_pin, thrus);
  makeLoopPath(thrus);
}

void
Sdc::makeLoopPath(ExceptionThruSeq *thrus)
{
  FalsePath *exception = new LoopPath(thrus, true);
  addException(exception);
}

void
Sdc::makeLoopExceptionThru(const Pin *pin,
			   ExceptionThruSeq *thrus)
{
  debugPrint(debug_, "levelize", 2, " %s", network_->pathName(pin));
  PinSet *pins = new PinSet(network_);
  pins->insert(pin);
  ExceptionThru *thru = makeExceptionThru(pins, nullptr, nullptr,
					  RiseFallBoth::riseFall());
  thrus->push_back(thru);
}

void
Sdc::deleteLoopExceptions()
{
  // erase prevents range iteration.
  ExceptionPathSet::Iterator except_iter(exceptions_);
  while (except_iter.hasNext()) {
  ExceptionPath *except = except_iter.next();
  if (except->isLoop())
    deleteException(except);
  }
}

////////////////////////////////////////////////////////////////

void
Sdc::addException(ExceptionPath *exception)
{
  debugPrint(debug_, "exception_merge", 1, "add exception for %s",
             exception->asString(network_));

  if (exception->isPathDelay()) {
    recordPathDelayInternalStartpoints(exception);
    recordPathDelayInternalEndpoints(exception);
    if (exception->to() == nullptr)
      path_delays_without_to_ = true;
  }

  // Check to see if the exception has from/to mixed object types.
  // If so, the priority of the exception is mixed.
  // Split it into separate exceptions that have consistent priority.
  ExceptionFrom *from = exception->from();
  if (from
      && (from->hasPins() || from->hasInstances())
      && from->hasClocks()) {
    PinSet *pins1 = from->pins() ? new PinSet(*from->pins()) : nullptr;
    InstanceSet *insts1 =
      from->instances() ? new InstanceSet(*from->instances()) : nullptr;
    ExceptionFrom *from1 = new ExceptionFrom(pins1, nullptr, insts1,
					     from->transition(), true, network_);
    ExceptionThruSeq *thrus1 = exceptionThrusClone(exception->thrus(), network_);
    ExceptionTo *to = exception->to();    
    ExceptionTo *to1 = to ? to->clone(network_) : nullptr;
    ExceptionPath *exception1 = exception->clone(from1, thrus1, to1, true);
    debugPrint(debug_, "exception_merge", 1, " split exception for %s",
               exception1->asString(network_));
    addException1(exception1);

    ClockSet *clks2 = new ClockSet(*from->clks());
    ExceptionFrom *from2 = new ExceptionFrom(nullptr, clks2, nullptr,
					     from->transition(), true, network_);
    ExceptionThruSeq *thrus2 = exceptionThrusClone(exception->thrus(), network_);
    ExceptionTo *to2 = to ? to->clone(network_) : nullptr;
    ExceptionPath *exception2 = exception->clone(from2, thrus2, to2, true);
    debugPrint(debug_, "exception_merge", 1, " split exception for %s",
               exception2->asString(network_));
    addException1(exception2);

    delete exception;
  }
  else
    addException1(exception);
}

void
Sdc::addException1(ExceptionPath *exception)
{
  ExceptionTo *to = exception->to();
  if (to
      && (to->hasPins() || to->hasInstances())
      && to->hasClocks()) {
    ExceptionFrom *from1 = exception->from() ? exception->from()->clone(network_):nullptr;
    ExceptionThruSeq *thrus1 = exceptionThrusClone(exception->thrus(), network_);
    PinSet *pins1 = to->pins() ? new PinSet(*to->pins()) : nullptr;
    InstanceSet *insts1 = to->instances() ? new InstanceSet(*to->instances()) : nullptr;
    ExceptionTo *to1 = new ExceptionTo(pins1, nullptr, insts1, to->transition(),
				       to->endTransition(), true, network_);
    ExceptionPath *exception1 = exception->clone(from1, thrus1, to1, true);
    debugPrint(debug_, "exception_merge", 1, " split exception for %s",
               exception1->asString(network_));
    addException2(exception1);

    ExceptionFrom *from2 = exception->from() ? exception->from()->clone(network_):nullptr;
    ExceptionThruSeq *thrus2 = exceptionThrusClone(exception->thrus(), network_);
    ClockSet *clks2 = new ClockSet(*to->clks());
    ExceptionTo *to2 = new ExceptionTo(nullptr, clks2, nullptr, to->transition(),
				       to->endTransition(), true, network_);
    ExceptionPath *exception2 = exception->clone(from2, thrus2, to2, true);
    debugPrint(debug_, "exception_merge", 1, " split exception for %s",
               exception2->asString(network_));
    addException2(exception2);

    delete exception;
  }
  else
    addException2(exception);
}

void
Sdc::addException2(ExceptionPath *exception)
{
  if (exception->isMultiCycle() || exception->isPathDelay())
    deleteMatchingExceptions(exception);
  recordException(exception);
  mergeException(exception);
}

// If a path delay/multicycle exception is redefined with a different
// delay/cycle count, the new exception overrides the existing
// exception.  Multiple related exceptions are merged to reduce the
// number of tags.  To support overrides, relevant merged exceptions must be
// expanded to find and delete or override the new exception.
// For example, the exception
//   set_multi_cycle_path -from {A B} -to {C D} 2
// is a merged representation of the following four exceptions:
//   set_multi_cycle_path -from A -to C 2
//   set_multi_cycle_path -from A -to D 2
//   set_multi_cycle_path -from B -to C 2
//   set_multi_cycle_path -from B -to D 2
// If the following exception is later defined,
//   set_multi_cycle_path -from A -to C 3
// The cycle count of one of the merged exceptions changes.
// This prevents the original four exceptions from merging into one
// exception.
//
// This situation is handled by breaking the original merged exception
// into multiple smaller exceptions that exclude the new subset
// exception.  This is NOT done by expanding the merged exception,
// since the number of exception points can be huge leading to serious
// run time problems.
//
// For the example above, the merged exception is broken down into the
// following set of exceptions that exclude the new subset exception.
//
//   set_multi_cycle_path -from {B} -to {C D} 2
//   set_multi_cycle_path -from {A} -to {D} 2
//
// In general, the merged exception is broken down as follows:
//
//   -from {merged_from - subset_from} -thru merged_thru... -to merged_to
//   -from merged_from -thru {merged_thru - subset_thru}... -to merged_to
//   -from merged_from -thru merged_thru... -to {merged_to - subset_to}
//
// Where the {set1 - set2} is the set difference of of the from/thru/to
// objects of the merged/subset exception.  If the set difference is empty,
// that group of exceptions matches the subset so it should not be included
// in the expansion.

void
Sdc::deleteMatchingExceptions(ExceptionPath *exception)
{
  debugPrint(debug_, "exception_merge", 1, "find matches for %s",
             exception->asString(network_));
  ExceptionPathSet matches;
  findMatchingExceptions(exception, matches);

  ExceptionPathSet expanded_matches;
  for (ExceptionPath *match : matches)
    // Expand the matching exception into a set of exceptions that
    // that do not cover the new exception.  Do not record them
    // to prevent merging with the match, which will be deleted.
    expandExceptionExcluding(match, exception, expanded_matches);

  for (ExceptionPath *match : matches)
    deleteException(match);

  for (ExceptionPath *match : expanded_matches)
    addException(match);
}

void
Sdc::findMatchingExceptions(ExceptionPath *exception,
			    ExceptionPathSet &matches)
{
  if (exception->from())
    findMatchingExceptionsFirstFrom(exception, matches);
  else if (exception->thrus())
    findMatchingExceptionsFirstThru(exception, matches);
  else if (exception->to())
    findMatchingExceptionsFirstTo(exception, matches);
}

void
Sdc::findMatchingExceptionsFirstFrom(ExceptionPath *exception,
				     ExceptionPathSet &matches)
{
  ExceptionFrom *from = exception->from();
  findMatchingExceptionsPins(exception, from->pins(),
                             first_from_pin_exceptions_,
                             matches);
  findMatchingExceptionsInsts(exception, from->instances(),
                              first_from_inst_exceptions_, matches);
  findMatchingExceptionsClks(exception, from->clks(),
                             first_from_clk_exceptions_,
                             matches);
}

void
Sdc::findMatchingExceptionsFirstThru(ExceptionPath *exception,
				     ExceptionPathSet &matches)
{
  ExceptionThru *thru = (*exception->thrus())[0];
  findMatchingExceptionsPins(exception, thru->pins(),
			     first_thru_pin_exceptions_,
			     matches);
  findMatchingExceptionsInsts(exception, thru->instances(),
			      first_thru_inst_exceptions_,
			      matches);
  if (!first_thru_net_exceptions_.empty()) {
    for (const Net *net : *thru->nets()) {
      // Potential matches includes exceptions that match net that are not
      // the first exception point.
      ExceptionPathSet *potential_matches =
	first_thru_net_exceptions_.findKey(net);
      if (potential_matches) {
        for (ExceptionPath *match : *potential_matches) {
	  ExceptionThru *match_thru = (*match->thrus())[0];
	  if (match_thru->nets()->hasKey(net)
	      && match->overrides(exception)
	      && match->intersectsPts(exception, network_))
	    matches.insert(match);
	}
      }
    }
  }
}

void
Sdc::findMatchingExceptionsFirstTo(ExceptionPath *exception,
				   ExceptionPathSet &matches)
{
  ExceptionTo *to = exception->to();
  findMatchingExceptionsPins(exception, to->pins(), first_to_pin_exceptions_,
			     matches);
  findMatchingExceptionsInsts(exception, to->instances(),
			      first_to_inst_exceptions_,
			      matches);
  findMatchingExceptionsClks(exception, to->clks(), first_to_clk_exceptions_,
			     matches);
}

void
Sdc::findMatchingExceptionsClks(ExceptionPath *exception,
				ClockSet *clks,
				ClockExceptionsMap &exception_map,
				ExceptionPathSet &matches)
{
  if (clks) {
    ExceptionPathSet clks_matches;
    for (Clock *clk : *clks)
      clks_matches.insertSet(exception_map.findKey(clk));
    findMatchingExceptions(exception, &clks_matches, matches);
  }
}

void
Sdc::findMatchingExceptionsPins(ExceptionPath *exception,
				PinSet *pins,
				PinExceptionsMap &exception_map,
				ExceptionPathSet &matches)
{
  if (pins) {
    ExceptionPathSet pins_matches;
    for (const Pin *pin : *pins)
      pins_matches.insertSet(exception_map.findKey(pin));
    findMatchingExceptions(exception, &pins_matches, matches);
  }
}

void
Sdc::findMatchingExceptionsInsts(ExceptionPath *exception,
				 InstanceSet *insts,
				 InstanceExceptionsMap &exception_map,
				 ExceptionPathSet &matches)
{
  if (insts) {
    ExceptionPathSet insts_matches;
    for (const Instance *inst : *insts)
      insts_matches.insertSet(exception_map.findKey(inst));
    findMatchingExceptions(exception, &insts_matches, matches);
  }
}

void
Sdc::findMatchingExceptions(ExceptionPath *exception,
			    ExceptionPathSet *potential_matches,
			    ExceptionPathSet &matches)
{
  if (potential_matches) {
    for (ExceptionPath *match : *potential_matches) {
      if (match->overrides(exception)
	  && match->intersectsPts(exception, network_))
	matches.insert(match);
    }
  }
}

void
Sdc::expandExceptionExcluding(ExceptionPath *exception,
			      ExceptionPath *excluding,
			      ExceptionPathSet &expansions)
{
  ExceptionFrom *from = exception->from();
  ExceptionThruSeq *thrus = exception->thrus();
  ExceptionTo *to = exception->to();
  if (from) {
    ExceptionFrom *from_cpy = from->clone(network_);
    from_cpy->deleteObjects(excluding->from(), network_);
    if (from_cpy->hasObjects()) {
      ExceptionThruSeq *thrus_cpy = nullptr;
      if (thrus)
	thrus_cpy = clone(thrus, network_);
      ExceptionTo *to_cpy = nullptr;
      if (to)
	to_cpy = to->clone(network_);
      ExceptionPath *expand = exception->clone(from_cpy,thrus_cpy,to_cpy,true);
      expansions.insert(expand);
    }
    else
      delete from_cpy;
  }
  if (thrus) {
    ExceptionThruSeq::Iterator thru_iter(thrus);
    ExceptionThruSeq::Iterator thru_iter2(excluding->thrus());
    while (thru_iter.hasNext()
	   && thru_iter2.hasNext()) {
      ExceptionThru *thru = thru_iter.next();
      ExceptionThru *thru2 = thru_iter2.next();
      ExceptionThru *thru_cpy = thru->clone(network_);
      thru_cpy->deleteObjects(thru2, network_);
      if (thru_cpy->hasObjects()) {
	ExceptionFrom *from_cpy = nullptr;
	if (from)
	  from_cpy = from->clone(network_);
	ExceptionThruSeq *thrus_cpy = new ExceptionThruSeq;
        for (ExceptionThru *thru1 : *thrus) {
	  if (thru1 == thru)
	    thrus_cpy->push_back(thru_cpy);
	  else {
	    ExceptionThru *thru_cpy = thru->clone(network_);
	    thrus_cpy->push_back(thru_cpy);
	  }
	}
	ExceptionTo *to_cpy = nullptr;
	if (to)
	  to_cpy = to->clone(network_);
	ExceptionPath *expand = exception->clone(from_cpy, thrus_cpy, to_cpy,
						 true);
	expansions.insert(expand);
      }
      else
	delete thru_cpy;
    }
  }
  if (to) {
    ExceptionTo *to_cpy = to->clone(network_);
    to_cpy->deleteObjects(excluding->to(), network_);
    if (to_cpy->hasObjects()) {
      ExceptionFrom *from_cpy = nullptr;
      if (from)
	from_cpy = from->clone(network_);
      ExceptionThruSeq *thrus_cpy = nullptr;
      if (thrus)
	thrus_cpy = clone(thrus, network_);
      ExceptionPath *expand = exception->clone(from_cpy,thrus_cpy,to_cpy,true);
      expansions.insert(expand);
    }
    else
      delete to_cpy;
  }
}

static ExceptionThruSeq *
clone(ExceptionThruSeq *thrus,
      Network *network)
{
  ExceptionThruSeq *thrus_cpy = new ExceptionThruSeq;
  for (ExceptionThru *thru : *thrus) {
    ExceptionThru *thru_cpy = thru->clone(network);
    thrus_cpy->push_back(thru_cpy);
  }
  return thrus_cpy;
}

////////////////////////////////////////////////////////////////

void
Sdc::recordException(ExceptionPath *exception)
{
  exceptions_.insert(exception);
  recordMergeHashes(exception);
  recordExceptionFirstPts(exception);
  checkForThruHpins(exception);
}

void
Sdc::checkForThruHpins(ExceptionPath *exception)
{
  ExceptionThruSeq *thrus = exception->thrus();
  if (thrus) {
    for (ExceptionThru *thru : *thrus) {
      if (thru->edges()) {
        have_thru_hpin_exceptions_ = true;
        break;
      }
    }
  }
}

void
Sdc::recordMergeHashes(ExceptionPath *exception)
{
  ExceptionPtIterator missing_pt_iter(exception);
  while (missing_pt_iter.hasNext()) {
    ExceptionPt *missing_pt = missing_pt_iter.next();
    recordMergeHash(exception, missing_pt);
  }
}

void
Sdc::recordMergeHash(ExceptionPath *exception,
		     ExceptionPt *missing_pt)
{
  size_t hash = exception->hash(missing_pt);
  debugPrint(debug_, "exception_merge", 3,
             "record merge hash %zu %s missing %s",
             hash,
             exception->asString(network_),
             missing_pt->asString(network_));
  ExceptionPathSet *set = exception_merge_hash_.findKey(hash);
  if (set == nullptr) {
    set = new ExceptionPathSet;
    exception_merge_hash_[hash] = set;
  }
  set->insert(exception);
}

// Record a mapping from first pin/clock/instance's to a set of exceptions.
// The first exception point is when the exception becomes active.
// After it becomes active, its state changes as the other
// exception points are traversed.
void
Sdc::recordExceptionFirstPts(ExceptionPath *exception)
{
  if (exception->from())
    recordExceptionFirstFrom(exception);
  else if (exception->thrus())
    recordExceptionFirstThru(exception);
  else if (exception->to())
    recordExceptionFirstTo(exception);
}

void
Sdc::recordExceptionFirstFrom(ExceptionPath *exception)
{
  ExceptionFrom *from = exception->from();
  recordExceptionPins(exception, from->pins(), first_from_pin_exceptions_);
  recordExceptionInsts(exception, from->instances(),
		       first_from_inst_exceptions_);
  recordExceptionClks(exception, from->clks(), first_from_clk_exceptions_);
}

void
Sdc::recordExceptionFirstThru(ExceptionPath *exception)
{
  ExceptionThru *thru = (*exception->thrus())[0];
  recordExceptionPins(exception, thru->pins(), first_thru_pin_exceptions_);
  recordExceptionInsts(exception, thru->instances(),
		       first_thru_inst_exceptions_);
  recordExceptionEdges(exception, thru->edges(), first_thru_edge_exceptions_);
  for (ExceptionThru *thru : *exception->thrus()) 
    recordExceptionNets(exception, thru->nets(), first_thru_net_exceptions_);
}

void
Sdc::recordExceptionFirstTo(ExceptionPath *exception)
{
  ExceptionTo *to = exception->to();
  recordExceptionPins(exception, to->pins(), first_to_pin_exceptions_);
  recordExceptionInsts(exception, to->instances(), first_to_inst_exceptions_);
  recordExceptionClks(exception, to->clks(), first_to_clk_exceptions_);
}

void
Sdc::recordExceptionClks(ExceptionPath *exception,
			 ClockSet *clks,
			 ClockExceptionsMap &exception_map)
{
  if (clks) {
    for (Clock *clk : *clks) {
      ExceptionPathSet *set = exception_map.findKey(clk);
      if (set == nullptr) {
        set = new ExceptionPathSet;
        exception_map[clk] = set;
      }
      set->insert(exception);
    }
  }
}

void
Sdc::recordExceptionEdges(ExceptionPath *exception,
			  EdgePinsSet *edges,
			  EdgeExceptionsMap &exception_map)
{
  if (edges) {
    for (const EdgePins &edge : *edges) {
      ExceptionPathSet *set = exception_map.findKey(edge);
      if (set == nullptr) {
        set = new ExceptionPathSet;
        exception_map.insert(edge, set);
      }
      set->insert(exception);
    }
  }
}

void
Sdc::recordExceptionPins(ExceptionPath *exception,
			 PinSet *pins,
			 PinExceptionsMap &exception_map)
{
  if (pins) {
    for (const Pin *pin : *pins) {
      ExceptionPathSet *set = exception_map.findKey(pin);
      if (set == nullptr) {
        set = new ExceptionPathSet;
        exception_map.insert(pin, set);
      }
      set->insert(exception);
    }
  }
}

void
Sdc::recordExceptionHpin(ExceptionPath *exception,
			 Pin *pin,
			 PinExceptionsMap &exception_map)
{
  ExceptionPathSet *set = exception_map.findKey(pin);
  if (set == nullptr) {
    set = new ExceptionPathSet;
    exception_map.insert(pin, set);
  }
  set->insert(exception);
}

void
Sdc::recordExceptionInsts(ExceptionPath *exception,
			  InstanceSet *insts,
			  InstanceExceptionsMap &exception_map)
{
  if (insts) {
    for (const Instance *inst : *insts) {
      ExceptionPathSet *set = exception_map.findKey(inst);
      if (set == nullptr) {
        set = new ExceptionPathSet;
        exception_map[inst] = set;
      }
      set->insert(exception);
    }
  }
}

void
Sdc::recordExceptionNets(ExceptionPath *exception,
			 NetSet *nets,
			 NetExceptionsMap &exception_map)
{
  if (nets) {
    for (const Net *net : *nets) {
      ExceptionPathSet *set = exception_map.findKey(net);
      if (set == nullptr) {
        set = new ExceptionPathSet;
        exception_map[net] = set;
      }
      set->insert(exception);
    }
  }
}

// Exceptions of the same type can be merged if they differ in exactly
// one exception point (-from, -thru or -to).
// For example, the following exceptions:
//   set_false_path -from {A B} -to C
//   set_false_path -from {A B} -to D
// can be merged to form:
//   set_false_path -from {A B} -to {C D}
//
// A hash is generated for each exception missing one exception point
// to find potential matches.  If a match is found, the exceptions are
// merged. Next we try to merge the surviving exception until we run
// out of merges.
void
Sdc::mergeException(ExceptionPath *exception)
{
  ExceptionPath *merged = findMergeMatch(exception);
  while (merged)
    merged = findMergeMatch(merged);
}

// Return the merged result.
ExceptionPath *
Sdc::findMergeMatch(ExceptionPath *exception)
{
  bool first_pt = true;
  ExceptionPtIterator missing_pt_iter(exception);
  while (missing_pt_iter.hasNext()) {
    ExceptionPt *missing_pt = missing_pt_iter.next();
    size_t hash = exception->hash(missing_pt);
    ExceptionPathSet *matches = exception_merge_hash_.findKey(hash);
    if (matches) {
      for (ExceptionPath *match : *matches) {
	ExceptionPt *match_missing_pt;
	if (match != exception
	    // Exceptions are not merged if their priorities are
	    // different.  This allows exceptions to be pruned during
	    // search at the endpoint.
	    && exception->mergeable(match)
	    && match->mergeablePts(exception, missing_pt, match_missing_pt)) {
	  debugPrint(debug_, "exception_merge", 1, "merge %s",
                     exception->asString(network_));
	  debugPrint(debug_, "exception_merge", 1, " with %s",
                     match->asString(network_));
	  // Unrecord the exception that is being merged away.
	  unrecordException(exception);
	  unrecordMergeHashes(match);
	  missing_pt->mergeInto(match_missing_pt, network_);
	  recordMergeHashes(match);
	  // First point maps only change if the exception point that
	  // is being merged is the first exception point.
	  if (first_pt)
	    recordExceptionFirstPts(match);
          // Have to wait until after exception point merge to delete
          // the exception.
	  delete exception;
	  return match;
	}
      }
    }
    first_pt = false;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

void
Sdc::deleteExceptions()
{
  exceptions_.deleteContentsClear();

  first_from_pin_exceptions_.deleteContentsClear();
  first_from_clk_exceptions_.deleteContentsClear();
  first_from_inst_exceptions_.deleteContentsClear();
  first_to_pin_exceptions_.deleteContentsClear();
  first_to_clk_exceptions_.deleteContentsClear();
  first_to_inst_exceptions_.deleteContentsClear();
  first_thru_pin_exceptions_.deleteContentsClear();
  first_thru_inst_exceptions_.deleteContentsClear();
  first_thru_net_exceptions_.deleteContentsClear();
  first_thru_edge_exceptions_.deleteContentsClear();
  first_thru_edge_exceptions_.clear();
  path_delay_internal_startpoints_.clear();
  path_delay_internal_endpoints_.clear();

  deleteExceptionPtHashMapSets(exception_merge_hash_);
  exception_merge_hash_.clear();
  have_thru_hpin_exceptions_ = false;
}

void
Sdc::deleteExceptionPtHashMapSets(ExceptionPathPtHash &map)
{
  map.deleteContents();
}

////////////////////////////////////////////////////////////////

void
Sdc::deleteExceptionsReferencing(Clock *clk)
{
  // erase prevents range iteration.
  ExceptionPathSet::ConstIterator exception_iter(exceptions_);
  while (exception_iter.hasNext()) {
    ExceptionPath *exception = exception_iter.next();
    bool deleted = false;
    ExceptionFrom *from = exception->from();
    if (from) {
      ClockSet *clks = from->clks();
      if (clks && clks->hasKey(clk)) {
	unrecordException(exception);
	from->deleteClock(clk);
	if (from->hasObjects())
	  recordException(exception);
	else {
	  deleteException(exception);
          deleted = true;
	}
      }
    }

    if (!deleted) {
      ExceptionTo *to = exception->to();
      if (to) {
	ClockSet *clks = to->clks();
	if (clks && clks->hasKey(clk)) {
	  unrecordException(exception);
	  to->deleteClock(clk);
	  if (to->hasObjects())
	    recordException(exception);
	  else
	    deleteException(exception);
	}
      }
    }
  }
}

void
Sdc::deleteException(ExceptionPath *exception)
{
  debugPrint(debug_, "exception_merge", 2, "delete %s",
             exception->asString(network_));
  unrecordException(exception);
  delete exception;
}

void
Sdc::unrecordException(ExceptionPath *exception)
{
  unrecordMergeHashes(exception);
  unrecordExceptionFirstPts(exception);
  exceptions_.erase(exception);
}

void
Sdc::unrecordMergeHashes(ExceptionPath *exception)
{
  ExceptionPtIterator missing_pt_iter(exception);
  while (missing_pt_iter.hasNext()) {
    ExceptionPt *missing_pt = missing_pt_iter.next();
    unrecordMergeHash(exception, missing_pt);
  }
}

void
Sdc::unrecordMergeHash(ExceptionPath *exception,
		       ExceptionPt *missing_pt)
{
  size_t hash = exception->hash(missing_pt);
  debugPrint(debug_, "exception_merge", 3,
             "unrecord merge hash %zu %s missing %s",
             hash,
             exception->asString(network_),
             missing_pt->asString(network_));
  ExceptionPathSet *matches = exception_merge_hash_.findKey(hash);
  if (matches)
    matches->erase(exception);
}

void
Sdc::unrecordExceptionFirstPts(ExceptionPath *exception)
{
  ExceptionFrom *from = exception->from();
  ExceptionThruSeq *thrus = exception->thrus();
  ExceptionTo *to = exception->to();
  if (from) {
    unrecordExceptionPins(exception, from->pins(), first_from_pin_exceptions_);
    unrecordExceptionClks(exception, from->clks(), first_from_clk_exceptions_);
    unrecordExceptionInsts(exception, from->instances(), first_from_inst_exceptions_);
  }
  else if (thrus) {
    ExceptionThru *thru = (*thrus)[0];
    unrecordExceptionPins(exception, thru->pins(), first_thru_pin_exceptions_);
    unrecordExceptionInsts(exception, thru->instances(),
			   first_thru_inst_exceptions_);
    unrecordExceptionNets(exception, thru->nets(), first_thru_net_exceptions_);
    unrecordExceptionEdges(exception, thru->edges(),
			   first_thru_edge_exceptions_);
  }
  else if (to) {
    unrecordExceptionPins(exception, to->pins(), first_to_pin_exceptions_);
    unrecordExceptionClks(exception, to->clks(), first_to_clk_exceptions_);
    unrecordExceptionInsts(exception, to->instances(),
			   first_to_inst_exceptions_);
  }
}

void
Sdc::unrecordExceptionClks(ExceptionPath *exception,
			   ClockSet *clks,
			   ClockExceptionsMap &exception_map)
{
  if (clks) {
    for (Clock *clk : *clks) {
      ExceptionPathSet *set = exception_map.findKey(clk);
      if (set)
        set->erase(exception);
    }
  }
}

void
Sdc::unrecordExceptionPins(ExceptionPath *exception,
			   PinSet *pins,
			   PinExceptionsMap &exception_map)
{
  if (pins) {
    for (const Pin *pin : *pins) {
      ExceptionPathSet *set = exception_map.findKey(pin);
      if (set)
        set->erase(exception);
    }
  }
}

void
Sdc::unrecordExceptionInsts(ExceptionPath *exception,
			    InstanceSet *insts,
			    InstanceExceptionsMap &exception_map)
{
  if (insts) {
    for (const Instance *inst : *insts) {
      ExceptionPathSet *set = exception_map.findKey(inst);
      if (set)
        set->erase(exception);
    }
  }
}

void
Sdc::unrecordExceptionEdges(ExceptionPath *exception,
			    EdgePinsSet *edges,
			    EdgeExceptionsMap &exception_map)
{
  if (edges) {
    for (const EdgePins &edge : *edges) {
      ExceptionPathSet *set = exception_map.findKey(edge);
      if (set)
        set->erase(exception);
    }
  }
}

void
Sdc::unrecordExceptionNets(ExceptionPath *exception,
			   NetSet *nets,
			   NetExceptionsMap &exception_map)
{
  if (nets) {
    for (const Net *net : *nets) {
      ExceptionPathSet *set = exception_map.findKey(net);
      if (set)
        set->erase(exception);
    }
  }
}

void
Sdc::unrecordExceptionHpin(ExceptionPath *exception,
			   Pin *pin,
			   PinExceptionsMap &exception_map)
{
  ExceptionPathSet *set = exception_map.findKey(pin);
  if (set)
    set->erase(exception);
}

////////////////////////////////////////////////////////////////

class ExpandException : public ExpandedExceptionVisitor
{
public:
  ExpandException(ExceptionPath *exception,
		  ExceptionPathSet &expansions,
		  Network *network);
  virtual void visit(ExceptionFrom *from, ExceptionThruSeq *thrus,
		     ExceptionTo *to);

private:
  ExceptionPathSet &expansions_;
};

ExpandException::ExpandException(ExceptionPath *exception,
				 ExceptionPathSet &expansions,
				 Network *network) :
  ExpandedExceptionVisitor(exception, network),
  expansions_(expansions)
{
}

void
ExpandException::visit(ExceptionFrom *from,
		       ExceptionThruSeq *thrus,
		       ExceptionTo *to)
{
  ExceptionFrom *from_clone = nullptr;
  if (from)
    from_clone = from->clone(network_);
  ExceptionThruSeq *thrus_clone = nullptr;
  if (thrus) {
    thrus_clone = new ExceptionThruSeq;
    for (ExceptionThru *thru : *thrus)
      thrus_clone->push_back(thru->clone(network_));
  }
  ExceptionTo *to_clone = nullptr;
  if (to)
    to_clone = to->clone(network_);
  ExceptionPath *expand = exception_->clone(from_clone, thrus_clone,
					    to_clone, true);
  expansions_.insert(expand);
}

// Expand exception from/thrus/to sets so there is only one exception
// point in each from/thru/to.
void
Sdc::expandException(ExceptionPath *exception,
		     ExceptionPathSet &expansions)
{
  ExpandException expander(exception, expansions, network_);
  expander.visitExpansions();
}

////////////////////////////////////////////////////////////////

void
Sdc::resetPath(ExceptionFrom *from,
	       ExceptionThruSeq *thrus,
	       ExceptionTo *to,
	       const MinMaxAll *min_max)
{
  checkFromThrusTo(from, thrus, to);
  ExceptionPathSet::Iterator except_iter(exceptions_);
  while (except_iter.hasNext()) {
    ExceptionPath *match = except_iter.next();
    if (match->resetMatch(from, thrus, to, min_max, network_)) {
      debugPrint(debug_, "exception_match", 3, "reset match %s",
                 match->asString(network_));
      ExceptionPathSet expansions;
      expandException(match, expansions);
      deleteException(match);
      ExceptionPathSet::Iterator expand_iter(expansions);
      while (expand_iter.hasNext()) {
	ExceptionPath *expand = expand_iter.next();
	if (expand->resetMatch(from, thrus, to, min_max, network_)) {
	  unrecordPathDelayInternalStartpoints(expand->from());
	  unrecordPathDelayInternalEndpoints(expand);
	  delete expand;
	}
	else
	  addException(expand);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

bool
Sdc::exceptionFromStates(const Pin *pin,
			 const RiseFall *rf,
			 const Clock *clk,
			 const RiseFall *clk_rf,
			 const MinMax *min_max,
			 ExceptionStateSet *&states) const
{
  return exceptionFromStates(pin, rf, clk, clk_rf, min_max, true, states);
}

bool
Sdc::exceptionFromStates(const Pin *pin,
			 const RiseFall *rf,
			 const Clock *clk,
			 const RiseFall *clk_rf,
			 const MinMax *min_max,
			 bool include_filter,
			 ExceptionStateSet *&states) const
{
  bool srch_from = true;
  if (pin) {
    if (srch_from && !first_from_pin_exceptions_.empty())
      srch_from &= exceptionFromStates(first_from_pin_exceptions_.findKey(pin),
				       pin, rf, min_max, include_filter,
				       states);
    if (srch_from && !first_thru_pin_exceptions_.empty())
      srch_from &= exceptionFromStates(first_thru_pin_exceptions_.findKey(pin),
				       pin, rf, min_max, include_filter,
				       states);

    if (srch_from
	&& (!first_from_inst_exceptions_.empty()
            || !first_thru_inst_exceptions_.empty())) {
      Instance *inst = network_->instance(pin);
      if (srch_from && !first_from_inst_exceptions_.empty())
	srch_from &= exceptionFromStates(first_from_inst_exceptions_.findKey(inst),
					 pin, rf, min_max, include_filter,
					 states);
      if (srch_from && !first_thru_inst_exceptions_.empty())
	srch_from &= exceptionFromStates(first_thru_inst_exceptions_.findKey(inst),
					 pin, rf, min_max, include_filter,
					 states);
    }
  }
  if (srch_from && clk && !first_from_clk_exceptions_.empty())
    srch_from &= exceptionFromStates(first_from_clk_exceptions_.findKey(clk),
				     pin, clk_rf, min_max, include_filter,
				     states);
  if (!srch_from) {
    delete states;
    states = nullptr;
  }
  return srch_from;
}

bool
Sdc::exceptionFromStates(const ExceptionPathSet *exceptions,
			 const Pin *pin,
			 const RiseFall *rf,
			 const MinMax *min_max,
			 bool include_filter,
			 ExceptionStateSet *&states) const
{
  if (exceptions) {
    for (ExceptionPath *exception : *exceptions) {
      if (exception->matches(min_max, false)
	  && (exception->from() == nullptr
	      || exception->from()->transition()->matches(rf))
	  && (include_filter || !exception->isFilter())) {
	ExceptionState *state = exception->firstState();
	if (state->matchesNextThru(nullptr, pin, rf, min_max, network_))
	  // -from clk -thru reg/clk
	  state = state->nextState();
	// If the exception is -from and has no -to transition it is
	// complete out of the gate.
	if (state->isComplete()
	    && exception->isFalse()) {
	  // Leave the completed false path state as a marker on the tag,
	  // but flush all other exception states because they are lower
	  // priority.
	  if (states == nullptr)
	    states = new ExceptionStateSet(network_);
	  states->clear();
	  states->insert(state);
	  // No need to examine other exceptions from this
	  // pin/clock/instance.
	  return false;
	}
	if (states == nullptr)
	  states = new ExceptionStateSet(network_);
	states->insert(state);
      }
    }
  }
  return true;
}

void
Sdc::exceptionFromClkStates(const Pin *pin,
			    const RiseFall *rf,
			    const Clock *clk,
			    const RiseFall *clk_rf,
			    const MinMax *min_max,
			    ExceptionStateSet *&states) const
{
  if (pin) {
    if (!first_from_pin_exceptions_.empty())
      exceptionFromStates(first_from_pin_exceptions_.findKey(pin),
			  nullptr, rf, min_max, true, states);
    if (!first_from_inst_exceptions_.empty()) {
      Instance *inst = network_->instance(pin);
      exceptionFromStates(first_from_inst_exceptions_.findKey(inst),
			  pin, rf, min_max, true, states);
    }
  }
  if (!first_from_clk_exceptions_.empty())
    exceptionFromStates(first_from_clk_exceptions_.findKey(clk),
			pin, clk_rf, min_max, true, states);
}

void
Sdc::filterRegQStates(const Pin *to_pin,
		      const RiseFall *to_rf,
		      const MinMax *min_max,
		      ExceptionStateSet *&states) const
{
  if (!first_from_pin_exceptions_.empty()) {
    const ExceptionPathSet *exceptions =
      first_from_pin_exceptions_.findKey(to_pin);
    if (exceptions) {
      for (ExceptionPath *exception : *exceptions) {
	// Hack for filter -from reg/Q.
	if (exception->isFilter()
	    && exception->matchesFirstPt(to_rf, min_max)) {
	  ExceptionState *state = exception->firstState();
	  if (states == nullptr)
	    states = new ExceptionStateSet(network_);
	  states->insert(state);
	}
      }
    }
  }
}

ExceptionStateSet *
Sdc::exceptionThruStates(const Pin *from_pin,
			 const Pin *to_pin,
			 const RiseFall *to_rf,
			 const MinMax *min_max) const
{
  ExceptionStateSet *states = nullptr;
  exceptionThruStates(first_thru_pin_exceptions_.findKey(to_pin),
                      to_rf, min_max, states);
  if (!first_thru_edge_exceptions_.empty()) {
    EdgePins edge_pins(from_pin, to_pin);
    exceptionThruStates(first_thru_edge_exceptions_.findKey(edge_pins),
			to_rf, min_max, states);
  }
  if (!first_thru_inst_exceptions_.empty()
      && (network_->direction(to_pin)->isAnyOutput()
	  || network_->isLatchData(to_pin))) {
    const Instance *to_inst = network_->instance(to_pin);
    exceptionThruStates(first_thru_inst_exceptions_.findKey(to_inst),
			to_rf, min_max, states);
  }
  return states;
}

void
Sdc::exceptionThruStates(const ExceptionPathSet *exceptions,
			 const RiseFall *to_rf,
			 const MinMax *min_max,
			 // Return value.
			 ExceptionStateSet *&states) const
{
  if (exceptions) {
    for (ExceptionPath *exception : *exceptions) {
      if (exception->matchesFirstPt(to_rf, min_max)) {
	ExceptionState *state = exception->firstState();
	if (states == nullptr)
	  states = new ExceptionStateSet(network_);
	states->insert(state);
      }
    }
  }
}

void
Sdc::exceptionTo(ExceptionPathType type,
		 const Pin *pin,
		 const RiseFall *rf,
		 const ClockEdge *clk_edge,
		 const MinMax *min_max,
		 bool match_min_max_exactly,
		 // Return values.
		 ExceptionPath *&hi_priority_exception,
		 int &hi_priority) const
{
  if (!first_to_inst_exceptions_.empty()) {
    Instance *inst = network_->instance(pin);
    exceptionTo(first_to_inst_exceptions_.findKey(inst), type, pin, rf,
		clk_edge, min_max, match_min_max_exactly,
		hi_priority_exception, hi_priority);
  }
  if (!first_to_pin_exceptions_.empty())
    exceptionTo(first_to_pin_exceptions_.findKey(pin), type, pin, rf,
		clk_edge, min_max, match_min_max_exactly,
		hi_priority_exception, hi_priority);
  if (clk_edge && !first_to_clk_exceptions_.empty())
    exceptionTo(first_to_clk_exceptions_.findKey(clk_edge->clock()),
		type, pin, rf, clk_edge, min_max, match_min_max_exactly,
		hi_priority_exception, hi_priority);
}

void
Sdc::exceptionTo(const ExceptionPathSet *to_exceptions,
		 ExceptionPathType type,
		 const Pin *pin,
		 const RiseFall *rf,
		 const ClockEdge *clk_edge,
		 const MinMax *min_max,
		 bool match_min_max_exactly,
		 // Return values.
		 ExceptionPath *&hi_priority_exception,
		 int &hi_priority) const
{
  if (to_exceptions) {
    for (ExceptionPath *exception : *to_exceptions) {
      exceptionTo(exception, type, pin, rf, clk_edge,
		  min_max, match_min_max_exactly,
		  hi_priority_exception, hi_priority);
    }
  }
}

void
Sdc::exceptionTo(ExceptionPath *exception,
		 ExceptionPathType type,
		 const Pin *pin,
		 const RiseFall *rf,
		 const ClockEdge *clk_edge,
		 const MinMax *min_max,
		 bool match_min_max_exactly,
		 // Return values.
		 ExceptionPath *&hi_priority_exception,
		 int &hi_priority) const
{
  if ((type == ExceptionPathType::any
       || exception->type() == type)
      && exceptionMatchesTo(exception, pin, rf, clk_edge, min_max,
			    match_min_max_exactly, false)) {
    int priority = exception->priority(min_max);
    if (hi_priority_exception == nullptr
	|| priority > hi_priority
	|| (priority == hi_priority
	    && exception->tighterThan(hi_priority_exception))) {
      hi_priority = priority;
      hi_priority_exception = exception;
    }
  }
}

bool
Sdc::exceptionMatchesTo(ExceptionPath *exception,
			const Pin *pin,
			const RiseFall *rf,
			const ClockEdge *clk_edge,
			const MinMax *min_max,
			bool match_min_max_exactly,
			bool require_to_pin) const
{
  ExceptionTo *to = exception->to();
  return exception->matches(min_max, match_min_max_exactly)
    && ((to == nullptr
	 && !require_to_pin)
	|| (to
	    && to->matches(pin, clk_edge, rf, network_)));
}

bool
Sdc::isCompleteTo(ExceptionState *state,
		  const Pin *pin,
		  const RiseFall *rf,
		  const ClockEdge *clk_edge,
		  const MinMax *min_max,
		  bool match_min_max_exactly,
		  bool require_to_pin) const
{
  return state->nextThru() == nullptr
    && exceptionMatchesTo(state->exception(), pin, rf, clk_edge,
			  min_max, match_min_max_exactly, require_to_pin);
}

bool
Sdc::isCompleteTo(ExceptionState *state,
		  const Pin *pin,
		  const RiseFall *rf,
		  const MinMax *min_max) const
{
  ExceptionPath *exception = state->exception();
  ExceptionTo *to = exception->to();
  return state->nextThru() == nullptr
    && to
    && exception->matches(min_max, true)
    && to->matches(pin, rf, network_);
}

////////////////////////////////////////////////////////////////

Wireload *
Sdc::wireload(const MinMax *min_max)
{
  return wireload_[min_max->index()];
}

void
Sdc::setWireload(Wireload *wireload,
		 const MinMaxAll *min_max)
{
  for (auto mm_index : min_max->rangeIndex())
    wireload_[mm_index] = wireload;
}

void
Sdc::setWireloadMode(WireloadMode mode)
{
  wireload_mode_ = mode;
}

WireloadMode
Sdc::wireloadMode()
{
  return wireload_mode_;
}

const WireloadSelection *
Sdc::wireloadSelection(const MinMax *min_max)
{
  const WireloadSelection *sel = wireload_selection_[min_max->index()];
  if (sel == nullptr) {
    // Look for a default.
    LibertyLibrary *lib = network_->defaultLibertyLibrary();
    if (lib) {
      WireloadSelection *default_sel = lib->defaultWireloadSelection();
      if (default_sel) {
	sel = default_sel;
	setWireloadSelection(default_sel, MinMaxAll::all());
      }
    }
  }
  return sel;
}

void
Sdc::setWireloadSelection(WireloadSelection *selection,
			  const MinMaxAll *min_max)
{
  for (auto mm_index : min_max->rangeIndex())
    wireload_selection_[mm_index] = selection;
}

////////////////////////////////////////////////////////////////

bool
Sdc::crprEnabled() const
{
  return crpr_enabled_;
}

void
Sdc::setCrprEnabled(bool enabled)
{
  crpr_enabled_ = enabled;
}

CrprMode
Sdc::crprMode() const
{
  return crpr_mode_;
}

void
Sdc::setCrprMode(CrprMode mode)
{
  crpr_mode_ = mode;
}

bool
Sdc::crprActive() const
{
  return analysis_type_ == AnalysisType::ocv
    && crpr_enabled_;
}

bool
Sdc::propagateGatedClockEnable() const
{
  return propagate_gated_clock_enable_;
}

void
Sdc::setPropagateGatedClockEnable(bool enable)
{
  propagate_gated_clock_enable_ = enable;
}

bool
Sdc::presetClrArcsEnabled() const
{
  return preset_clr_arcs_enabled_;
}

void
Sdc::setPresetClrArcsEnabled(bool enable)
{
  preset_clr_arcs_enabled_ = enable;
}

bool
Sdc::condDefaultArcsEnabled() const
{
  return cond_default_arcs_enabled_;
}

void
Sdc::setCondDefaultArcsEnabled(bool enabled)
{
  cond_default_arcs_enabled_ = enabled;
}

bool
Sdc::isDisabledCondDefault(Edge *edge) const
{
  return !cond_default_arcs_enabled_
    && edge->timingArcSet()->isCondDefault();
}

bool
Sdc::bidirectInstPathsEnabled() const
{
  return bidirect_inst_paths_enabled_;
}

void
Sdc::setBidirectInstPathsEnabled(bool enabled)
{
  bidirect_inst_paths_enabled_ = enabled;
}

// Delay calculation propagates slews from a bidirect driver
// to the bidirect port and back through the bidirect driver when
// sta_bidirect_inst_paths_enabled_ is true.
bool
Sdc::bidirectDrvrSlewFromLoad(const Pin *pin) const
{
  return bidirect_inst_paths_enabled_
    && network_->direction(pin)->isBidirect()
    && network_->isTopLevelPort(pin);
}

bool
Sdc::bidirectNetPathsEnabled() const
{
  return bidirect_inst_paths_enabled_;
}

void
Sdc::setBidirectNetPathsEnabled(bool enabled)
{
  bidirect_inst_paths_enabled_ = enabled;
}

bool
Sdc::recoveryRemovalChecksEnabled() const
{
  return recovery_removal_checks_enabled_;
}

void
Sdc::setRecoveryRemovalChecksEnabled(bool enabled)
{
  recovery_removal_checks_enabled_ = enabled;
}

bool
Sdc::gatedClkChecksEnabled() const
{
  return gated_clk_checks_enabled_;
}

void
Sdc::setGatedClkChecksEnabled(bool enabled)
{
  gated_clk_checks_enabled_ = enabled;
}

bool
Sdc::dynamicLoopBreaking() const
{
  return dynamic_loop_breaking_;
}

void
Sdc::setDynamicLoopBreaking(bool enable)
{
  if (dynamic_loop_breaking_ != enable) {
    if (levelize_->levelized()) {
      if (enable)
	makeLoopExceptions();
      else
	deleteLoopExceptions();
    }
    dynamic_loop_breaking_ = enable;
  }
}

bool
Sdc::propagateAllClocks() const
{
  return propagate_all_clks_;
}

void
Sdc::setPropagateAllClocks(bool prop)
{
  propagate_all_clks_ = prop;
}

bool
Sdc::clkThruTristateEnabled() const
{
  return clk_thru_tristate_enabled_;
}

void
Sdc::setClkThruTristateEnabled(bool enable)
{
  clk_thru_tristate_enabled_ = enable;
}

ClockEdge *
Sdc::defaultArrivalClockEdge() const
{
  return default_arrival_clk_->edge(RiseFall::rise());
}

bool
Sdc::useDefaultArrivalClock()
{
  return use_default_arrival_clock_;
}

void
Sdc::setUseDefaultArrivalClock(bool enable)
{
  use_default_arrival_clock_ = enable;
}

////////////////////////////////////////////////////////////////

void
Sdc::connectPinAfter(const Pin *pin)
{
  if (have_thru_hpin_exceptions_) {
    PinSet *drvrs = network_->drivers(pin);
    for (ExceptionPath *exception : exceptions_) {
      ExceptionPt *first_pt = exception->firstPt();
      ExceptionThruSeq *thrus = exception->thrus();
      if (thrus) {
        for (ExceptionThru *thru : *exception->thrus()) {
          if (thru->edges()) {
            thru->connectPinAfter(drvrs, network_);
            if (first_pt == thru)
              recordExceptionEdges(exception, thru->edges(),
                                   first_thru_edge_exceptions_);
          }
        }
      }
    }
  }
}

void
Sdc::disconnectPinBefore(const Pin *pin)
{
  if (have_thru_hpin_exceptions_) {
    for (ExceptionPath *exception : exceptions_) {
      ExceptionPt *first_pt = exception->firstPt();
      ExceptionThruSeq *thrus = exception->thrus();
      if (thrus) {
        for (ExceptionThru *thru : *exception->thrus()) {
          if (thru->edges()) {
            thru->disconnectPinBefore(pin, network_);
            if (thru == first_pt)
              recordExceptionEdges(exception, thru->edges(),
                                   first_thru_edge_exceptions_);
          }
        }
      }
    }
  }
  for (int corner_index = 0; corner_index < corners_->count(); corner_index++)
    drvr_pin_wire_cap_maps_[corner_index].erase(pin);
}

void
Sdc::clkHpinDisablesChanged(const Pin *pin)
{
  if (isLeafPinClock(pin))
    clkHpinDisablesInvalid();
}

////////////////////////////////////////////////////////////////

// Find the leaf load pins corresponding to pin.
// If the pin is hierarchical, the leaf pins are:
//   hierarchical  input - load pins  inside the hierarchical instance
//   hierarchical output - load pins outside the hierarchical instance
void
findLeafLoadPins(const Pin *pin,
		 const Network *network,
		 PinSet *leaf_pins)
{
  if (network->isHierarchical(pin)) {
    PortDirection *dir = network->direction(pin);
    bool is_input = dir->isAnyInput();
    bool is_output = dir->isAnyOutput();
    const Instance *hinst = network->instance(pin);
    PinConnectedPinIterator *pin_iter = network->connectedPinIterator(pin);
    while (pin_iter->hasNext()) {
      const Pin *pin1 = pin_iter->next();
      bool is_inside = network->isInside(pin1, hinst);
      if (((is_input && is_inside)
	   || (is_output && !is_inside))
	  && network->isLoad(pin1))
	leaf_pins->insert(pin1);
    }
    delete pin_iter;
  }
  else
    leaf_pins->insert(pin);
}

// Find the leaf driver pins corresponding to pin.
// If the pin is hierarchical, the leaf pins are:
//   hierarchical  input - driver pins outside the hierarchical instance
//   hierarchical output - driver pins  inside the hierarchical instance
void
findLeafDriverPins(const Pin *pin,
		   const Network *network,
		   PinSet *leaf_pins)
{
  if (network->isHierarchical(pin)) {
    PortDirection *dir = network->direction(pin);
    bool is_input = dir->isAnyInput();
    bool is_output = dir->isAnyOutput();
    const Instance *hinst = network->instance(pin);
    PinConnectedPinIterator *pin_iter = network->connectedPinIterator(pin);
    while (pin_iter->hasNext()) {
      const Pin *pin1 = pin_iter->next();
      bool is_inside = network->isInside(pin1, hinst);
      if (((is_input && !is_inside)
	   || (is_output && is_inside))
	  && network->isDriver(pin1))
	leaf_pins->insert(pin1);
    }
    delete pin_iter;
  }
  else
    leaf_pins->insert(pin);
}

} // namespace
