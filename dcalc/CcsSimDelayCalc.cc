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

#include "CcsSimDelayCalc.hh"

#include <cmath> // abs

#include "Debug.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "Network.hh"
#include "Corner.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "DmpDelayCalc.hh"

namespace sta {

using std::abs;
using std::make_shared;

// Lawrence Pillage - “Electronic Circuit & System Simulation Methods” 1998
// McGraw-Hill, Inc. New York, NY.

ArcDelayCalc *
makeCcsSimDelayCalc(StaState *sta)
{
  return new CcsSimDelayCalc(sta);
}

CcsSimDelayCalc::CcsSimDelayCalc(StaState *sta) :
  DelayCalcBase(sta),
  dcalc_args_(nullptr),
  load_pin_index_map_(network_),
  dcalc_failed_(false),
  pin_node_map_(network_),
  make_waveforms_(false),
  waveform_drvr_pin_(nullptr),
  waveform_load_pin_(nullptr),
  table_dcalc_(makeDmpCeffElmoreDelayCalc(sta))
{
}

CcsSimDelayCalc::~CcsSimDelayCalc()
{
  delete table_dcalc_;
}

ArcDelayCalc *
CcsSimDelayCalc::copy()
{
  return new CcsSimDelayCalc(this);
}

Parasitic *
CcsSimDelayCalc::findParasitic(const Pin *drvr_pin,
                               const RiseFall *,
                               const DcalcAnalysisPt *dcalc_ap)
{
  const Corner *corner = dcalc_ap->corner();
  Parasitic *parasitic = nullptr;
  // set_load net has precidence over parasitics.
  if (!sdc_->drvrPinHasWireCap(drvr_pin, corner)) {
    const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
    if (parasitics_->haveParasitics())
      parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  }
  return parasitic;
}

Parasitic *
CcsSimDelayCalc::reduceParasitic(const Parasitic *parasitic_network,
                                 const Pin *,
                                 const RiseFall *,
                                 const DcalcAnalysisPt *)
{
  return const_cast<Parasitic *>(parasitic_network);
}

ArcDcalcResult
CcsSimDelayCalc::inputPortDelay(const Pin *drvr_pin,
                                float in_slew,
                                const RiseFall *rf,
                                const Parasitic *parasitic,
                                const LoadPinIndexMap &load_pin_index_map,
                                const DcalcAnalysisPt *dcalc_ap)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();

  const Parasitic *pi_elmore = nullptr;
  if (parasitic && parasitics_->isParasiticNetwork(parasitic)) {
    const ParasiticAnalysisPt *ap = dcalc_ap->parasiticAnalysisPt();
    parasitics_->reduceToPiElmore(parasitic, drvr_pin, rf,
                                  dcalc_ap->corner(),
                                  dcalc_ap->constraintMinMax(), ap);
    pi_elmore = parasitics_->findPiElmore(drvr_pin, rf, ap);
  }

  for (auto load_pin_index : load_pin_index_map) {
    const Pin *load_pin = load_pin_index.first;
    size_t load_idx = load_pin_index.second;
    ArcDelay wire_delay = 0.0;
    Slew load_slew = in_slew;
    bool elmore_exists = false;
    float elmore = 0.0;
    if (pi_elmore)
      parasitics_->findElmore(pi_elmore, load_pin, elmore, elmore_exists);
    if (elmore_exists)
      // Input port with no external driver.
      dspfWireDelaySlew(load_pin, rf, in_slew, elmore, wire_delay, load_slew);
    thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, load_slew);
  }
  return dcalc_result;
}

ArcDcalcResult
CcsSimDelayCalc::gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap)
{
  ArcDcalcArgSeq dcalc_args;
  dcalc_args.emplace_back(nullptr, drvr_pin, nullptr, arc, in_slew, parasitic);
  ArcDcalcResultSeq dcalc_results = gateDelays(dcalc_args, load_cap,
                                               load_pin_index_map, dcalc_ap);
  return dcalc_results[0];
}

