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

#include "PrimaDelayCalc.hh"

#include <cmath> // abs

#include "Debug.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "Corner.hh"
#include "Graph.hh"
#include "Parasitics.hh"
#include "GraphDelayCalc.hh"
#include "DmpDelayCalc.hh"

#include <Eigen/LU>
#include <Eigen/QR>

namespace sta {

using std::abs;
using std::make_shared;
using Eigen::SparseLU;
using Eigen::HouseholderQR;
using Eigen::ColPivHouseholderQR;

// Lawrence Pillage - “Electronic Circuit & System Simulation Methods” 1998
// McGraw-Hill, Inc. New York, NY.

ArcDelayCalc *
makePrimaDelayCalc(StaState *sta)
{
  return new PrimaDelayCalc(sta);
}

PrimaDelayCalc::PrimaDelayCalc(StaState *sta) :
  DelayCalcBase(sta),
  dcalc_args_(nullptr),
  load_pin_index_map_(nullptr),
  pin_node_map_(network_),
  node_index_map_(ParasiticNodeLess(parasitics_, network_)),
  prima_order_(3),
  make_waveforms_(false),
  waveform_drvr_pin_(nullptr),
  waveform_load_pin_(nullptr),
  watch_pin_values_(network_),
  table_dcalc_(makeDmpCeffElmoreDelayCalc(sta))
{
}

PrimaDelayCalc::PrimaDelayCalc(const PrimaDelayCalc &dcalc) :
  DelayCalcBase(dcalc),
  dcalc_args_(nullptr),
  load_pin_index_map_(nullptr),
  pin_node_map_(network_),
  node_index_map_(ParasiticNodeLess(parasitics_, network_)),
  prima_order_(dcalc.prima_order_),
  make_waveforms_(false),
  waveform_drvr_pin_(nullptr),
  waveform_load_pin_(nullptr),
  watch_pin_values_(network_),
  table_dcalc_(makeDmpCeffElmoreDelayCalc(this))
{
}

PrimaDelayCalc::~PrimaDelayCalc()
{
  delete table_dcalc_;
}

ArcDelayCalc *
PrimaDelayCalc::copy()
{
  return new PrimaDelayCalc(*this);
}

// Notify algorithm components.
void
PrimaDelayCalc::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  table_dcalc_->copyState(sta);
}

Parasitic *
PrimaDelayCalc::findParasitic(const Pin *drvr_pin,
                              const RiseFall *rf,
                              const DcalcAnalysisPt *dcalc_ap)
{
  const Corner *corner = dcalc_ap->corner();
  const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  // set_load net has precidence over parasitics.
  if (sdc_->drvrPinHasWireCap(drvr_pin, corner)
      || network_->direction(drvr_pin)->isInternal())
    return nullptr;
  Parasitic *parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  if (parasitic)
    return parasitic;
  const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
  Wireload *wireload = sdc_->wireload(cnst_min_max);
  if (wireload) {
    float pin_cap, wire_cap, fanout;
    bool has_wire_cap;
    graph_delay_calc_->netCaps(drvr_pin, rf, dcalc_ap, pin_cap, wire_cap,
                               fanout, has_wire_cap);
    parasitic = parasitics_->makeWireloadNetwork(drvr_pin, wireload,
                                                 fanout, cnst_min_max,
                                                 parasitic_ap);
  }
  return parasitic;
}

Parasitic *
PrimaDelayCalc::reduceParasitic(const Parasitic *,
                                const Pin *,
                                const RiseFall *,
                                const DcalcAnalysisPt *)
{
  return nullptr;
}

