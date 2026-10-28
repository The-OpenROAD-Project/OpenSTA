// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "PrimaDelayCalc.hh"

#include <Eigen/LU>
#include <Eigen/QR>
#include <algorithm> // swap
#include <cmath>  // abs
#include <limits>
#include <memory>
#include <string_view>

#include "Debug.hh"
#include "DmpDelayCalc.hh"
#include "Format.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "PortDirection.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "TimingArc.hh"
#include "Units.hh"

// “Electronic Circuit & System Simulation Methods”
// Lawrence Pillage, 1998, McGraw-Hill, Inc. New York, NY.

// "PRIMA: Passive Reduced-order Interconnect Macromodeling Algorithm",
// Altan Odabasioglu, Mustafa Celik, and Lawrence T. Pileggi
// IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems,
// vol. 17, no. 8, August 1998

namespace sta {

using ParasiticSet = std::set<const Parasitic*>;

ArcDelayCalc *
makePrimaDelayCalc(StaState *sta)
{
  return new PrimaDelayCalc(sta);
}

PrimaDelayCalc::PrimaDelayCalc(StaState *sta) :
  DelayCalcBase(sta),
  pin_node_map_(network_),
  watch_pin_values_(network_),
  table_dcalc_(makeDmpCeffElmoreDelayCalc(sta))
{
}

PrimaDelayCalc::PrimaDelayCalc(const PrimaDelayCalc &dcalc) :
  DelayCalcBase(dcalc),
  pin_node_map_(network_),
  prima_order_(dcalc.prima_order_),
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
                              const Scene *scene,
                              const MinMax *min_max)
{
  const Sdc *sdc = scene->sdc();
  Parasitics *parasitics = scene->parasitics(min_max);
  if (parasitics == nullptr
      // set_load net has precedence over parasitics.
      || network_->direction(drvr_pin)->isInternal())
    return nullptr;
  Parasitic *parasitic = parasitics->findParasiticNetwork(drvr_pin);
  if (parasitic)
    return parasitic;
  Wireload *wireload = sdc->wireload(min_max);
  if (wireload) {
    float pin_cap, wire_cap, fanout;
    bool has_wire_cap;
    graph_delay_calc_->netCaps(drvr_pin, rf, scene, min_max, pin_cap, wire_cap,
                               fanout, has_wire_cap);
    parasitic =
        parasitics->makeWireloadNetwork(drvr_pin, wireload, fanout, scene, min_max);
  }
  return parasitic;
}

Parasitic *
PrimaDelayCalc::reduceParasitic(const Parasitic *,
                                const Pin *,
                                const RiseFall *,
                                const Scene *,
                                const MinMax *)
{
  // Parasitics API does not support reduced matrix models.
  return nullptr;
}

ArcDcalcResult
PrimaDelayCalc::inputPortDelay(const Pin *drvr_pin,
                               float in_slew,
                               const RiseFall *rf,
                               const Parasitic *parasitic,
                               const LoadPinIndexMap &load_pin_index_map,
                               const Scene *scene,
                               const MinMax *min_max)
{
  Parasitics *parasitics = scene->parasitics(min_max);
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();
  const Parasitic *pi_elmore = nullptr;
  if (parasitic && parasitics->isParasiticNetwork(parasitic))
    pi_elmore = parasitics->reduceToPiElmore(parasitic, drvr_pin,
                                             rf, scene, min_max);

  for (const auto [load_pin, load_idx] : load_pin_index_map) {
    double wire_delay = 0.0;
    double load_slew = in_slew;
    bool elmore_exists = false;
    float elmore = 0.0;
    if (pi_elmore)
      parasitics->findElmore(pi_elmore, load_pin, elmore, elmore_exists);
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
                          const Scene *scene,
                          const MinMax *min_max)
{
  ArcDcalcArgSeq dcalc_args;
  dcalc_args.emplace_back(nullptr, drvr_pin, nullptr, arc, in_slew, load_cap,
                          parasitic);
  dcalc_args[0].setSceneArc(scene, min_max);
  ArcDcalcResultSeq dcalc_results = gateDelays(dcalc_args, load_pin_index_map,
                                               scene, min_max);
  return dcalc_results[0];
}

ArcDcalcResultSeq
PrimaDelayCalc::gateDelays(ArcDcalcArgSeq &dcalc_args,
                           const LoadPinIndexMap &load_pin_index_map,
                           const Scene *scene,
                           const MinMax *min_max)
{
  dcalc_args_ = &dcalc_args;
  drvr_arg_ = &dcalc_args.front();
  load_pin_index_map_ = &load_pin_index_map;
  drvr_count_ = dcalc_args.size();
  scene_ = scene;
  min_max_ = min_max;
  drvr_rf_ = drvr_arg_->arc()->toEdge()->asRiseFall();
  parasitic_network_ = drvr_arg_->parasitic();
  load_cap_ = drvr_arg_->loadCap();
  parasitics_ = scene->parasitics(min_max);
  node_index_map_ = NodeIndexMap(ParasiticNodeLess(parasitics_, network_));

  bool arg_fail = checkArgs(dcalc_args, scene, min_max);
  if (arg_fail)
    return tableDcalcResults();
  else {
    simulate();
    return dcalcResults();
  }
}

// Return true on failure.
// Use falureReason() to get failure string.
bool
PrimaDelayCalc::checkArgs(ArcDcalcArgSeq &dcalc_args,
                          const Scene *scene,
                          const MinMax *min_max)
{
  drvr_count_ = dcalc_args.size();
  output_waveforms_.resize(drvr_count_);
  failure_reason_.clear();
  failure_arg_ = nullptr;
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = dcalc_args[drvr_idx];
    GateTableModel *table_model = dcalc_arg.arc()->gateTableModel(scene, min_max);
    if (table_model) {
      if (dcalc_arg.parasitic()) {
        OutputWaveforms *output_waveforms = table_model->outputWaveforms();
        float in_slew = dcalc_arg.inSlewFlt();
        if (output_waveforms) {
          const LibertyLibrary *drvr_library = dcalc_arg.drvrLibrary();
          float vdd;
          bool vdd_exists;
          drvr_library->supplyVoltage("VDD", vdd, vdd_exists);
          if (vdd_exists) {
            if (drvr_idx == 0) {
              // Assume drivers are in the same library.
              const RiseFall *drvr_rf = dcalc_arg.drvrEdge();
              vdd_ = vdd;
              vth_ = drvr_library->outputThreshold(drvr_rf) * vdd_;
              vl_ = drvr_library->slewLowerThreshold(drvr_rf) * vdd_;
              vh_ = drvr_library->slewUpperThreshold(drvr_rf) * vdd_;
            }
          }
          else {
            failure_reason_ = "vdd not defined";
            failure_arg_ = &dcalc_arg;
          }

          // Bounds check because extrapolating waveforms does not work for shit.
          if (output_waveforms->slewAxis()->inBounds(in_slew)) {
            if (output_waveforms->capAxis()->inBounds(dcalc_arg.loadCap())) {
              output_waveforms_[drvr_idx] = output_waveforms;
              debugPrint(debug_, "prima", 1, "{} {} {}",
                         dcalc_arg.drvrCell()->name(),
                         dcalc_arg.drvrEdge()->to_string().c_str(),
                         scene->name());
              LibertyCell *drvr_cell = dcalc_arg.drvrCell();
              drvr_cell->ensureVoltageWaveforms(scenes_);
            }
            else {
              failure_reason_ = "load cap out of bounds";
              failure_arg_ = &dcalc_arg;
            }
          }
          else {
            failure_reason_ = "input slew out of bounds";
            failure_arg_ = &dcalc_arg;
          }
        }
        else {
          failure_reason_ = "no output waveforms";
          failure_arg_ = &dcalc_arg;
        }
      }
      else {
        failure_reason_ = "no parasitic";
        failure_arg_ = &dcalc_arg;
      }
    }
    else {
      failure_reason_ = "no table model";
      failure_arg_ = &dcalc_arg;
    }
  }
  if (!failure_reason_.empty())
    debugPrint(debug_,"prima", 1, "arg check failed {}.", failure_reason_);
  return !failure_reason_.empty();
}