ArcDcalcResultSeq
CcsSimDelayCalc::gateDelays(ArcDcalcArgSeq &dcalc_args,
                            float load_cap,
                            const LoadPinIndexMap &load_pin_index_map,
                            const DcalcAnalysisPt *dcalc_ap)
{
  dcalc_args_ = &dcalc_args;
  load_pin_index_map_ = load_pin_index_map;
  drvr_count_ = dcalc_args.size();
  load_cap_ = load_cap;
  dcalc_ap_ = dcalc_ap;
  drvr_rf_ = dcalc_args[0].arc()->toEdge()->asRiseFall();
  dcalc_failed_ = false;
  parasitic_network_ = dcalc_args[0].parasitic();
  ArcDcalcResultSeq dcalc_results(drvr_count_);

  size_t drvr_count = dcalc_args.size();
  output_waveforms_.resize(drvr_count);
  ref_time_.resize(drvr_count);

  for (size_t drvr_idx = 0; drvr_idx < dcalc_args.size(); drvr_idx++) {
    ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
    GateTableModel *table_model = gateTableModel(dcalc_arg.arc(), dcalc_ap);
    if (table_model && dcalc_arg.parasitic()) {
      OutputWaveforms *output_waveforms = table_model->outputWaveforms();
      float in_slew = delayAsFloat(dcalc_arg.inSlew());
      if (output_waveforms
          // Bounds check because extrapolating waveforms does not work for shit.
          && output_waveforms->slewAxis()->inBounds(in_slew)
          && output_waveforms->capAxis()->inBounds(load_cap)) {
        output_waveforms_[drvr_idx] = output_waveforms;
        ref_time_[drvr_idx] = output_waveforms->referenceTime(in_slew);
        debugPrint(debug_, "ccs_dcalc", 1, "%s %s",
                   network_->libertyPort(dcalc_arg.drvrPin())->libertyCell()->name(),
                   drvr_rf_->asString());

        LibertyCell *drvr_cell = dcalc_arg.arc()->to()->libertyCell();
        const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
        bool vdd_exists;
        drvr_library->supplyVoltage("VDD", vdd_, vdd_exists);
        if (!vdd_exists)
          report_->error(1720, "VDD not defined in library %s", drvr_library->name());
        drvr_cell->ensureVoltageWaveforms();
        if (drvr_idx == 0) {
          vth_ = drvr_library->outputThreshold(drvr_rf_) * vdd_;
          vl_ = drvr_library->slewLowerThreshold(drvr_rf_) * vdd_;
          vh_ = drvr_library->slewUpperThreshold(drvr_rf_) * vdd_;
        }
      }
      else
        dcalc_failed_ = true;
    }
    else
      dcalc_failed_ = true;
  }

  if (dcalc_failed_) {
    const Parasitic *parasitic_network = dcalc_args[0].parasitic();
    for (size_t drvr_idx = 0; drvr_idx < dcalc_args.size(); drvr_idx++) {
      ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
      Parasitic *pi_elmore = nullptr;
      const Pin *drvr_pin = dcalc_arg.drvrPin();
      if (parasitic_network) {
        const ParasiticAnalysisPt *ap = dcalc_ap_->parasiticAnalysisPt();
        parasitics_->reduceToPiElmore(parasitic_network, drvr_pin, drvr_rf_,
                                      dcalc_ap_->corner(),
                                      dcalc_ap_->constraintMinMax(), ap);
        pi_elmore = parasitics_->findPiElmore(drvr_pin, drvr_rf_, ap);
        dcalc_arg.setParasitic(pi_elmore);
      }
    }
    dcalc_results = table_dcalc_->gateDelays(dcalc_args, load_cap,
                                             load_pin_index_map, dcalc_ap);
  }
  else {
    simulate(dcalc_args);

    for (size_t drvr_idx = 0; drvr_idx < dcalc_args.size(); drvr_idx++) {
      ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
      ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
      const Pin *drvr_pin = dcalc_arg.drvrPin();
      size_t drvr_node = pin_node_map_[drvr_pin];
      ThresholdTimes &drvr_times = threshold_times_[drvr_node];
      ArcDelay gate_delay = drvr_times[threshold_vth] - ref_time_[drvr_idx];
      Slew drvr_slew = abs(drvr_times[threshold_vh] - drvr_times[threshold_vl]);
      dcalc_result.setGateDelay(gate_delay);
      dcalc_result.setDrvrSlew(drvr_slew);
      debugPrint(debug_, "ccs_dcalc", 2,
                 "%s gate delay %s slew %s",
                 network_->pathName(drvr_pin),
                 delayAsString(gate_delay, this),
                 delayAsString(drvr_slew, this));

      dcalc_result.setLoadCount(load_pin_index_map.size());
      for (auto load_pin_index : load_pin_index_map) {
        const Pin *load_pin = load_pin_index.first;
        size_t load_idx = load_pin_index.second;
        size_t load_node = pin_node_map_[load_pin];
        ThresholdTimes &wire_times = threshold_times_[load_node];
        ThresholdTimes &drvr_times = threshold_times_[drvr_node];
        ArcDelay wire_delay = wire_times[threshold_vth] - drvr_times[threshold_vth];
        Slew load_slew = abs(wire_times[threshold_vh] - wire_times[threshold_vl]);
        debugPrint(debug_, "ccs_dcalc", 2,
                   "load %s %s delay %s slew %s",
                   network_->pathName(load_pin),
                   drvr_rf_->asString(),
                   delayAsString(wire_delay, this),
                   delayAsString(load_slew, this));

        LibertyLibrary *drvr_library =
          network_->libertyPort(load_pin)->libertyCell()->libertyLibrary();
        thresholdAdjust(load_pin, drvr_library, drvr_rf_, wire_delay, load_slew);
        dcalc_result.setWireDelay(load_idx, wire_delay);
        dcalc_result.setLoadSlew(load_idx, load_slew);
      }
    }
  }
  return dcalc_results;
}

