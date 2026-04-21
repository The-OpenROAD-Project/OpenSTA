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

// "Performance Computation for Precharacterized CMOS Gates with RC Loads",
// Florentin Dartu, Noel Menezes and Lawrence Pileggi, IEEE Transactions
// on Computer-Aided Design of Integrated Circuits and Systems, Vol 15, No 5,
// May 1996, pg 544-553.
//
// The only real change from the paper is that Vl, the measured low
// slew voltage is matched instead of y20 in eqn 12.

#include "DmpCeff.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

#include "ArcDelayCalc.hh"
#include "Debug.hh"
#include "FindRoot.hh"
#include "Format.hh"
#include "Liberty.hh"
#include "Parasitics.hh"
#include "Report.hh"
#include "Sdc.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "Units.hh"
#include "Variables.hh"

namespace sta {

// Indices of Newton-Raphson parameter vector.
enum DmpParam { t0, dt, ceff };

static const char *dmp_param_index_strings[] = {"t0", "dt", "Ceff"};

// Indices of Newton-Raphson function value vector.
enum DmpFunc { y20, y50, ipi };

static const char *dmp_func_index_strings[] = {"y20", "y50", "Ipi"};

static double
exp2(double x);

class DmpError : public Exception
{
public:
  DmpError(std::string_view what);
  const char *what() const noexcept override { return what_.c_str(); }

private:
  std::string what_;
};

static double
gateModelRd(const LibertyCell *cell,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double in_slew,
            double c2,
            double c1,
            const Pvt *pvt);
////////////////////////////////////////////////////////////////

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

DmpAlg::DmpAlg(int nr_order,
               StaState *sta) :
  StaState(sta),
  nr_order_(nr_order)
{
}

void
DmpAlg::init(const LibertyLibrary *drvr_library,
             const LibertyCell *drvr_cell,
             const Pvt *pvt,
             const GateTableModel *gate_model,
             const RiseFall *rf,
             double rd,
             double in_slew,
             // Pi model.
             double c2,
             double rpi,
             double c1)
{
  drvr_library_ = drvr_library;
  drvr_cell_ = drvr_cell;
  pvt_ = pvt;
  gate_model_ = gate_model;
  rd_ = rd;
  in_slew_ = in_slew;
  c2_ = c2;
  rpi_ = rpi;
  c1_ = c1;
  driver_valid_ = false;
  vth_ = drvr_library->outputThreshold(rf);
  vl_ = drvr_library->slewLowerThreshold(rf);
  vh_ = drvr_library->slewUpperThreshold(rf);
  slew_derate_ = drvr_library->slewDerateFromLibrary();
}

// Find Ceff, delta_t and t0 for the driver.
void
DmpAlg::findDriverParams(double ceff)
{
  if (nr_order_ == 3)
    x_[DmpParam::ceff] = ceff;
  auto [t_vth, t_vl, slew] = gateDelays(ceff);
  // Scale slew to 0-100%
  double dt = slew / (vh_ - vl_);
  double t0 = t_vth + std::log(1.0 - vth_) * rd_ * ceff - vth_ * dt;
  x_[DmpParam::dt] = dt;
  x_[DmpParam::t0] = t0;
  newtonRaphson();
  t0_ = x_[DmpParam::t0];
  dt_ = x_[DmpParam::dt];
  debugPrint(debug_, "dmp_ceff", 3, "    t0 = {} dt = {} ceff = {}",
             units_->timeUnit()->asString(t0_), units_->timeUnit()->asString(dt_),
             units_->capacitanceUnit()->asString(x_[DmpParam::ceff]));
  if (debug_->check("dmp_ceff", 4))
    showVo();
}

std::pair<double, double>
DmpAlg::gateCapDelaySlew(double ceff)
{
  float model_delay, model_slew;
  gate_model_->gateDelay(pvt_, in_slew_, ceff, model_delay, model_slew);
  double delay = model_delay;
  double slew = model_slew;
  return {delay, slew};
}

std::tuple<double, double, double>
DmpAlg::gateDelays(double ceff)
{
  auto [t_vth, table_slew] = gateCapDelaySlew(ceff);
  // Convert reported/table slew to measured slew.
  double slew = table_slew * slew_derate_;
  double t_vl = t_vth - slew * (vth_ - vl_) / (vh_ - vl_);
  return {t_vth, t_vl, slew};
}

std::pair<double, double>
DmpAlg::y(double t,
          double t0,
          double dt,
          double cl)
{
  double t1 = t - t0;
  if (t1 <= 0.0) {
    double y = 0.0;
    return {y, t1};
  }
  if (t1 <= dt) {
    double y = y0(t1, cl) / dt;
    return {y, t1};
  }
  double y = (y0(t1, cl) - y0(t1 - dt, cl)) / dt;
  return {y, t1};
}

double
DmpAlg::y0(double t,
           double cl)
{
  return t - rd_ * cl * (1.0 - exp2(-t / (rd_ * cl)));
}

std::tuple<double, double, double>
DmpAlg::dy(double t,
           double t0,
           double dt,
           double cl)
{
  double t1 = t - t0;
  if (t1 <= 0.0) {
    double dydt0 = 0.0;
    double dyddt = 0.0;
    double dydcl = 0.0;
    return {dydt0, dyddt, dydcl};
  }
  if (t1 <= dt) {
    double dydt0 = -y0dt(t1, cl) / dt;
    double dyddt = -y0(t1, cl) / (dt * dt);
    double dydcl = y0dcl(t1, cl) / dt;
    return {dydt0, dyddt, dydcl};
  }
  double dydt0 = -(y0dt(t1, cl) - y0dt(t1 - dt, cl)) / dt;
  double dyddt = -(y0(t1, cl) + y0(t1 - dt, cl)) / (dt * dt) + y0dt(t1 - dt, cl) / dt;
  double dydcl = (y0dcl(t1, cl) - y0dcl(t1 - dt, cl)) / dt;
  return {dydt0, dyddt, dydcl};
}

double
DmpAlg::y0dt(double t,
             double cl)
{
  return 1.0 - exp2(-t / (rd_ * cl));
}

double
DmpAlg::y0dcl(double t,
              double cl)
{
  return rd_ * ((1.0 + t / (rd_ * cl)) * exp2(-t / (rd_ * cl)) - 1);
}

void
DmpAlg::showX()
{
  for (int i = 0; i < nr_order_; i++)
    report_->report("{:4} {:12.3e}", dmp_param_index_strings[i], x_[i]);
}

void
DmpAlg::showFvec()
{
  for (int i = 0; i < nr_order_; i++)
    report_->report("{:4} {:12.3e}", dmp_func_index_strings[i], fvec_[i]);
}

void
DmpAlg::showJacobian()
{
  std::string line = "    ";
  for (int j = 0; j < nr_order_; j++)
    line += sta::format("{:>12}", dmp_param_index_strings[j]);
  report_->reportLine(line);
  for (int i = 0; i < nr_order_; i++) {
    line.clear();
    line += sta::format("{:4} ", dmp_func_index_strings[i]);
    for (int j = 0; j < nr_order_; j++)
      line += sta::format("{:12.3e} ", fjac_[i][j]);
    report_->reportLine(line);
  }
}

std::pair<double, double>
DmpAlg::findDriverDelaySlew()
{
  double t_upper = voCrossingUpperBound();
  double delay = findVoCrossing(vth_, t0_, t_upper);
  double tl = findVoCrossing(vl_, t0_, delay);
  double th = findVoCrossing(vh_, delay, t_upper);
  // Convert measured slew to table slew.
  double slew = (th - tl) / slew_derate_;
  return {delay, slew};
}

// Find t such that vo(t)=v.
double
DmpAlg::findVoCrossing(double vth,
                       double t_lower,
                       double t_upper)
{
  FindRootFunc vo_func = [&](double t, double &y, double &dy) {
    auto [vo, dvo_dt] = Vo(t);
    y = vo - vth;
    dy = dvo_dt;
  };
  auto [t_vth, failed] = findRoot(vo_func, t_lower, t_upper, vth_time_tol_,
                                  find_root_max_iter_);
  if (failed)
    throw DmpError("find Vo crossing failed");
  return t_vth;
}

std::pair<double, double>
DmpAlg::Vo(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0.0) {
    double vo = 0.0;
    double dvo_dt = 0.0;
    return {vo, dvo_dt};
  }
  if (t1 <= dt_) {
    auto [v0, dv0_dt] = V0(t1);

    double vo = v0 / dt_;
    double dvo_dt = dv0_dt / dt_;
    return {vo, dvo_dt};
  }
  auto [v0, dv0_dt] = V0(t1);