ArcDcalcResult
PrimaDelayCalc::inputPortDelay(const Pin *drvr_pin,
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
    pi_elmore = parasitics_->reduceToPiElmore(parasitic, drvr_pin, rf,
                                              dcalc_ap->corner(),
                                              dcalc_ap->constraintMinMax(), ap);
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
PrimaDelayCalc::gateDelay(const Pin *drvr_pin,
                          const TimingArc *arc,
                          const Slew &in_slew,
                          float load_cap,
                          const Parasitic *parasitic,
                          const LoadPinIndexMap &load_pin_index_map,
                          const DcalcAnalysisPt *dcalc_ap)
{
  ArcDcalcArgSeq dcalc_args;
  dcalc_args.emplace_back(nullptr, drvr_pin, nullptr, arc, in_slew, load_cap, parasitic);
  ArcDcalcResultSeq dcalc_results = gateDelays(dcalc_args, load_pin_index_map, dcalc_ap);
  return dcalc_results[0];
}

ArcDcalcResultSeq
PrimaDelayCalc::gateDelays(ArcDcalcArgSeq &dcalc_args,
                           const LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap)
{
  dcalc_args_ = &dcalc_args;
  load_pin_index_map_ = &load_pin_index_map;
  drvr_count_ = dcalc_args.size();
  dcalc_ap_ = dcalc_ap;
  drvr_rf_ = dcalc_args[0].arc()->toEdge()->asRiseFall();
  parasitic_network_ = dcalc_args[0].parasitic();
  load_cap_ = dcalc_args[0].loadCap();

  bool failed = false;
  output_waveforms_.resize(drvr_count_);
  const DcalcAnalysisPtSeq &dcalc_aps = corners_->dcalcAnalysisPts();
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
    GateTableModel *table_model = dcalc_arg.arc()->gateTableModel(dcalc_ap);
    if (table_model && dcalc_arg.parasitic()) {
      OutputWaveforms *output_waveforms = table_model->outputWaveforms();
      float in_slew = dcalc_arg.inSlewFlt();
      if (output_waveforms
          // Bounds check because extrapolating waveforms does not work for shit.
          && output_waveforms->slewAxis()->inBounds(in_slew)
          && output_waveforms->capAxis()->inBounds(dcalc_arg.loadCap())) {
        output_waveforms_[drvr_idx] = output_waveforms;
        debugPrint(debug_, "ccs_dcalc", 1, "%s %s",
                   dcalc_arg.drvrCell()->name(),
                   drvr_rf_->asString());
        LibertyCell *drvr_cell = dcalc_arg.drvrCell();
        const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
        bool vdd_exists;
        drvr_library->supplyVoltage("VDD", vdd_, vdd_exists);
        if (!vdd_exists)
          report_->error(1720, "VDD not defined in library %s", drvr_library->name());
        drvr_cell->ensureVoltageWaveforms(dcalc_aps);
        if (drvr_idx == 0) {
          vth_ = drvr_library->outputThreshold(drvr_rf_) * vdd_;
          vl_ = drvr_library->slewLowerThreshold(drvr_rf_) * vdd_;
          vh_ = drvr_library->slewUpperThreshold(drvr_rf_) * vdd_;
        }
      }
      else
        failed = true;
    }
    else
      failed = true;
  }

  if (failed)
    return tableDcalcResults();
  else {
    simulate();
    return dcalcResults();
  }
}

ArcDcalcResultSeq
PrimaDelayCalc::tableDcalcResults()
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    if (drvr_pin) {
      const RiseFall *rf = dcalc_arg.drvrEdge();
      const Parasitic *parasitic = table_dcalc_->findParasitic(drvr_pin, rf, dcalc_ap_);
      dcalc_arg.setParasitic(parasitic);
    }
  }
  return table_dcalc_->gateDelays(*dcalc_args_, *load_pin_index_map_, dcalc_ap_);
}

void
PrimaDelayCalc::simulate()
{
  initSim();
  stampEqns();
  setXinit();

  if (prima_order_ > 0
      && node_count_ > prima_order_) {
    primaReduce();
    simulate1(Gq_, Cq_, Bq_, xq_init_, Vq_, prima_order_);
  }
  else {
    MatrixXd x_to_v = MatrixXd::Identity(order_, order_);
    simulate1(G_, C_, B_, x_init_, x_to_v, order_);
  }
}