void
CcsSimDelayCalc::simulate(ArcDcalcArgSeq &dcalc_args)
{
  const Pin *drvr_pin = dcalc_args[0].drvrPin();
  LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  const MinMax *min_max = dcalc_ap_->delayMinMax();
  drive_resistance_ = drvr_port->driveResistance(drvr_rf_, min_max);

  initSim();
  stampConductances();

  // The conductance matrix does not change as long as the time step is constant.
  // Factor stamping and LU decomposition of the conductance matrix
  // outside of the simulation loop.
  // Prevent copying of matrix.
  conductances_.makeCompressed();
  // LU factor conductances.
  solver_.compute(conductances_);

  for (size_t drvr_idx = 0; drvr_idx < dcalc_args.size(); drvr_idx++) {
    ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
    // Find initial ceff.
    ceff_[drvr_idx] = load_cap_;
    // voltageTime is always for a rising waveform so 0.0v is initial voltage.
    drvr_current_[drvr_idx] =
      output_waveforms_[drvr_idx]->voltageCurrent(delayAsFloat(dcalc_arg.inSlew()),
                                                  ceff_[drvr_idx], 0.0);
  }
  // Initial time depends on ceff which impact delay, so use a sim step
  // to find an initial ceff.
  setCurrents();
  voltages_ = solver_.solve(currents_);
  updateCeffIdrvr();
  initNodeVoltages();

  // voltageTime is always for a rising waveform so 0.0v is initial voltage.
  double time_begin = output_waveforms_[0]->voltageTime(dcalc_args[0].inSlewFlt(),
                                                        ceff_[0], 0.0);
  // Limit in case load voltage waveforms don't get to final value.
  double time_end = time_begin + maxTime();

  if (make_waveforms_)
    recordWaveformStep(time_begin);

  for (double time = time_begin; time <= time_end; time += time_step_) {
    stampConductances();
    conductances_.makeCompressed();
    solver_.compute(conductances_);
    setCurrents();
    voltages_ = solver_.solve(currents_);

    debugPrint(debug_, "ccs_dcalc", 3, "%s ceff %s VDrvr %.4f Idrvr %s",
               delayAsString(time, this),
               units_->capacitanceUnit()->asString(ceff_[0]),
               voltages_[pin_node_map_[dcalc_args[0].drvrPin()]],
               units_->currentUnit()->asString(drvr_current_[0], 4));

    updateCeffIdrvr();
    measureThresholds(time);
    if (make_waveforms_)
      recordWaveformStep(time);

    bool loads_finished = true;
    for (auto load_node1 : pin_node_map_) {
      size_t load_node = load_node1.second;
      if ((drvr_rf_ == RiseFall::rise()
           && voltages_[load_node] < vh_ + (vdd_ - vh_) * .5)
          || (drvr_rf_ == RiseFall::fall()
              && (voltages_[load_node] > vl_ * .5))) {
        loads_finished = false;
        break;
      }
    }
    if (loads_finished)
      break;

    time_step_prev_ = time_step_;
    // swap faster than copying with '='.
    voltages_prev2_.swap(voltages_prev1_);
    voltages_prev1_.swap(voltages_);
  }
}