  auto [v0_dt, dv0_dt_dt] = V0(t1 - dt_);

  double vo = (v0 - v0_dt) / dt_;
  double dvo_dt = (dv0_dt - dv0_dt_dt) / dt_;
  return {vo, dvo_dt};
}

void
DmpAlg::showVo()
{
  report_->report("  t    vo(t)");
  double ub = voCrossingUpperBound();
  const double step = dt_ / 10.0;
  for (int i = 0;; ++i) {
    double t = t0_ + step * i;
    if (!(t < t0_ + ub))
      break;
    report_->report(" {:g} {:g}", t, Vo(t).first);
  }
}

std::pair<double, double>
DmpAlg::loadDelaySlew(const Pin *,
                      double elmore)
{
  if (!driver_valid_
      || elmore == 0.0
      // Elmore delay is small compared to driver slew.
      || elmore < drvr_slew_ * 1e-3) {
    double delay = elmore;
    double slew = drvr_slew_;
    return {delay, slew};
  }
  // Use the driver thresholds and rely on thresholdAdjust to
  // convert the delay and slew to the load's thresholds.
  try {
    elmore_ = elmore;
    p3_ = 1.0 / elmore;
    if (debug_->check("dmp_ceff", 4))
      showVl();
    double t_lower = t0_;
    double t_upper = vlCrossingUpperBound();
    double load_delay = findVlCrossing(vth_, t_lower, t_upper);
    double tl = findVlCrossing(vl_, t_lower, load_delay);
    double th = findVlCrossing(vh_, load_delay, t_upper);
    // Measure delay from Vo, the load dependent source excitation.
    double delay = load_delay - vo_delay_;
    // Convert measured slew to reported/table slew.
    double slew = (th - tl) / slew_derate_;
    if (delay < 0.0) {
      // Only report a problem if the difference is significant.
      if (-delay > vth_time_tol_ * vo_delay_)
        fail("load delay less than zero");
      // Use elmore delay.
      delay = elmore;
    }
    if (slew < drvr_slew_) {
      // Only report a problem if the difference is significant.
      if ((drvr_slew_ - slew) > vth_time_tol_ * drvr_slew_)
        fail("load slew less than driver slew");
      slew = drvr_slew_;
    }
    return {delay, slew};
  } catch (DmpError &error) {
    fail(error.what());
    double delay = elmore_;
    double slew = drvr_slew_;
    return {delay, slew};
  }
}

