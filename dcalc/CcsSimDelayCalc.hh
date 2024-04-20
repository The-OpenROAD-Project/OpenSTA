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

#pragma once

#include <vector>
#include <Eigen/SparseCore>
#include <Eigen/SparseLU>

#include "LumpedCapDelayCalc.hh"
#include "ArcDcalcWaveforms.hh"

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
using Eigen::SparseLU;

typedef Map<const Pin*, size_t, PinIdLess> PinNodeMap;
typedef Map<const ParasiticNode*, size_t> NodeIndexMap;
typedef Map<const Pin*, size_t> PortIndexMap;
typedef SparseMatrix<double> MatrixSd;

ArcDelayCalc *
makeCcsSimDelayCalc(StaState *sta);

class CcsSimDelayCalc : public DelayCalcBase, public ArcDcalcWaveforms
{
public:
  CcsSimDelayCalc(StaState *sta);
  ~CcsSimDelayCalc();
  ArcDelayCalc *copy() override;
  Parasitic *findParasitic(const Pin *drvr_pin,
                           const RiseFall *rf,
                           const DcalcAnalysisPt *dcalc_ap) override;
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
                               float load_cap,
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

  Table1 inputWaveform(const Pin *in_pin,
                       const RiseFall *in_rf,
                       const Corner *corner,
                       const MinMax *min_max) override;
  Table1 drvrWaveform(const Pin *in_pin,
                      const RiseFall *in_rf,
                      const Pin *drvr_pin,
                      const RiseFall *drvr_rf,
                      const Corner *corner,
                      const MinMax *min_max) override;
 Table1 loadWaveform(const Pin *in_pin,
                     const RiseFall *in_rf,
                     const Pin *drvr_pin,
                     const RiseFall *drvr_rf,
                     const Pin *load_pin,
                     const Corner *corner,
                     const MinMax *min_max) override;

protected:
  void simulate(ArcDcalcArgSeq &dcalc_args);
  virtual double maxTime();
  virtual double timeStep();
  void updateCeffIdrvr();
  void initSim();
  void findLoads();
  virtual void findNodeCount();
  void setOrder();
  void initNodeVoltages();
  void simulateStep();
  virtual void stampConductances();
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
  virtual void setCurrents();
  void insertCapCurrentSrc(size_t n1,
                           double cap);
  void insertCapaCurrentSrc(size_t n1,
                            size_t n2,
                            double cap);
  void insertCurrentSrc(size_t n1,
                        double current);
  void insertCurrentSrc(size_t n1,
                        size_t n2,
                        double current);
  void measureThresholds(double time);
  void measureThresholds(double time,
                         size_t i);
  void loadDelaySlew(const Pin *load_pin,
                     // Return values.
                     ArcDelay &delay,
                     Slew &slew);
  void recordWaveformStep(double time);
  void makeWaveforms(const Pin *in_pin,
                     const RiseFall *in_rf,
                     const Pin *drvr_pin,
                     const RiseFall *drvr_rf,
                     const Pin *load_pin,
                     const Corner *corner,
                     const MinMax *min_max);
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

  // Tmp for gateDelay/loadDelay api.
  ArcDcalcResult dcalc_result_;
  LoadPinIndexMap load_pin_index_map_;

  bool dcalc_failed_;
  size_t node_count_;  // Parasitic network node count
  PinNodeMap pin_node_map_;     // Parasitic pin -> array index
  NodeIndexMap node_index_map_; // Parasitic node -> array index
  vector<OutputWaveforms*> output_waveforms_;
  vector<float> ref_time_;
  double drive_resistance_;
  double resistance_sum_;
  
  vector<double> node_capacitances_;
  bool includes_pin_caps_;
  float coupling_cap_multiplier_;
  
  // Indexed by driver index.
  vector<double> ceff_;
  vector<double> drvr_current_;

  double time_step_;
  double time_step_prev_;
  // I = GV
  // currents_ = conductances_ * voltages_
  VectorXd currents_;
  MatrixSd conductances_;
  VectorXd voltages_;
  VectorXd voltages_prev1_;
  VectorXd voltages_prev2_;
  SparseLU<MatrixSd> solver_;

  // Waveform recording.
  bool make_waveforms_;
  const Pin *waveform_drvr_pin_;
  const Pin *waveform_load_pin_;
  FloatSeq drvr_voltages_;
  FloatSeq load_voltages_;
  FloatSeq times_;

  size_t drvr_idx_;

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