std::string
PrimaDelayCalc::failureReason()
{
  const Pin *drvr_pin = failure_arg_->drvrPin();
  const Instance *inst = network_->instance(drvr_pin);
  LibertyPort *from = failure_arg_->arc()->from();
  LibertyPort *to = failure_arg_->arc()->to();
  return sta::format("{} {} -> {} {}",
                     sdc_network_->pathName(inst),
                     from->name(),
                     to->name(),
                     failure_reason_);
}

ArcDcalcResultSeq
PrimaDelayCalc::tableDcalcResults()
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    if (drvr_pin) {
      const RiseFall *rf = dcalc_arg.drvrEdge();
      const Parasitic *parasitic =
          table_dcalc_->findParasitic(drvr_pin, rf, scene_, min_max_);
      dcalc_arg.setParasitic(parasitic);
    }
  }
  return table_dcalc_->gateDelays(*dcalc_args_, *load_pin_index_map_, scene_,
                                  min_max_);
}

void
PrimaDelayCalc::simulate()
{
  initSim();
  stampEqns();
  setXinit();

  if (prima_order_ > 0
      && node_count_ > prima_order_) {
    primaReduce2();
    simulate1(Gq_, Cq_, Bq_, xq_init_, Vq_, prima_order_);
  }
  else {
    Eigen::MatrixXd x_to_v = Eigen::MatrixXd::Identity(order_, order_);
    simulate1(G_, C_, B_, x_init_, x_to_v, order_);
  }
}

void
PrimaDelayCalc::simulate1(const MatrixSd &G,
                          const MatrixSd &C,
                          const Eigen::MatrixXd &B,
                          const Eigen::VectorXd &x_init,
                          const Eigen::MatrixXd &x_to_v,
                          size_t order)
{
  Eigen::VectorXd x(order);
  Eigen::VectorXd x_prev1(order);
  Eigen::VectorXd x_prev2(order);

  v_.resize(order);
  v_prev1_.resize(order);
  v_prev2_.resize(order);

  initCeffIdrvr();
  x = x_prev1 = x_prev2 = x_init;
  v_ = v_prev1_ = v_prev2_ = x_to_v * x_init;

  MatrixSd A(order, order);
  Eigen::SparseLU<MatrixSd> A_solver;

  // Backward Euler bootstrap (initial ceff).
  double boot_coeff = 1.0 / time_step_;
  A = G + boot_coeff * C;
  A.makeCompressed();
  A_solver.compute(A);

  Eigen::VectorXd rhs(order);
  rhs = B * u_ + boot_coeff * C * x_prev1;
  x = A_solver.solve(rhs);
  v_ = x_to_v * x;

  updateCeffIdrvr(time_begin_);
  x = x_prev1 = x_prev2 = x_init;
  v_ = v_prev1_ = v_prev2_ = x_to_v * x_init;

  recordWaveformStep(time_begin_ - time_step_);

  // Fixed step: BE for first march step; uniform BDF2 thereafter (stable vs
  // uneven-grid BDF2 with per-step dt, which rings on stiff CCS).
  // BDF2: (G + 3*C/(2*h))*x_{n+1} = B*u + C/(2*h)*(4*x_n - x_{n-1}).
  for (int march_step = 0; ; march_step++) {
    double time = time_begin_ + time_step_ * march_step;
    if (time > time_end_)
      break;
    debugPrint(debug_, "prima", 3, "{}", delayAsString(time, this));

    if (march_step == 0)
      rhs = B * u_ + (1.0 / time_step_) * C * x_prev1;
    else {
      if (march_step == 1) {
        A = G + (3.0 / (2.0 * time_step_)) * C;
        A.makeCompressed();
        A_solver.compute(A);
      }
      rhs = B * u_ + (1.0 / (2.0 * time_step_)) * C * (4.0 * x_prev1 - x_prev2);
    }
    x = A_solver.solve(rhs);
    v_ = x_to_v * x;

    updateCeffIdrvr(time);

    measureThresholds(time);
    recordWaveformStep(time);

    if (loadWaveformsFinished())
      break;

    time_step_prev_ = time_step_;
    x_prev2.swap(x_prev1);
    x_prev1.swap(x);
    v_prev2_.swap(v_prev1_);
    v_prev1_.swap(v_);
  }
}