double
CcsSimDelayCalc::timeStep()
{
  // Needs to use LTE for time step dynamic control.
  return drive_resistance_ * load_cap_ * .02;
}

double
CcsSimDelayCalc::maxTime()
{
  return (*dcalc_args_)[0].inSlewFlt()
    + (drive_resistance_ + resistance_sum_) * load_cap_ * 2;
}

void
CcsSimDelayCalc::initSim()
{
  ceff_.resize(drvr_count_);
  drvr_current_.resize(drvr_count_);

  findNodeCount();
  setOrder();

  initNodeVoltages();

  // time step required by initCapacitanceCurrents
  time_step_ = time_step_prev_ = timeStep();
  debugPrint(debug_, "ccs_dcalc", 1, "time step %s", delayAsString(time_step_, this));

  // Reset waveform recording.
  times_.clear();
  drvr_voltages_.clear();
  load_voltages_.clear();

  measure_thresholds_ = {vl_, vth_, vh_};
}

void
CcsSimDelayCalc::findNodeCount()
{
  includes_pin_caps_ = parasitics_->includesPinCaps(parasitic_network_);
  coupling_cap_multiplier_ = 1.0;

  node_capacitances_.clear();
  pin_node_map_.clear();
  node_index_map_.clear();

  for (ParasiticNode *node : parasitics_->nodes(parasitic_network_)) {
    if (!parasitics_->isExternal(node)) {
      size_t node_idx = node_index_map_.size();
      node_index_map_[node] = node_idx;
      const Pin *pin = parasitics_->pin(node);
      if (pin) {
        pin_node_map_[pin] = node_idx;
        debugPrint(debug_, "ccs_dcalc", 1, "pin %s node %lu",
                   network_->pathName(pin),
                   node_idx);
      }
      double cap = parasitics_->nodeGndCap(node) + pinCapacitance(node);
      node_capacitances_.push_back(cap);
    }
  }

  for (ParasiticCapacitor *capacitor : parasitics_->capacitors(parasitic_network_)) {
    float cap = parasitics_->value(capacitor) * coupling_cap_multiplier_;
      ParasiticNode *node1 = parasitics_->node1(capacitor);
    if (!parasitics_->isExternal(node1)) {
      size_t node_idx = node_index_map_[node1];
      node_capacitances_[node_idx] += cap;
    }
    ParasiticNode *node2 = parasitics_->node2(capacitor);
    if (!parasitics_->isExternal(node2)) {
      size_t node_idx = node_index_map_[node2];
      node_capacitances_[node_idx] += cap;
    }
  }
  node_count_ = node_index_map_.size();
}