// Find t such that vl(t)=v.
double
DmpAlg::findVlCrossing(double vth,
                       double t_lower,
                       double t_upper)
{
  FindRootFunc vl_func = [&](double t, double &y, double &dy) {
    auto [vl, vl_dt] = Vl(t);
    y = vl - vth;
    dy = vl_dt;
  };
  auto [t_vth, failed] = findRoot(vl_func, t_lower, t_upper, vth_time_tol_,
                                  find_root_max_iter_);
  if (failed)
    throw DmpError("find Vl crossing failed");
  return t_vth;
}

double
DmpAlg::vlCrossingUpperBound()
{
  return voCrossingUpperBound() + elmore_ * 2.0;
}

std::pair<double, double>
DmpAlg::Vl(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0.0)
    return {0.0, 0.0};
  if (t1 <= dt_) {
    auto [vl0, dvl0_dt] = Vl0(t1);
    return {vl0 / dt_, dvl0_dt / dt_};
  }
  auto [vl0, dvl0_dt] = Vl0(t1);

  auto [vl0_dt, dvl0_dt_dt] = Vl0(t1 - dt_);

  double vl = (vl0 - vl0_dt) / dt_;
  double dvl_dt = (dvl0_dt - dvl0_dt_dt) / dt_;
  return {vl, dvl_dt};
}

void
DmpAlg::showVl()
{
  report_->report("  t    vl(t)");
  double ub = vlCrossingUpperBound();
  const double step = ub / 10.0;
  const double t_end = t0_ + ub * 2.0;
  for (int i = 0;; ++i) {
    double t = t0_ + step * i;
    if (!(t < t_end))
      break;
    report_->report(" {:g} {:g}", t, Vl(t).first);
  }
}

void
DmpAlg::fail(std::string_view reason)
{
  // Report failures with a unique debug flag.
  if (debug_->check("dmp_ceff", 1) || debug_->check("dcalc_error", 1))
    report_->report("delay_calc: DMP failed - {} c2={} rpi={} c1={} rd={}", reason,
                    units_->capacitanceUnit()->asString(c2_),
                    units_->resistanceUnit()->asString(rpi_),
                    units_->capacitanceUnit()->asString(c1_),
                    units_->resistanceUnit()->asString(rd_));
}

////////////////////////////////////////////////////////////////

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

DmpCap::DmpCap(StaState *sta) :
  DmpAlg(1,
         sta)
{
}

void
DmpCap::init(const LibertyLibrary *drvr_library,
             const LibertyCell *drvr_cell,
             const Pvt *pvt,
             const GateTableModel *gate_model,
             const RiseFall *rf,
             double rd,
             double in_slew,
             double c2,
             double rpi,
             double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP cap");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd, in_slew,
               c2, rpi, c1);
  ceff_ = c1 + c2;
}

std::pair<double, double>
DmpCap::gateDelaySlew()
{
  debugPrint(debug_, "dmp_ceff", 3, "    ceff = {}",
             units_->capacitanceUnit()->asString(ceff_));
  auto [delay, slew] = gateCapDelaySlew(ceff_);
  drvr_slew_ = slew;
  return {delay, slew};
}

std::pair<double, double>
DmpCap::loadDelaySlew(const Pin *,
                      double elmore)
{
  double delay = elmore;
  double slew = drvr_slew_;
  return {delay, slew};
}

void
DmpCap::evalDmpEqns()
{
}

std::pair<double, double>
DmpCap::V0(double)
{
  double vo = 0.0;
  double dvo_dt = 0.0;
  return {vo, dvo_dt};
}

double
DmpCap::voCrossingUpperBound()
{
  return 0.0;
}

std::pair<double, double>
DmpCap::Vl0(double)
{
  double vl = 0.0;
  double dvl_dt = 0.0;
  return {vl, dvl_dt};
}

////////////////////////////////////////////////////////////////

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

DmpPi::DmpPi(StaState *sta) :
  DmpAlg(3,
         sta)
{
}