void
PrimaDelayCalc::simulate1(const MatrixSd &G,
                 const MatrixSd &C,
                 const MatrixXd &B,
                 const VectorXd &x_init,
                 const MatrixXd &x_to_v,
                 const size_t order)
{
  VectorXd x(order);
  VectorXd x_prev(order);
  VectorXd x_prev2(order);

  v_.resize(order);
  v_prev_.resize(order);

  initCeffIdrvr();
  x = x_prev = x_prev2 = x_init;
  v_ = v_prev_ = x_to_v * x_init;

  time_step_ = time_step_prev_ = timeStep();
  debugPrint(debug_, "ccs_dcalc", 1, "time step %s", delayAsString(time_step_, this));

  MatrixSd A(order, order);
  A = G + (2.0 / time_step_) * C;
  A.makeCompressed();
  SparseLU<MatrixSd> A_solver;
  A_solver.compute(A);

  // Initial time depends on ceff which impact delay, so use a sim step
  // to find an initial ceff.
  setPortCurrents();
  VectorXd rhs(order);
  rhs = B * u_ + (1.0 / time_step_) * C * (3.0 * x_prev - x_prev2);
  x = A_solver.solve(rhs);
  v_ = x_to_v * x;

  updateCeffIdrvr();
  x = x_prev = x_prev2 = x_init;
  v_ = v_prev_ = x_to_v * x_init;

  // voltageTime is always for a rising waveform so 0.0v is initial voltage.
  double time_begin = output_waveforms_[0]->voltageTime((*dcalc_args_)[0].inSlewFlt(),
                                                        ceff_[0], 0.0);
  // Limit in case load voltage waveforms don't get to final value.
  double time_end = time_begin + maxTime();

  if (make_waveforms_)
    recordWaveformStep(time_begin);

  for (double time = time_begin; time <= time_end; time += time_step_) {
    setPortCurrents();
    rhs = B * u_ + (1.0 / time_step_) * C * (3.0 * x_prev - x_prev2);
    x = A_solver.solve(rhs);
    v_ = x_to_v * x;
    
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[0];
    debugPrint(debug_, "ccs_dcalc", 3, "%s ceff %s VDrvr %.4f Idrvr %s",
               delayAsString(time, this),
               units_->capacitanceUnit()->asString(ceff_[0]),
               voltage(dcalc_arg.drvrPin()),
               units_->currentUnit()->asString(drvr_current_[0], 4));

    updateCeffIdrvr();

    measureThresholds(time);
    if (make_waveforms_)
      recordWaveformStep(time);

    if (loadWaveformsFinished())
      break;

    time_step_prev_ = time_step_;
    x_prev2.swap(x_prev);
    x_prev.swap(x);
    v_prev_.swap(v_);
  }
}

double
PrimaDelayCalc::timeStep()
{
  // Needs to use LTE for time step dynamic control.
  return driverResistance() * load_cap_ * .02;
}

double
PrimaDelayCalc::maxTime()
{
  return (*dcalc_args_)[0].inSlewFlt()
    + (driverResistance() + resistance_sum_) * load_cap_ * 4;
}

float
PrimaDelayCalc::driverResistance()
{
  const Pin *drvr_pin = (*dcalc_args_)[0].drvrPin();
  LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  const MinMax *min_max = dcalc_ap_->delayMinMax();
  return drvr_port->driveResistance(drvr_rf_, min_max);
}

void
PrimaDelayCalc::initSim()
{
  ceff_.resize(drvr_count_);
  drvr_current_.resize(drvr_count_);

  findNodeCount();
  setOrder();

  // Reset waveform recording.
  times_.clear();
  drvr_voltages_.clear();
  load_voltages_.clear();

  measure_thresholds_ = {vl_, vth_, vh_};
}

void
PrimaDelayCalc::findNodeCount()
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
    if (node1
        && !parasitics_->isExternal(node1)) {
      size_t node_idx = node_index_map_[node1];
      node_capacitances_[node_idx] += cap;
    }
    ParasiticNode *node2 = parasitics_->node2(capacitor);
    if (node2
        && !parasitics_->isExternal(node2)) {
      size_t node_idx = node_index_map_[node2];
      node_capacitances_[node_idx] += cap;
    }
  }
  node_count_ = node_index_map_.size();
}