float
CcsSimDelayCalc::pinCapacitance(ParasiticNode *node)
{
  const Pin *pin = parasitics_->pin(node);
  float pin_cap = 0.0;
  if (pin) {
    Port *port = network_->port(pin);
    LibertyPort *lib_port = network_->libertyPort(port);
    const Corner *corner = dcalc_ap_->corner();
    const MinMax *cnst_min_max = dcalc_ap_->constraintMinMax();
    if (lib_port) {
      if (!includes_pin_caps_)
        pin_cap = sdc_->pinCapacitance(pin, drvr_rf_, corner, cnst_min_max);
    }
    else if (network_->isTopLevelPort(pin))
      pin_cap = sdc_->portExtCap(port, drvr_rf_, corner, cnst_min_max);
  }
  return pin_cap;
}

void
CcsSimDelayCalc::setOrder()
{
  currents_.resize(node_count_);
  voltages_.resize(node_count_);
  voltages_prev1_.resize(node_count_);
  voltages_prev2_.resize(node_count_);
  // Matrix resize also zeros.
  conductances_.resize(node_count_, node_count_);
  threshold_times_.resize(node_count_);
}

void
CcsSimDelayCalc::initNodeVoltages()
{
  double drvr_init_volt = (drvr_rf_ == RiseFall::rise()) ? 0.0 : vdd_;
  for (size_t i = 0; i < node_count_; i++) {
    voltages_[i] = drvr_init_volt;
    voltages_prev1_[i] = drvr_init_volt;
    voltages_prev2_[i] = drvr_init_volt;
  }
}

void
CcsSimDelayCalc::simulateStep()
{
  setCurrents();
  voltages_ = solver_.solve(currents_);
}

void
CcsSimDelayCalc::stampConductances()
{
  conductances_.setZero();
  for (size_t node_idx = 0; node_idx < node_count_; node_idx++)
    stampCapacitance(node_idx, node_capacitances_[node_idx]);

  resistance_sum_ = 0.0;
  for (ParasiticResistor *resistor : parasitics_->resistors(parasitic_network_)) {
    ParasiticNode *node1 = parasitics_->node1(resistor);
    ParasiticNode *node2 = parasitics_->node2(resistor);
    // One commercial extractor creates resistors with identical from/to nodes.
    if (node1 != node2) {
      size_t node_idx1 = node_index_map_[node1];
      size_t node_idx2 = node_index_map_[node2];
      float resistance = parasitics_->value(resistor);
      stampConductance(node_idx1, node_idx2, 1.0 / resistance);
      resistance_sum_ += resistance;
    }
  }
}

// Grounded resistor.
void
CcsSimDelayCalc::stampConductance(size_t n1,
                                  double g)
{
  conductances_.coeffRef(n1, n1) += g;
}

// Floating resistor.
void
CcsSimDelayCalc::stampConductance(size_t n1,
                                  size_t n2,
                                  double g)
{
  conductances_.coeffRef(n1, n1) += g;
  conductances_.coeffRef(n2, n2) += g;
  conductances_.coeffRef(n1, n2) -= g;
  conductances_.coeffRef(n2, n1) -= g;
}

// Grounded capacitance.
void
CcsSimDelayCalc::stampCapacitance(size_t n1,
                                  double cap)
{
  double g = cap * 2.0 / time_step_;
  stampConductance(n1, g);
}

// Floating capacitance.
void
CcsSimDelayCalc::stampCapacitance(size_t n1,
                                  size_t n2,
                                  double cap)
{
  double g = cap * 2.0 / time_step_;
  stampConductance(n1, n2, g);
}

////////////////////////////////////////////////////////////////