void
DmpPi::init(const LibertyLibrary *drvr_library,
            const LibertyCell *drvr_cell,
            const Pvt *pvt,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double rd,
            double in_slew,
            double c2,
            double rpi,
            double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP Pi");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd, in_slew,
               c2, rpi, c1);

  // Find poles/zeros.
  z1_ = 1.0 / (rpi_ * c1_);
  k0_ = 1.0 / (rd_ * c2_);
  double a = rpi_ * rd_ * c1_ * c2_;
  double b = rd_ * (c1_ + c2_) + rpi_ * c1_;
  double sqrt_ = std::sqrt(b * b - 4 * a);
  p1_ = (b + sqrt_) / (2 * a);
  p2_ = (b - sqrt_) / (2 * a);

  double p1p2 = (p1_ * p2_);
  k2_ = z1_ / p1p2;
  k1_ = (1.0 - k2_ * (p1_ + p2_)) / p1p2;
  k4_ = (k1_ * p1_ + k2_) / (p2_ - p1_);
  k3_ = -k1_ - k4_;

  double z_ = (c1_ + c2_) / (rpi_ * c1_ * c2_);
  A_ = z_ / p1p2;
  B_ = (z_ - p1_) / (p1_ * (p1_ - p2_));
  D_ = (z_ - p2_) / (p2_ * (p2_ - p1_));
}

std::pair<double, double>
DmpPi::gateDelaySlew()
{
  driver_valid_ = false;
  double delay = 0.0;
  double slew = 0.0;
  try {
    findDriverParamsPi();
    ceff_ = x_[DmpParam::ceff];
    auto [table_delay, table_slew] = gateCapDelaySlew(ceff_);
    delay = table_delay;
    // slew = table_slew;
    try {
      auto [vo_delay, vo_slew] = findDriverDelaySlew();
      driver_valid_ = true;
      // Save Vo delay to measure load wire delay waveform.
      vo_delay_ = vo_delay;
      // delay = vo_delay;
      slew = vo_slew;
    } catch (DmpError &error) {
      fail(error.what());
      // Fall back to table slew.
      slew = table_slew;
    }
  } catch (DmpError &error) {
    fail(error.what());
    // Driver calculation failed - use Ceff=c1+c2.
    ceff_ = c1_ + c2_;
    std::tie(delay, slew) = gateCapDelaySlew(ceff_);
  }
  drvr_slew_ = slew;
  return {delay, slew};
}

void
DmpPi::findDriverParamsPi()
{
  try {
    findDriverParams(c2_ + c1_);
  } catch (DmpError &) {
    findDriverParams(c2_);
  }
}

// Given x_ as a vector of input parameters, fill fvec_ with the
// equations evaluated at x_ and fjac_ with the jacobian evaluated at x_.
void
DmpPi::evalDmpEqns()
{
  double t0 = x_[DmpParam::t0];
  double dt = x_[DmpParam::dt];
  double ceff = x_[DmpParam::ceff];

  if (ceff < 0.0)
    throw DmpError("eqn eval failed: ceff < 0");
  if (ceff > (c1_ + c2_))
    throw DmpError("eqn eval failed: ceff > c2 + c1");

  auto [t_vth, t_vl, slew] = gateDelays(ceff);
  if (slew == 0.0)
    throw DmpError("eqn eval failed: slew = 0");

  double ceff_time = slew / (vh_ - vl_);
  ceff_time = std::min(ceff_time, 1.4 * dt);

  if (dt <= 0.0)
    throw DmpError("eqn eval failed: dt < 0");

  double exp_p1_dt = exp2(-p1_ * dt);
  double exp_p2_dt = exp2(-p2_ * dt);
  double exp_dt_rd_ceff = exp2(-dt / (rd_ * ceff));

  double y50 = y(t_vth, t0, dt, ceff).first;
  // Match Vl.
  double y20 = y(t_vl, t0, dt, ceff).first;
  fvec_[DmpFunc::ipi] = ipiIceff(t0, dt, ceff_time, ceff);
  fvec_[DmpFunc::y50] = y50 - vth_;
  fvec_[DmpFunc::y20] = y20 - vl_;
  fjac_[DmpFunc::ipi][DmpParam::t0] = 0.0;
  fjac_[DmpFunc::ipi][DmpParam::dt] =
      (-A_ * dt + B_ * dt * exp_p1_dt - (2 * B_ / p1_) * (1.0 - exp_p1_dt)
       + D_ * dt * exp_p2_dt - (2 * D_ / p2_) * (1.0 - exp_p2_dt)
       + rd_ * ceff
           * (dt + dt * exp_dt_rd_ceff - 2 * rd_ * ceff * (1.0 - exp_dt_rd_ceff)))
      / (rd_ * dt * dt * dt);
  fjac_[DmpFunc::ipi][DmpParam::ceff] =
      (2 * rd_ * ceff - dt - (2 * rd_ * ceff + dt) * exp2(-dt / (rd_ * ceff)))
      / (dt * dt);

  std::tie(fjac_[DmpFunc::y20][DmpParam::t0],
           fjac_[DmpFunc::y20][DmpParam::dt],
           fjac_[DmpFunc::y20][DmpParam::ceff]) = dy(t_vl, t0, dt, ceff);

  std::tie(fjac_[DmpFunc::y50][DmpParam::t0],
           fjac_[DmpFunc::y50][DmpParam::dt],
           fjac_[DmpFunc::y50][DmpParam::ceff]) = dy(t_vth, t0, dt, ceff);

  if (debug_->check("dmp_ceff", 4)) {
    showX();
    showFvec();
    showJacobian();
    report_->report(".................");
  }
}

