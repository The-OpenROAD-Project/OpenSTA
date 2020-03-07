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

#include <algorithm>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "Stats.hh"
#include "Debug.hh"
#include "Mutex.hh"
#include "Report.hh"
#include "PatternMatch.hh"
#include "MinMax.hh"
#include "PortDirection.hh"
#include "RiseFallMinMax.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
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
#include "Graph.hh"
#include "Levelize.hh"
#include "HpinDrvrLoad.hh"
#include "Corner.hh"
#include "Sdc.hh"

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
static void
annotateGraphDisabledWireEdge(Pin *from_pin,
			      Pin *to_pin,
			      bool annotate,
			      Graph *graph);

////////////////////////////////////////////////////////////////

Sdc::Sdc(StaState *sta) :
  StaState(sta),
  derating_factors_(nullptr),
  net_derating_factors_(nullptr),
  inst_derating_factors_(nullptr),
  cell_derating_factors_(nullptr),
  clk_index_(0),
  clk_insertions_(nullptr),
  clk_group_exclusions_(nullptr),
  clk_group_same_(nullptr),
  clk_sense_map_(network_),
  clk_gating_check_(nullptr),
  input_delay_index_(0),
  port_cap_map_(nullptr),
  net_wire_cap_map_(nullptr),
  drvr_pin_wire_cap_map_(nullptr),
  first_from_pin_exceptions_(nullptr),
  first_from_clk_exceptions_(nullptr),
  first_from_inst_exceptions_(nullptr),
  first_thru_pin_exceptions_(nullptr),
  first_thru_inst_exceptions_(nullptr),
  first_thru_net_exceptions_(nullptr),
  first_to_pin_exceptions_(nullptr),
  first_to_clk_exceptions_(nullptr),
  first_to_inst_exceptions_(nullptr),
  first_thru_edge_exceptions_(nullptr),
  path_delay_internal_startpoints_(nullptr),
  path_delay_internal_endpoints_(nullptr)
{
  initVariables();
  sdc_ = this;
  setWireload(nullptr, MinMaxAll::all());
  setWireloadSelection(nullptr, MinMaxAll::all());
  setOperatingConditions(nullptr, MinMaxAll::all());
  makeDefaultArrivalClock();
  initInstancePvtMaps();
}

void
Sdc::makeDefaultArrivalClock()
{
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(0.0);
  default_arrival_clk_ = new Clock("input port clock", clk_index_++);
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
  if (clk_insertions_)
    clk_insertions_->clear();

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
}

void
Sdc::deleteConstraints()
{
  clocks_.deleteContents();
  delete default_arrival_clk_;
  clock_pin_map_.deleteContents();
  clock_leaf_pin_map_.deleteContents();
  clk_latencies_.deleteContents();
  if (clk_insertions_) {
    clk_insertions_->deleteContents();
    delete clk_insertions_;
    clk_insertions_ = nullptr;
  }

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

  DataChecksMap::Iterator data_checks_iter1(data_checks_from_map_);
  while (data_checks_iter1.hasNext()) {
    DataCheckSet *checks = data_checks_iter1.next();
    checks->deleteContents();
    delete checks;
  }

  DataChecksMap::Iterator data_checks_iter2(data_checks_to_map_);
  while (data_checks_iter2.hasNext()) {
    DataCheckSet *checks = data_checks_iter2.next();
    delete checks;
  }

  for (auto input_delay : input_delays_)
    delete input_delay;
  input_delay_pin_map_.deleteContents();
  input_delay_leaf_pin_map_.deleteContents();
  input_delay_ref_pin_map_.deleteContents();
  input_delay_internal_pin_map_.deleteContents();

  for (auto output_delay : output_delays_)
    delete output_delay;
  output_delay_pin_map_.deleteContents();
  output_delay_ref_pin_map_.deleteContents();
  output_delay_leaf_pin_map_.deleteContents();

  clk_hpin_disables_.deleteContentsClear();
  clk_hpin_disables_valid_ = false;

  clearCycleAcctings();
  deleteExceptions();
  clearGroupPathMap();
  deleteInstancePvts();
  deleteDeratingFactors();
  removeLoadCaps();
  clk_sense_map_.clear();
}

void
Sdc::deleteInstancePvts()
{
  // Multiple instances can share a pvt, so put them in a set
  // so they are only deleted once.
  PvtSet pvts;
  for (auto mm_index : MinMax::rangeIndex()) {
    InstancePvtMap *pvt_map = instance_pvt_maps_[mm_index];
    InstancePvtMap::Iterator pvt_iter(pvt_map);
    while (pvt_iter.hasNext()) {
      Pvt *pvt = pvt_iter.next();
      pvts.insert(pvt);
    }
    delete pvt_map;
  }
  pvts.deleteContents();
}

void
Sdc::removeNetLoadCaps()
{
  delete [] net_wire_cap_map_;
  net_wire_cap_map_ = nullptr;

  delete [] drvr_pin_wire_cap_map_;
  drvr_pin_wire_cap_map_ = nullptr;
}

void
Sdc::removeLoadCaps()
{
  if (port_cap_map_) {
    port_cap_map_->deleteContents();
    delete port_cap_map_;
    port_cap_map_ = nullptr;
  }
  removeNetLoadCaps();
}

void
Sdc::removeLibertyAnnotations()
{
  DisabledCellPortsMap::Iterator disabled_iter(disabled_cell_ports_);
  while (disabled_iter.hasNext()) {
    DisabledCellPorts *disable = disabled_iter.next();
    LibertyCell *cell = disable->cell();
    if (disable->all())
      cell->setIsDisabledConstraint(false);

    LibertyPortSet::Iterator from_iter(disable->from());
    while (from_iter.hasNext()) {
      LibertyPort *from = from_iter.next();
      from->setIsDisabledConstraint(false);
    }

    LibertyPortSet::Iterator to_iter(disable->to());
    while (to_iter.hasNext()) {
      LibertyPort *to = to_iter.next();
      to->setIsDisabledConstraint(false);
    }

    if (disable->timingArcSets()) {
      TimingArcSetSet::Iterator arc_iter(disable->timingArcSets());
      while (arc_iter.hasNext()) {
	TimingArcSet *arc_set = arc_iter.next();
	arc_set->setIsDisabledConstraint(false);
      }
    }

    LibertyPortPairSet::Iterator from_to_iter(disable->fromTo());
    while (from_to_iter.hasNext()) {
      LibertyPortPair *pair = from_to_iter.next();
      const LibertyPort *from = pair->first;
      const LibertyPort *to = pair->second;
      LibertyCellTimingArcSetIterator arc_iter(cell, from, to);
      while (arc_iter.hasNext()) {
	TimingArcSet *arc_set = arc_iter.next();
	arc_set->setIsDisabledConstraint(false);
      }
    }
  }

  LibertyPortSet::Iterator port_iter(disabled_lib_ports_);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    port->setIsDisabledConstraint(false);
  }
}

void
Sdc::initInstancePvtMaps()
{
  for (auto mm_index : MinMax::rangeIndex())
    instance_pvt_maps_[mm_index] = nullptr;
}

////////////////////////////////////////////////////////////////

void
Sdc::searchPreamble()
{
  ensureClkHpinDisables();
  ensureClkGroupExclusions();
}

////////////////////////////////////////////////////////////////

bool
Sdc::isConstrained(const Pin *pin) const
{
  Pin *pin1 = const_cast<Pin*>(pin);
  Port *port = network_->isTopLevelPort(pin) ? network_->port(pin) : nullptr;
  return clock_pin_map_.hasKey(pin)
    || propagated_clk_pins_.hasKey(pin1)
    || hasClockLatency(pin)
    || hasClockInsertion(pin)
    || pin_clk_uncertainty_map_.hasKey(pin)
    || pin_clk_gating_check_map_.hasKey(pin)
    || data_checks_from_map_.hasKey(pin)
    || data_checks_to_map_.hasKey(pin)
    || input_delay_pin_map_.hasKey(pin)
    || output_delay_pin_map_.hasKey(pin)
    || port_slew_limit_map_.hasKey(port)
    || pin_cap_limit_map_.hasKey(pin1)
    || port_cap_limit_map_.hasKey(port)
    || port_fanout_limit_map_.hasKey(port)
    || hasPortExtCap(port)
    || disabled_pins_.hasKey(pin1)
    || disabled_ports_.hasKey(port)
    || disabled_clk_gating_checks_pin_.hasKey(pin1)
    || (first_from_pin_exceptions_
	&& first_from_pin_exceptions_->hasKey(pin))
    || (first_thru_pin_exceptions_
	&& first_thru_pin_exceptions_->hasKey(pin))
    || (first_to_pin_exceptions_
	&& first_to_pin_exceptions_->hasKey(pin))
    || input_drive_map_.hasKey(port)
    || logic_value_map_.hasKey(pin)
    || case_value_map_.hasKey(pin)
    || pin_latch_borrow_limit_map_.hasKey(pin)
    || pin_min_pulse_width_map_.hasKey(pin);
}

bool
Sdc::isConstrained(const Instance *inst) const
{
  Instance *inst1 = const_cast<Instance*>(inst);
  return (instance_pvt_maps_[MinMax::minIndex()]
	  && instance_pvt_maps_[MinMax::minIndex()]->hasKey(inst1))
    || (instance_pvt_maps_[MinMax::maxIndex()]
	  && instance_pvt_maps_[MinMax::maxIndex()]->hasKey(inst1))
    || (inst_derating_factors_
	&& inst_derating_factors_->hasKey(inst))
    || inst_clk_gating_check_map_.hasKey(inst)
    || disabled_inst_ports_.hasKey(inst1)
    || (first_from_inst_exceptions_
	&& first_from_inst_exceptions_->hasKey(inst))
    || (first_thru_inst_exceptions_
	&& first_thru_inst_exceptions_->hasKey(inst))
    || (first_to_inst_exceptions_->hasKey(inst)
	&& first_to_inst_exceptions_)
    || inst_latch_borrow_limit_map_.hasKey(inst)
    || inst_min_pulse_width_map_.hasKey(inst);
}