float
PrimaDelayCalc::pinCapacitance(ParasiticNode *node)
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
PrimaDelayCalc::setOrder()
{
  port_count_ = drvr_count_;
  order_ = node_count_ + port_count_;

  // Matrix resize also zeros.
  G_.resize(order_, order_);
  C_.resize(order_, order_);
  B_.resize(order_, port_count_);
  u_.resize(port_count_);
  threshold_times_.resize(node_count_);
}

void
PrimaDelayCalc::initCeffIdrvr()
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    ceff_[drvr_idx] = load_cap_;
    // voltageTime is always for a rising waveform so 0.0v is initial voltage.
    drvr_current_[drvr_idx] =
      output_waveforms_[drvr_idx]->voltageCurrent(dcalc_arg.inSlewFlt(),
                                                  ceff_[drvr_idx], 0.0);
  }
}

void
PrimaDelayCalc::setXinit()
{
  x_init_.resize(order_);
  double drvr_init_volt = (drvr_rf_ == RiseFall::rise()) ? 0.0 : vdd_;
  // Init node voltages.
  for (size_t n = 0; n < node_count_ + port_count_; n++)
    x_init_[n] = drvr_init_volt;
  // Init port voltages.
  for (size_t p = 0; p < port_count_; p++)
    x_init_[node_count_ + p] = drvr_init_volt;
}

void
PrimaDelayCalc::stampEqns()
{
  G_.setZero();
  C_.setZero();
  B_.setZero();

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

  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    size_t drvr_node = pin_node_map_[dcalc_arg.drvrPin()];
    G_.coeffRef(node_count_ + drvr_idx, drvr_node) = 1.0;
    G_.coeffRef(node_count_ + drvr_idx, node_count_ + drvr_idx) = -1.0;
    // special sauce
    G_.coeffRef(drvr_node, drvr_node) += 1e-6;
    B_.coeffRef(drvr_node, drvr_idx) = 1.0;
  }

  if (debug_->check("ccs_dcalc", 3)) {
    reportMatrix("G", G_);
    reportMatrix("C", C_);
    reportMatrix("B", B_);
  }
}

// Grounded resistor.
void
PrimaDelayCalc::stampConductance(size_t n1,
                                 double g)
{
  G_.coeffRef(n1, n1) += g;
}

// Floating resistor.
void
PrimaDelayCalc::stampConductance(size_t n1,
                                 size_t n2,
                                 double g)
{
  G_.coeffRef(n1, n1) += g;
  G_.coeffRef(n2, n2) += g;
  G_.coeffRef(n1, n2) -= g;
  G_.coeffRef(n2, n1) -= g;
}

// Grounded capacitance.
void
PrimaDelayCalc::stampCapacitance(size_t n1,
                                 double cap)
{
  C_.coeffRef(n1, n1) += cap;
}

// Floating capacitance.
void
PrimaDelayCalc::stampCapacitance(size_t n1,
                                 size_t n2,
                                 double cap)
{
  C_.coeffRef(n1, n1) += cap;
  C_.coeffRef(n2, n2) += cap;
  C_.coeffRef(n1, n2) -= cap;
  C_.coeffRef(n2, n1) -= cap;
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::setPortCurrents()
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++)
    u_[drvr_idx] = drvr_current_[drvr_idx];
}