float
PrimaDelayCalc::maxParasiticPathResistance(const ArcDcalcArg &dcalc_arg)
{
  const Pin *drvr_pin = dcalc_arg.drvrPin();
  const Parasitic *parasitic = dcalc_arg.parasitic();
  ParasiticNode *drvr_node = parasitics_->findParasiticNode(parasitic, drvr_pin);
  ParasiticNodeResistorMap resistor_map=parasitics_->parasiticNodeResistorMap(parasitic);
  ParasiticNodeSet visited_nodes;
  return parasiticDFS(drvr_node, nullptr, resistor_map, visited_nodes);
}

float
PrimaDelayCalc::parasiticDFS(ParasiticNode *node,
                             const ParasiticResistor *from_res,
                             ParasiticNodeResistorMap &resistor_map,
                             ParasiticNodeSet &visited_nodes)
{
  float max_res = 0.0;
  visited_nodes.insert(node);
  ParasiticResistorSeq &resistors = resistor_map[node];
  for (ParasiticResistor *resistor : resistors) {
    ParasiticNode *onode = parasitics_->otherNode(resistor, node);
    if (resistor != from_res
        && !visited_nodes.contains(onode)) {
      float r = parasitics_->value(resistor);
      max_res = std::max(max_res, r + parasiticDFS(onode, resistor,
                                                   resistor_map, visited_nodes));
    }
  }
  visited_nodes.erase(node);
  return max_res;
}

float
PrimaDelayCalc::driverResistance(const ArcDcalcArg &dcalc_arg)
{
  const Pin *drvr_pin = dcalc_arg.drvrPin();
  LibertyPort *drvr_port = network_->libertyPort(drvr_pin);
  LibertyPort *scene_port = drvr_port->scenePort(scene_, min_max_);
  if (scene_port == nullptr) {
    report_->warn(1754, "Port {}/{}/{} has no equivalent in scene {}.",
                  drvr_port->libertyLibrary()->name(),
                  drvr_port->libertyCell()->name(),
                  drvr_port->name(),
                  scene_->name());
    scene_port = drvr_port;
  }
  const RiseFall *drvr_rf = drvr_arg_->drvrEdge();
  return scene_port->driveResistance(drvr_rf, min_max_);
}

void
PrimaDelayCalc::initSim()
{
  ceff_.resize(drvr_count_);
  ceff_vth_.assign(drvr_count_, 0.0);
  drvr_current_.resize(drvr_count_);
  drvr_current1_.resize(drvr_count_);
  waveform_begin_.resize(drvr_count_);
  waveform_end_.resize(drvr_count_);

  findNodeCount();
  setOrder();

  // Reset waveform recording.
  times_.clear();
  measure_thresholds_ = {vl_, vth_, vh_};
}

void
PrimaDelayCalc::findNodeCount()
{
  coupling_cap_multiplier_ = 1.0;

  pin_node_map_.clear();
  node_index_map_.clear();
  node_count_ = 0;

  // Collect the nodes that enter G by walking out from the drivers through
  // resistors. G is conductance-only, so a node with no resistive path to a
  ParasiticSet visited_parasitics;
  for (const ArcDcalcArg &dcalc_arg : *dcalc_args_) {
    const Parasitic *parasitic = dcalc_arg.parasitic();
    if (!visited_parasitics.contains(parasitic)) {
      ParasiticNodeResistorMap resistor_map =
        parasitics_->parasiticNodeResistorMap(parasitic);
      std::vector<ParasiticNode *> queue;
      for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
        const Pin *drvr_pin = (*dcalc_args_)[drvr_idx].drvrPin();
        ParasiticNode *drvr_node =
          parasitics_->findParasiticNode(parasitic, drvr_pin);
        if (drvr_node && !parasitics_->isExternal(drvr_node)
            && !node_index_map_.contains(drvr_node)) {
          placeNode(drvr_node, node_count_++);
          queue.push_back(drvr_node);
        }
      }
      while (!queue.empty()) {
        ParasiticNode *node = queue.back();
        queue.pop_back();
        size_t node_index = node_index_map_[node];
        auto resistor_itr = resistor_map.find(node);
        if (resistor_itr != resistor_map.end()) {
          for (ParasiticResistor *resistor : resistor_itr->second) {
            ParasiticNode *next_node = parasitics_->otherNode(resistor, node);
            if (next_node
                && !parasitics_->isExternal(next_node)
                && !node_index_map_.contains(next_node)) {
              bool shorted = parasitics_->value(resistor) == 0;
              placeNode(next_node, shorted ? node_index : node_count_++);
              queue.push_back(next_node);
            }
          }
        }
      }
      visited_parasitics.insert(parasitic);
    }
  }
}