// Eqn 13, Eqn 14.
double
DmpPi::ipiIceff(double,
                double dt,
                double ceff_time,
                double ceff)
{
  double exp_p1_dt = exp2(-p1_ * ceff_time);
  double exp_p2_dt = exp2(-p2_ * ceff_time);
  double exp_dt_rd_ceff = exp2(-ceff_time / (rd_ * ceff));
  double ipi = (A_ * ceff_time + (B_ / p1_) * (1.0 - exp_p1_dt)
                + (D_ / p2_) * (1.0 - exp_p2_dt))
      / (rd_ * ceff_time * dt);
  double iceff =
      (rd_ * ceff * ceff_time - (rd_ * ceff) * (rd_ * ceff) * (1.0 - exp_dt_rd_ceff))
      / (rd_ * ceff_time * dt);
  return ipi - iceff;
}

std::pair<double, double>
DmpPi::V0(double t)
{
  double exp_p1 = exp2(-p1_ * t);
  double exp_p2 = exp2(-p2_ * t);
  double vo = k0_ * (k1_ + k2_ * t + k3_ * exp_p1 + k4_ * exp_p2);
  double dvo_dt = k0_ * (k2_ - k3_ * p1_ * exp_p1 - k4_ * p2_ * exp_p2);
  return {vo, dvo_dt};
}

std::pair<double, double>
DmpPi::Vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D4 = -p3_ * k0_ * k4_ / (p2_ - p3_);
  double D5 =
      k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_) + p3_ * k4_ / (p2_ - p3_));
  double exp_p1 = exp2(-p1_ * t);
  double exp_p2 = exp2(-p2_ * t);
  double exp_p3 = exp2(-p3_ * t);
  double vl = D1 + t + D3 * exp_p1 + D4 * exp_p2 + D5 * exp_p3;
  double dvl_dt = 1.0 - D3 * p1_ * exp_p1 - D4 * p2_ * exp_p2 - D5 * p3_ * exp_p3;
  return {vl, dvl_dt};
}

double
DmpPi::voCrossingUpperBound()
{
  return t0_ + dt_ + (c1_ + c2_) * (rd_ + rpi_) * 2.0;
}

////////////////////////////////////////////////////////////////

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

DmpOnePole::DmpOnePole(StaState *sta) :
  DmpAlg(2,
         sta)
{
}

void
DmpOnePole::evalDmpEqns()
{
  double t0 = x_[DmpParam::t0];
  double dt = x_[DmpParam::dt];

  auto [t_vth, t_vl, ignore1] = gateDelays(ceff_);
  double ignore2;

  if (dt <= 0.0)
    dt = x_[DmpParam::dt] = (t_vl - t_vth) / 100;

  fvec_[DmpFunc::y50] = y(t_vth, t0, dt, ceff_).first - vth_;
  fvec_[DmpFunc::y20] = y(t_vl, t0, dt, ceff_).first - vl_;

  if (debug_->check("dmp_ceff", 4)) {
    showX();
    showFvec();
  }

  std::tie(fjac_[DmpFunc::y20][DmpParam::t0],
           fjac_[DmpFunc::y20][DmpParam::dt],
           ignore2) = dy(t_vl, t0, dt, ceff_);

  std::tie(fjac_[DmpFunc::y50][DmpParam::t0],
           fjac_[DmpFunc::y50][DmpParam::dt],
           ignore2) = dy(t_vth, t0, dt, ceff_);

  if (debug_->check("dmp_ceff", 4)) {
    showJacobian();
    report_->report(".................");
  }
}

double
DmpOnePole::voCrossingUpperBound()
{
  return t0_ + dt_ + ceff_ * rd_ * 2.0;
}

////////////////////////////////////////////////////////////////

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

DmpZeroC2::DmpZeroC2(StaState *sta) :
  DmpOnePole(sta)
{
}

void
DmpZeroC2::init(const LibertyLibrary *drvr_library,
                const LibertyCell *drvr_cell,
                const Pvt *pvt,
                const GateTableModel *gate_model,
                const RiseFall *rf,
                double rd,
                double in_slew,
                double c2,
                double rpi,
                double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP C2=0");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd, in_slew,
               c2, rpi, c1);
  ceff_ = c1;

  z1_ = 1.0 / (rpi_ * c1_);
  p1_ = 1.0 / (c1_ * (rd_ + rpi_));

  k0_ = p1_ / z1_;
  k2_ = 1.0 / k0_;
  k1_ = (p1_ - z1_) / (p1_ * p1_);
  k3_ = -k1_;
}

