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

#pragma once

#include <optional>
#include <utility>

#include "LibertyClass.hh"
#include "LumpedCapDelayCalc.hh"

namespace sta {

class GateTableModel;

// Base class for Dartu/Menezes/Pileggi algorithm.
// Derived classes handle different cases of zero values in the Pi model.
class DmpAlg : public StaState
{
public:
  DmpAlg(int nr_order,
         StaState *sta);
  ~DmpAlg() override = default;
  virtual std::string_view name() = 0;
  // Set driver model and pi model parameters for delay calculation.
  virtual void init(const LibertyLibrary *library,
                    const LibertyCell *drvr_cell,
                    const Pvt *pvt,
                    const GateTableModel *gate_model,
                    const RiseFall *rf,
                    double rd,
                    double in_slew,
                    double c2,
                    double rpi,
                    double c1);
  virtual std::pair<double, double> gateDelaySlew() = 0;
  virtual std::pair<double, double> loadDelaySlew(const Pin *load_pin,
                                                  double elmore);
  double ceff() { return ceff_; }

  // Given x_ as a vector of input parameters, fill fvec_ with the
  // equations evaluated at x_ and fjac_ with the jabobian evaluated at x_.
  virtual void evalDmpEqns() = 0;
  // Output response to vs(t) ramp driving pi model load (vo, dvo_dt).
  std::pair<double, double> Vo(double t);
  // Load response to driver waveform (vl, dvl/dt).
  std::pair<double, double> Vl(double t);

protected:
  void luDecomp();
  void luSolve();
  void newtonRaphson();
  // Find driver parameters t0, delta_t, Ceff.
  void findDriverParams(double ceff);
  std::pair<double, double> gateCapDelaySlew(double ceff);
  std::tuple<double, double, double> gateDelays(double ceff);
  // Partial derivatives of y(t) jacobian (dydt0, dyddt, dydcl).
  std::tuple<double, double, double> dy(double t,
                                        double t0,
                                        double dt,
                                        double cl);
  double y0dt(double t,
              double cl);
  double y0dcl(double t,
               double cl);
  void showX();
  void showFvec();
  void showJacobian();
  std::pair<double, double> findDriverDelaySlew();
  double findVoCrossing(double vth,
                        double t_lower,
                        double t_upper);
  void showVo();
  double findVlCrossing(double vth,
                        double t_lower,
                        double t_upper);
  void showVl();
  void fail(std::string_view reason);

  // Output response to vs(t) ramp driving capacitive load (y, t1).
  std::pair<double, double> y(double t,
                              double t0,
                              double dt,
                              double cl);
  // Output response to unit ramp driving capacitive load.
  double y0(double t,
            double cl);
  // Output response to unit ramp driving pi model load.
  // Unit ramp output at pi load (vo, dvo_dt).
  virtual std::pair<double, double> V0(double t) = 0;
  // Upper bound on time that vo crosses vh.
  virtual double voCrossingUpperBound() = 0;
  // Load responce to driver unit ramp.
  // Unit ramp load response (vl, dvl_dt).
  virtual std::pair<double, double> Vl0(double t) = 0;
  // Upper bound on time that vl crosses vh.
  double vlCrossingUpperBound();

  // Inputs to the delay calculator.
  const LibertyCell *drvr_cell_;
  const LibertyLibrary *drvr_library_;
  const Pvt *pvt_;
  const GateTableModel *gate_model_;
  double in_slew_;
  double c2_{0.0};
  double rpi_{0.0};
  double c1_{0.0};

  double rd_;
  // Logic threshold (percentage of supply voltage).
  double vth_;
  // Slew lower limit (percentage of supply voltage).
  double vl_;
  // Slew upper limit (percentage of supply voltage).
  double vh_;
  // Table slews are scaled by slew_derate to get
  // measured slews from vl to vh.
  double slew_derate_;

  // Driver parameters calculated by this algorithm.
  double t0_;
  double dt_;
  double ceff_;

  // Driver parameter Newton-Raphson state.
  int nr_order_;

  static constexpr int max_nr_order_ = 3;

  std::array<double, max_nr_order_> x_;
  std::array<double, max_nr_order_> fvec_;
  std::array<std::array<double, max_nr_order_>, max_nr_order_> fjac_;
  std::array<double, max_nr_order_> scale_;
  std::array<double, max_nr_order_> p_;
  std::array<int, max_nr_order_> index_;

  // Driver slew used to check load delay.
  double drvr_slew_;
  double vo_delay_;
  // True if the driver parameters are valid for finding the load delays.
  bool driver_valid_;
  // Load rspf elmore delay.
  double elmore_;
  double p3_;