// Add node to network at index (shared by drivers and by the
// resistor walk). A merged short reuses the near node's index.
void
PrimaDelayCalc::placeNode(ParasiticNode *node,
                          size_t index)
{
  node_index_map_[node] = index;
  const Pin *pin = parasitics_->pin(node);
  if (pin) {
    pin_node_map_[pin] = index;
    debugPrint(debug_, "prima", 1, "pin {} node {}",
               network_->pathName(pin), index);
  }
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
  // Initialize thhresold crossings to unset.
  ThresholdTimes unset_times;
  unset_times.fill(unset_value_);
  threshold_times_.assign(node_count_, unset_times);
}

void
PrimaDelayCalc::initCeffIdrvr()
{
  time_begin_ = std::numeric_limits<double>::max();
  time_end_ = 0.0;
  time_step_ = std::numeric_limits<double>::max();
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    float in_slew = delayAsFloat(dcalc_arg.inSlew());
    float load_cap = dcalc_arg.loadCap();
    float ceff = load_cap;
    ceff_[drvr_idx] = ceff;

    float input_delay = dcalc_arg.inputDelay();
    double waveform_begin = input_delay
      + output_waveforms_[drvr_idx]->beginTime(in_slew, ceff);
    waveform_begin_[drvr_idx] = waveform_begin;
    double waveform_end = input_delay
      + output_waveforms_[drvr_idx]->endTime(in_slew, ceff);
    waveform_end_[drvr_idx] = waveform_end;
    debugPrint(debug_, "prima", 3,
               "{} waveform begin {} end {}",
               network_->pathName(dcalc_arg.drvrPin()),
               delayAsString(waveform_begin, this),
               delayAsString(waveform_end, this));

    double drvr_current = output_waveforms_[drvr_idx]->voltageCurrent(in_slew, ceff, 0.0);
    drvr_current1_[drvr_idx] = drvr_current;
    drvr_current_[drvr_idx] = drvr_current;
    // Set port current.
    u_[drvr_idx] = drvr_current;

    time_begin_ = std::min(time_begin_, waveform_begin);
    double drvr_r = driverResistance(dcalc_arg);
    // Simulate at least to the end of the driver waveform so the
    // slew thresholds are crossed.
    double time_max = std::max(waveform_begin + delayAsFloat(dcalc_arg.inSlew()),
                               waveform_end)
      + (drvr_r + maxParasiticPathResistance(dcalc_arg)) * load_cap * 2.0;
    time_end_ = std::max(time_end_, time_max);
    time_step_ = std::min(time_step_, drvr_r * load_cap * .02);
  }
  time_step_prev_ = time_step_;

  debugPrint(debug_, "prima", 1, "time begin {} end {} step {}",
             delayAsString(time_begin_, this),
             delayAsString(time_end_, this),
             delayAsString(time_step_, this));
}

void
PrimaDelayCalc::setXinit()
{
  x_init_.resize(order_);

  // Init node voltages.
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    const RiseFall *drvr_rf = dcalc_arg.drvrEdge();
    double drvr_init_volt = (drvr_rf == RiseFall::rise()) ? 0.0 : vdd_;
    const Parasitic *parasitic = dcalc_arg.parasitic();
    size_t drvr_node = pin_node_map_[dcalc_arg.drvrPin()];
    x_init_[drvr_node] = drvr_init_volt;
    // Init port voltage.
    x_init_[node_count_ + drvr_idx] = drvr_init_volt;
    // Init parastic nodes.
    for (ParasiticNode *node : parasitics_->nodes(parasitic)) {
      if (!parasitics_->isExternal(node)) {
        auto [node_idx, exists] = nodeIndex(node);
        if (exists)
          x_init_[node_idx] = drvr_init_volt;
      }
    }
  }
}

std::pair<size_t, bool>
PrimaDelayCalc::nodeIndex(const ParasiticNode *node)
{
  auto node_index = node_index_map_.find(node);
  if (node_index != node_index_map_.end())
    return {node_index->second, true};
  else
    return {0, false};
}

void
PrimaDelayCalc::stampEqns()
{
  G_.setZero();
  C_.setZero();
  B_.setZero();

  // The stamps below do not visit the nodes in index order. Reserve room in
  // each column so an out of order coeffRef() insert does not shift the packed
  // array to make room, which makes stamping quadratic in the node count.
  constexpr int non_zero_entry_count = 8;
  G_.reserve(Eigen::VectorXi::Constant(order_, non_zero_entry_count));
  C_.reserve(Eigen::VectorXi::Constant(order_, non_zero_entry_count));

  NetSet drvr_nets(network_);
  for (ArcDcalcArg &dcalc_arg : *dcalc_args_) {
    const Net *net = dcalc_arg.drvrNet(network_);
    drvr_nets.insert(net);
  }

  ParasiticSet visited_parasitics;
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    stampDriver(dcalc_arg, drvr_idx);
    const Parasitic *parasitic = dcalc_arg.parasitic();
    if (!visited_parasitics.contains(parasitic)) {
      stampResistors(parasitic);
      stampCapacitors(parasitic, dcalc_arg, drvr_nets);
      visited_parasitics.insert(parasitic);
    }
  }

  if (debug_->check("prima", 4)) {
    reportMatrix("G", G_);
    reportMatrix("C", C_);
    reportMatrix("B", B_);
  }
}

void
PrimaDelayCalc::stampDriver(const ArcDcalcArg &dcalc_arg,
                            size_t drvr_idx)
{
    size_t drvr_node = pin_node_map_[dcalc_arg.drvrPin()];
    G_.coeffRef(node_count_ + drvr_idx, drvr_node) = 1.0;
    G_.coeffRef(node_count_ + drvr_idx, node_count_ + drvr_idx) = -1.0;
    // special sauce
    G_.coeffRef(drvr_node, drvr_node) += 1e-6;
    B_.coeffRef(drvr_node, drvr_idx) = 1.0;
}