std::pair<double, double>
DmpZeroC2::gateDelaySlew()
{
  double delay = 0.0;
  double slew = 0.0;
  try {
    findDriverParams(c1_);
    ceff_ = c1_;
    std::tie(delay, slew) = findDriverDelaySlew();
    driver_valid_ = true;
    vo_delay_ = delay;
  }
  catch (DmpError &error) {
    fail(error.what());
    // Fall back to table slew.
    driver_valid_ = false;
    ceff_ = c1_;
    std::tie(delay, slew) = gateCapDelaySlew(ceff_);
  }
  drvr_slew_ = slew;
  return {delay, slew};
}

std::pair<double, double>
DmpZeroC2::V0(double t)
{
  double exp_p1 = exp2(-p1_ * t);
  double vo = k0_ * (k1_ + k2_ * t + k3_ * exp_p1);
  double dvo_dt = k0_ * (k2_ - k3_ * p1_ * exp_p1);
  return {vo, dvo_dt};
}

std::pair<double, double>
DmpZeroC2::Vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_));
  double exp_p1 = exp2(-p1_ * t);
  double exp_p3 = exp2(-p3_ * t);
  double vl = D1 + t + D3 * exp_p1 + D5 * exp_p3;
  double dvl_dt = 1.0 - D3 * p1_ * exp_p1 - D5 * p3_ * exp_p3;
  return {vl, dvl_dt};
}

double
DmpZeroC2::voCrossingUpperBound()
{
  return t0_ + dt_ + c1_ * (rd_ + rpi_) * 2.0;
}

////////////////////////////////////////////////////////////////

// Newton-Raphson iteration to find zeros of a function.
// driver_param_tol_ is the scale that all changes in x must be under (1.0 = 100%).
// evalDmpEqns() fills fvec_ and fjac_.
void
DmpAlg::newtonRaphson()
{
  for (int k = 0; k < newton_raphson_max_iter_; k++) {
    evalDmpEqns();
    for (int i = 0; i < nr_order_; i++)
      // Right-hand side of linear equations.
      p_[i] = -fvec_[i];
    luDecomp();
    luSolve();

    bool all_under_x_tol = true;
    for (int i = 0; i < nr_order_; i++) {
      if (std::abs(p_[i]) > std::abs(x_[i]) * driver_param_tol_)
        all_under_x_tol = false;
      x_[i] += p_[i];
    }
    if (all_under_x_tol) {
      evalDmpEqns();
      return;
    }
  }
  throw DmpError("Newton-Raphson max iterations exceeded");
}

// luDecomp, luSolve based on MatClass from C. R. Birchenhall,
// University of Manchester
// ftp://ftp.mcc.ac.uk/pub/matclass/libmat.tar.Z

// Crout's Method of LU decomposition of square matrix, with implicit
// partial pivoting.  fjac_ is overwritten. U is explicit in the upper
// triangle and L is in multiplier form in the subdiagionals i.e. subdiag
// a[i,j] is the multiplier used to eliminate the [i,j] term.
//
// Replaces fjac_[0..nr_order_-1][*] by the LU decomposition.
// index_[0..nr_order_-1] is an output vector of the row permutations.
void
DmpAlg::luDecomp()
{
  const int size = nr_order_;

  // Find implicit scaling factors.
  for (int i = 0; i < size; i++) {
    double big = 0.0;
    for (int j = 0; j < size; j++) {
      double temp = std::abs(fjac_[i][j]);
      big = std::max(temp, big);
    }
    if (big == 0.0)
      throw DmpError("LU decomposition: no non-zero row element");
    scale_[i] = 1.0 / big;
  }
  int size_1 = size - 1;
  for (int j = 0; j < size; j++) {
    // Run down jth column from top to diag, to form the elements of U.
    for (int i = 0; i < j; i++) {
      double sum = fjac_[i][j];
      for (int k = 0; k < i; k++)
        sum -= fjac_[i][k] * fjac_[k][j];
      fjac_[i][j] = sum;
    }
    // Run down jth subdiag to form the residuals after the elimination
    // of the first j-1 subdiags.  These residuals diviyded by the
    // appropriate diagonal term will become the multipliers in the
    // elimination of the jth. subdiag. Find index of largest scaled
    // term in imax.
    double big = 0.0;
    int imax = 0;
    for (int i = j; i < size; i++) {
      double sum = fjac_[i][j];
      for (int k = 0; k < j; k++)
        sum -= fjac_[i][k] * fjac_[k][j];
      fjac_[i][j] = sum;
      double dum = scale_[i] * std::abs(sum);
      if (dum >= big) {
        big = dum;
        imax = i;
      }
    }
    // Permute current row with imax.
    if (j != imax) {
      // Yes, do so...
      for (int k = 0; k < size; k++) {
        double dum = fjac_[imax][k];
        fjac_[imax][k] = fjac_[j][k];
        fjac_[j][k] = dum;
      }
      scale_[imax] = scale_[j];
    }
    index_[j] = imax;
    // If diag term is not zero divide subdiag to form multipliers.
    if (fjac_[j][j] == 0.0)
      fjac_[j][j] = tiny_double_;
    if (j != size_1) {
      double pivot = 1.0 / fjac_[j][j];
      for (int i = j + 1; i < size; i++)
        fjac_[i][j] *= pivot;
    }
  }
}

