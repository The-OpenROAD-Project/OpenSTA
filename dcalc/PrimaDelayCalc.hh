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

#pragma once

#include <vector>
#include <map>
#include <Eigen/SparseCore>
#include <Eigen/SparseLU>

#include "Map.hh"
#include "LumpedCapDelayCalc.hh"
#include "ArcDcalcWaveforms.hh"
#include "Parasitics.hh"

namespace sta {

class ArcDelayCalc;
class StaState;
class Corner;

using std::vector;
using std::array;
using Eigen::MatrixXd;
using Eigen::MatrixXcd;
using Eigen::VectorXd;
using Eigen::SparseMatrix;
using Eigen::Index;
using std::map;

typedef Map<const Pin*, size_t, PinIdLess> PinNodeMap;
typedef map<const ParasiticNode*, size_t, ParasiticNodeLess> NodeIndexMap;
typedef Map<const Pin*, size_t> PortIndexMap;
typedef SparseMatrix<double> MatrixSd;
typedef Map<const Pin*, VectorXd, PinIdLess> PinLMap;
typedef map<const Pin*, FloatSeq, PinIdLess> WatchPinValuesMap;

typedef Table1 Waveform;

ArcDelayCalc *
makePrimaDelayCalc(StaState *sta);

class PrimaDelayCalc : public DelayCalcBase,
                       public ArcDcalcWaveforms
{
public:
  PrimaDelayCalc(StaState *sta);
  PrimaDelayCalc(const PrimaDelayCalc &dcalc);
  ~PrimaDelayCalc();
  ArcDelayCalc *copy() override;
  void copyState(const StaState *sta) override;
  const char *name() const override { return "prima"; }
  void setPrimaReduceOrder(size_t order);
  Parasitic *findParasitic(const Pin *drvr_pin,
                           const RiseFall *rf,
                           const DcalcAnalysisPt *dcalc_ap) override;
  bool reduceSupported() const override { return false; }
  Parasitic *reduceParasitic(const Parasitic *parasitic_network,
                             const Pin *drvr_pin,
                             const RiseFall *rf,
                             const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResult inputPortDelay(const Pin *drvr_pin,
                                float in_slew,
                                const RiseFall *rf,
                                const Parasitic *parasitic,
                                const LoadPinIndexMap &load_pin_index_map,
                                const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResult gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResultSeq gateDelays(ArcDcalcArgSeq &dcalc_args,
                               const LoadPinIndexMap &load_pin_index_map,
                               const DcalcAnalysisPt *dcalc_ap) override;
  string reportGateDelay(const Pin *drvr_pin,
                         const TimingArc *arc,
                         const Slew &in_slew,
                         float load_cap,
                         const Parasitic *parasitic,
                         const LoadPinIndexMap &load_pin_index_map,
                         const DcalcAnalysisPt *dcalc_ap,
                         int digits) override;

  // Record waveform for drvr/load pin.
  void watchPin(const Pin *pin) override;
  void clearWatchPins() override;
  PinSeq watchPins() const override;
  Waveform watchWaveform(const Pin *pin) override;
  
protected:
  ArcDcalcResultSeq tableDcalcResults();
  void simulate();
  void simulate1(const MatrixSd &G,
                 const MatrixSd &C,
                 const MatrixXd &B,
                 const VectorXd &x_init,
                 const MatrixXd &x_to_v,
                 const size_t order);
  double maxTime();
  double timeStep();
  float driverResistance();
  void updateCeffIdrvr();
  void initSim();
  void findLoads();
  void findNodeCount();
  void setOrder();
  void initCeffIdrvr();
  void setXinit();
  void stampEqns();
  void stampConductance(size_t n1,
                        double g);
  void stampConductance(size_t n1,
                        size_t n2,
                        double g);
  void stampCapacitance(size_t n1,
                        double cap);
  void stampCapacitance(size_t n1,
                        size_t n2,
                        double cap);
  float pinCapacitance(ParasiticNode *node);
  void setPortCurrents();
  void measureThresholds(double time);
  double voltage(const Pin *pin);
  double voltage(size_t node_idx);
  double voltagePrev(size_t node_idx);
  bool loadWaveformsFinished();
  ArcDcalcResultSeq dcalcResults();

  void recordWaveformStep(double time);
  void makeWaveforms(const Pin *in_pin,
                     const RiseFall *in_rf,
                     const Pin *drvr_pin,
                     const RiseFall *drvr_rf,
                     const Pin *load_pin,
                     const Corner *corner,
                     const MinMax *min_max);
  void primaReduce();
  void primaReduce2();

  void reportMatrix(const char *name,
                    MatrixSd &matrix);
  void reportMatrix(const char *name,
                    MatrixXd &matrix);
  void reportMatrix(const char *name,
                    VectorXd &matrix);
  void reportVector(const char *name,
                    vector<double> &matrix);
  void reportMatrix(MatrixSd &matrix);
  void reportMatrix(MatrixXd &matrix);
  void reportMatrix(VectorXd &matrix);
  void reportVector(vector<double> &matrix);

  ArcDcalcArgSeq *dcalc_args_;
  size_t drvr_count_;
  float load_cap_;
  const DcalcAnalysisPt *dcalc_ap_;
  const Parasitic *parasitic_network_;
  const RiseFall *drvr_rf_;
  const LoadPinIndexMap *load_pin_index_map_;

  PinNodeMap pin_node_map_;     // Parasitic pin -> array index
  NodeIndexMap node_index_map_; // Parasitic node -> array index
  vector<OutputWaveforms*> output_waveforms_;
  double resistance_sum_;
  
  vector<double> node_capacitances_;
  bool includes_pin_caps_;
  float coupling_cap_multiplier_;
  
  size_t node_count_;           // Parasitic network node count
  size_t port_count_;           // aka drvr_count_
  size_t order_;                // node_count_ + port_count_

  // MNA node eqns
  // G*x(t) + C*x'(t) = B*u(t)
  MatrixSd G_;
  MatrixSd C_;
  MatrixXd B_;
  VectorXd x_init_;
  VectorXd u_;

  // Prima reduced MNA eqns
  size_t prima_order_;
  MatrixXd Vq_;
  MatrixSd Gq_;
  MatrixSd Cq_;
  MatrixXd Bq_;
  VectorXd xq_init_;

  // Node voltages.
  VectorXd v_;                  // voltage[node_idx]
  VectorXd v_prev_;

  // Indexed by driver index.
  vector<double> ceff_;
  vector<double> drvr_current_;

  double time_step_;
  double time_step_prev_;

  // Waveform recording.
  bool make_waveforms_;
  const Pin *waveform_drvr_pin_;
  const Pin *waveform_load_pin_;
  FloatSeq drvr_voltages_;
  FloatSeq load_voltages_;
  WatchPinValuesMap watch_pin_values_;
  FloatSeq times_;

  float vdd_;
  float vth_;
  float vl_;
  float vh_;

  static constexpr size_t threshold_vl = 0;
  static constexpr size_t threshold_vth = 1;
  static constexpr size_t threshold_vh = 2;
  static constexpr size_t measure_threshold_count_ = 3;
  typedef array<double, measure_threshold_count_> ThresholdTimes;
  // Vl Vth Vh
  ThresholdTimes measure_thresholds_;
  // Indexed by node number.
  vector<ThresholdTimes> threshold_times_;

  // Delay calculator to use when ccs waveforms are missing from liberty.
  ArcDelayCalc *table_dcalc_;

  using ArcDelayCalc::reduceParasitic;
};

} // namespacet