void
PrimaDelayCalc::stampResistors(const Parasitic *parasitic)
{
  for (ParasiticResistor *resistor : parasitics_->resistors(parasitic)) {
    auto [node_idx1, exsits1] = nodeIndex(parasitics_->node1(resistor));
    auto [node_idx2, exsits2] = nodeIndex(parasitics_->node2(resistor));
    // Skip a resistor with a node left out of the network.
    if (exsits1 && exsits2) {
      float resistance = parasitics_->value(resistor);
      // Skip a self loop / merged short (same index) or a non-positive (short)
      // resistance; stamping 1/resistance would be infinite.
      if (node_idx1 != node_idx2 && resistance > 0.0)
        stampConductance(node_idx1, node_idx2, 1.0 / resistance);
    }
  }
}

void
PrimaDelayCalc::stampCapacitors(const Parasitic *parasitic,
                                const ArcDcalcArg &dcalc_arg,
                                NetSet &drvr_nets)
{
  const RiseFall *drvr_rf = dcalc_arg.drvrEdge();
  bool includes_pin_caps = parasitics_->includesPinCaps(parasitic);
  // Grounded capacitors.
  for (ParasiticNode *node : parasitics_->nodes(parasitic)) {
    if (!parasitics_->isExternal(node)) {
      auto [node_idx, exists] = nodeIndex(node);
      if (exists) {
        double cap = parasitics_->nodeGndCap(node);
        const Pin *pin = parasitics_->pin(node);
        if (pin)
          cap += pinCapacitance(pin, drvr_rf, includes_pin_caps);
        stampCapacitance(node_idx, cap);
      }
    }
  }

  // Coupling capcacitors.
  const Net *drvr_net = dcalc_arg.drvrNet(network_);
  for (ParasiticCapacitor *capacitor : parasitics_->capacitors(parasitic)) {
    ParasiticNode *node1 = parasitics_->node1(capacitor);
    ParasiticNode *node2 = parasitics_->node2(capacitor);
    float cap = parasitics_->value(capacitor);
    const Net *net1 = node1 ? parasitics_->net(node1, network_) : nullptr;
    const Net *net2 = node2 ? parasitics_->net(node2, network_) : nullptr;
    if (net2 == drvr_net) {
      std::swap(net1, net2);
      std::swap(node1, node2);
    }
    auto [node_idx1, exists1] = nodeIndex(node1);
    if (exists1) {
      if (net2 && drvr_nets.contains(net2)) {
        auto [node_idx2, exists2] = nodeIndex(node2);
        if (exists2)
          // Stamp half the capacitance because the coupled net will do the same.
          stampCapacitance(node_idx1, node_idx2, cap * .5);
      }
      else
        stampCapacitance(node_idx1, cap);
    }
  }
}

float
PrimaDelayCalc::pinCapacitance(const Pin *pin,
                               const RiseFall *rf,
                               bool includes_pin_caps)
{
  Port *port = network_->port(pin);
  LibertyPort *lib_port = network_->libertyPort(port);
  const Sdc *sdc = scene_->sdc();
  if (lib_port) {
    if (!includes_pin_caps)
      return sdc->pinCapacitance(pin, rf, scene_, min_max_);
  }
  else if (network_->isTopLevelPort(pin))
    return sdc->portExtCap(port, rf, min_max_);
  return 0.0;
}

// Grounded resistor.
void
PrimaDelayCalc::stampConductance(size_t n1,
                                 double g)
{
  // NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
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
PrimaDelayCalc::updateCeffIdrvr(double time)
{
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    const ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    const Pin *drvr_pin = dcalc_arg.drvrPin();
    const RiseFall *drvr_rf = dcalc_arg.drvrEdge();
    size_t node_idx = pin_node_map_[drvr_pin];
    double v1 = voltage(node_idx);
    double vdd = vdd_;
    float in_slew = delayAsFloat(dcalc_arg.inSlew());
    OutputWaveforms *output_waveform = output_waveforms_[drvr_idx];
    double drvr_current;
    if (time < waveform_begin_[drvr_idx]) {
      float hold_resistance = holdResistanceBefore(dcalc_arg);
      if (drvr_rf == RiseFall::rise())
        drvr_current = -v1 / hold_resistance;
      else
        drvr_current = (vdd_-v1) / hold_resistance;
      debugPrint(debug_, "prima", 3,
                 " {} ceff {} VDrvr {:.3f} Idrvr {} hold_r {} before",
                 drvr_idx,
                 units_->capacitanceUnit()->asString(ceff_[drvr_idx]),
                 v1,
                 units_->currentUnit()->asString(drvr_current, 4),
                 units_->resistanceUnit()->asString(hold_resistance));
    }
    else if (time > waveform_end_[drvr_idx]) {
      float hold_resistance = holdResistanceAfter(output_waveform);
      if (drvr_rf == RiseFall::rise())
        drvr_current = (vdd_-v1) / hold_resistance;
      else
        drvr_current = -v1 / hold_resistance;
      debugPrint(debug_, "prima", 3,
                 " {} ceff {} VDrvr {:.3f} Idrvr {} hold_r {} hold after",
                 drvr_idx,
                 units_->capacitanceUnit()->asString(ceff_[drvr_idx]),
                 v1,
                 units_->currentUnit()->asString(drvr_current, 4),
                 units_->resistanceUnit()->asString(hold_resistance));
    }
    else {
      float input_delay = dcalc_arg.inputDelay();
      double Icap = drvr_current_[drvr_idx] + drvr_current1_[drvr_idx];
      double v2 = voltagePrev2(node_idx);
      double dv = v1 - v2;
      if ((dv > 0.0 && Icap > 0.0)
          || (dv < 0.0 && Icap < 0.0)) {
        float ceff = Icap * time_step_ / dv;
        ceff = std::min(dcalc_arg.loadCap(), ceff);
        ceff_[drvr_idx] = ceff;
        waveform_end_[drvr_idx] = input_delay
          + output_waveforms_[drvr_idx]->endTime(in_slew, ceff);
        // Record the Ceff at Vth.
        if ((v1 <= vth_ && v2 > vth_)
            || (v1 >= vth_ && v2 < vth_))
          ceff_vth_[drvr_idx] = ceff;
      }
      std::string region;
      if (drvr_rf == RiseFall::rise()) {
        // Rising edge.
        if (v1 < vdd_ * .25) {
          drvr_current =
            output_waveforms_[drvr_idx]->timeCurrent(in_slew,
                                                     ceff_[drvr_idx],
                                                     time - input_delay);
          region = "time_current";
        }
        else {
          drvr_current = output_waveform->voltageCurrent(in_slew, ceff_[drvr_idx], v1);
          region = "volt_current";
        }
      }
      else {
        // Falling edge.
        if (v1 > vdd_ * .75) {
          drvr_current =
            output_waveforms_[drvr_idx]->timeCurrent(in_slew,
                                                     ceff_[drvr_idx],
                                                     time - input_delay);
          region = "time_current";
        }
        else {
          drvr_current = output_waveform->voltageCurrent(in_slew, ceff_[drvr_idx],
                                                         vdd - v1);
          region = "volt_current";
        }
      }
      debugPrint(debug_, "prima", 3, " {} ceff {} end {} VDrvr {:.3f} Idrvr {} {}",
                 drvr_idx,
                 units_->capacitanceUnit()->asString(ceff_[drvr_idx]),
                 delayAsString(waveform_end_[drvr_idx], this),
                 v1,
                 units_->currentUnit()->asString(drvr_current, 4),
                 region);
    }
    drvr_current1_[drvr_idx] = drvr_current_[drvr_idx];
    drvr_current_[drvr_idx] = drvr_current;
    // Set port current.
    u_[drvr_idx] = drvr_current;
  }
}