void
PrimaDelayCalc::updateCeffIdrvr()
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    size_t node_idx = pin_node_map_[drvr_pin];
    double drvr_current = drvr_current_[drvr_idx];
    double v1 = voltage(node_idx);
    double v2 = voltagePrev(node_idx);
    double dv = v1 - v2;
    if (drvr_rf_ == RiseFall::rise()) {
      if (drvr_current != 0.0
          && dv > 0.0) {
        double ceff = drvr_current * time_step_ / dv;
        if (output_waveforms_[drvr_idx]->capAxis()->inBounds(ceff))
          ceff_[drvr_idx] = ceff;
      }
      if (v1 > (vdd_ - .01))
        // Whoa partner. Head'n for the weeds.
        drvr_current_[drvr_idx] = 0.0;
      else
        drvr_current_[drvr_idx] =
          output_waveforms_[drvr_idx]->voltageCurrent(dcalc_arg.inSlewFlt(),
                                                      ceff_[drvr_idx], v1);
    }
    else {
      if (drvr_current != 0.0
          && dv < 0.0) {
        double ceff = drvr_current * time_step_ / dv;
        if (output_waveforms_[drvr_idx]->capAxis()->inBounds(ceff))
          ceff_[drvr_idx] = ceff;
      }
      if (v1 < 0.01) {
        // Whoa partner. Head'n for the weeds.
        drvr_current_[drvr_idx] = 0.0;
      }
      else
        drvr_current_[drvr_idx] =
          output_waveforms_[drvr_idx]->voltageCurrent(dcalc_arg.inSlewFlt(),
                                                      ceff_[drvr_idx],
                                                      vdd_ - v1);
    }
  }
}