// Solves fjac_ * x = p_ for x, assuming fjac_ is LU form from luDecomp.
// Solution overwrites p_.
void
DmpAlg::luSolve()
{
  const int size = nr_order_;

  // Transform p_ allowing for leading zeros.
  int non_zero = -1;
  for (int i = 0; i < size; i++) {
    int iperm = index_[i];
    double sum = p_[iperm];
    p_[iperm] = p_[i];
    if (non_zero != -1) {
      for (int j = non_zero; j <= i - 1; j++)
        sum -= fjac_[i][j] * p_[j];
    }
    else {
      if (sum != 0.0)
        non_zero = i;
    }
    p_[i] = sum;
  }
  // Backsubstitution.
  for (int i = size - 1; i >= 0; i--) {
    double sum = p_[i];
    for (int j = i + 1; j < size; j++)
      sum -= fjac_[i][j] * p_[j];
    p_[i] = sum / fjac_[i][i];
  }
}

////////////////////////////////////////////////////////////////

bool DmpCeffDelayCalc::unsuppored_model_warned_ = false;

DmpCeffDelayCalc::DmpCeffDelayCalc(StaState *sta) :
  LumpedCapDelayCalc(sta),
  dmp_cap_(new DmpCap(sta)),
  dmp_pi_(new DmpPi(sta)),
  dmp_zero_c2_(new DmpZeroC2(sta))
{
}

DmpCeffDelayCalc::~DmpCeffDelayCalc()
{
  delete dmp_cap_;
  delete dmp_pi_;
  delete dmp_zero_c2_;
}

ArcDcalcResult
DmpCeffDelayCalc::gateDelay(const Pin *drvr_pin,
                            const TimingArc *arc,
                            const Slew &in_slew,
                            float load_cap,
                            const Parasitic *parasitic,
                            const LoadPinIndexMap &load_pin_index_map,
                            const Scene *scene,
                            const MinMax *min_max)
{
  parasitics_ = scene->parasitics(min_max);
  const RiseFall *rf = arc->toEdge()->asRiseFall();
  const LibertyCell *drvr_cell = arc->from()->libertyCell();
  const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();

  GateTableModel *table_model = arc->gateTableModel(scene, min_max);
  if (table_model && parasitic) {
    float in_slew1 = delayAsFloat(in_slew);
    float c2, rpi, c1;
    parasitics_->piModel(parasitic, c2, rpi, c1);
    if (std::isnan(c2) || std::isnan(c1) || std::isnan(rpi))
      report_->error(1040, "parasitic Pi model has NaNs.");
    const Pvt *pvt = pinPvt(drvr_pin, scene, min_max);
    setCeffAlgorithm(drvr_library, drvr_cell, pvt,
                     table_model, rf, in_slew1, c2, rpi, c1);
    auto [gate_delay, drvr_slew] = gateDelaySlew();

    // Fill in pocv parameters.
    double ceff = dmp_alg_->ceff();
    ArcDelay gate_delay2(gate_delay);
    Slew drvr_slew2(drvr_slew);
    if (variables_->pocvEnabled())
      table_model->gateDelayPocv(pvt, in_slew1, ceff, min_max,
                                 variables_->pocvMode(),
                                 gate_delay2, drvr_slew2);
    ArcDcalcResult dcalc_result(load_pin_index_map.size());
    dcalc_result.setGateDelay(gate_delay2);
    dcalc_result.setDrvrSlew(drvr_slew2);

    for (const auto &[load_pin, load_idx] : load_pin_index_map) {
      double wire_delay;
      double load_slew;
      loadDelaySlew(load_pin, drvr_slew, rf, drvr_library, parasitic,
                    wire_delay, load_slew);
      // Copy pocv params from driver.
      ArcDelay wire_delay2(gate_delay2);
      Slew load_slew2(drvr_slew2);
      delaySetMean(wire_delay2, wire_delay);
      delaySetMean(load_slew2, load_slew);
      dcalc_result.setWireDelay(load_idx, wire_delay2);
      dcalc_result.setLoadSlew(load_idx, load_slew2);
    }
    return dcalc_result;
  }
  else {
    ArcDcalcResult dcalc_result =
        LumpedCapDelayCalc::gateDelay(drvr_pin, arc, in_slew, load_cap, parasitic,
                                      load_pin_index_map, scene, min_max);
    if (parasitic && !unsuppored_model_warned_) {
      unsuppored_model_warned_ = true;
      report_->warn(1041,
                    "cell {} delay model not supported on SPF parasitics by DMP "
                    "delay calculator",
                    drvr_cell->name());
    }
    return dcalc_result;
  }
}