float
PrimaDelayCalc::holdResistanceBefore(const ArcDcalcArg &dcalc_arg)
{
  const RiseFall *rf_opp = dcalc_arg.arc()->toEdge()->asRiseFall()->opposite();
  for (const TimingArc *arc : dcalc_arg.edge()->timingArcSet()->arcs()) {
    if (arc->toEdge()->asRiseFall() == rf_opp) {
      GateTableModel *table_model = arc->gateTableModel(scene_, min_max_);
      OutputWaveforms *output_waveforms = table_model->outputWaveforms();
      if (output_waveforms)
        return output_waveforms->finalResistance();
    }
  }
  return 0.0;
}

float
PrimaDelayCalc::holdResistanceAfter(OutputWaveforms *output_waveform)
{
  return output_waveform->finalResistance();
}

bool
PrimaDelayCalc::loadWaveformsFinished()
{
  const RiseFall *drvr_rf = drvr_arg_->drvrEdge();
  for (const auto [load_pin, load_idx] : *load_pin_index_map_) {
    size_t load_node = pin_node_map_[load_pin];
    double v = voltage(load_node);
    if ((drvr_rf == RiseFall::rise()
         && v < vh_ + (vdd_ - vh_) * .5)
        || (drvr_rf == RiseFall::fall()
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
  for (const auto [pin, node_idx] : pin_node_map_) {
    double v = voltage(node_idx);
    double v_prev = voltagePrev1(node_idx);
    for (size_t m = 0; m < measure_threshold_count_; m++) {
      double th = measure_thresholds_[m];
      if ((v_prev < th && th <= v)
          || (v_prev > th && th >= v)) {
        double t_cross = time - time_step_ + (th - v_prev) * time_step_ / (v - v_prev);
        debugPrint(debug_, "ccs_measure", 1, "node {} cross {:.2f} {}",
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
PrimaDelayCalc::voltagePrev1(size_t node_idx)
{
  return v_prev1_[node_idx];
}

double
PrimaDelayCalc::voltagePrev2(size_t node_idx)
{
  return v_prev2_[node_idx];
}

static bool
isUnset(double value)
{
  return std::isnan(value);
}

// Return time1 - time2, or unset_delay when either crossing was not measured.
static double
measuredDelay(double time1,
              double time2,
              double unset_delay)
{
  double delay = unset_delay;
  if (!isUnset(time1) && !isUnset(time2))
    delay = time1 - time2;
  return delay;
}

// Return |time1 - time2|, or unset_slew when either crossing was not measured.
static double
measuredSlew(double time1,
             double time2,
             double unset_slew)
{
  double slew = unset_slew;
  if (!isUnset(time1) && !isUnset(time2))
    slew = std::abs(time1 - time2);
  return slew;
}

ArcDcalcResultSeq
PrimaDelayCalc::dcalcResults()
{
  ArcDcalcResultSeq dcalc_results(drvr_count_);
  const Net *drvr_net = drvr_arg_->drvrNet(network_);
  for (size_t drvr_idx = 0; drvr_idx < drvr_count_; drvr_idx++) {
    ArcDcalcArg &dcalc_arg = (*dcalc_args_)[drvr_idx];
    // Only annotate load delays/slews on the driver net.
    if (dcalc_arg.drvrNet(network_) == drvr_net) {
      ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
      const Pin *drvr_pin = dcalc_arg.drvrPin();
      const RiseFall *drvr_rf = dcalc_arg.drvrEdge();
      const LibertyLibrary *drvr_library = dcalc_arg.drvrLibrary();
      size_t drvr_node = pin_node_map_[drvr_pin];
      ThresholdTimes &drvr_times = threshold_times_[drvr_node];
      float ref_time = output_waveforms_[drvr_idx]->referenceTime(dcalc_arg.inSlewFlt());
      double unset = min_max_->initValue();
      double gate_delay = measuredDelay(drvr_times[threshold_vth], ref_time, unset);
      double drvr_slew = measuredSlew(drvr_times[threshold_vh],
                                      drvr_times[threshold_vl], unset);

      ArcDelay gate_delay2(gate_delay);
      Slew drvr_slew2(drvr_slew);
      delaySlewPocv(dcalc_arg, drvr_idx, gate_delay2, drvr_slew2);
      dcalc_result.setGateDelay(gate_delay2);
      dcalc_result.setDrvrSlew(drvr_slew2);

      debugPrint(debug_, "prima", 1, "{} gate delay {} slew {}",
                 network_->pathName(drvr_pin),
                 delayAsString(gate_delay, this),
                 delayAsString(drvr_slew, this));

      dcalc_result.setLoadCount(load_pin_index_map_->size());
      for (const auto [load_pin, load_idx] : *load_pin_index_map_) {
        size_t load_node = pin_node_map_[load_pin];
        ThresholdTimes &wire_times = threshold_times_[load_node];
        ThresholdTimes &drvr_times = threshold_times_[drvr_node];
        double wire_delay = measuredDelay(wire_times[threshold_vth],
                                          drvr_times[threshold_vth], unset);
        double load_slew = measuredSlew(wire_times[threshold_vh],
                                        wire_times[threshold_vl], unset);
        debugPrint(debug_, "prima", 1, "load {} {} delay {} slew {}",
                   network_->pathName(load_pin),
                   drvr_rf_->shortName(),
                   delayAsString(wire_delay, this),
                   delayAsString(load_slew, this));

        thresholdAdjust(load_pin, drvr_library, drvr_rf, wire_delay, load_slew);
        dcalc_result.setWireDelay(load_idx, wire_delay);
        dcalc_result.setLoadSlew(load_idx, load_slew);
      }
    }
  }
  return dcalc_results;
}

// Fill in pocv parameters in gate_delay/drvr_slew.
void
PrimaDelayCalc::delaySlewPocv(ArcDcalcArg &dcalc_arg,
                              size_t drvr_idx,
                              ArcDelay &gate_delay,
                              Slew &drvr_slew)
{
  if (variables_->pocvEnabled()) {
    GateTableModel *table_model = dcalc_arg.arc()->gateTableModel(scene_, min_max_);
    if (table_model) {
      double ceff = ceff_vth_[drvr_idx];
      if (ceff == 0.0)
        ceff = dcalc_arg.loadCap();
      float in_slew = delayAsFloat(dcalc_arg.inSlew());
      const Pvt *pvt = pinPvt(dcalc_arg.drvrPin(), scene_, min_max_);
      table_model->gateDelayPocv(pvt, in_slew, ceff, min_max_,
                                 variables_->pocvMode(),
                                 gate_delay, drvr_slew);
    }
  }
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
  Eigen::SparseLU<MatrixSd> G_solver(G_);
  if (G_solver.info() != Eigen::Success)
    report_->error(1752, "G matrix is singular.");
  Eigen::MatrixXd R(order_, port_count_);
  R = G_solver.solve(B_);

  // Step 4
  Eigen::HouseholderQR<Eigen::MatrixXd> R_solver(R);
  Eigen::MatrixXd Q = R_solver.householderQ();

  // Vq is "X" in the prima paper (too many "x" variables in the paper).
  Vq_.resize(order_, prima_order_);
  // Vq = first port_count columns of Q.
  Vq_.block(0, 0, order_, port_count_) = Q.block(0, 0, order_, port_count_);

  // Step 6 - Arnolid iteration
  for (size_t k = 1; k < prima_order_; k++) {
    Eigen::VectorXd V = C_ * Vq_.col(k - 1);
    Vq_.col(k) = G_solver.solve(V);

    // Modified Gram-Schmidt orthonormalization
    for (size_t j = 0; j < k; j++) {
      double H = Vq_.col(j).transpose() * Vq_.col(k);
      Vq_.col(k) = Vq_.col(k) - H * Vq_.col(j);
    }
    Eigen::VectorXd Vq_k = Vq_.col(k);
    Eigen::HouseholderQR<Eigen::MatrixXd> Vq_k_solver(Vq_k);
    Eigen::MatrixXd VqQ = Vq_k_solver.householderQ();
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

  if (debug_->check("prima", 4)) {
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
  Eigen::SparseLU<MatrixSd> G_solver(G_);
  Eigen::MatrixXd R(order_, port_count_);
  R = G_solver.solve(B_);

  // Step 4
  Eigen::HouseholderQR<Eigen::MatrixXd> R_solver(R);
  Eigen::MatrixXd Q = R_solver.householderQ();

  // Vq is "X" in the prima paper (too many "x" variables in the paper).
  size_t n = ceil(prima_order_ / static_cast<double>(port_count_));
  Eigen::MatrixXd Vq(order_, n * port_count_);
  // // Vq = first port_count columns of Q.
  Vq.block(0, 0, order_, port_count_) = Q.block(0, 0, order_, port_count_);

  // Step 6 - Arnolid iteration
  for (size_t k = 1; k < n; k++) {
    Eigen::MatrixXd V = C_ * Vq.block(0, (k - 1) * port_count_, order_, port_count_);
    Eigen::MatrixXd GV = G_solver.solve(V);
    Vq.block(0, k * port_count_, order_, port_count_) = GV;

    // Modified Gram-Schmidt orthonormalization
    for (size_t j = 0; j < k; j++) {
      Eigen::MatrixXd H =
          Vq.block(0, j * port_count_, order_, port_count_).transpose()
          * Vq.block(0, k * port_count_, order_, port_count_);
      Vq.block(0, k * port_count_, order_, port_count_) =
          Vq.block(0, k * port_count_, order_, port_count_)
          - Vq.block(0, j * port_count_, order_, port_count_) * H;
    }
    Eigen::MatrixXd Vq_k = Vq.block(0, k * port_count_, order_, port_count_);
    Eigen::HouseholderQR<Eigen::MatrixXd> Vq_k_solver(Vq_k);
    Eigen::MatrixXd VqQ = Vq_k_solver.householderQ();
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

  if (debug_->check("prima", 4)) {
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
  if (!watch_pin_values_.empty()) {
  times_.push_back(time);
    for (auto& [pin, waveform] : watch_pin_values_) {
      auto pin_node_id = pin_node_map_.find(pin);
      if (pin_node_id != pin_node_map_.end()) {
        double pin_v = voltage(pin_node_id->second);
    waveform.push_back(pin_v);
  }
    }
  }
}

////////////////////////////////////////////////////////////////

std::string
PrimaDelayCalc::reportGateDelay(const Pin *drvr_pin,
                                const TimingArc *arc,
                                const Slew &in_slew,
                                float load_cap,
                                const Parasitic *parasitic,
                                const LoadPinIndexMap &load_pin_index_map,
                                const Scene *scene,
                                const MinMax *min_max,
                                int digits)
{
  ArcDcalcArgSeq dcalc_args;
  dcalc_args.emplace_back(nullptr, drvr_pin, nullptr, arc, in_slew,
                          load_cap, parasitic);
  bool arg_fail = checkArgs(dcalc_args, scene, min_max);
  if (arg_fail) {
    const RiseFall *rf = arc->toEdge()->asRiseFall();
    const Parasitic *reduced = table_dcalc_->findParasitic(drvr_pin, rf,
                                                           scene, min_max);
    return table_dcalc_->reportGateDelay(drvr_pin, arc, in_slew, load_cap,
                                         reduced, load_pin_index_map, scene,
                                         min_max, digits);
  }
  else {
    GateTimingModel *model = arc->gateModel(scene, min_max);
    // Delay calc to find ceff.
    gateDelay(drvr_pin, arc, in_slew, load_cap, parasitic,
              load_pin_index_map, scene, min_max);
    float in_slew1 = delayAsFloat(in_slew);
    float ceff = ceff_vth_[0];
    return model->reportGateDelay(pinPvt(drvr_pin, scene, min_max),
                                  in_slew1, ceff, min_max,
                                  PocvMode::scalar, digits);
  }
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::watchPin(const Pin *pin)
{
  watch_pin_values_[pin] = FloatSeq();
}

void
PrimaDelayCalc::clearWatchPins()
{
  watch_pin_values_.clear();
}

PinSeq
PrimaDelayCalc::watchPins() const
{
  PinSeq pins;
  for (const auto& [pin, values] : watch_pin_values_)
    pins.push_back(pin);
  return pins;
}

Waveform
PrimaDelayCalc::watchWaveform(const Pin *pin)
{
  for (ArcDcalcArg &dcalc_arg : *dcalc_args_) {
    if (dcalc_arg.inPin() == pin)
      return inputWaveform(dcalc_arg, scene_, min_max_, this);
  }
  FloatSeq &voltages = watch_pin_values_[pin];
  TableAxisPtr time_axis =
      std::make_shared<TableAxis>(TableAxisVariable::time, FloatSeq(times_));
  Table waveform(new FloatSeq(voltages), time_axis);
  return waveform;
}

////////////////////////////////////////////////////////////////

void
PrimaDelayCalc::reportMatrix(std::string_view name,
                             MatrixSd &matrix)
{
  report_->report("{}", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportMatrix(std::string_view name,
                             Eigen::MatrixXd &matrix)
{
  report_->report("{}", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportMatrix(std::string_view name,
                             Eigen::VectorXd &matrix)
{
  report_->report("{}", name);
  reportMatrix(matrix);
}

void
PrimaDelayCalc::reportVector(std::string_view name,
                             std::vector<double> &matrix)
{
  report_->report("{}", name);
  reportVector(matrix);
}

void
PrimaDelayCalc::reportMatrix(MatrixSd &matrix)
{
  for (Eigen::Index i = 0; i < matrix.rows(); i++) {
    std::string line = "| ";
    for (Eigen::Index j = 0; j < matrix.cols(); j++)
      line += sta::format("{:10.3e}", matrix.coeff(i, j)) + " ";
    line += "|";
    report_->reportLine(line);
  }
}

void
PrimaDelayCalc::reportMatrix(Eigen::MatrixXd &matrix)
{
  for (Eigen::Index i = 0; i < matrix.rows(); i++) {
    std::string line = "| ";
    for (Eigen::Index j = 0; j < matrix.cols(); j++)
      line += sta::format("{:10.3e}", matrix.coeff(i, j)) + " ";
    line += "|";
    report_->reportLine(line);
  }
}

void
PrimaDelayCalc::reportMatrix(Eigen::VectorXd &matrix)
{
  std::string line = "| ";
  for (Eigen::Index i = 0; i < matrix.rows(); i++)
    line += sta::format("{:10.3e}", matrix.coeff(i)) + " ";
  line += "|";
  report_->reportLine(line);
}

void
PrimaDelayCalc::reportVector(std::vector<double> &matrix)
{
  std::string line = "| ";
  for (const double &entry : matrix)
    line += sta::format("{:10.3e}", entry) + " ";
  line += "|";
  report_->reportLine(line);
}

}  // namespace sta