  // Tolerance (as a scale of value) for driver parameters (Ceff, delta t, t0).
  static constexpr double driver_param_tol_ = .01;
  // Waveform threshold crossing time tolerance (1.0 = 100%).
  static constexpr double vth_time_tol_ = .01;
  // Max iterations for findRoot.
  static constexpr int find_root_max_iter_ = 20;
  static inline int newton_raphson_max_iter_ = 100;
  // A small number used by luDecomp.
  static constexpr double tiny_double_ = 1.0e-20;
};

// Capacitive load.
class DmpCap : public DmpAlg
{
public:
  DmpCap(StaState *sta);
  std::string_view name() override { return "cap"; }
  void init(const LibertyLibrary *library,
            const LibertyCell *drvr_cell,
            const Pvt *pvt,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double rd,
            double in_slew,
            double c2,
            double rpi,
            double c1) override;
  std::pair<double, double> gateDelaySlew() override;
  std::pair<double, double> loadDelaySlew(const Pin *,
                                           double elmore) override;
  void evalDmpEqns() override;

protected:
  double voCrossingUpperBound() override;
  std::pair<double, double> V0(double t) override;
  std::pair<double, double> Vl0(double t) override;
};

// No non-zero pi model parameters, two poles, one zero
class DmpPi : public DmpAlg
{
public:
  DmpPi(StaState *sta);
  std::string_view name() override { return "Pi"; }
  void init(const LibertyLibrary *library,
            const LibertyCell *drvr_cell,
            const Pvt *pvt,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double rd,
            double in_slew,
            double c2,
            double rpi,
            double c1) override;
  std::pair<double, double> gateDelaySlew() override;
  void evalDmpEqns() override;

protected:
  double voCrossingUpperBound() override;
  std::pair<double, double> V0(double t) override;
  std::pair<double, double> Vl0(double t) override;

private:
  void findDriverParamsPi();
  double ipiIceff(double t0,
                  double dt,
                  double ceff_time,
                  double ceff);

  // Poles/zero.
  double p1_{0.0};
  double p2_{0.0};
  double z1_{0.0};
  // Residues.
  double k0_{0.0};
  double k1_{0.0};
  double k2_{0.0};
  double k3_{0.0};
  double k4_{0.0};
  // Ipi coefficients.
  double A_{0.0};
  double B_{0.0};
  double D_{0.0};
};

// Capacitive load, so Ceff is known.
// Solve for t0, delta t.
class DmpOnePole : public DmpAlg
{
public:
  DmpOnePole(StaState *sta);
  void evalDmpEqns() override;

protected:
  double voCrossingUpperBound() override;
};

// C2 = 0, one pole, one zero.
class DmpZeroC2 : public DmpOnePole
{
public:
  DmpZeroC2(StaState *sta);
  std::string_view name() override { return "c2=0"; }
  void init(const LibertyLibrary *drvr_library,
            const LibertyCell *drvr_cell,
            const Pvt *pvt,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double rd,
            double in_slew,
            double c2,
            double rpi,
            double c1) override;
  std::pair<double, double> gateDelaySlew() override;

protected:
  std::pair<double, double> V0(double t) override;
  std::pair<double, double> Vl0(double t) override;
  double voCrossingUpperBound() override;

private:
  // Pole/zero.
  double p1_{0.0};
  double z1_{0.0};
  // Residues.
  double k0_{0.0};
  double k1_{0.0};
  double k2_{0.0};
  double k3_{0.0};
};

// Delay calculator using Dartu/Menezes/Pileggi effective capacitance
// algorithm for RSPF loads.
class DmpCeffDelayCalc : public LumpedCapDelayCalc
{
public:
  DmpCeffDelayCalc(StaState *sta);
  bool reduceSupported() const override { return true; }
  ArcDcalcResult gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const Scene *scene,
                           const MinMax *min_max) override;
  std::string reportGateDelay(const Pin *drvr_pin,
                              const TimingArc *arc,
                              const Slew &in_slew,
                              float load_cap,
                              const Parasitic *parasitic,
                              const LoadPinIndexMap &load_pin_index_map,
                              const Scene *scene,
                              const MinMax *min_max,
                              int digits) override;
  void copyState(const StaState *sta) override;

protected:
  virtual void loadDelaySlew(const Pin *load_pin,
                             double drvr_slew,
                             const RiseFall *rf,
                             const LibertyLibrary *drvr_library,
                             const Parasitic *parasitic,
                             // Return values.
                             double &wire_delay,
                             double &load_slew) = 0;
  std::pair<double, double> gateDelaySlew();
  std::optional<std::pair<double, double>>
  loadDelaySlewElmore(const Pin *load_pin,
                      double elmore);
  // Select the appropriate special case Dartu/Menezes/Pileggi algorithm.
  void setCeffAlgorithm(const LibertyLibrary *library,
                        const LibertyCell *cell,
                        const Pvt *pvt,
                        const GateTableModel *gate_model,
                        const RiseFall *rf,
                        double in_slew,
                        double c2,
                        double rpi,
                        double c1);

  const Parasitics *parasitics_;
  static bool unsuppored_model_warned_;

private:
  // Dmp algorithms for each special pi model case.
  DmpCap dmp_cap_;
  DmpPi dmp_pi_;
  DmpZeroC2 dmp_zero_c2_;
  DmpAlg *dmp_alg_{nullptr};
};

} // namespace sta