void
DmpCeffDelayCalc::setCeffAlgorithm(const LibertyLibrary *drvr_library,
                                   const LibertyCell *drvr_cell,
                                   const Pvt *pvt,
                                   const GateTableModel *gate_model,
                                   const RiseFall *rf,
                                   double in_slew,
                                   double c2,
                                   double rpi,
                                   double c1)
{
  double rd = 0.0;
  if (gate_model) {
    rd = gateModelRd(drvr_cell, gate_model, rf, in_slew, c2, c1, pvt);
    // Zero Rd means the table is constant and thus independent of load cap.
    if (rd < 1e-2
        // Rpi is small compared to Rd, which makes the load capacitive.
        || rpi < rd * 1e-3
        // c1/Rpi can be ignored.
        || (c1 == 0.0 || c1 < c2 * 1e-3 || rpi == 0.0))
      dmp_alg_ = dmp_cap_;
    else if (c2 < c1 * 1e-3)
      dmp_alg_ = dmp_zero_c2_;
    else
      // The full monty.
      dmp_alg_ = dmp_pi_;
  }
  else
    dmp_alg_ = dmp_cap_;
  dmp_alg_->init(drvr_library, drvr_cell, pvt, gate_model, rf, rd, in_slew,
                 c2, rpi, c1);
  debugPrint(debug_, "dmp_ceff", 3,
             "    DMP in_slew = {} c2 = {} rpi = {} c1 = {} Rd = {} ({} alg)",
             units_->timeUnit()->asString(in_slew),
             units_->capacitanceUnit()->asString(c2),
             units_->resistanceUnit()->asString(rpi),
             units_->capacitanceUnit()->asString(c1),
             units_->resistanceUnit()->asString(rd), dmp_alg_->name());
}

std::string
DmpCeffDelayCalc::reportGateDelay(const Pin *drvr_pin,
                                  const TimingArc *arc,
                                  const Slew &in_slew,
                                  float load_cap,
                                  const Parasitic *parasitic,
                                  const LoadPinIndexMap &load_pin_index_map,
                                  const Scene *scene,
                                  const MinMax *min_max,
                                  int digits)
{
  ArcDcalcResult dcalc_result =
      gateDelay(drvr_pin, arc, in_slew, load_cap, parasitic, load_pin_index_map,
                scene, min_max);
  GateTableModel *model = arc->gateTableModel(scene, min_max);
  float c_eff = 0.0;
  std::string result;
  const LibertyCell *drvr_cell = arc->to()->libertyCell();
  const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
  const Units *units = drvr_library->units();
  const Unit *cap_unit = units->capacitanceUnit();
  const Unit *res_unit = units->resistanceUnit();
  if (parasitic && dmp_alg_) {
    Parasitics *parasitics = scene->parasitics(min_max);

    c_eff = dmp_alg_->ceff();
    float c2, rpi, c1;
    parasitics->piModel(parasitic, c2, rpi, c1);
    result += "Pi model C2=";
    result += cap_unit->asString(c2, digits);
    result += " Rpi=";
    result += res_unit->asString(rpi, digits);
    result += " C1=";
    result += cap_unit->asString(c1, digits);
    result += ", Ceff=";
    result += cap_unit->asString(c_eff, digits);
    result += '\n';
  }
  else
    c_eff = load_cap;
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    result += model->reportGateDelay(pinPvt(drvr_pin, scene, min_max),
                                     in_slew1, c_eff, min_max,
                                     variables_->pocvMode(), digits);
    result += "Driver waveform slew = ";
    result += delayAsString(dcalc_result.drvrSlew(), min_max, digits, this);
    result += '\n';
  }
  return result;
}

static double
gateModelRd(const LibertyCell *cell,
            const GateTableModel *gate_model,
            const RiseFall *rf,
            double in_slew,
            double c2,
            double c1,
            const Pvt *pvt)
{
  float cap1 = c1 + c2;
  float cap2 = cap1 + 1e-15;
  float d1, d2, s1, s2;
  gate_model->gateDelay(pvt, in_slew, cap1, d1, s1);
  gate_model->gateDelay(pvt, in_slew, cap2, d2, s2);
  double vth = cell->libertyLibrary()->outputThreshold(rf);
  float rd = -std::log(vth) * std::abs(d1 - d2) / (cap2 - cap1);
  return rd;
}

std::pair<double, double>
DmpCeffDelayCalc::gateDelaySlew()
{
  return dmp_alg_->gateDelaySlew();
}

std::optional<std::pair<double, double>>
DmpCeffDelayCalc::loadDelaySlewElmore(const Pin *load_pin,
                                      double elmore)
{
  if (dmp_alg_)
    return dmp_alg_->loadDelaySlew(load_pin, elmore);
  return std::nullopt;
}

// Notify algorithm components.
void
DmpCeffDelayCalc::copyState(const StaState *sta)
{
  StaState::copyState(sta);
  dmp_cap_->copyState(sta);
  dmp_pi_->copyState(sta);
  dmp_zero_c2_->copyState(sta);
}

DmpError::DmpError(std::string_view what) :
  what_(what)
{
  //sta::print(stdout, "DmpError {}\n", what);
}

// This saves about 2.5% in overall run time on designs with SPEF.
// https://codingforspeed.com/using-faster-exponential-approximation
static double
exp2(double x)
{
  if (x < -12.0)
    // exp(-12) = 6.1e-6
    return 0.0;
  else {
    double y = 1.0 + x / 4096.0;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    y *= y;
    return y;
  }
}

}  // namespace sta