void
CcsSimDelayCalc::setCurrents()
{
  currents_.setZero(node_count_);
  for (size_t i = 0; i < drvr_count_; i++) {
    size_t drvr_node = pin_node_map_[(*dcalc_args_)[i].drvrPin()];
    insertCurrentSrc(drvr_node, drvr_current_[i]);
  }

  for (size_t node_idx = 0; node_idx < node_count_; node_idx++)
    insertCapCurrentSrc(node_idx, node_capacitances_[node_idx]);
}

void
CcsSimDelayCalc::insertCapCurrentSrc(size_t n1,
                                     double cap)
{
  // Direct implementation of figure 4.11 in
  // “Electronic Circuit & System Simulation Methods” allowing for time
  // step changes.
  // double g0 = 2.0 * cap / time_step_;
  // double g1 = 2.0 * cap / time_step_prev_;
  // double dv = voltages_prev2_[n1] - voltages_prev1_[n1];
  // double ieq_prev = cap * dv / time_step_ + g0 * voltages_prev1_[n1];
  // double i_cap = (g0 + g1) * voltages_prev1_[n1] - ieq_prev;

  // Above simplified.
  // double i_cap
  //   =       cap / time_step_ * voltages_prev1_[n1]
  //   + 2.0 * cap / time_step_prev_ * voltages_prev1_[n1]
  //   -       cap / time_step_ * voltages_prev2_[n1];

  // Simplified for constant time step.
  double i_cap
    = 3.0 * cap / time_step_ * voltages_prev1_[n1]
    -       cap / time_step_ * voltages_prev2_[n1];
  insertCurrentSrc(n1, i_cap);
}

void
CcsSimDelayCalc::insertCapaCurrentSrc(size_t n1,
                                      size_t n2,
                                      double cap)
{
  double g0 = 2.0 * cap / time_step_;
  double g1 = 2.0 * cap / time_step_prev_;
  double dv = (voltages_prev2_[n1] - voltages_prev2_[n2])
    - (voltages_prev1_[n1] - voltages_prev1_[n2]);
  double ieq_prev = cap * dv / time_step_ + g0*(voltages_prev1_[n1]-voltages_prev1_[n2]);
  double i_cap = (g0 + g1) * (voltages_prev1_[n1] - voltages_prev1_[n2]) - ieq_prev;
  insertCurrentSrc(n1, n2, i_cap);
}

void
CcsSimDelayCalc::insertCurrentSrc(size_t n1,
                                  double current)
{
  currents_.coeffRef(n1) += current;
}

void
CcsSimDelayCalc::insertCurrentSrc(size_t n1,
                                  size_t n2,
                                  double current)
{
  currents_.coeffRef(n1) += current;
  currents_.coeffRef(n2) -= current;
}

void
CcsSimDelayCalc::updateCeffIdrvr()
{
  for (size_t i = 0; i < drvr_count_; i++) {
    size_t drvr_node = pin_node_map_[(*dcalc_args_)[i].drvrPin()];
    double dv = voltages_[drvr_node] - voltages_prev1_[drvr_node];
    if (drvr_rf_ == RiseFall::rise()) {
      if (drvr_current_[i] != 0.0
          && dv > 0.0) {
        double ceff = drvr_current_[i] * time_step_ / dv;
        if (output_waveforms_[i]->capAxis()->inBounds(ceff))
          ceff_[i] = ceff;
      }

      double v = voltages_[drvr_node];
      if (voltages_[drvr_node] > (vdd_ - .01))
        // Whoa partner. Head'n for the weeds.
        drvr_current_[i] = 0.0;
      else
        drvr_current_[i] =
          output_waveforms_[i]->voltageCurrent((*dcalc_args_)[i].inSlewFlt(),
                                               ceff_[i], v);
    }
    else {
      if (drvr_current_[i] != 0.0
          && dv < 0.0) {
        double ceff = drvr_current_[i] * time_step_ / dv;
        if (output_waveforms_[i]->capAxis()->inBounds(ceff))
          ceff_[i] = ceff;
      }
      double v = vdd_ - voltages_[drvr_node];
      if (voltages_[drvr_node] < 0.01)
        // Whoa partner. Head'n for the weeds.
        drvr_current_[i] = 0.0;
      else
        drvr_current_[i] =
          output_waveforms_[i]->voltageCurrent((*dcalc_args_)[i].inSlewFlt(),
                                               ceff_[i], v);
    }
  }
}