bool
Sdc::isConstrained(const Net *net) const
{
  Net *net1 = const_cast<Net*>(net);
  return (net_derating_factors_
	  && net_derating_factors_->hasKey(net))
    || hasNetWireCap(net1)
    || net_res_map_.hasKey(net1)
    || (first_thru_net_exceptions_
	&& first_thru_net_exceptions_->hasKey(net));
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

Pvt *
Sdc::pvt(Instance *inst,
	 const MinMax *min_max) const
{
  InstancePvtMap *pvt_map = instance_pvt_maps_[min_max->index()];
  if (pvt_map)
    return pvt_map->findKey(inst);
  else
    return nullptr;
}

void
Sdc::setPvt(Instance *inst, const
	    MinMaxAll *min_max,
	    Pvt *pvt)
{
  for (auto mm_index : min_max->rangeIndex()) {
    InstancePvtMap *pvt_map = instance_pvt_maps_[mm_index];
    if (pvt_map == nullptr) {
      pvt_map = new InstancePvtMap;
      instance_pvt_maps_[mm_index] = pvt_map;
    }
    (*pvt_map)[inst] = pvt;
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
  if (net_derating_factors_ == nullptr)
    net_derating_factors_ = new NetDeratingFactorsMap;
  DeratingFactorsNet *factors = net_derating_factors_->findKey(net);
  if (factors == nullptr) {
    factors = new DeratingFactorsNet;
    (*net_derating_factors_)[net] = factors;
  }
  factors->setFactor(clk_data, rf, early_late, derate);
}

void
Sdc::setTimingDerate(const Instance *inst,
		     TimingDerateType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  if (inst_derating_factors_ == nullptr)
    inst_derating_factors_ = new InstDeratingFactorsMap;
  DeratingFactorsCell *factors = inst_derating_factors_->findKey(inst);
  if (factors == nullptr) {
    factors = new DeratingFactorsCell;
    (*inst_derating_factors_)[inst] = factors;
  }
  factors->setFactor(type, clk_data, rf, early_late, derate);
}

void
Sdc::setTimingDerate(const LibertyCell *cell,
		     TimingDerateType type,
		     PathClkOrData clk_data,
		     const RiseFallBoth *rf,
		     const EarlyLate *early_late,
		     float derate)
{
  if (cell_derating_factors_ == nullptr)
    cell_derating_factors_ = new CellDeratingFactorsMap;
  DeratingFactorsCell *factors = cell_derating_factors_->findKey(cell);
  if (factors == nullptr) {
    factors = new DeratingFactorsCell;
    (*cell_derating_factors_)[cell] = factors;
  }
  factors->setFactor(type, clk_data, rf, early_late, derate);
}

float
Sdc::timingDerateInstance(const Pin *pin,
			  TimingDerateType type,
			  PathClkOrData clk_data,
			  const RiseFall *rf,
			  const EarlyLate *early_late) const
{
  if (inst_derating_factors_) {
    const Instance *inst = network_->instance(pin);
    DeratingFactorsCell *factors = inst_derating_factors_->findKey(inst);
    if (factors) {
      float factor;
      bool exists;
      factors->factor(type, clk_data, rf, early_late, factor, exists);
      if (exists)
	return factor;
    }
  }

  if (cell_derating_factors_) {
    const Instance *inst = network_->instance(pin);
    const LibertyCell *cell = network_->libertyCell(inst);
    if (cell) {
      DeratingFactorsCell *factors = cell_derating_factors_->findKey(cell);
      float factor;
      bool exists;
      if (factors) {
	factors->factor(type, clk_data, rf, early_late, factor, exists);
	if (exists)
	  return factor;
      }
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
  if (net_derating_factors_) {
    const Net *net = network_->net(pin);
    DeratingFactorsNet *factors = net_derating_factors_->findKey(net);
    if (factors) {
      float factor;
      bool exists;
      factors->factor(clk_data, rf, early_late, factor, exists);
      if (exists)
	return factor;
    }
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
Sdc::deleteDeratingFactors()
{
  if (net_derating_factors_) {
    NetDeratingFactorsMap::Iterator net_iter(net_derating_factors_);
    while (net_iter.hasNext()) {
      DeratingFactorsNet *factors = net_iter.next();
      delete factors;
    }
    delete net_derating_factors_;
    net_derating_factors_ = nullptr;
  }

  if (inst_derating_factors_) {
    InstDeratingFactorsMap::Iterator inst_iter(inst_derating_factors_);
    while (inst_iter.hasNext()) {
      DeratingFactorsCell *factors = inst_iter.next();
      delete factors;
    }
    delete inst_derating_factors_;
    inst_derating_factors_ = nullptr;
  }

  if (cell_derating_factors_) {
    CellDeratingFactorsMap::Iterator cell_iter(cell_derating_factors_);
    while (cell_iter.hasNext()) {
      DeratingFactorsCell *factors = cell_iter.next();
      delete factors;
    }
    delete cell_derating_factors_;
    cell_derating_factors_ = nullptr;
  }

  delete derating_factors_;
  derating_factors_ = nullptr;
}

////////////////////////////////////////////////////////////////

void
Sdc::setDriveCell(LibertyLibrary *library,
		  LibertyCell *cell,
		  Port *port,
		  LibertyPort *from_port,
		  float *from_slews,
		  LibertyPort *to_port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max)
{
  ensureInputDrive(port)->setDriveCell(library, cell, from_port, from_slews,
				       to_port, rf, min_max);
}

void
Sdc::setInputSlew(Port *port,
		  const RiseFallBoth *rf,
		  const MinMaxAll *min_max,
		  float slew)
{
  ensureInputDrive(port)->setSlew(rf, min_max, slew);
}

void
Sdc::setDriveResistance(Port *port,
			const RiseFallBoth *rf,
			const MinMaxAll *min_max,
			float res)
{
  ensureInputDrive(port)->setDriveResistance(rf, min_max, res);
}

InputDrive *
Sdc::ensureInputDrive(Port *port)
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
Sdc::slewLimit(Clock *clk, const RiseFall *rf,
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
  slew = 0.0;
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
Sdc::slewLimit(const Pin *pin,
	       const MinMax *min_max,
	       float &slew,
	       bool &exists)
{
  slew = 0.0;
  MinMaxFloatValues values;
  pin_slew_limit_map_.findKey(pin, values, exists);
  if (exists)
    values.value(min_max, slew, exists);
}

void
Sdc::setSlewLimit(const Pin *pin,
		  const MinMax *min_max,
		  float slew)
{
  MinMaxFloatValues &values = pin_slew_limit_map_[pin];
  values.setValue(min_max, slew);
}

void
Sdc::slewLimitPins(ConstPinSeq &pins)
{
  PinSlewLimitMap::Iterator iter(pin_slew_limit_map_);
  while (iter.hasNext()) {
    const Pin *pin;
    MinMaxFloatValues values;
    iter.next(pin, values);
    pins.push_back(pin);
  }
}

void
Sdc::slewLimit(Cell *cell,
	       const MinMax *min_max,
	       float &slew,
	       bool &exists)
{
  slew = 0.0;
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
  fanout = 0.0;
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
    clk = new Clock(name, clk_index_++);
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
			Pin *pll_out,
			Pin *pll_fdbk,
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
    clk = new Clock(name, clk_index_++);
    clocks_.push_back(clk);
    clock_name_map_[clk->name()] = clk;
  }
  clk->initGeneratedClk(pins, add_to_pins, src_pin, master_clk,
			pll_out, pll_fdbk,
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
  PinSet::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    ClockSet *pin_clks = clock_pin_map_.findKey(pin);
    if (pin_clks) {
      ClockSet::Iterator clk_iter(pin_clks);
      while (clk_iter.hasNext()) {
	Clock *clk = clk_iter.next();
	clks.insert(clk);
      }
    }
  }
  ClockSet::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    deleteClkPinMappings(clk);
    PinSet::Iterator pin_iter(pins);
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      clk->deletePin(pin);
    }
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
  for (Pin *pin : clk->pins()) {
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
  for (Pin *pin : clk->pins()) {
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
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
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

void
Sdc::findClocksMatching(PatternMatch *pattern,
			ClockSeq *clks) const
{
  if (!pattern->hasWildcards()) {
    Clock *clk = findClock(pattern->pattern());
    if (clk)
      clks->push_back(clk);
  }
  else {
    for (auto clk : clocks_) {
      if (pattern->match(clk->name()))
	clks->push_back(clk);
    }
  }
}

ClockIterator *
Sdc::clockIterator()
{
  return new ClockIterator(clocks_);
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

bool
ClkHpinDisableLess::operator()(const ClkHpinDisable *disable1,
			       const ClkHpinDisable *disable2) const
{
  int clk_index1 = disable1->clk()->index();
  int clk_index2 = disable2->clk()->index();
  if (clk_index1 == clk_index2) {
    const Pin *from_pin1 = disable1->fromPin();
    const Pin *from_pin2 = disable2->fromPin();
    if (from_pin1 == from_pin2) {
      const Pin *to_pin1 = disable1->toPin();
      const Pin *to_pin2 = disable2->toPin();
      return to_pin1 < to_pin2;
    }
    else
      return from_pin1 < from_pin2;
  }
  else
    return clk_index1 < clk_index2;
}

class FindClkHpinDisables : public HpinDrvrLoadVisitor
{
public:
  FindClkHpinDisables(Clock *clk,
		      const Network *network,
		      Sdc *sdc);
  ~FindClkHpinDisables();
  bool drvrLoadExists(Pin *drvr,
		      Pin *load);

protected:
  virtual void visit(HpinDrvrLoad *drvr_load);
  void makeClkHpinDisables(Pin *clk_src,
			   Pin *drvr,
			   Pin *load);

  Clock *clk_;
  PinPairSet drvr_loads_;
  const Network *network_;
  Sdc *sdc_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindClkHpinDisables);
};

FindClkHpinDisables::FindClkHpinDisables(Clock *clk,
					 const Network *network,
					 Sdc *sdc) :
  HpinDrvrLoadVisitor(),
  clk_(clk),
  network_(network),
  sdc_(sdc)
{
}

FindClkHpinDisables::~FindClkHpinDisables()
{
  drvr_loads_.deleteContents();
}

void
FindClkHpinDisables::visit(HpinDrvrLoad *drvr_load)
{
  Pin *drvr = drvr_load->drvr();
  Pin *load = drvr_load->load();

  makeClkHpinDisables(drvr, drvr, load);

  PinSet *hpins_from_drvr = drvr_load->hpinsFromDrvr();
  PinSet::Iterator hpin_iter(hpins_from_drvr);
  while (hpin_iter.hasNext()) {
    Pin *hpin = hpin_iter.next();
    makeClkHpinDisables(hpin, drvr, load);
  }
  drvr_loads_.insert(new PinPair(drvr, load));
}

void
FindClkHpinDisables::makeClkHpinDisables(Pin *clk_src,
					 Pin *drvr,
					 Pin *load)
{
  ClockSet *clks = sdc_->findClocks(clk_src);
  ClockSet::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    if (clk != clk_)
      // Do not propagate clock from source pin if another
      // clock is defined on a hierarchical pin between the
      // driver and load.
      sdc_->makeClkHpinDisable(clk, drvr, load);
  }
}

bool
FindClkHpinDisables::drvrLoadExists(Pin *drvr,
				    Pin *load)
{
  PinPair probe(drvr, load);
  return drvr_loads_.hasKey(&probe);
}

void
Sdc::makeClkHpinDisable(Clock *clk,
			Pin *drvr,
			Pin *load)
{
  ClkHpinDisable probe(clk, drvr, load);
  if (!clk_hpin_disables_.hasKey(&probe)) {
    ClkHpinDisable *disable = new ClkHpinDisable(clk, drvr, load);
    clk_hpin_disables_.insert(disable);
  }
}

void
Sdc::ensureClkHpinDisables()
{
  if (!clk_hpin_disables_valid_) {
    clk_hpin_disables_.deleteContentsClear();
    for (auto clk : clocks_) {
      for (Pin *src : clk->pins()) {
	if (network_->isHierarchical(src)) {
	  FindClkHpinDisables visitor(clk, network_, this);
	  visitHpinDrvrLoads(src, network_, &visitor);
	  // Disable fanouts from the src driver pins that do
	  // not go thru the hierarchical src pin.
	  for (Pin *lpin : clk->leafPins()) {
	    Vertex *vertex, *bidirect_drvr_vertex;
	    graph_->pinVertices(lpin, vertex, bidirect_drvr_vertex);
	    makeVertexClkHpinDisables(clk, vertex, visitor);
	    if (bidirect_drvr_vertex)
	      makeVertexClkHpinDisables(clk, bidirect_drvr_vertex, visitor);
	  }
	}
      }
    }
    clk_hpin_disables_valid_ = true;
  }
}

void
Sdc::makeVertexClkHpinDisables(Clock *clk,
			       Vertex *vertex,
			       FindClkHpinDisables &visitor)
{
  VertexOutEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->isWire()) {
      Pin *drvr = edge->from(graph_)->pin();
      Pin *load = edge->to(graph_)->pin();
      if (!visitor.drvrLoadExists(drvr, load))
	makeClkHpinDisable(clk, drvr, load);
    }
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
  if (clk->leafPins().hasKey(const_cast<Pin*>(from_pin))) {
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
  return propagated_clk_pins_.hasKey(const_cast<Pin*>(pin));
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
  if (pin && graph_ && network_->isHierarchical(pin))
    annotateHierClkLatency(pin, latency);

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
  const Pin *pin = latency->pin();
  if (pin && graph_ && network_->isHierarchical(pin))
    deannotateHierClkLatency(pin);
  clk_latencies_.erase(latency);
  delete latency;
}

void
Sdc::deleteClockLatenciesReferencing(Clock *clk)
{
  ClockLatencies::Iterator latency_iter(clk_latencies_);
  while (latency_iter.hasNext()) {
    ClockLatency *latency = latency_iter.next();
    if (latency->clock() == clk)
      deleteClockLatency(latency);
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
  InterClockUncertaintySet::Iterator inter_iter(inter_clk_uncertainties_);
  while (inter_iter.hasNext()) {
    InterClockUncertainty *uncertainties = inter_iter.next();
    if (uncertainties->src() == clk
	|| uncertainties->target() == clk)
      deleteInterClockUncertainty(uncertainties);
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
  if (clk_insertions_ == nullptr)
    clk_insertions_ = new ClockInsertions;
  ClockInsertion probe(clk, pin);
  ClockInsertion *insertion = clk_insertions_->findKey(&probe);
  if (insertion == nullptr) {
    insertion = new ClockInsertion(clk, pin);
    clk_insertions_->insert(insertion);
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
  if (clk_insertions_ == nullptr)
    clk_insertions_ = new ClockInsertions;
  ClockInsertion probe(clk, pin);
  ClockInsertion *insertion = clk_insertions_->findKey(&probe);
  if (insertion == nullptr) {
    insertion = new ClockInsertion(clk, pin);
    clk_insertions_->insert(insertion);
  }
  insertion->setDelay(rf, min_max, early_late, delay);
}

void
Sdc::removeClockInsertion(const Clock *clk,
			  const Pin *pin)
{
  if (clk_insertions_) {
    ClockInsertion probe(clk, pin);
    ClockInsertion *insertion = clk_insertions_->findKey(&probe);
    if (insertion != nullptr)
      deleteClockInsertion(insertion);
  }
}

void
Sdc::deleteClockInsertion(ClockInsertion *insertion)
{
  clk_insertions_->erase(insertion);
  delete insertion;
}

void
Sdc::deleteClockInsertionsReferencing(Clock *clk)
{
  ClockInsertions::Iterator insertion_iter(clk_insertions_);
  while (insertion_iter.hasNext()) {
    ClockInsertion *insertion = insertion_iter.next();
    if (insertion->clock() == clk)
      deleteClockInsertion(insertion);
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
  if (clk_insertions_) {
    ClockInsertion probe(nullptr, pin);
    return clk_insertions_->hasKey(&probe);
  }
  else
    return false;
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
  if (clk_insertions_) {
    if (clk && pin) {
      ClockInsertion probe(clk, pin);
      insert = clk_insertions_->findKey(&probe);
    }
    if (insert == nullptr && pin) {
      ClockInsertion probe(nullptr, pin);
      insert = clk_insertions_->findKey(&probe);
    }
    if (insert == nullptr && clk) {
      ClockInsertion probe(clk, nullptr);
      insert = clk_insertions_->findKey(&probe);
    }
  }
  if (insert)
    insert->delay(rf, min_max, early_late, insertion, exists);
  else {
    insertion = 0.0;
    exists = false;
  }
}

////////////////////////////////////////////////////////////////

bool
ClockLatencyPinClkLess::operator()(const ClockLatency *latency1,
				   const ClockLatency *latency2) const
{
  const Clock *clk1 = latency1->clock();
  const Clock *clk2 = latency2->clock();
  const Pin *pin1 = latency1->pin();
  const Pin *pin2 = latency2->pin();
  return clk1 < clk2
    || (clk1 == clk2
	&& pin1 < pin2);
}

////////////////////////////////////////////////////////////////

bool
ClockInsertionPinClkLess::operator()(const ClockInsertion *insert1,
				     const ClockInsertion *insert2)const
{
  const Clock *clk1 = insert1->clock();
  const Clock *clk2 = insert2->clock();
  const Pin *pin1 = insert1->pin();
  const Pin *pin2 = insert2->pin();
  return (clk1 && clk2
	  && (clk1 < clk2
	      || (clk1 == clk2
		  && pin1 && pin2
		  && pin1 < pin2)))
    || (clk1 == nullptr && clk2)
    || (clk1 == nullptr && clk2 == nullptr
	&& ((pin1 && pin2
	     && pin1 < pin2)
	    || (pin1 == nullptr && pin2)));
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

ClockGroupIterator *
Sdc::clockGroupIterator()
{
  return new ClockGroupIterator(clk_groups_name_map_);
}

void
Sdc::ensureClkGroupExclusions()
{
  if (clk_group_exclusions_ == nullptr) {
    clk_group_exclusions_ = new ClockPairSet;
    clk_group_same_ = new ClockPairSet;
    for (auto name_clk_groups : clk_groups_name_map_)
      makeClkGroupExclusions(name_clk_groups.second);
  }
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
  ClockSet *clks1 = group1->clks();
  for (auto clk1 : *clks1) {
    for (Clock *clk2 : clocks_) {
      if (clk2 != clk1
	  && !group1->isMember(clk2))
	clk_group_exclusions_->insert(ClockPair(clk1, clk2));
    }
  }
  makeClkGroupSame(group1);
}

void
Sdc::makeClkGroupExclusions(ClockGroupSet *groups)
{
  for (auto group1 : *groups) {
    ClockSet *clks1 = group1->clks();
    for (auto group2 : *groups) {
      if (group1 != group2) {
	ClockSet *clks2 = group2->clks();
	for (auto clk1 : *clks1) {
	  for (auto clk2 : *clks2) {
	    // ClockPair is symmetric so only add one clk1/clk2 pair.
	    if (clk1->index() < clk2->index()) {
	      clk_group_exclusions_->insert(ClockPair(clk1, clk2));
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
  ClockSet *clks = group->clks();
  for (auto clk1 : *clks) {
    for (auto clk2 : *clks) {
      if (clk1->index() <= clk2->index()) {
	ClockPair clk_pair(clk1, clk2);
	if (!clk_group_same_->hasKey(clk_pair))
	  clk_group_same_->insert(clk_pair);
      }
    }
  }
}

void
Sdc::clearClkGroupExclusions()
{
  if (clk_group_exclusions_) {
    delete clk_group_exclusions_;
    delete clk_group_same_;
    clk_group_exclusions_ = nullptr;
    clk_group_same_ = nullptr;
  }
}

bool
Sdc::sameClockGroup(const Clock *clk1,
		    const Clock *clk2)
{
  if (clk1 && clk2) {
    ClockPair clk_pair(clk1, clk2);
    bool excluded = clk_group_exclusions_->hasKey(clk_pair);
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
  return clk_group_same_->hasKey(clk_pair);
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
    if (groups->logicallyExclusive())
      removeClockGroups(groups);
  }
  else {
    ClockGroupsNameMap::Iterator groups_iter(clk_groups_name_map_);
    while (groups_iter.hasNext()) {
      ClockGroups *groups = groups_iter.next();
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
    if (groups->physicallyExclusive())
      removeClockGroups(groups);
  }
  else {
    ClockGroupsNameMap::Iterator groups_iter(clk_groups_name_map_);
    while (groups_iter.hasNext()) {
      ClockGroups *groups = groups_iter.next();
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
    if (groups->asynchronous())
      removeClockGroups(groups);
  }
  else {
    ClockGroupsNameMap::Iterator groups_iter(clk_groups_name_map_);
    while (groups_iter.hasNext()) {
      ClockGroups *groups = groups_iter.next();
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
  ClockGroupsNameMap::Iterator groups_iter(clk_groups_name_map_);
  while (groups_iter.hasNext()) {
    ClockGroups *groups = groups_iter.next();
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
  PinSet::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    if (clks) {
      ClockSet::Iterator clk_iter(clks);
      while (clk_iter.hasNext()) {
	Clock *clk = clk_iter.next();
	setClockSense(pin, clk, sense);
      }
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
  if (src == nullptr)
    src = tgt;
  CycleAccting *acct;
  CycleAccting probe(src, tgt);
  acct = cycle_acctings_.findKey(&probe);
  if (acct == nullptr) {
    UniqueLock lock(cycle_acctings_lock_);
    // Recheck with lock.
    acct = cycle_acctings_.findKey(&probe);
    if (acct == nullptr) {
      acct = new CycleAccting(src, tgt);
      if (src == defaultArrivalClockEdge())
	acct->findDefaultArrivalSrcDelays();
      else
	acct->findDelays(this);
      cycle_acctings_.insert(acct);
    }
  }
  return acct;
}

void
Sdc::reportClkToClkMaxCycleWarnings()
{
  // Find cycle acctings that exceed max cycle count.  Eliminate
  // duplicate warnings between different src/tgt clk edges.
  ClockPairSet clk_warnings;
  ClockPairSeq clk_warnings2;
  CycleAcctingSet::Iterator acct_iter(cycle_acctings_);
  while (acct_iter.hasNext()) {
    CycleAccting *acct = acct_iter.next();
    if (acct->maxCyclesExceeded()) {
      Clock *src = acct->src()->clock();
      Clock *tgt = acct->target()->clock();
      // Canonicalize the warning wrt src/tgt.
      if (src->index() > tgt->index())
	std::swap(src, tgt);
      ClockPair clk_pair(src, tgt);
      if (!clk_warnings.hasKey(clk_pair)) {
	clk_warnings.insert(clk_pair);
	clk_warnings2.push_back(clk_pair);
      }
    }
  }

  // Sort clk pairs so that results are stable.
  sort(clk_warnings2, ClockPairLess());

  for (auto pair : clk_warnings2) {
    report_->warn("No common period was found between clocks %s and %s.\n",
		  pair.first->name(),
		  pair.second->name());
  }
}

void
Sdc::clearCycleAcctings()
{
  cycle_acctings_.deleteContentsClear();
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

  if (graph_)
    annotateGraphConstrained(to, true);
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
Sdc::setLatchBorrowLimit(Pin *pin,
			 float limit)
{
  pin_latch_borrow_limit_map_[pin] = limit;
}

void
Sdc::setLatchBorrowLimit(Instance *inst,
			 float limit)
{
  inst_latch_borrow_limit_map_[inst] = limit;
}

void
Sdc::setLatchBorrowLimit(Clock *clk,
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
Sdc::latchBorrowLimit(Pin *data_pin,
		      Pin *enable_pin,
		      Clock *clk,
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
Sdc::setInputDelay(Pin *pin,
		   const RiseFallBoth *rf,
		   Clock *clk,
		   const RiseFall *clk_rf,
		   Pin *ref_pin,
		   bool source_latency_included,
		   bool network_latency_included,
		   const MinMaxAll *min_max,
		   bool add,
		   float delay)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  InputDelay *input_delay = findInputDelay(pin, clk_edge, ref_pin);
  if (input_delay == nullptr)
    input_delay = makeInputDelay(pin, clk_edge, ref_pin);
  if (add) {
    RiseFallMinMax *delays = input_delay->delays();
    delays->mergeValue(rf, min_max, delay);
  }
  else {
    deleteInputDelays(pin, input_delay);
    RiseFallMinMax *delays = input_delay->delays();
    delays->setValue(rf, min_max, delay);
  }
  input_delay->setSourceLatencyIncluded(source_latency_included);
  input_delay->setNetworkLatencyIncluded(network_latency_included);
}

InputDelay *
Sdc::makeInputDelay(Pin *pin,
		    ClockEdge *clk_edge,
		    Pin *ref_pin)
{
  InputDelay *input_delay = new InputDelay(pin, clk_edge, ref_pin,
					   input_delay_index_++,
					   network_);
  input_delays_.insert(input_delay);
  InputDelaySet *inputs = input_delay_pin_map_.findKey(pin);
  if (inputs == nullptr) {
    inputs = new InputDelaySet;
    input_delay_pin_map_[pin] = inputs;
  }
  inputs->insert(input_delay);

  if (ref_pin) {
    InputDelaySet *ref_inputs = input_delay_ref_pin_map_.findKey(ref_pin);
    if (ref_inputs == nullptr) {
      ref_inputs = new InputDelaySet;
      input_delay_ref_pin_map_[ref_pin] = ref_inputs;
    }
    ref_inputs->insert(input_delay);
  }

  for (Pin *lpin : input_delay->leafPins()) {
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
		    ClockEdge *clk_edge,
		    Pin *ref_pin)
{
  InputDelaySet *inputs = input_delay_pin_map_.findKey(pin);
  if (inputs) {
    for (InputDelay *input_delay : *inputs) {
      if (input_delay->clkEdge() == clk_edge
	  && input_delay->refPin() == ref_pin)
	return input_delay;
    }
  }
  return nullptr;
}

void
Sdc::removeInputDelay(Pin *pin,
		      RiseFallBoth *rf,
		      Clock *clk,
		      RiseFall *clk_rf,
		      MinMaxAll *min_max)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  InputDelay *input_delay = findInputDelay(pin, clk_edge, nullptr);
  if (input_delay) {
    RiseFallMinMax *delays = input_delay->delays();
    delays->removeValue(rf, min_max);
    if (delays->empty())
      deleteInputDelay(input_delay);
  }
}

void
Sdc::deleteInputDelays(Pin *pin,
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
Sdc::deleteInputDelaysReferencing(Clock *clk)
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

  Pin *pin = input_delay->pin();
  InputDelaySet *inputs = input_delay_pin_map_[pin];
  inputs->erase(input_delay);

  for (Pin *lpin : input_delay->leafPins()) {
    InputDelaySet *inputs = input_delay_leaf_pin_map_[lpin];
    inputs->erase(input_delay);
  }

  delete input_delay;
}

////////////////////////////////////////////////////////////////

void
Sdc::setOutputDelay(Pin *pin,
		    const RiseFallBoth *rf,
		    Clock *clk,
		    const RiseFall *clk_rf,
		    Pin *ref_pin,
		    bool source_latency_included,
		    bool network_latency_included,
		    const MinMaxAll *min_max,
		    bool add,
		    float delay)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  OutputDelay *output_delay = findOutputDelay(pin, clk_edge, ref_pin);
  if (output_delay == nullptr)
    output_delay = makeOutputDelay(pin, clk_edge, ref_pin);
  if (add) {
    RiseFallMinMax *delays = output_delay->delays();
    delays->mergeValue(rf, min_max, delay);
  }
  else {
    deleteOutputDelays(pin, output_delay);
    RiseFallMinMax *delays = output_delay->delays();
    delays->setValue(rf, min_max, delay);
  }
  output_delay->setSourceLatencyIncluded(source_latency_included);
  output_delay->setNetworkLatencyIncluded(network_latency_included);
}

OutputDelay *
Sdc::findOutputDelay(const Pin *pin,
		     ClockEdge *clk_edge,
		     Pin *ref_pin)
{
  OutputDelaySet *outputs = output_delay_pin_map_.findKey(pin);
  if (outputs) {
    for (OutputDelay *output_delay : *outputs) {
      if (output_delay->clkEdge() == clk_edge
	  && output_delay->refPin() == ref_pin)
	return output_delay;
    }
  }
  return nullptr;
}

OutputDelay *
Sdc::makeOutputDelay(Pin *pin,
		     ClockEdge *clk_edge,
		     Pin *ref_pin)
{
  OutputDelay *output_delay = new OutputDelay(pin, clk_edge, ref_pin,
					      network_);
  output_delays_.insert(output_delay);
  OutputDelaySet *outputs = output_delay_pin_map_.findKey(pin);
  if (outputs == nullptr) {
    outputs = new OutputDelaySet;
    output_delay_pin_map_[pin] = outputs;
  }
  outputs->insert(output_delay);

  if (ref_pin) {
    OutputDelaySet *ref_outputs = output_delay_ref_pin_map_.findKey(ref_pin);
    if (ref_outputs == nullptr) {
      ref_outputs = new OutputDelaySet;
      output_delay_ref_pin_map_[ref_pin] = ref_outputs;
    }
    ref_outputs->insert(output_delay);
  }

  for (Pin *lpin : output_delay->leafPins()) {
    OutputDelaySet *leaf_outputs = output_delay_leaf_pin_map_[lpin];
    if (leaf_outputs == nullptr) {
      leaf_outputs = new OutputDelaySet;
      output_delay_leaf_pin_map_[lpin] = leaf_outputs;
    }
    leaf_outputs->insert(output_delay);
    if (graph_)
      annotateGraphConstrained(lpin, true);
  }
  return output_delay;
}

void
Sdc::removeOutputDelay(Pin *pin,
		       RiseFallBoth *rf,
		       Clock *clk,
		       RiseFall *clk_rf,
		       MinMaxAll *min_max)
{
  ClockEdge *clk_edge = clk ? clk->edge(clk_rf) : nullptr;
  OutputDelay *output_delay = findOutputDelay(pin, clk_edge, nullptr);
  if (output_delay) {
    RiseFallMinMax *delays = output_delay->delays();
    delays->removeValue(rf, min_max);
  }
}

void
Sdc::deleteOutputDelays(Pin *pin,
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
Sdc::deleteOutputDelaysReferencing(Clock *clk)
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

  Pin *pin = output_delay->pin();
 OutputDelaySet *outputs = output_delay_pin_map_[pin];
  outputs->erase(output_delay);

  for (Pin *lpin : output_delay->leafPins()) {
    OutputDelaySet *outputs = output_delay_leaf_pin_map_[lpin];
    outputs->erase(output_delay);
  }

  delete output_delay;
}

////////////////////////////////////////////////////////////////

void
Sdc::setPortExtPinCap(Port *port,
		      const RiseFall *rf,
		      const MinMax *min_max,
		      float cap)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port);
  port_cap->setPinCap(cap, rf, min_max);
}

void
Sdc::setPortExtWireCap(Port *port,
		       bool subtract_pin_cap,
		       const RiseFall *rf,
		       const Corner *corner,
		       const MinMax *min_max,
		       float cap)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port);
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
Sdc::portExtCap(Port *port) const
{
  if (port_cap_map_)
    return port_cap_map_->findKey(port);
  else
    return nullptr;
}

bool
Sdc::hasPortExtCap(Port *port) const
{
  if (port_cap_map_)
    return port_cap_map_->hasKey(port);
  else
    return false;
}

void
Sdc::portExtCap(Port *port,
		const RiseFall *rf,
		const MinMax *min_max,
		// Return values.
		float &pin_cap,
		bool &has_pin_cap,
		float &wire_cap,
		bool &has_wire_cap,
		int &fanout,
		bool &has_fanout) const
{
  if (port_cap_map_) {
    PortExtCap *port_cap = port_cap_map_->findKey(port);
    if (port_cap) {
      port_cap->pinCap(rf, min_max, pin_cap, has_pin_cap);
      port_cap->wireCap(rf, min_max, wire_cap, has_wire_cap);
      port_cap->fanout(min_max, fanout, has_fanout);
      return;
    }
  }
  pin_cap = 0.0F;
  has_pin_cap = false;
  wire_cap = 0.0F;
  has_wire_cap = false;
  fanout = 0.0F;
  has_fanout = false;
}

float
Sdc::portExtCap(Port *port,
		const RiseFall *rf,
		const MinMax *min_max) const
{
  float pin_cap, wire_cap;
  int fanout;
  bool has_pin_cap, has_wire_cap, has_fanout;
  portExtCap(port, rf, min_max,
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
Sdc::drvrPinHasWireCap(const Pin *pin)
{
  return drvr_pin_wire_cap_map_
    && drvr_pin_wire_cap_map_->hasKey(const_cast<Pin*>(pin));
}

void
Sdc::drvrPinWireCap(const Pin *pin,
		    const Corner *corner,
		    const MinMax *min_max,
		    // Return values.
		    float &cap,
		    bool &exists) const
{
  if (drvr_pin_wire_cap_map_) {
    MinMaxFloatValues *values =
      drvr_pin_wire_cap_map_[corner->index()].findKey(const_cast<Pin*>(pin));
    if (values)
      return values->value(min_max, cap, exists);
  }
  cap = 0.0;
  exists = false;
}

void
Sdc::setNetWireCap(Net *net,
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
      Pin *pin = pin_iter->next();
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
  if (net_wire_cap_map_ == nullptr)
    net_wire_cap_map_ = new NetWireCapMap[corners_->count()];
  bool make_drvr_entry = !net_wire_cap_map_[corner->index()].hasKey(net);
  MinMaxFloatValues &values = net_wire_cap_map_[corner->index()][net];
  values.setValue(min_max, wire_cap);

  // Only need to do this when there is new net_wire_cap_map_ entry.
  if (make_drvr_entry) {
    for (Pin *pin : *network_->drivers(net)) {
      if (drvr_pin_wire_cap_map_ == nullptr)
	drvr_pin_wire_cap_map_ = new PinWireCapMap[corners_->count()];
      drvr_pin_wire_cap_map_[corner->index()][pin] = &values;
    }
  }
}

bool
Sdc::hasNetWireCap(Net *net) const
{
  if (net_wire_cap_map_) {
    for (int i = 0; i < corners_->count(); i++) {
      if (net_wire_cap_map_[i].hasKey(net))
	return true;
    }
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
		  bool &has_set_load) const
{
  netCaps(pin, rf, op_cond, corner, min_max,
	  pin_cap, wire_cap, fanout, has_set_load);
  float net_wire_cap;
  bool has_net_wire_cap;
  drvrPinWireCap(pin, corner, min_max, net_wire_cap, has_net_wire_cap);
  if (has_net_wire_cap) {
    wire_cap += net_wire_cap;
    has_set_load = true;
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
  bool has_set_load;
  connectedCap(pin, rf, op_cond, corner, min_max,
	       pin_cap, wire_cap, fanout, has_set_load);
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
	      bool &has_set_load,
	      const Sdc *sdc);
  virtual void operator()(Pin *pin);

protected:
  const RiseFall *rf_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *min_max_;
  float &pin_cap_;
  float &wire_cap_;
  float &fanout_;
  bool &has_set_load_;
  const Sdc *sdc_;

private:
  DISALLOW_COPY_AND_ASSIGN(FindNetCaps);
};

FindNetCaps::FindNetCaps(const RiseFall *rf,
			 const OperatingConditions *op_cond,
			 const Corner *corner,
			 const MinMax *min_max,
			 float &pin_cap,
			 float &wire_cap,
			 float &fanout,
			 bool &has_set_load,
			 const Sdc *sdc) :
  PinVisitor(),
  rf_(rf),
  op_cond_(op_cond),
  corner_(corner),
  min_max_(min_max),
  pin_cap_(pin_cap),
  wire_cap_(wire_cap),
  fanout_(fanout),
  has_set_load_(has_set_load),
  sdc_(sdc)
{
}

void
FindNetCaps::operator()(Pin *pin)
{
  sdc_->pinCaps(pin, rf_, op_cond_, corner_, min_max_,
		pin_cap_, wire_cap_, fanout_, has_set_load_);
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
	     bool &has_set_load) const
{
  pin_cap = 0.0;
  wire_cap = 0.0;
  fanout = 0.0;
  has_set_load = false;
  FindNetCaps visitor(rf, op_cond, corner, min_max, pin_cap,
		      wire_cap, fanout, has_set_load, this);
  network_->visitConnectedPins(const_cast<Pin*>(drvr_pin), visitor);
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
	     float &fanout,
	     bool &has_set_load) const
{
  if (network_->isTopLevelPort(pin)) {
    Port *port = network_->port(pin);
    bool is_output = network_->direction(port)->isAnyOutput();
    float port_pin_cap, port_wire_cap;
    int port_fanout;
    bool has_pin_cap, has_wire_cap, has_fanout;
    portExtCap(port, rf, min_max,
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
    has_set_load |= has_pin_cap || has_wire_cap;
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
  Pvt *inst_pvt = nullptr;
  if (inst)
    inst_pvt = pvt(inst, min_max);
  LibertyPort *corner_port = port->cornerPort(corner->libertyIndex(min_max));
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
Sdc::setResistance(Net *net,
		   const MinMaxAll *min_max,
		   float res)
{
  MinMaxFloatValues &values = net_res_map_[net];
  values.setValue(min_max, res);
}

void
Sdc::resistance(Net *net,
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
Sdc::setPortExtFanout(Port *port,
		      const MinMax *min_max,
		      int fanout)
{
  PortExtCap *port_cap = ensurePortExtPinCap(port);
  port_cap->setFanout(fanout, min_max);
}

void
Sdc::portExtFanout(Port *port,
		   const MinMax *min_max,
		   // Return values.
		   int &fanout,
		   bool &exists)
{
  PortExtCap *port_cap = portExtCap(port);
  if (port_cap)
    port_cap->fanout(min_max, fanout, exists);
  else {
    fanout = 0.0;
    exists = false;
  }
}

int
Sdc::portExtFanout(Port *port,
		   const MinMax *min_max)
{
  int fanout;
  bool exists;
  portExtFanout(port, min_max, fanout, exists);
  if (exists)
    return fanout;
  else
    return 0.0;
}

PortExtCap *
Sdc::ensurePortExtPinCap(Port *port)
{
  if (port_cap_map_ == nullptr)
    port_cap_map_ = new PortExtCapMap;
  PortExtCap *port_cap = port_cap_map_->findKey(port);
  if (port_cap == nullptr) {
    port_cap = new PortExtCap(port);
    (*port_cap_map_)[port] = port_cap;
  }
  return port_cap;
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
    LibertyCellTimingArcSetIterator arc_iter(cell, from, to);
    while (arc_iter.hasNext()) {
      TimingArcSet *arc_set = arc_iter.next();
      arc_set->setIsDisabledConstraint(true);
    }
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
      LibertyCellTimingArcSetIterator arc_iter(cell, from, to);
      while (arc_iter.hasNext()) {
	TimingArcSet *arc_set = arc_iter.next();
	arc_set->setIsDisabledConstraint(false);
      }
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
  if (graph_) {
    Pin *pin = network_->findPin(network_->topInstance(), port);
    annotateGraphDisabled(pin, true);
  }
}

void
Sdc::removeDisable(Port *port)
{
  if (graph_) {
    Pin *pin = network_->findPin(network_->topInstance(), port);
    annotateGraphDisabled(pin, false);
  }
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

  if (graph_)
    setEdgeDisabledInstPorts(disabled_inst, true);
}

void
Sdc::removeDisable(Instance *inst,
		   LibertyPort *from,
		   LibertyPort *to)
{
  DisabledInstancePorts *disabled_inst = disabled_inst_ports_.findKey(inst);
  if (disabled_inst) {
    if (graph_)
      setEdgeDisabledInstPorts(disabled_inst, false);
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
  PinPair probe(from, to);
  if (!disabled_wire_edges_.hasKey(&probe)) {
    PinPair *pair = new PinPair(from, to);
    disabled_wire_edges_.insert(pair);
    if (graph_)
      annotateGraphDisabledWireEdge(from, to, true, graph_);
  }
}

void
Sdc::removeDisable(Pin *from,
		   Pin *to)
{
  annotateGraphDisabledWireEdge(from, to, false, graph_);
  PinPair probe(from, to);
  disabled_wire_edges_.erase(&probe);
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
  virtual void visit(Pin *drvr,
		     Pin *load);

  PinPairSet *pairs_;
  Graph *graph_;

private:
  DISALLOW_COPY_AND_ASSIGN(DisableEdgesThruHierPin);
};

DisableEdgesThruHierPin::DisableEdgesThruHierPin(PinPairSet *pairs,
						 Graph *graph) :
  HierPinThruVisitor(),
  pairs_(pairs),
  graph_(graph)
{
}

void
DisableEdgesThruHierPin::visit(Pin *drvr,
			       Pin *load)
{
  PinPair probe(drvr, load);
  if (!pairs_->hasKey(&probe)) {
    PinPair *pair = new PinPair(drvr, load);
    pairs_->insert(pair);
    if (graph_)
      annotateGraphDisabledWireEdge(drvr, load, true, graph_);
  }
}

void
Sdc::disable(Pin *pin)
{
  if (network_->isHierarchical(pin)) {
    // Add leaf pins thru hierarchical pin to disabled_edges_.
    DisableEdgesThruHierPin visitor(&disabled_wire_edges_, graph_);
    visitDrvrLoadsThruHierPin(pin, network_, &visitor);
  }
  else {
    disabled_pins_.insert(pin);
    if (graph_)
      annotateGraphDisabled(pin, true);
  }
}

class RemoveDisableEdgesThruHierPin : public HierPinThruVisitor
{
public:
  RemoveDisableEdgesThruHierPin(PinPairSet *pairs, Graph *graph);

protected:
  virtual void visit(Pin *drvr, Pin *load);

  PinPairSet *pairs_;
  Graph *graph_;

private:
  DISALLOW_COPY_AND_ASSIGN(RemoveDisableEdgesThruHierPin);
};

RemoveDisableEdgesThruHierPin::RemoveDisableEdgesThruHierPin(PinPairSet *pairs,
							     Graph *graph) :
  HierPinThruVisitor(),
  pairs_(pairs),
  graph_(graph)
{
}

void
RemoveDisableEdgesThruHierPin::visit(Pin *drvr,
				     Pin *load)
{
  if (graph_)
    annotateGraphDisabledWireEdge(drvr, load, false, graph_);
  PinPair probe(drvr, load);
  PinPair *pair = pairs_->findKey(&probe);
  if (pair) {
    pairs_->erase(pair);
    delete pair;
  }
}

void
Sdc::removeDisable(Pin *pin)
{
  if (network_->isHierarchical(pin)) {
    // Remove leaf pins thru hierarchical pin from disabled_edges_.
    RemoveDisableEdgesThruHierPin visitor(&disabled_wire_edges_, graph_);
    visitDrvrLoadsThruHierPin(pin, network_, &visitor);
  }
  else {
    if (graph_)
      annotateGraphDisabled(pin, false);
    disabled_pins_.erase(pin);
  }
}

bool
Sdc::isDisabled(const Pin *pin) const
{
  Port *port = network_->port(pin);
  LibertyPort *lib_port = network_->libertyPort(pin);
  return disabled_pins_.hasKey(const_cast<Pin*>(pin))
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
    PinPair pair(const_cast<Pin*>(from_pin), const_cast<Pin*>(to_pin));
    return disabled_wire_edges_.hasKey(&pair);
  }
  else {
    LibertyCell *cell = network_->libertyCell(inst);
    LibertyPort *from_port = network_->libertyPort(from_pin);
    LibertyPort *to_port = network_->libertyPort(to_pin);
    DisabledInstancePorts *disabled_inst =
      disabled_inst_ports_.findKey(const_cast<Instance*>(inst));
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

DisabledInstancePortsMap *
Sdc::disabledInstancePorts()
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
  return disabled_clk_gating_checks_inst_.hasKey(const_cast<Instance*>(inst));
}

bool
Sdc::isDisableClockGatingCheck(const Pin *pin)
{
  return disabled_clk_gating_checks_pin_.hasKey(const_cast<Pin*>(pin));
}

////////////////////////////////////////////////////////////////

void
Sdc::setLogicValue(Pin *pin,
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
Sdc::setCaseAnalysis(Pin *pin,
		     LogicValue value)
{
  case_value_map_[pin] = value;
}

void
Sdc::removeCaseAnalysis(Pin *pin)
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
    return new ExceptionFrom(from_pins, from_clks, from_insts, from_rf, true);
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
    return new ExceptionTo(pins, clks, insts, rf, end_rf, true);
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
       && !network_->isTopLevelPort(pin))
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
    LibertyCellTimingArcSetIterator set_iter(cell, nullptr, port);
    while (set_iter.hasNext()) {
      TimingArcSet *set = set_iter.next();
      TimingRole *role = set->role();
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
    PinSet::Iterator pin_iter(from->pins());
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      if (!(network_->isRegClkPin(pin)
	    || network_->isTopLevelPort(pin))) {
	if (path_delay_internal_startpoints_ == nullptr)
	  path_delay_internal_startpoints_ = new PinSet;
	path_delay_internal_startpoints_->insert(pin);
      }
    }
  }
}

void
Sdc::unrecordPathDelayInternalStartpoints(ExceptionFrom *from)
{
  if (from
      && from->hasPins()
      && path_delay_internal_startpoints_) {
    PinSet::Iterator pin_iter(from->pins());
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      if (!(network_->isRegClkPin(pin)
	    || network_->isTopLevelPort(pin))
	  && !pathDelayFrom(pin))
	path_delay_internal_startpoints_->erase(pin);
    }
  }
}

bool
Sdc::pathDelayFrom(const Pin *pin)
{
  if (first_from_pin_exceptions_) {
    ExceptionPathSet *exceptions = first_from_pin_exceptions_->findKey(pin);
    ExceptionPathSet::ConstIterator exception_iter(exceptions);
    while (exception_iter.hasNext()) {
      ExceptionPath *exception = exception_iter.next();
      if (exception->isPathDelay())
	return true;
    }
  }
  return false;
}

bool
Sdc::isPathDelayInternalStartpoint(const Pin *pin) const
{
  return path_delay_internal_startpoints_
    && path_delay_internal_startpoints_->hasKey(const_cast<Pin*>(pin));
}

PinSet *
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
    PinSet::Iterator pin_iter(to->pins());
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      if (!(hasLibertyChecks(pin)
	    || network_->isTopLevelPort(pin))) {
	if (path_delay_internal_endpoints_ == nullptr)
	  path_delay_internal_endpoints_ = new PinSet;
	path_delay_internal_endpoints_->insert(pin);
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
      && path_delay_internal_endpoints_) {
    PinSet::Iterator pin_iter(to->pins());
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      if (!(hasLibertyChecks(pin)
	    || network_->isTopLevelPort(pin))
	  && !pathDelayTo(pin))
	path_delay_internal_endpoints_->erase(pin);
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
      LibertyCellTimingArcSetIterator timing_iter(cell, nullptr, port);
      while (timing_iter.hasNext()) {
	TimingArcSet *arc_set = timing_iter.next();
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
  if (first_to_pin_exceptions_) {
    ExceptionPathSet *exceptions = first_to_pin_exceptions_->findKey(pin);
    ExceptionPathSet::ConstIterator exception_iter(exceptions);
    while (exception_iter.hasNext()) {
      ExceptionPath *exception = exception_iter.next();
      if (exception->isPathDelay())
	return true;
    }
  }
  return false;
}

bool
Sdc::isPathDelayInternalEndpoint(const Pin *pin) const
{
  return path_delay_internal_endpoints_
    && path_delay_internal_endpoints_->hasKey(const_cast<Pin*>(pin));
}

////////////////////////////////////////////////////////////////

void
Sdc::clearGroupPathMap()
{
  // GroupPath exceptions are deleted with other exceptions.
  // Delete group_path name strings.
  GroupPathIterator group_path_iter(group_path_map_);
  while (group_path_iter.hasNext()) {
    const char *name;
    GroupPathSet *groups;
    group_path_iter.next(name, groups);
    stringDelete(name);
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
    internalError("group path name and is_default are mutually exclusive.");
  else if (name) {
    GroupPath *group_path = new GroupPath(name, is_default, from, thrus, to,
					  true, comment);
    addException(group_path);
    // A named group path can have multiple exceptions.
    GroupPathSet *groups = group_path_map_.findKey(name);
    if (groups == nullptr) {
      groups = new GroupPathSet;
      group_path_map_[stringCopy(name)] = groups;
    }
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

GroupPathIterator *
Sdc::groupPathIterator()
{
  return new GroupPathIterator(group_path_map_);
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
  GraphLoopSeq::Iterator loop_iter(levelize_->loops());
  while (loop_iter.hasNext()) {
    GraphLoop *loop = loop_iter.next();
    makeLoopExceptions(loop);
  }
}

// Make a -thru pin false path from every edge entering the loop
// around the loop and back.
void
Sdc::makeLoopExceptions(GraphLoop *loop)
{
  debugPrint0(debug_, "loop", 2, "Loop false path\n");
  EdgeSeq::Iterator loop_edge_iter(loop->edges());
  while (loop_edge_iter.hasNext()) {
    Edge *edge = loop_edge_iter.next();
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
Sdc::makeLoopException(Pin *loop_input_pin,
		       Pin *loop_pin,
		       Pin *loop_prev_pin)
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
Sdc::makeLoopExceptionThru(Pin *pin,
			   ExceptionThruSeq *thrus)
{
  debugPrint1(debug_, "levelize", 2, " %s\n", network_->pathName(pin));
  PinSet *pins = new PinSet;
  pins->insert(pin);
  ExceptionThru *thru = makeExceptionThru(pins, nullptr, nullptr,
					  RiseFallBoth::riseFall());
  thrus->push_back(thru);
}

void
Sdc::deleteLoopExceptions()
{
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
  debugPrint1(debug_, "exception_merge", 1, "add exception for %s\n",
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
					     from->transition(), true);
    ExceptionThruSeq *thrus1 = exceptionThrusClone(exception->thrus(), network_);
    ExceptionTo *to = exception->to();    
    ExceptionTo *to1 = to ? to->clone() : nullptr;
    ExceptionPath *exception1 = exception->clone(from1, thrus1, to1, true);
    debugPrint1(debug_, "exception_merge", 1, " split exception for %s\n",
		exception1->asString(network_));
    addException1(exception1);

    ClockSet *clks2 = new ClockSet(*from->clks());
    ExceptionFrom *from2 = new ExceptionFrom(nullptr, clks2, nullptr,
					     from->transition(), true);
    ExceptionThruSeq *thrus2 = exceptionThrusClone(exception->thrus(), network_);
    ExceptionTo *to2 = to ? to->clone() : nullptr;
    ExceptionPath *exception2 = exception->clone(from2, thrus2, to2, true);
    debugPrint1(debug_, "exception_merge", 1, " split exception for %s\n",
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
    ExceptionFrom *from1 = exception->from()->clone();
    ExceptionThruSeq *thrus1 = exceptionThrusClone(exception->thrus(), network_);
    PinSet *pins1 = to->pins() ? new PinSet(*to->pins()) : nullptr;
    InstanceSet *insts1 = to->instances() ? new InstanceSet(*to->instances()) : nullptr;
    ExceptionTo *to1 = new ExceptionTo(pins1, nullptr, insts1,
				       to->transition(),
				       to->endTransition(), true);
    ExceptionPath *exception1 = exception->clone(from1, thrus1, to1, true);
    debugPrint1(debug_, "exception_merge", 1, " split exception for %s\n",
		exception1->asString(network_));
    addException2(exception1);

    ExceptionFrom *from2 = exception->from()->clone();
    ExceptionThruSeq *thrus2 = exceptionThrusClone(exception->thrus(), network_);
    ClockSet *clks2 = new ClockSet(*to->clks());
    ExceptionTo *to2 = new ExceptionTo(nullptr, clks2, nullptr, to->transition(),
				       to->endTransition(), true);
    ExceptionPath *exception2 = exception->clone(from2, thrus2, to2, true);
    debugPrint1(debug_, "exception_merge", 1, " split exception for %s\n",
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
  debugPrint1(debug_, "exception_merge", 1, "find matches for %s\n",
	      exception->asString(network_));
  ExceptionPathSet matches;
  findMatchingExceptions(exception, matches);

  ExceptionPathSet expanded_matches;
  ExceptionPathSet::Iterator match_iter1(matches);
  while (match_iter1.hasNext()) {
    ExceptionPath *match = match_iter1.next();
    // Expand the matching exception into a set of exceptions that
    // that do not cover the new exception.  Do not record them
    // to prevent merging with the match, which will be deleted.
    expandExceptionExcluding(match, exception, expanded_matches);
  }

  ExceptionPathSet::Iterator match_iter2(matches);
  while (match_iter2.hasNext()) {
    ExceptionPath *match = match_iter2.next();
    deleteException(match);
  }

  ExceptionPathSet::Iterator expanded_iter(expanded_matches);
  while (expanded_iter.hasNext()) {
    ExceptionPath *expand = expanded_iter.next();
    addException(expand);
  }
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
  if (first_from_pin_exceptions_)
    findMatchingExceptionsPins(exception, from->pins(),
			       first_from_pin_exceptions_,
			       matches);
  if (first_from_inst_exceptions_)
    findMatchingExceptionsInsts(exception, from->instances(),
				first_from_inst_exceptions_, matches);
  if (first_from_clk_exceptions_)
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
  if (first_thru_net_exceptions_) {
    NetSet::Iterator net_iter(thru->nets());
    if (net_iter.hasNext()) {
      Net *net = net_iter.next();
      // Potential matches includes exceptions that match net that are not
      // the first exception point.
      ExceptionPathSet *potential_matches =
	first_thru_net_exceptions_->findKey(net);
      if (potential_matches) {
	ExceptionPathSet::Iterator match_iter(potential_matches);
	while (match_iter.hasNext()) {
	  ExceptionPath *match = match_iter.next();
	  ExceptionThru *match_thru = (*match->thrus())[0];
	  if (match_thru->nets()->hasKey(net)
	      && match->overrides(exception)
	      && match->intersectsPts(exception))
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
				ClockExceptionsMap *exception_map,
				ExceptionPathSet &matches)
{
  if (exception_map) {
    ExceptionPathSet clks_matches;
    ClockSet::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      clks_matches.insertSet(exception_map->findKey(clk));
    }
    findMatchingExceptions(exception, &clks_matches, matches);
  }
}

void
Sdc::findMatchingExceptionsPins(ExceptionPath *exception,
				PinSet *pins,
				PinExceptionsMap *exception_map,
				ExceptionPathSet &matches)
{
  if (exception_map) {
    ExceptionPathSet pins_matches;
    PinSet::Iterator pin_iter(pins);
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      pins_matches.insertSet(exception_map->findKey(pin));
    }
    findMatchingExceptions(exception, &pins_matches, matches);
  }
}

void
Sdc::findMatchingExceptionsInsts(ExceptionPath *exception,
				 InstanceSet *insts,
				 InstanceExceptionsMap *exception_map,
				 ExceptionPathSet &matches)
{
  if (exception_map) {
    ExceptionPathSet insts_matches;
    InstanceSet::Iterator inst_iter(insts);
    while (inst_iter.hasNext()) {
      Instance *inst = inst_iter.next();
      insts_matches.insertSet(exception_map->findKey(inst));
    }
    findMatchingExceptions(exception, &insts_matches, matches);
  }
}

void
Sdc::findMatchingExceptions(ExceptionPath *exception,
			    ExceptionPathSet *potential_matches,
			    ExceptionPathSet &matches)
{
  if (potential_matches) {
    ExceptionPathSet::Iterator match_iter(potential_matches);
    while (match_iter.hasNext()) {
      ExceptionPath *match = match_iter.next();
      if (match->overrides(exception)
	  && match->intersectsPts(exception))
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
    ExceptionFrom *from_cpy = from->clone();
    from_cpy->deleteObjects(excluding->from());
    if (from_cpy->hasObjects()) {
      ExceptionThruSeq *thrus_cpy = nullptr;
      if (thrus)
	thrus_cpy = clone(thrus, network_);
      ExceptionTo *to_cpy = nullptr;
      if (to)
	to_cpy = to->clone();
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
      thru_cpy->deleteObjects(thru2);
      if (thru_cpy->hasObjects()) {
	ExceptionFrom *from_cpy = nullptr;
	if (from)
	  from_cpy = from->clone();
	ExceptionThruSeq *thrus_cpy = new ExceptionThruSeq;
	ExceptionThruSeq::Iterator thru_iter1(thrus);
	while (thru_iter1.hasNext()) {
	  ExceptionThru *thru1 = thru_iter1.next();
	  if (thru1 == thru)
	    thrus_cpy->push_back(thru_cpy);
	  else {
	    ExceptionThru *thru_cpy = thru->clone(network_);
	    thrus_cpy->push_back(thru_cpy);
	  }
	}
	ExceptionTo *to_cpy = nullptr;
	if (to)
	  to_cpy = to->clone();
	ExceptionPath *expand = exception->clone(from_cpy, thrus_cpy, to_cpy,
						 true);
	expansions.insert(expand);
      }
      else
	delete thru_cpy;
    }
  }
  if (to) {
    ExceptionTo *to_cpy = to->clone();
    to_cpy->deleteObjects(excluding->to());
    if (to_cpy->hasObjects()) {
      ExceptionFrom *from_cpy = nullptr;
      if (from)
	from_cpy = from->clone();
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
clone(ExceptionThruSeq *thrus, Network *network)
{
  ExceptionThruSeq *thrus_cpy = new ExceptionThruSeq;
  ExceptionThruSeq::Iterator thru_iter(thrus);
  while (thru_iter.hasNext()) {
    ExceptionThru *thru = thru_iter.next();
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
  debugPrint3(debug_, "exception_merge", 3,
	      "record merge hash %zu %s missing %s\n",
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
  ExceptionThruSeq::Iterator thru_iter(exception->thrus());
  while (thru_iter.hasNext()) {
    ExceptionThru *thru = thru_iter.next();
    recordExceptionNets(exception, thru->nets(), first_thru_net_exceptions_);
  }
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
			 ClockExceptionsMap *&exception_map)
{
  ClockSet::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    ExceptionPathSet *set = nullptr;
    if (exception_map == nullptr)
      exception_map = new ClockExceptionsMap;
    else
      set = exception_map->findKey(clk);
    if (set == nullptr) {
      set = new ExceptionPathSet;
      (*exception_map)[clk] = set;
    }
    set->insert(exception);
  }
}

void
Sdc::recordExceptionEdges(ExceptionPath *exception,
			  EdgePinsSet *edges,
			  EdgeExceptionsMap *&exception_map)
{
  EdgePinsSet::Iterator edge_iter(edges);
  while (edge_iter.hasNext()) {
    EdgePins *edge = edge_iter.next();
    ExceptionPathSet *set = nullptr;
    if (exception_map == nullptr)
      exception_map = new EdgeExceptionsMap;
    else
      set = exception_map->findKey(edge);
    if (set == nullptr) {
      set = new ExceptionPathSet;
      // Copy the EdgePins so it is owned by the map.
      edge = new EdgePins(*edge);
      exception_map->insert(edge, set);
    }
    set->insert(exception);
  }
}

void
Sdc::recordExceptionPins(ExceptionPath *exception,
			 PinSet *pins,
			 PinExceptionsMap *&exception_map)
{
  PinSet::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    ExceptionPathSet *set = nullptr;
    if (exception_map == nullptr)
      exception_map = new PinExceptionsMap;
    else
      set = exception_map->findKey(pin);
    if (set == nullptr) {
      set = new ExceptionPathSet;
      exception_map->insert(pin, set);
    }
    set->insert(exception);
  }
}

void
Sdc::recordExceptionHpin(ExceptionPath *exception,
			 Pin *pin,
			 PinExceptionsMap *&exception_map)
{
  ExceptionPathSet *set = nullptr;
  if (exception_map == nullptr)
    exception_map = new PinExceptionsMap;
  else
    set = exception_map->findKey(pin);
  if (set == nullptr) {
    set = new ExceptionPathSet;
    exception_map->insert(pin, set);
  }
  set->insert(exception);
}

void
Sdc::recordExceptionInsts(ExceptionPath *exception,
			  InstanceSet *insts,
			  InstanceExceptionsMap *&exception_map)
{
  InstanceSet::Iterator inst_iter(insts);
  while (inst_iter.hasNext()) {
    Instance *inst = inst_iter.next();
    ExceptionPathSet *set = nullptr;
    if (exception_map == nullptr)
      exception_map = new InstanceExceptionsMap;
    else
      set = exception_map->findKey(inst);
    if (set == nullptr) {
      set = new ExceptionPathSet;
      (*exception_map)[inst] = set;
    }
    set->insert(exception);
  }
}

void
Sdc::recordExceptionNets(ExceptionPath *exception,
			 NetSet *nets,
			 NetExceptionsMap *&exception_map)
{
  NetSet::Iterator net_iter(nets);
  while (net_iter.hasNext()) {
    const Net *net = net_iter.next();
    ExceptionPathSet *set = nullptr;
    if (exception_map == nullptr)
      exception_map = new NetExceptionsMap;
    else
      set = exception_map->findKey(net);
    if (set == nullptr) {
      set = new ExceptionPathSet;
      (*exception_map)[net] = set;
    }
    set->insert(exception);
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
      ExceptionPathSet::Iterator match_iter(matches);
      while (match_iter.hasNext()) {
	ExceptionPath *match = match_iter.next();
	ExceptionPt *match_missing_pt;
	if (match != exception
	    // Exceptions are not merged if their priorities are
	    // different.  This allows exceptions to be pruned during
	    // search at the endpoint.
	    && exception->mergeable(match)
	    && match->mergeablePts(exception, missing_pt, match_missing_pt)) {
	  debugPrint1(debug_, "exception_merge", 1, "merge %s\n",
		      exception->asString(network_));
	  debugPrint1(debug_, "exception_merge", 1, " with %s\n",
		      match->asString(network_));
	  // Unrecord the exception that is being merged away.
	  unrecordException(exception);
	  unrecordMergeHashes(match);
	  missing_pt->mergeInto(match_missing_pt);
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
  ExceptionPathSet::Iterator except_iter(exceptions_);
  while (except_iter.hasNext()) {
    delete except_iter.next();
  }
  exceptions_.clear();

  deleteExceptionMap(first_from_pin_exceptions_);
  deleteExceptionMap(first_from_clk_exceptions_);
  deleteExceptionMap(first_from_inst_exceptions_);
  deleteExceptionMap(first_to_pin_exceptions_);
  deleteExceptionMap(first_to_clk_exceptions_);
  deleteExceptionMap(first_to_inst_exceptions_);
  deleteExceptionMap(first_thru_pin_exceptions_);
  deleteExceptionMap(first_thru_inst_exceptions_);
  deleteExceptionMap(first_thru_net_exceptions_);
  deleteExceptionMap(first_thru_edge_exceptions_);

  delete path_delay_internal_startpoints_;
  path_delay_internal_startpoints_ = nullptr;

  delete path_delay_internal_endpoints_;
  path_delay_internal_endpoints_ = nullptr;

  deleteExceptionPtHashMapSets(exception_merge_hash_);
  exception_merge_hash_.clear();
}

void
Sdc::deleteExceptionPtHashMapSets(ExceptionPathPtHash &map)
{
  ExceptionPathPtHash::Iterator set_iter(map);
  while (set_iter.hasNext())
    delete set_iter.next();
}

void
Sdc::deleteExceptionMap(PinExceptionsMap *&exception_map)
{
  PinExceptionsMap::Iterator set_iter(exception_map);
  while (set_iter.hasNext()) {
    const Pin *pin;
    ExceptionPathSet *set;
    set_iter.next(pin, set);
    delete set;
  }
  delete exception_map;
  exception_map = nullptr;
}

void
Sdc::deleteExceptionMap(InstanceExceptionsMap *&exception_map)
{
  InstanceExceptionsMap::Iterator set_iter(exception_map);
  while (set_iter.hasNext()) {
    const Instance *inst;
    ExceptionPathSet *set;
    set_iter.next(inst, set);
    delete set;
  }
  delete exception_map;
  exception_map = nullptr;
}

void
Sdc::deleteExceptionMap(NetExceptionsMap *&exception_map)
{
  NetExceptionsMap::Iterator set_iter(exception_map);
  while (set_iter.hasNext()) {
    const Net *net;
    ExceptionPathSet *set;
    set_iter.next(net, set);
    delete set;
  }
  delete exception_map;
  exception_map = nullptr;
}

void
Sdc::deleteExceptionMap(ClockExceptionsMap *&exception_map)
{
  ClockExceptionsMap::Iterator set_iter(exception_map);
  while (set_iter.hasNext()) {
    const Clock *clk;
    ExceptionPathSet *set;
    set_iter.next(clk, set);
    delete set;
  }
  delete exception_map;
  exception_map = nullptr;
}

void
Sdc::deleteExceptionMap(EdgeExceptionsMap *&exception_map)
{
  EdgeExceptionsMap::Iterator set_iter(exception_map);
  while (set_iter.hasNext()) {
    const EdgePins *edge_pins;
    ExceptionPathSet *set;
    set_iter.next(edge_pins, set);
    delete set;
    delete edge_pins;
  }
  delete exception_map;
  exception_map = nullptr;
}

////////////////////////////////////////////////////////////////

void
Sdc::deleteExceptionsReferencing(Clock *clk)
{
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
  debugPrint1(debug_, "exception_merge", 2, "delete %s\n",
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
  debugPrint3(debug_, "exception_merge", 3,
	      "unrecord merge hash %zu %s missing %s\n",
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
    unrecordExceptionInsts(exception, from->instances(),
			   first_from_inst_exceptions_);
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
			   ClockExceptionsMap *exception_map)
{
  ClockSet::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    ExceptionPathSet *set = exception_map->findKey(clk);
    if (set)
      set->erase(exception);
  }
}

void
Sdc::unrecordExceptionPins(ExceptionPath *exception,
			   PinSet *pins,
			   PinExceptionsMap *exception_map)
{
  PinSet::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    ExceptionPathSet *set = exception_map->findKey(pin);
    if (set)
      set->erase(exception);
  }
}

void
Sdc::unrecordExceptionInsts(ExceptionPath *exception,
			    InstanceSet *insts,
			    InstanceExceptionsMap *exception_map)
{
  InstanceSet::Iterator inst_iter(insts);
  while (inst_iter.hasNext()) {
    Instance *inst = inst_iter.next();
    ExceptionPathSet *set = exception_map->findKey(inst);
    if (set)
      set->erase(exception);
  }
}

void
Sdc::unrecordExceptionEdges(ExceptionPath *exception,
			    EdgePinsSet *edges,
			    EdgeExceptionsMap *exception_map)
{
  EdgePinsSet::Iterator edge_iter(edges);
  while (edge_iter.hasNext()) {
    EdgePins *edge = edge_iter.next();
    ExceptionPathSet *set = exception_map->findKey(edge);
    if (set)
      set->erase(exception);
  }
}

void
Sdc::unrecordExceptionNets(ExceptionPath *exception,
			   NetSet *nets,
			   NetExceptionsMap *exception_map)
{
  NetSet::Iterator net_iter(nets);
  while (net_iter.hasNext()) {
    const Net *net = net_iter.next();
    ExceptionPathSet *set = exception_map->findKey(net);
    if (set)
      set->erase(exception);
  }
}

void
Sdc::unrecordExceptionHpin(ExceptionPath *exception,
			   Pin *pin,
			   PinExceptionsMap *&exception_map)
{
  ExceptionPathSet *set = exception_map->findKey(pin);
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

private:
  DISALLOW_COPY_AND_ASSIGN(ExpandException);
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
    from_clone = from->clone();
  ExceptionThruSeq *thrus_clone = nullptr;
  if (thrus) {
    thrus_clone = new ExceptionThruSeq;
    ExceptionThruSeq::Iterator thru_iter(thrus);
    while (thru_iter.hasNext()) {
      ExceptionThru *thru = thru_iter.next();
      thrus_clone->push_back(thru->clone(network_));
    }
  }
  ExceptionTo *to_clone = nullptr;
  if (to)
    to_clone = to->clone();
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
    if (match->resetMatch(from, thrus, to, min_max)) {
      debugPrint1(debug_, "exception_match", 3, "reset match %s\n",
		  match->asString(network_));
      ExceptionPathSet expansions;
      expandException(match, expansions);
      deleteException(match);
      ExceptionPathSet::Iterator expand_iter(expansions);
      while (expand_iter.hasNext()) {
	ExceptionPath *expand = expand_iter.next();
	if (expand->resetMatch(from, thrus, to, min_max)) {
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
    if (srch_from && first_from_pin_exceptions_)
      srch_from &= exceptionFromStates(first_from_pin_exceptions_->findKey(pin),
				       nullptr, rf, min_max, include_filter,
				       states);
    if (srch_from && first_thru_pin_exceptions_)
      srch_from &= exceptionFromStates(first_thru_pin_exceptions_->findKey(pin),
				       nullptr, rf, min_max, include_filter,
				       states);

    if (srch_from
	&& (first_from_inst_exceptions_ || first_thru_inst_exceptions_)) {
      Instance *inst = network_->instance(pin);
      if (srch_from && first_from_inst_exceptions_)
	srch_from &= exceptionFromStates(first_from_inst_exceptions_->findKey(inst),
					 pin, rf, min_max, include_filter,
					 states);
      if (srch_from && first_thru_inst_exceptions_)
	srch_from &= exceptionFromStates(first_thru_inst_exceptions_->findKey(inst),
					 pin, rf, min_max, include_filter,
					 states);
    }
  }
  if (srch_from && clk && first_from_clk_exceptions_)
    srch_from &= exceptionFromStates(first_from_clk_exceptions_->findKey(clk),
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
    ExceptionPathSet::ConstIterator exception_iter(exceptions);
    while (exception_iter.hasNext()) {
      ExceptionPath *exception = exception_iter.next();
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
	    states = new ExceptionStateSet;
	  states->clear();
	  states->insert(state);
	  // No need to examine other exceptions from this
	  // pin/clock/instance.
	  return false;
	}
	if (states == nullptr)
	  states = new ExceptionStateSet;
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
    if (first_from_pin_exceptions_)
      exceptionFromStates(first_from_pin_exceptions_->findKey(pin),
			  nullptr, rf, min_max, true, states);
    if (first_from_inst_exceptions_) {
      Instance *inst = network_->instance(pin);
      exceptionFromStates(first_from_inst_exceptions_->findKey(inst),
			  pin, rf, min_max, true, states);
    }
  }
  if (first_from_clk_exceptions_)
    exceptionFromStates(first_from_clk_exceptions_->findKey(clk),
			pin, clk_rf, min_max, true, states);
}

void
Sdc::filterRegQStates(const Pin *to_pin,
		      const RiseFall *to_rf,
		      const MinMax *min_max,
		      ExceptionStateSet *&states) const
{
  if (first_from_pin_exceptions_) {
    const ExceptionPathSet *exceptions =
      first_from_pin_exceptions_->findKey(to_pin);
    if (exceptions) {
      ExceptionPathSet::ConstIterator exception_iter(exceptions);
      while (exception_iter.hasNext()) {
	ExceptionPath *exception = exception_iter.next();
	// Hack for filter -from reg/Q.
	if (exception->isFilter()
	    && exception->matchesFirstPt(to_rf, min_max)) {
	  ExceptionState *state = exception->firstState();
	  if (states == nullptr)
	    states = new ExceptionStateSet;
	  states->insert(state);
	}
      }
    }
  }
}

void
Sdc::exceptionThruStates(const Pin *from_pin,
			 const Pin *to_pin,
			 const RiseFall *to_rf,
			 const MinMax *min_max,
			 ExceptionStateSet *&states) const
{
  if (first_thru_pin_exceptions_)
    exceptionThruStates(first_thru_pin_exceptions_->findKey(to_pin),
			to_rf, min_max, states);
  if (first_thru_edge_exceptions_) {
    EdgePins edge_pins(const_cast<Pin*>(from_pin), const_cast<Pin*>(to_pin));
    exceptionThruStates(first_thru_edge_exceptions_->findKey(&edge_pins),
			to_rf, min_max, states);
  }
  if (first_thru_inst_exceptions_
      && (network_->direction(to_pin)->isAnyOutput()
	  || network_->isLatchData(to_pin))) {
    const Instance *to_inst = network_->instance(to_pin);
    exceptionThruStates(first_thru_inst_exceptions_->findKey(to_inst),
			to_rf, min_max, states);
  }
}

void
Sdc::exceptionThruStates(const ExceptionPathSet *exceptions,
			 const RiseFall *to_rf,
			 const MinMax *min_max,
			 // Return value.
			 ExceptionStateSet *&states) const
{
  if (exceptions) {
    ExceptionPathSet::ConstIterator exception_iter(exceptions);
    while (exception_iter.hasNext()) {
      ExceptionPath *exception = exception_iter.next();
      if (exception->matchesFirstPt(to_rf, min_max)) {
	ExceptionState *state = exception->firstState();
	if (states == nullptr)
	  states = new ExceptionStateSet;
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
  if (first_to_inst_exceptions_) {
    Instance *inst = network_->instance(pin);
    exceptionTo(first_to_inst_exceptions_->findKey(inst), type, pin, rf,
		clk_edge, min_max, match_min_max_exactly,
		hi_priority_exception, hi_priority);
  }
  if (first_to_pin_exceptions_)
    exceptionTo(first_to_pin_exceptions_->findKey(pin), type, pin, rf,
		clk_edge, min_max, match_min_max_exactly,
		hi_priority_exception, hi_priority);
  if (clk_edge && first_to_clk_exceptions_)
    exceptionTo(first_to_clk_exceptions_->findKey(clk_edge->clock()),
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
    ExceptionPathSet::ConstIterator exception_iter(to_exceptions);
    while (exception_iter.hasNext()) {
      ExceptionPath *exception = exception_iter.next();
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

////////////////////////////////////////////////////////////////

Wireload *
Sdc::wireloadDefaulted(const MinMax *min_max)
{
  Wireload *wireload1 = wireload(min_max);
  if (wireload1 == nullptr) {
    LibertyLibrary *default_lib = network_->defaultLibertyLibrary();
    if (default_lib)
      wireload1 = default_lib->defaultWireload();
  }
  return wireload1;
}

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
Sdc::connectPinAfter(Pin *pin)
{
  PinSet *drvrs = network_->drivers(pin);
  ExceptionPathSet::Iterator except_iter(exceptions_);
  while (except_iter.hasNext()) {
    ExceptionPath *exception = except_iter.next();
    ExceptionPt *first_pt = exception->firstPt();
    ExceptionThruSeq::Iterator thru_iter(exception->thrus());
    while (thru_iter.hasNext()) {
      ExceptionThru *thru = thru_iter.next();
      if (thru->edges()) {
	thru->connectPinAfter(drvrs, network_);
	if (first_pt == thru)
	  recordExceptionEdges(exception, thru->edges(),
			       first_thru_edge_exceptions_);
      }
    }
  }
}

void
Sdc::disconnectPinBefore(Pin *pin)
{
  ExceptionPathSet::Iterator except_iter(exceptions_);
  while (except_iter.hasNext()) {
    ExceptionPath *exception = except_iter.next();
    ExceptionPt *first_pt = exception->firstPt();
    ExceptionThruSeq::Iterator thru_iter(exception->thrus());
    while (thru_iter.hasNext()) {
      ExceptionThru *thru = thru_iter.next();
      if (thru->edges()) {
	thru->disconnectPinBefore(pin, network_);
	if (thru == first_pt)
	  recordExceptionEdges(exception, thru->edges(),
			       first_thru_edge_exceptions_);
      }
    }
  }
}

void
Sdc::clkHpinDisablesChanged(Pin *pin)
{
  if (isLeafPinClock(pin))
    clkHpinDisablesInvalid();
}

////////////////////////////////////////////////////////////////

// Annotate constraints to the timing graph.
void
Sdc::annotateGraph(bool annotate)
{
  Stats stats(debug_);
  // All output pins are considered constrained because
  // they may be downstream from a set_min/max_delay -from that
  // does not have a set_output_delay.
  annotateGraphConstrainOutputs();
  annotateDisables(annotate);
  annotateGraphOutputDelays(annotate);
  annotateGraphDataChecks(annotate);
  annotateHierClkLatency(annotate);
  stats.report("Annotate constraints to graph");
}

void
Sdc::annotateGraphConstrainOutputs()
{
  Instance *top_inst = network_->topInstance();
  InstancePinIterator *pin_iter = network_->pinIterator(top_inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyOutput())
      annotateGraphConstrained(pin, true);
  }
  delete pin_iter;
}

void
Sdc::annotateDisables(bool annotate)
{
  PinSet::Iterator pin_iter(disabled_pins_);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    annotateGraphDisabled(pin, annotate);
  }

  Instance *top_inst = network_->topInstance();
  PortSet::Iterator port_iter(disabled_ports_);
  while (port_iter.hasNext()) {
    Port *port = port_iter.next();
    Pin *pin = network_->findPin(top_inst, port);
    annotateGraphDisabled(pin, annotate);
  }

  PinPairSet::Iterator pair_iter(disabled_wire_edges_);
  while (pair_iter.hasNext()) {
    PinPair *pair = pair_iter.next();
    annotateGraphDisabledWireEdge(pair->first, pair->second, annotate, graph_);
  }

  EdgeSet::Iterator edge_iter(disabled_edges_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge->setIsDisabledConstraint(annotate);
  }

  DisabledInstancePortsMap::Iterator disable_inst_iter(disabled_inst_ports_);
  while (disable_inst_iter.hasNext()) {
    DisabledInstancePorts *disabled_inst = disable_inst_iter.next();
    setEdgeDisabledInstPorts(disabled_inst, annotate);
  }
}

class DisableHpinEdgeVisitor : public HierPinThruVisitor
{
public:
  DisableHpinEdgeVisitor(bool annotate, Graph *graph);
  virtual void visit(Pin *from_pin,
		     Pin *to_pin);

protected:
  bool annotate_;
  Graph *graph_;

private:
  DISALLOW_COPY_AND_ASSIGN(DisableHpinEdgeVisitor);
};

DisableHpinEdgeVisitor::DisableHpinEdgeVisitor(bool annotate,
					       Graph *graph) :
  HierPinThruVisitor(),
  annotate_(annotate),
  graph_(graph)
{
}

void
DisableHpinEdgeVisitor::visit(Pin *from_pin,
			      Pin *to_pin)
{
  annotateGraphDisabledWireEdge(from_pin, to_pin, annotate_, graph_);
}

static void
annotateGraphDisabledWireEdge(Pin *from_pin,
			      Pin *to_pin,
			      bool annotate,
			      Graph *graph)
{
  Vertex *from_vertex = graph->pinDrvrVertex(from_pin);
  Vertex *to_vertex = graph->pinLoadVertex(to_pin);
  if (from_vertex && to_vertex) {
    VertexOutEdgeIterator edge_iter(from_vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()
	  && edge->to(graph) == to_vertex)
	edge->setIsDisabledConstraint(annotate);
    }
  }
}

void
Sdc::annotateGraphDisabled(const Pin *pin,
			   bool annotate)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  vertex->setIsDisabledConstraint(annotate);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsDisabledConstraint(annotate);
}

void
Sdc::setEdgeDisabledInstPorts(DisabledInstancePorts *disabled_inst,
			      bool annotate)
{
  setEdgeDisabledInstPorts(disabled_inst, disabled_inst->instance(), annotate);
}

void
Sdc::setEdgeDisabledInstPorts(DisabledPorts *disabled_port,
			      Instance *inst,
			      bool annotate)
{
  if (disabled_port->all()) {
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      // set_disable_timing instance does not disable timing checks.
      setEdgeDisabledInstFrom(pin, false, annotate);
    }
    delete pin_iter;
  }

  // Disable from pins.
  LibertyPortSet::Iterator from_iter(disabled_port->from());
  while (from_iter.hasNext()) {
    LibertyPort *from_port = from_iter.next();
    Pin *from_pin = network_->findPin(inst, from_port);
    if (from_pin)
      setEdgeDisabledInstFrom(from_pin, true, annotate);
  }

  // Disable to pins.
  LibertyPortSet::Iterator to_iter(disabled_port->to());
  while (to_iter.hasNext()) {
    LibertyPort *to_port = to_iter.next();
    Pin *to_pin = network_->findPin(inst, to_port);
    if (to_pin) {
      if (network_->direction(to_pin)->isAnyOutput()) {
	Vertex *vertex = graph_->pinDrvrVertex(to_pin);
	if (vertex) {
	  VertexInEdgeIterator edge_iter(vertex, graph_);
	  while (edge_iter.hasNext()) {
	    Edge *edge = edge_iter.next();
	    edge->setIsDisabledConstraint(annotate);
	  }
	}
      }
    }
  }

  // Disable from/to pins.
  LibertyPortPairSet::Iterator from_to_iter(disabled_port->fromTo());
  while (from_to_iter.hasNext()) {
    LibertyPortPair *pair = from_to_iter.next();
    const LibertyPort *from_port = pair->first;
    const LibertyPort *to_port = pair->second;
    Pin *from_pin = network_->findPin(inst, from_port);
    Pin *to_pin = network_->findPin(inst, to_port);
    if (from_pin && network_->direction(from_pin)->isAnyInput()
	&& to_pin) {
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
      Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
      if (from_vertex && to_vertex) {
	VertexOutEdgeIterator edge_iter(from_vertex, graph_);
	while (edge_iter.hasNext()) {
	  Edge *edge = edge_iter.next();
	  if (edge->to(graph_) == to_vertex)
	    edge->setIsDisabledConstraint(annotate);
	}
      }
    }
  }
}

void
Sdc::setEdgeDisabledInstFrom(Pin *from_pin,
			     bool disable_checks,
			     bool annotate)
{
  if (network_->direction(from_pin)->isAnyInput()) {
    Vertex *from_vertex = graph_->pinLoadVertex(from_pin);
    if (from_vertex) {
      VertexOutEdgeIterator edge_iter(from_vertex, graph_);
      while (edge_iter.hasNext()) {
	Edge *edge = edge_iter.next();
	if (disable_checks
	    || !edge->role()->isTimingCheck())
	  edge->setIsDisabledConstraint(annotate);
      }
    }
  }
}

void
Sdc::annotateGraphOutputDelays(bool annotate)
{
  for (OutputDelay *output_delay : output_delays_) {
    for (Pin *lpin : output_delay->leafPins())
      annotateGraphConstrained(lpin, annotate);
  }
}

void
Sdc::annotateGraphDataChecks(bool annotate)
{
  DataChecksMap::Iterator data_checks_iter(data_checks_to_map_);
  while (data_checks_iter.hasNext()) {
    DataCheckSet *checks = data_checks_iter.next();
    DataCheckSet::Iterator check_iter(checks);
    // There may be multiple data checks on a single pin,
    // but we only need to mark it as constrained once.
    if (check_iter.hasNext()) {
      DataCheck *check = check_iter.next();
      annotateGraphConstrained(check->to(), annotate);
    }
  }
}

void
Sdc::annotateGraphConstrained(const PinSet *pins,
			      bool annotate)
{
  PinSet::ConstIterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    annotateGraphConstrained(pin, annotate);
  }
}

void
Sdc::annotateGraphConstrained(const InstanceSet *insts,
			      bool annotate)
{
  InstanceSet::ConstIterator inst_iter(insts);
  while (inst_iter.hasNext()) {
    const Instance *inst = inst_iter.next();
    annotateGraphConstrained(inst, annotate);
  }
}

void
Sdc::annotateGraphConstrained(const Instance *inst,
			      bool annotate)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyInput())
      annotateGraphConstrained(pin, annotate);
  }
  delete pin_iter;
}

void
Sdc::annotateGraphConstrained(const Pin *pin,
			      bool annotate)
{
  Vertex *vertex, *bidirect_drvr_vertex;
  graph_->pinVertices(pin, vertex, bidirect_drvr_vertex);
  // Pin may be hierarchical and have no vertex.
  if (vertex)
    vertex->setIsConstrained(annotate);
  if (bidirect_drvr_vertex)
    bidirect_drvr_vertex->setIsConstrained(annotate);
}

void
Sdc::annotateHierClkLatency(bool annotate)
{
  if (annotate) {
    ClockLatencies::Iterator latency_iter(clk_latencies_);
    while (latency_iter.hasNext()) {
      ClockLatency *latency = latency_iter.next();
      const Pin *pin = latency->pin();
      if (pin && network_->isHierarchical(pin))
	annotateHierClkLatency(pin, latency);
    }
  }
  else
    edge_clk_latency_.clear();
}

void
Sdc::annotateHierClkLatency(const Pin *hpin,
			    ClockLatency *latency)
{
  EdgesThruHierPinIterator edge_iter(hpin, network_, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge_clk_latency_[edge] = latency;
  }
}

void
Sdc::deannotateHierClkLatency(const Pin *hpin)
{
  EdgesThruHierPinIterator edge_iter(hpin, network_, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edge_clk_latency_.erase(edge);
  }
}

ClockLatency *
Sdc::clockLatency(Edge *edge) const
{
  return edge_clk_latency_.findKey(edge);
}

void
Sdc::clockLatency(Edge *edge,
		  const RiseFall *rf,
		  const MinMax *min_max,
		  // Return values.
		  float &latency,
		  bool &exists) const
{
  ClockLatency *latencies = edge_clk_latency_.findKey(edge);
  if (latencies)
    latencies->delay(rf, min_max, latency, exists);
  else {
    latency = 0.0;
    exists = false;
  }
}

////////////////////////////////////////////////////////////////

// Find the leaf load pins corresponding to pin.
// If the pin is hierarchical, the leaf pins are:
//   hierarchical  input - load pins  inside the hierarchical instance
//   hierarchical output - load pins outside the hierarchical instance
void
findLeafLoadPins(Pin *pin,
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
      Pin *pin1 = pin_iter->next();
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
findLeafDriverPins(Pin *pin,
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
      Pin *pin1 = pin_iter->next();
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

////////////////////////////////////////////////////////////////

ClockIterator::ClockIterator(Sdc *sdc) :
  ClockSeq::Iterator(sdc->clocks())
{
}

ClockIterator::ClockIterator(ClockSeq &clocks) :
  ClockSeq::Iterator(clocks)
{
}

////////////////////////////////////////////////////////////////

ClockGroupIterator::ClockGroupIterator(Sdc *sdc) :
  ClockGroupsNameMap::Iterator(sdc->clk_groups_name_map_)
{
}

ClockGroupIterator::ClockGroupIterator(ClockGroupsNameMap &clk_groups_name_map) :
  ClockGroupsNameMap::Iterator(clk_groups_name_map)
{
}

////////////////////////////////////////////////////////////////

GroupPathIterator::GroupPathIterator(Sdc *sdc) :
  GroupPathIterator(sdc->group_path_map_)
{
}

GroupPathIterator::GroupPathIterator(GroupPathMap &group_path_map) :
  GroupPathMap::Iterator(group_path_map)
{
}

} // namespace