bool
PrimaDelayCalc::loadWaveformsFinished()
{
  for (auto pin_node : pin_node_map_) {
    size_t node_idx = pin_node.second;
    double v = voltage(node_idx);
    if ((drvr_rf_ == RiseFall::rise()
         && v < vh_ + (vdd_ - vh_) * .5)
        || (drvr_rf_ == RiseFall::fall()
            && (v > vl_ * .5))) {
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::measureThresholds(double time)
{
  for (auto pin_node1 : pin_node_map_) {
    size_t node_idx = pin_node1.second;
    double v = voltage(node_idx);
    double v_prev = voltagePrev(node_idx);
    for (size_t m = 0; m < measure_threshold_count_; m++) {
      double th = measure_thresholds_[m];
      if ((v_prev < th && th <= v)
          || (v_prev > th && th >= v)) {
        double t_cross = time - time_step_ + (th - v_prev) * time_step_ / (v - v_prev);
        debugPrint(debug_, "ccs_measure", 1, "node %lu cross %.2f %s",
                   node_idx,
                   th,
                   delayAsString(t_cross, this));
        threshold_times_[node_idx][m] = t_cross;
      }
    }
  }
}

double
PrimaDelayCalc::voltage(const Pin *pin)
{
  size_t node_idx = pin_node_map_[pin];
  return v_[node_idx];
}

double
PrimaDelayCalc::voltage(size_t node_idx)
{
  return v_[node_idx];
}

double
PrimaDelayCalc::voltagePrev(size_t node_idx)
{
  return v_prev_[node_idx];
}

ArcDcalcResultSeq
PrimaDelayCalc::dcalcResults()
{
  ArcDcalcResultSeq dcalc_results(drvr_count_);
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    const LibertyLibrary *drvr_library = dcalc_arg.drvrLibrary();
    size_t drvr_node = pin_node_map_[drvr_pin];
    ThresholdTimes &drvr_times = threshold_times_[drvr_node];
    float ref_time = output_waveforms_[drvr_idx]->referenceTime(dcalc_arg.inSlewFlt());
    ArcDelay gate_delay = drvr_times[threshold_vth] - ref_time;
    Slew drvr_slew = abs(drvr_times[threshold_vh] - drvr_times[threshold_vl]);
    dcalc_result.setGateDelay(gate_delay);
    dcalc_result.setDrvrSlew(drvr_slew);
    debugPrint(debug_, "ccs_dcalc", 2,
               "%s gate delay %s slew %s",
               network_->pathName(drvr_pin),
               delayAsString(gate_delay, this),
               delayAsString(drvr_slew, this));

    dcalc_result.setLoadCount(load_pin_index_map_->size());
    for (auto load_pin_index : *load_pin_index_map_) {
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

      thresholdAdjust(load_pin, drvr_library, drvr_rf_, wire_delay, load_slew);
      dcalc_result.setWireDelay(load_idx, wire_delay);
      dcalc_result.setLoadSlew(load_idx, load_slew);
    }
  }
  return dcalc_results;
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::setPrimaReduceOrder(size_t order)
{
  prima_order_ = order;
}

// This version fills in one column of the orthonomal matrix
// at a time as in the Gram-Schmidt wikipedia algorithm.
void
PrimaDelayCalc::primaReduce()
{
  G_.makeCompressed();
  // Step 3: solve G*R = B for R
  SparseLU<MatrixSd> G_solver(G_);
  if (G_solver.info() != Eigen::Success)
    report_->error(1752, "G matrix is singular.");
  MatrixXd R(order_, port_count_);
  R = G_solver.solve(B_);

  // Step 4
  HouseholderQR<MatrixXd> R_solver(R);
  MatrixXd Q = R_solver.householderQ();

  // Vq is "X" in the prima paper (too many "x" variables in the paper).
  Vq_.resize(order_, prima_order_);
  // Vq = first port_count columns of Q.
  Vq_.block(0, 0, order_, port_count_) = Q.block(0, 0, order_, port_count_);

  // Step 6 - Arnolid iteration
  for (size_t k = 1; k < prima_order_; k++) {
    VectorXd V = C_ * Vq_.col(k - 1);
    Vq_.col(k) = G_solver.solve(V);

    // Modified Gram-Schmidt orthonormalization
    for (size_t j = 0; j < k; j++) {
      double H = Vq_.col(j).transpose() * Vq_.col(k);
      Vq_.col(k) = Vq_.col(k) - H * Vq_.col(j);
    }
    VectorXd Vq_k = Vq_.col(k);
    HouseholderQR<MatrixXd> Vq_k_solver(Vq_k);
    MatrixXd VqQ = Vq_k_solver.householderQ();
    Vq_.col(k) = VqQ.col(0);
  }

  // Step 8 - Matrix projection
  MatrixSd Vqs = Vq_.sparseView();
  Cq_ = Vqs.transpose() * C_ * Vqs;
  Gq_ = Vqs.transpose() * G_ * Vqs;
  Bq_ = Vqs.transpose() * B_;

  // x = Vq * x~
  // solve x_init = Vq * x~_init for x~_init
  xq_init_ = Vq_.colPivHouseholderQr().solve(x_init_);

  if (debug_->check("ccs_dcalc", 3)) {
    reportMatrix("Vq", Vq_);
    reportMatrix("G~", Gq_);
    reportMatrix("C~", Cq_);
    reportMatrix("B~", Bq_);
  }
}

// This version fills in port_count columns of the orthonomal matrix
// at a time as shown in the prima algorithm figure 4.
void
PrimaDelayCalc::primaReduce2()
{
  G_.makeCompressed();
  // Step 3: solve G*R = B for R
  SparseLU<MatrixSd> G_solver(G_);
  MatrixXd R(order_, port_count_);
  R = G_solver.solve(B_);

  // Step 4
  HouseholderQR<MatrixXd> R_solver(R);
  MatrixXd Q = R_solver.householderQ();

  // Vq is "X" in the prima paper (too many "x" variables in the paper).
  size_t n = ceil(prima_order_ / static_cast<double>(port_count_));
  MatrixXd Vq(order_, n * port_count_);
  // // Vq = first port_count columns of Q.
  Vq.block(0, 0, order_, port_count_) = Q.block(0, 0, order_, port_count_);

  // Step 6 - Arnolid iteration
  for (size_t k = 1; k < n; k++) {
    MatrixXd V = C_ * Vq.block(0, (k - 1) * port_count_, order_, port_count_);
    MatrixXd GV = G_solver.solve(V);
    Vq.block(0, k * port_count_, order_, port_count_) = GV;

    // Modified Gram-Schmidt orthonormalization
    for (size_t j = 0; j < k; j++) {
      MatrixXd H = Vq.block(0, j * port_count_, order_, port_count_).transpose()
        * Vq.block(0, k * port_count_, order_, port_count_);
      Vq.block(0, k * port_count_, order_, port_count_) =
        Vq.block(0, k * port_count_, order_, port_count_) - Vq.block(0, j * port_count_, order_, port_count_) * H;
    }
    MatrixXd Vq_k = Vq.block(0, k * port_count_, order_, port_count_);
    HouseholderQR<MatrixXd> Vq_k_solver(Vq_k);
    MatrixXd VqQ = Vq_k_solver.householderQ();
    Vq.block(0, k * port_count_, order_, port_count_) = 
      VqQ.block(0, 0, order_, port_count_);
  }
  Vq_.resize(order_, prima_order_);
  Vq_ = Vq.block(0, 0, order_, prima_order_);

  // Step 8 - Matrix projection
  MatrixSd Vqs = Vq_.sparseView();
  Cq_ = Vqs.transpose() * C_ * Vqs;
  Gq_ = Vqs.transpose() * G_ * Vqs;
  Bq_ = Vqs.transpose() * B_;

  // x = Vq * x~
  // solve x_init = Vq * x~_init for x~_init
  xq_init_ = Vq_.colPivHouseholderQr().solve(x_init_);

  if (debug_->check("ccs_dcalc", 3)) {
    reportMatrix("Vq", Vq_);
    reportMatrix("G~", Gq_);
    reportMatrix("C~", Cq_);
    reportMatrix("B~", Bq_);
  }
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::recordWaveformStep(double time)
{
  times_.push_back(time);
  if (waveform_drvr_pin_) {
    double drvr_v = voltage(waveform_drvr_pin_);
    drvr_voltages_.push_back(drvr_v);
  }
  if (waveform_load_pin_) {
    double load_v = voltage(waveform_load_pin_);
    load_voltages_.push_back(load_v);
  }
  for (auto &pin_wave : watch_pin_values_) {
    const Pin *pin = pin_wave.first;
    FloatSeq &waveform = pin_wave.second;
    double pin_v = voltage(pin);
    waveform.push_back(pin_v);
  }
}

////////////////////////////////////////////////////////////////

string
PrimaDelayCalc::reportGateDelay(const Pin *drvr_pin,
                                const TimingArc *arc,
                                const Slew &in_slew,
                                float load_cap,
                                const Parasitic *,
                                const LoadPinIndexMap &,
                                const DcalcAnalysisPt *dcalc_ap,
                                int digits)
{
  GateTimingModel *model = arc->gateModel(dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pinPvt(drvr_pin, dcalc_ap), in_slew1, load_cap,
                                  false, digits);
  }
  return "";
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::watchPin(const Pin *pin)
{
  watch_pin_values_[pin] = FloatSeq();
  make_waveforms_ = true;
}

void
PrimaDelayCalc::clearWatchPins()
{
  watch_pin_values_.clear();
  make_waveforms_ = false;
}

PinSeq
PrimaDelayCalc::watchPins() const
{
  PinSeq pins;
  for (auto pin_values : watch_pin_values_) {
    const Pin *pin = pin_values.first;
    pins.push_back(pin);
  }
  return pins;
}

Waveform
PrimaDelayCalc::watchWaveform(const Pin *pin)
{
  FloatSeq &voltages = watch_pin_values_[pin];
  TableAxisPtr time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                  new FloatSeq(times_));
  Table1 waveform(new FloatSeq(voltages), time_axis);
  return waveform;
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::reportMatrix(const char *name,
                             MatrixSd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportMatrix(const char *name,
                             MatrixXd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportMatrix(const char *name,
                             VectorXd &matrix)
{
  report_->reportLine("%s", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportVector(const char *name,
                             vector<double> &matrix)
{
  report_->reportLine("%s", name);
  reportVector(matrix);
}
  
void
PrimaDelayCalc::reportMatrix(MatrixSd &matrix)
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
PrimaDelayCalc::reportMatrix(MatrixXd &matrix)
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
PrimaDelayCalc::reportMatrix(VectorXd &matrix)
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
PrimaDelayCalc::reportVector(vector<double> &matrix)
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