////////////////////////////////////////////////////////////////

void
CcsSimDelayCalc::measureThresholds(double time)
{
  for (auto pin_node1 : pin_node_map_) {
    size_t pin_node = pin_node1.second;
    measureThresholds(time, pin_node);
  }
}

void
CcsSimDelayCalc::measureThresholds(double time,
                                   size_t n)
{
  double v = voltages_[n];
  double v_prev = voltages_prev1_[n];
  for (size_t m = 0; m < measure_threshold_count_; m++) {
    double th = measure_thresholds_[m];
    if ((v_prev < th && th <= v)
        || (v_prev > th && th >= v)) {
      double t_cross = time - time_step_ + (th - v_prev) * time_step_ / (v - v_prev);
      debugPrint(debug_, "ccs_measure", 1, "node %lu cross %.2f %s",
                 n,
                 th,
                 delayAsString(t_cross, this));
      threshold_times_[n][m] = t_cross;
    }
  }
}

void
CcsSimDelayCalc::recordWaveformStep(double time)
{
  times_.push_back(time);
  size_t drvr_node = pin_node_map_[waveform_drvr_pin_];
  drvr_voltages_.push_back(voltages_[drvr_node]);
  if (waveform_load_pin_) {
    size_t load_node = pin_node_map_[waveform_load_pin_];
    load_voltages_.push_back(voltages_[load_node]);
  }
}

////////////////////////////////////////////////////////////////

string
CcsSimDelayCalc::reportGateDelay(const Pin *drvr_pin,
                                 const TimingArc *arc,
                                 const Slew &in_slew,
                                 float load_cap,
                                 const Parasitic *,
                                 const LoadPinIndexMap &,
                                 const DcalcAnalysisPt *dcalc_ap,
                                 int digits)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pinPvt(drvr_pin, dcalc_ap), in_slew1, load_cap,
                                  false, digits);
  }
  return "";
}

////////////////////////////////////////////////////////////////

// Waveform accessors for swig/tcl.
Table1
CcsSimDelayCalc::drvrWaveform(const Pin *in_pin,
                              const RiseFall *in_rf,
                              const Pin *drvr_pin,
                              const RiseFall *drvr_rf,
                              const Corner *corner,
                              const MinMax *min_max)
{
  makeWaveforms(in_pin, in_rf, drvr_pin, drvr_rf, nullptr, corner, min_max);
  TableAxisPtr time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                  new FloatSeq(times_));
  Table1 waveform(new FloatSeq(drvr_voltages_), time_axis);
  return waveform;
}

Table1
CcsSimDelayCalc::loadWaveform(const Pin *in_pin,
                              const RiseFall *in_rf,
                              const Pin *drvr_pin,
                              const RiseFall *drvr_rf,
                              const Pin *load_pin,
                              const Corner *corner,
                              const MinMax *min_max)
{
  makeWaveforms(in_pin, in_rf, drvr_pin, drvr_rf, load_pin, corner, min_max);
  TableAxisPtr time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                  new FloatSeq(times_));
  Table1 waveform(new FloatSeq(load_voltages_), time_axis);
  return waveform;
}

Table1
CcsSimDelayCalc::inputWaveform(const Pin *in_pin,
                               const RiseFall *in_rf,
                               const Corner *corner,
                               const MinMax *min_max)
{
  LibertyPort *port = network_->libertyPort(in_pin);
  if (port) {
    DriverWaveform *driver_waveform = port->driverWaveform(in_rf);
    const Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
    DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
    float in_slew = delayAsFloat(graph_->slew(in_vertex, in_rf, dcalc_ap->index()));
    LibertyLibrary *library = port->libertyLibrary();
    float vdd;
    bool vdd_exists;
    library->supplyVoltage("VDD", vdd, vdd_exists);
    if (!vdd_exists)
      report_->error(1721, "VDD not defined in library %s", library->name());
    Table1 in_waveform = driver_waveform->waveform(in_slew);
    // Scale the waveform from 0:vdd.
    FloatSeq *scaled_values = new FloatSeq;
    for (float value : *in_waveform.values())
      scaled_values->push_back(value * vdd);
    return Table1(scaled_values, in_waveform.axis1ptr());
  }
  return Table1();
}

void
CcsSimDelayCalc::makeWaveforms(const Pin *in_pin,
                               const RiseFall *in_rf,
                               const Pin *drvr_pin,
                               const RiseFall *drvr_rf,
                               const Pin *load_pin,
                               const Corner *corner,
                               const MinMax *min_max)
{
  Edge *edge;
  const TimingArc *arc;
  graph_->gateEdgeArc(in_pin, in_rf, drvr_pin, drvr_rf, edge, arc);
  if (arc) {
    DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
    const Parasitic *parasitic = findParasitic(drvr_pin, drvr_rf, dcalc_ap);
    if (parasitic) {
      make_waveforms_ = true;
      waveform_drvr_pin_ = drvr_pin;
      waveform_load_pin_ = load_pin;
      Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
      graph_delay_calc_->findDriverArcDelays(drvr_vertex, edge, arc, dcalc_ap, this);
      make_waveforms_ = false;
      waveform_drvr_pin_ = nullptr;
      waveform_load_pin_ = nullptr;
    }
  }
}

////////////////////////////////////////////////////////////////

void
CcsSimDelayCalc::reportMatrix(const char *name,
                              MatrixSd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
CcsSimDelayCalc::reportMatrix(const char *name,
                              MatrixXd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
CcsSimDelayCalc::reportMatrix(const char *name,
                              VectorXd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
CcsSimDelayCalc::reportVector(const char *name,
                              vector<double> &matrix)
{
  report_->reportLine("%s", name);
  reportVector(matrix);
}
  
void
CcsSimDelayCalc::reportMatrix(MatrixSd &matrix)
{
  for (Index i = 0; i < matrix.rows(); i++) {
    string line = "| ";
    for (Index j = 0; j < matrix.cols(); j++) {
      string entry = stdstrPrint("%10.3e", matrix.coeff(i, j));
      line += entry;
      line += " ";
    }
    line += "|";
    report_->reportLineString(line);
  }
}

void
CcsSimDelayCalc::reportMatrix(MatrixXd &matrix)
{
  for (Index i = 0; i < matrix.rows(); i++) {
    string line = "| ";
    for (Index j = 0; j < matrix.cols(); j++) {
      string entry = stdstrPrint("%10.3e", matrix.coeff(i, j));
      line += entry;
      line += " ";
    }
    line += "|";
    report_->reportLineString(line);
  }
}

void
CcsSimDelayCalc::reportMatrix(VectorXd &matrix)
{
  string line = "| ";
  for (Index i = 0; i < matrix.rows(); i++) {
    string entry = stdstrPrint("%10.3e", matrix.coeff(i));
    line += entry;
    line += " ";
  }
  line += "|";
  report_->reportLineString(line);
}

void
CcsSimDelayCalc::reportVector(vector<double> &matrix)
{
  string line = "| ";
  for (size_t i = 0; i < matrix.size(); i++) {
    string entry = stdstrPrint("%10.3e", matrix[i]);
    line += entry;
    line += " ";
  }
  line += "|";
  report_->reportLineString(line);
}

} // namespace
