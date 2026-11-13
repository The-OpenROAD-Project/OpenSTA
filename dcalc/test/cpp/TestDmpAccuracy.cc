// Accuracy, physical-invariant, numerical-stability, and cache-consistency tests
// for the DMP delay calculators (`dmp_ceff_elmore` and `dmp_ceff_two_pole`) and
// 2-pole parasitic reduction (`reduceToPiPoleResidue2`).
//
// Every test uses self-contained programmatic fixtures and analytical / linear
// RC circuit ODE ground-truth helpers (zero external PDK or simulator
// dependencies).

#include <gtest/gtest.h>
#include <tcl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ArcDelayCalc.hh"
#include "Delay.hh"
#include "DelayCalc.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "PocvMode.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Sta.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "Transition.hh"
#include "Variables.hh"

namespace sta {
namespace {

////////////////////////////////////////////////////////////////
// Exact Analytical & Linear-RC ODE Reference Helpers
////////////////////////////////////////////////////////////////

// Normalized charge Q * Rd / dt = (x - (1 - exp(-x))) / x^2 for normalized
// ramp duration x = dt / tau, using a Taylor series when x < 1e-2.
double
rampCharge(double x)
{
  if (x < 1.0e-2) {
    return 0.5
        - x
        * (1.0 / 6.0
           - x * (1.0 / 24.0 - x * (1.0 / 120.0 - x * (1.0 / 720.0 - x / 5040.0))));
  }
  return (x + std::expm1(-x)) / (x * x);
}

template <typename Pred>
double
bisectUpper(double lo,
            double hi,
            int iters,
            Pred &&pred)
{
  for (int i = 0; i < iters; ++i) {
    const double mid = 0.5 * (lo + hi);
    if (pred(mid)) {
      lo = mid;
    }
    else {
      hi = mid;
    }
  }
  return 0.5 * (lo + hi);
}

// Normalized 0 -> 1 step response for a single-pole RC system H(s) = 1 / (1 +
// tau * s) excited by a linear ramp from 0 to 1 over duration `dt`.
double
yRampSinglePole(double t,
                double tau,
                double dt)
{
  if (t <= 0.0) {
    return 0.0;
  }
  if (tau <= 0.0) {
    return std::min(1.0, t / dt);
  }
  if (t <= dt) {
    return (t * t) / (tau * dt) * rampCharge(t / tau);
  }
  return 1.0 + (tau / dt) * std::expm1(-dt / tau) * std::exp(-(t - dt) / tau);
}

double
solveSinglePoleRampCrossing(double tau,
                            double dt,
                            double v_target)
{
  return bisectUpper(0.0, dt + std::max(1.0e-12, 30.0 * tau), 90, [&](double t) {
    return yRampSinglePole(t, tau, dt) < v_target;
  });
}

// Closed-form poles/residues of a 2-stage RC ladder:
//   Vsrc --[r1]-- n1(c1) --[r2]-- n2(c2)
// Computes fast/slow poles using Vieta's formula to prevent quadratic
// cancellation, and exact residues at either near tap (n1) or far tap (n2).
struct TwoPoleTransfer
{
  double p1;
  double p2;
  double k1;
  double k2;
};

// Normalized 0 -> 1 response for impulse h(t) = k1*e^(-p1*t) + k2*e^(-p2*t)
// (with DC gain k1/p1 + k2/p2 = 1) excited by a linear ramp over duration `dt`.
// Evaluated in overflow-free form for arbitrarily large p * dt.
double
yRampTwoPole(double t,
             double p1,
             double p2,
             double k1,
             double k2,
             double dt)
{
  if (t <= 0.0) {
    return 0.0;
  }
  const double ramp_t = std::min(t, dt);
  const double tail_t = t - ramp_t;
  auto pole_lag = [&](double p, double k) {
    const double decay = tail_t > 0.0 ? std::exp(-p * tail_t) : 1.0;
    return (k / (p * p)) * decay * (-std::expm1(-p * ramp_t));
  };
  return (ramp_t - (pole_lag(p1, k1) + pole_lag(p2, k2))) / dt;
}

double
yRampTwoPole(double t,
             const TwoPoleTransfer &tf,
             double dt)
{
  return yRampTwoPole(t, tf.p1, tf.p2, tf.k1, tf.k2, dt);
}

double
solveTwoPoleRampCrossing(double p1,
                         double p2,
                         double k1,
                         double k2,
                         double dt,
                         double v_target)
{
  const double hi = dt + std::max(1.0e-12, 30.0 / std::min(p1, p2));
  return bisectUpper(0.0, hi, 90, [&](double t) {
    return yRampTwoPole(t, p1, p2, k1, k2, dt) < v_target;
  });
}

double
solveTwoPoleRampCrossing(const TwoPoleTransfer &tf,
                         double dt,
                         double v_target)
{
  return solveTwoPoleRampCrossing(tf.p1, tf.p2, tf.k1, tf.k2, dt, v_target);
}

TwoPoleTransfer
computeTwoStageLadder(double r1,
                      double c1,
                      double r2,
                      double c2,
                      bool near_tap)
{
  const double b1 = r1 * (c1 + c2) + r2 * c2;
  const double b2 = r1 * c1 * r2 * c2;
  const double disc = std::sqrt(b1 * b1 - 4.0 * b2);
  const double p1 = (b1 + disc) / (2.0 * b2);
  const double p2 = 1.0 / (b2 * p1);
  const double z_inv = near_tap ? r2 * c2 : 0.0;
  const double residue_scale = (p1 * p2) / (p1 - p2);
  const double k1 = residue_scale * (z_inv * p1 - 1.0);
  const double k2 = residue_scale * (1.0 - z_inv * p2);
  return {p1, p2, k1, k2};
}

////////////////////////////////////////////////////////////////
// Self-Contained Temporary Design & Liberty Helper Fixture
////////////////////////////////////////////////////////////////

class ScopedTempDir
{
public:
  ScopedTempDir()
  {
    static std::atomic<unsigned long long> counter{0};
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    std::ostringstream oss;
    oss << "opensta_dmp_accuracy_" << static_cast<long>(getpid()) << "_" << id;
    dir_ = std::filesystem::temp_directory_path() / oss.str();
    std::filesystem::create_directories(dir_);
  }

  ~ScopedTempDir()
  {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  ScopedTempDir(const ScopedTempDir &) = delete;
  ScopedTempDir &operator=(const ScopedTempDir &) = delete;

  std::string writeFile(std::string_view name,
                        std::string_view contents) const
  {
    const std::filesystem::path file_path = dir_ / std::string(name);
    std::ofstream out(file_path, std::ios::binary);
    out << contents;
    out.close();
    return file_path.string();
  }

private:
  std::filesystem::path dir_;
};

std::string
formatFloatRow(const std::vector<double> &vals)
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6);
  for (size_t i = 0; i < vals.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << vals[i];
  }
  return oss.str();
}

std::string
makeNldmBufLibText(std::string_view lib_name,
                   std::string_view cell_name,
                   double vth_rise,
                   double vth_fall,
                   double vl_rise,
                   double vl_fall,
                   double vh_rise,
                   double vh_fall,
                   double derate,
                   double pin_cap_ff,
                   std::string_view idx1,
                   std::string_view idx2,
                   std::string_view d_rows,
                   std::string_view s_rows)
{
  std::ostringstream lib;
  lib << std::fixed << std::setprecision(4) << "library (" << lib_name << ") {\n"
      << "  delay_model : table_lookup;\n"
      << "  time_unit : \"1ps\";\n"
      << "  voltage_unit : \"1V\";\n"
      << "  current_unit : \"1mA\";\n"
      << "  pulling_resistance_unit : \"1kohm\";\n"
      << "  leakage_power_unit : \"1nW\";\n"
      << "  capacitive_load_unit (1, ff);\n"
      << "  nom_process : 1.0;\n"
      << "  nom_temperature : 25.0;\n"
      << "  nom_voltage : 0.8;\n"
      << "  input_threshold_pct_rise : " << vth_rise << ";\n"
      << "  output_threshold_pct_rise : " << vth_rise << ";\n"
      << "  input_threshold_pct_fall : " << vth_fall << ";\n"
      << "  output_threshold_pct_fall : " << vth_fall << ";\n"
      << "  slew_lower_threshold_pct_rise : " << vl_rise << ";\n"
      << "  slew_lower_threshold_pct_fall : " << vl_fall << ";\n"
      << "  slew_upper_threshold_pct_rise : " << vh_rise << ";\n"
      << "  slew_upper_threshold_pct_fall : " << vh_fall << ";\n"
      << "  slew_derate_from_library : " << derate << ";\n"
      << "  lu_table_template (t2) {\n"
      << "    variable_1 : input_net_transition;\n"
      << "    variable_2 : total_output_net_capacitance;\n"
      << "    index_1 (\"" << idx1 << "\");\n"
      << "    index_2 (\"" << idx2 << "\");\n"
      << "  }\n"
      << "  cell (" << cell_name << ") {\n"
      << "    pin (A) { direction : input; capacitance : " << pin_cap_ff << "; }\n"
      << "    pin (Y) {\n"
      << "      direction : output;\n"
      << "      function : \"A\";\n"
      << "      timing () {\n"
      << "        related_pin : \"A\";\n"
      << "        timing_sense : positive_unate;\n"
      << "        cell_rise (t2) { values (" << d_rows << "); }\n"
      << "        cell_fall (t2) { values (" << d_rows << "); }\n"
      << "        rise_transition (t2) { values (" << s_rows << "); }\n"
      << "        fall_transition (t2) { values (" << s_rows << "); }\n"
      << "      }\n"
      << "    }\n"
      << "  }\n"
      << "}\n";
  return lib.str();
}

std::string
makeLinearNldmLibText(std::string_view lib_name,
                      std::string_view cell_name,
                      double vth_rise,
                      double vth_fall,
                      double vl_rise,
                      double vl_fall,
                      double vh_rise,
                      double vh_fall,
                      double derate,
                      double pin_cap_ff,
                      const std::vector<double> &in_slews_ps,
                      const std::vector<double> &caps_ff,
                      const std::vector<double> &delays_ps,
                      const std::vector<double> &slews_ps)
{
  const auto repeat_row = [&](const std::vector<double> &vals) {
    const std::string quoted = "\"" + formatFloatRow(vals) + "\"";
    std::string rows;
    rows.reserve(in_slews_ps.size() * (quoted.size() + 2));
    for (size_t r = 0; r < in_slews_ps.size(); ++r) {
      if (r > 0) {
        rows += ", ";
      }
      rows += quoted;
    }
    return rows;
  };
  return makeNldmBufLibText(lib_name, cell_name, vth_rise, vth_fall, vl_rise,
                            vl_fall, vh_rise, vh_fall, derate, pin_cap_ff,
                            formatFloatRow(in_slews_ps), formatFloatRow(caps_ff),
                            repeat_row(delays_ps), repeat_row(slews_ps));
}

// Generates a 100% causal single-pole linear-RC NLDM library from an exact
// Thevenin model: driver resistance `rd_ohm`, intrinsic step shift `t0_s`,
// internal ramp duration `dt_s`, and threshold fractions (`vth`, `vl`, `vh`).
std::string
makeCausalOnePoleLibText(std::string_view lib_name,
                         std::string_view cell_name,
                         double rd_ohm,
                         double t0_s,
                         double dt_s,
                         double vth,
                         double vl,
                         double vh,
                         double pin_cap_ff,
                         const std::vector<double> &caps_ff)
{
  std::vector<double> delays_ps;
  std::vector<double> slews_ps;
  delays_ps.reserve(caps_ff.size());
  slews_ps.reserve(caps_ff.size());
  for (double c_ff : caps_ff) {
    const double tau = rd_ohm * c_ff * 1e-15;
    delays_ps.push_back((t0_s + solveSinglePoleRampCrossing(tau, dt_s, vth)) * 1e12);
    const double t_vl = solveSinglePoleRampCrossing(tau, dt_s, vl);
    const double t_vh = solveSinglePoleRampCrossing(tau, dt_s, vh);
    slews_ps.push_back((t_vh - t_vl) * 1e12);
  }
  return makeLinearNldmLibText(lib_name, cell_name, vth * 100.0, (1.0 - vth) * 100.0,
                               vl * 100.0, vl * 100.0, vh * 100.0, vh * 100.0, 1.0,
                               pin_cap_ff, {1.0, 100.0}, caps_ff, delays_ps,
                               slews_ps);
}

struct StageMetrics
{
  double gate_delay{0.0};
  double drvr_slew{0.0};
  double wire_delay{0.0};
  double load_slew{0.0};
};

double
bilinearEval2d(const std::vector<double> &x_axis,
               const std::vector<double> &y_axis,
               const std::vector<std::vector<double>> &grid,
               double x,
               double y)
{
  auto axis_coord = [](const std::vector<double> &axis, double v) {
    size_t i = 0;
    while (i + 2 < axis.size() && v >= axis[i + 1]) {
      ++i;
    }
    return std::pair{i, (v - axis[i]) / (axis[i + 1] - axis[i])};
  };
  const auto [ix, tx] = axis_coord(x_axis, x);
  const auto [iy, ty] = axis_coord(y_axis, y);
  const double y_lo = std::lerp(grid[ix][iy], grid[ix + 1][iy], tx);
  const double y_hi = std::lerp(grid[ix][iy + 1], grid[ix + 1][iy + 1], tx);
  return std::lerp(y_lo, y_hi, ty);
}

double
extractMarkedValue(std::string_view text,
                   std::string_view marker)
{
  const size_t pos = text.find(marker);
  if (pos == std::string_view::npos) {
    return std::nan("");
  }
  const size_t start = pos + marker.size();
  std::istringstream iss{std::string(text.substr(start))};
  double val = std::nan("");
  if (!(iss >> val)) {
    return std::nan("");
  }
  return val;
}

class DmpAccuracyTest : public ::testing::Test
{
protected:
  struct LadderCrossings
  {
    double t_vth;
    double t_vl;
    double t_vh;
  };

  // Closed-form 1996 Dartu-Menezes-Pileggi Pi equilibrium solver matching total
  // Pi charge and capacitive charge over [t0, t0 + dt] (without 1.4*dt overshoot
  // and using Vieta's formula for p2). Used as analytical DMP ground truth for
  // general non-linear NLDM tables where no closed-form Thevenin driver exists.
  static double solveExactDmpCeff(double rd,
                                  double rpi,
                                  double c2,
                                  double c1,
                                  double vth,
                                  double vl,
                                  double vh,
                                  double derate,
                                  const std::vector<double> &caps_f,
                                  const std::vector<double> &slews_s)
  {
    const double a = rpi * rd * c1 * c2;
    const double b = rd * (c1 + c2) + rpi * c1;
    const double disc = std::sqrt(b * b - 4.0 * a);
    const double p1 = (b + disc) / (2.0 * a);
    const double p2 = 1.0 / (a * p1);
    const double z = (c1 + c2) / (rpi * c1 * c2);
    const double w1 = (p1 - z) / (p1 - p2);
    const double w2 = (z - p2) / (p1 - p2);

    auto interp_slew = [&](double cap) -> double {
      if (caps_f.size() == 1) {
        return slews_s[0];
      }
      const auto it = std::upper_bound(caps_f.begin() + 1, caps_f.end() - 1, cap);
      const size_t idx = static_cast<size_t>(it - caps_f.begin()) - 1;
      const double w = (cap - caps_f[idx]) / (caps_f[idx + 1] - caps_f[idx]);
      return slews_s[idx] + w * (slews_s[idx + 1] - slews_s[idx]);
    };

    // Solve ramp duration dt matching (y(u_vl) = vl, y(u_vl + delta) = vth).
    auto solve_dt = [&](double c) -> double {
      const double slew = interp_slew(c) * derate;
      const double delta = slew * (vth - vl) / (vh - vl);
      const double rc = rd * c;
      return bisectUpper(1.0e-15, 10.0 * slew / (vh - vl), 70, [&](double dt) {
        const double u_vl = solveSinglePoleRampCrossing(rc, dt, vl);
        return yRampSinglePole(u_vl + delta, rc, dt) > vth;
      });
    };

    return bisectUpper(c2, c1 + c2, 80, [&](double c) {
      const double dt = solve_dt(c);
      const double q_pi = w1 * rampCharge(p1 * dt) + w2 * rampCharge(p2 * dt);
      return q_pi > rampCharge(dt / (rd * c));
    });
  }

  // Second-order A-stable Crank-Nicolson (trapezoidal) RC ladder integrator for
  // multi-section ladders excited by a linear 0 -> 1 voltage ramp of duration
  // `dt_ramp`. Returns interpolated crossing times at `vth`, `vl`, and `vh` for
  // `tap_idx` (0-based node index).
  static LadderCrossings solveTrapezoidalRcLadder(const std::vector<double> &r_vals,
                                                  const std::vector<double> &c_vals,
                                                  size_t tap_idx,
                                                  double dt_ramp,
                                                  double vth,
                                                  double vl,
                                                  double vh,
                                                  double h_step,
                                                  double t_max)
  {
    // Crank-Nicolson: (2/h * C + G) v_{k+1} = (2/h * C - G) v_k + b_k + b_{k+1}.
    // Substituting z = v_{k+1} + v_k gives (2/h * C + G) z = (4/h * C) v_k
    // + b_k + b_{k+1}, eliminating G * v_k. Factor the constant matrix once.
    struct Node
    {
      double four_c_over_h;
      double g_left;
      double inv_d;
      double upper;
    };
    const size_t n = r_vals.size();
    const double two_over_h = 2.0 / h_step;
    std::vector<Node> nodes;
    nodes.reserve(n);
    double prev_upper = 0.0;
    for (size_t i = 0; i < n; ++i) {
      const double g_left = 1.0 / r_vals[i];
      const double g_right = (i + 1 < n) ? (1.0 / r_vals[i + 1]) : 0.0;
      const double two_c_over_h = two_over_h * c_vals[i];
      const double inv_d =
          1.0 / (two_c_over_h + g_left + g_right - g_left * prev_upper);
      prev_upper = g_right * inv_d;
      nodes.push_back({2.0 * two_c_over_h, g_left, inv_d, prev_upper});
    }

    std::vector<double> v(n, 0.0);
    std::vector<double> z(n, 0.0);
    LadderCrossings out{-1.0, -1.0, -1.0};
    const int steps = static_cast<int>(std::ceil(t_max / h_step));
    double prev_v_tap = 0.0;
    double prev_src = 0.0;

    for (int step = 1; step <= steps; ++step) {
      const double curr_t = step * h_step;
      const double curr_src = (curr_t < dt_ramp) ? (curr_t / dt_ramp) : 1.0;

      // Input source term g_0 * (prev_src + curr_src) acts as z_{-1}.
      double z_prev = prev_src + curr_src;
      for (size_t i = 0; i < n; ++i) {
        const Node &node = nodes[i];
        z_prev = (node.four_c_over_h * v[i] + node.g_left * z_prev) * node.inv_d;
        z[i] = z_prev;
      }
      double z_next = 0.0;
      for (size_t k = n; k > 0; --k) {
        const size_t i = k - 1;
        z_next = z[i] + nodes[i].upper * z_next;
        v[i] = z_next - v[i];
      }

      const double prev_t = (step - 1) * h_step;
      const double curr_v_tap = v[tap_idx];
      auto check_cross = [&](double target, double &slot) {
        if (slot < 0.0 && prev_v_tap <= target && curr_v_tap >= target
            && curr_v_tap > prev_v_tap) {
          const double frac = (target - prev_v_tap) / (curr_v_tap - prev_v_tap);
          slot = prev_t + frac * h_step;
        }
      };
      check_cross(vth, out.t_vth);
      check_cross(vl, out.t_vl);
      check_cross(vh, out.t_vh);
      if (out.t_vth >= 0.0 && out.t_vl >= 0.0 && out.t_vh >= 0.0) {
        return out;
      }
      prev_v_tap = curr_v_tap;
      prev_src = curr_src;
    }
    return out;
  }

  // Helper Liberty text for an ideal voltage-ramp driver (constant cell delay
  // across load capacitance -> Rd = 0 -> exact driver ramp dt = slew/(vh - vl)).
  static std::string makeIdealRampDriverLib(std::string_view lib_name,
                                            double vl,
                                            double vh,
                                            const std::vector<double> &in_slews_ps,
                                            double pin_cap_ff = 1.0)
  {
    std::ostringstream d_rows;
    std::ostringstream s_rows;
    s_rows << std::fixed << std::setprecision(4);
    for (size_t i = 0; i < in_slews_ps.size(); ++i) {
      if (i > 0) {
        d_rows << ", ";
        s_rows << ", ";
      }
      d_rows << "\"10.0, 10.0\"";
      s_rows << "\"" << in_slews_ps[i] << ", " << in_slews_ps[i] << "\"";
    }
    return makeNldmBufLibText(lib_name, "IDEAL_DRV", 50.0, 50.0, vl, vl, vh, vh,
                              1.0, pin_cap_ff, formatFloatRow(in_slews_ps),
                              "1.0, 500.0", d_rows.str(), s_rows.str());
  }

  void SetUp() override
  {
    interp_ = Tcl_CreateInterp();
    initSta();
    sta_ = new Sta;
    Sta::setSta(sta_);
    sta_->makeComponents();
    sta_->setTclInterp(interp_);
  }

  void TearDown() override
  {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_) {
      Tcl_DeleteInterp(interp_);
      interp_ = nullptr;
    }
  }

  LibertyLibrary *readLibertyText(std::string_view lib_text,
                                  std::string_view file_name = "lib.lib")
  {
    return sta_->readLiberty(temp_dir_.writeFile(file_name, lib_text),
                             sta_->cmdScene(), MinMaxAll::all(), false);
  }

  void loadVerilogAndLink(std::string_view verilog_text,
                          const char *top_name = "top")
  {
    const std::string v_path = temp_dir_.writeFile("test.v", verilog_text);
    ASSERT_TRUE(sta_->readVerilog(v_path));
    ASSERT_TRUE(sta_->linkDesign(top_name, true));
  }

  Pin *findPin(std::string_view path_name) const
  {
    return sta_->network()->findPin(path_name);
  }

  void setInputTransition(const char *port_name,
                          double slew_s)
  {
    Network *network = sta_->network();
    Cell *top_cell = network->cell(network->topInstance());
    Port *port = network->findPort(top_cell, port_name);
    ASSERT_NE(port, nullptr);
    sta_->setInputSlew(port, RiseFallBoth::riseFall(), MinMaxAll::all(),
                       static_cast<float>(slew_s), sta_->cmdSdc());
  }

  void setPiElmoreModel(Pin *drvr_pin,
                        Pin *load_pin,
                        double c2_f,
                        double rpi_ohm,
                        double c1_f,
                        double elmore_s)
  {
    for (const RiseFall *rf : RiseFall::range()) {
      sta_->makePiElmore(drvr_pin, rf, MinMaxAll::all(), static_cast<float>(c2_f),
                         static_cast<float>(rpi_ohm), static_cast<float>(c1_f));
      sta_->setElmore(drvr_pin, load_pin, rf, MinMaxAll::all(),
                      static_cast<float>(elmore_s));
    }
  }

  // Builds a detailed RC ladder net parasitic:
  //   drvr_pin --[r0]-- tap0 --[r1]-- tap1 ...
  // If `includes_pin_caps` is false, caller supplies wire cap only; if true,
  // total grounded node capacitance is given directly in `node_caps_f`.
  void buildRcLadderParasitic(const Net *net,
                              const std::vector<Pin *> &pins,
                              const std::vector<double> &r_ohms,
                              const std::vector<double> &node_caps_f,
                              bool includes_pin_caps = true)
  {
    ASSERT_EQ(pins.size(), node_caps_f.size());
    ASSERT_EQ(r_ohms.size() + 1, pins.size());
    const Scene *scene = sta_->cmdScene();
    const Network *network = sta_->network();
    for (const MinMax *mm : MinMax::range()) {
      Parasitics *parasitics = scene->parasitics(mm);
      Parasitic *pnet =
          sta_->makeParasiticNetwork(net, includes_pin_caps, scene, mm);
      ParasiticNode *prev = nullptr;
      for (size_t i = 0; i < pins.size(); ++i) {
        ParasiticNode *node = pins[i]
            ? parasitics->ensureParasiticNode(pnet, pins[i], network)
            : parasitics->ensureParasiticNode(pnet, net,
                                              static_cast<uint32_t>(i + 1), network);
        parasitics->incrCap(node, static_cast<float>(node_caps_f[i]));
        if (prev) {
          parasitics->makeResistor(pnet, static_cast<uint32_t>(i),
                                   static_cast<float>(r_ohms[i - 1]), prev, node);
        }
        prev = node;
      }
    }
  }

  Edge *findWireEdge(const Pin *drvr_pin,
                     const Pin *load_pin) const
  {
    Graph *graph = sta_->graph();
    Vertex *drvr_vtx = graph->pinDrvrVertex(drvr_pin);
    Vertex *load_vtx = graph->pinLoadVertex(load_pin);
    VertexOutEdgeIterator out_iter(drvr_vtx, graph);
    while (out_iter.hasNext()) {
      Edge *e = out_iter.next();
      if (e->isWire() && e->to(graph) == load_vtx) {
        return e;
      }
    }
    return nullptr;
  }

  std::pair<double,
            double>
  measurePortWire(Pin *port_pin,
                  Pin *load_pin,
                  const RiseFall *rf = RiseFall::rise(),
                  const MinMax *mm = MinMax::max())
  {
    Graph *graph = sta_->graph();
    Edge *wire_edge = findWireEdge(port_pin, load_pin);
    EXPECT_NE(wire_edge, nullptr);
    const DcalcAPIndex ap = sta_->cmdScene()->dcalcAnalysisPtIndex(mm);
    const double wd =
        wire_edge ? delayAsFloat(graph->wireArcDelay(wire_edge, rf, ap)) : 0.0;
    const double ls =
        delayAsFloat(graph->slew(graph->pinLoadVertex(load_pin), rf, ap));
    return {wd, ls};
  }

  StageMetrics measureStageArc(Pin *in_pin,
                               const RiseFall *in_rf,
                               Pin *drvr_pin,
                               const RiseFall *out_rf,
                               Pin *load_pin,
                               const MinMax *mm = MinMax::max())
  {
    Graph *graph = sta_->graph();
    Edge *gate_edge = nullptr;
    const TimingArc *gate_arc = nullptr;
    graph->gateEdgeArc(in_pin, in_rf, drvr_pin, out_rf, gate_edge, gate_arc);
    EXPECT_NE(gate_edge, nullptr);
    EXPECT_NE(gate_arc, nullptr);

    const DcalcAPIndex ap = sta_->cmdScene()->dcalcAnalysisPtIndex(mm);
    const auto [wire_delay, load_slew] =
        measurePortWire(drvr_pin, load_pin, out_rf, mm);
    StageMetrics m;
    if (gate_edge && gate_arc) {
      m.gate_delay = delayAsFloat(graph->arcDelay(gate_edge, gate_arc, ap));
    }
    m.drvr_slew =
        delayAsFloat(graph->slew(graph->pinDrvrVertex(drvr_pin), out_rf, ap));
    m.wire_delay = wire_delay;
    m.load_slew = load_slew;
    return m;
  }

  StageMetrics measureStage(Pin *in_pin,
                            Pin *drvr_pin,
                            Pin *load_pin,
                            const RiseFall *rf = RiseFall::rise(),
                            const MinMax *mm = MinMax::max())
  {
    return measureStageArc(in_pin, rf, drvr_pin, rf, load_pin, mm);
  }

  ScopedTempDir temp_dir_;
  Sta *sta_{nullptr};
  Tcl_Interp *interp_{nullptr};
};

////////////////////////////////////////////////////////////////
// Positive Baseline, Regime Continuity & Coverage Tests
////////////////////////////////////////////////////////////////

// Verifies the analytical 1-pole and 2-pole reference helpers: DC gain == 1.0,
// t == dt ramp continuity, overflow-free behavior at large p*dt, and exact
// collapse of computeTwoStageLadder / yRampTwoPole to 1-pole Rd*(C1+C2) as
// Rpi -> 0 and to 1-pole Rd*C2 (at the near tap) as Rpi -> inf.
TEST_F(DmpAccuracyTest,
       ReferenceHelpersSelfTest)
{
  const double r1 = 1000.0;
  const double c_near = 10.0e-15;
  const double c_far = 90.0e-15;
  const double dt = 20.0e-12;
  auto dc_gain = [](const TwoPoleTransfer &tf) {
    return tf.k1 / tf.p1 + tf.k2 / tf.p2;
  };

  // Single-pole continuity across t = dt and zero initial condition.
  const double tau_tot = r1 * (c_near + c_far);
  EXPECT_DOUBLE_EQ(yRampSinglePole(0.0, tau_tot, dt), 0.0);
  EXPECT_NEAR(yRampSinglePole(std::nextafter(dt, 0.0), tau_tot, dt),
              yRampSinglePole(dt, tau_tot, dt), 1.0e-12);
  EXPECT_NEAR(yRampSinglePole(dt, tau_tot, dt),
              yRampSinglePole(std::nextafter(dt, 1.0), tau_tot, dt), 1.0e-12);

  // DC gain == 1.0 across nominal, tiny c_far, and large r2.
  for (bool near_tap : {false, true}) {
    const TwoPoleTransfer nom =
        computeTwoStageLadder(r1, c_near, 500.0, c_far, near_tap);
    EXPECT_NEAR(dc_gain(nom), 1.0, 1.0e-12);
    EXPECT_NEAR(dc_gain(computeTwoStageLadder(r1, c_near, 500.0, 1.0e-30, near_tap)),
                1.0, 1.0e-12);
    EXPECT_NEAR(dc_gain(computeTwoStageLadder(r1, c_near, 1.0e9, c_far, near_tap)),
                1.0, 1.0e-12);

    const double y2p_dt = yRampTwoPole(dt, nom, dt);
    EXPECT_DOUBLE_EQ(yRampTwoPole(0.0, nom, dt), 0.0);
    EXPECT_NEAR(yRampTwoPole(std::nextafter(dt, 0.0), nom, dt), y2p_dt, 1.0e-12);
    EXPECT_NEAR(yRampTwoPole(std::nextafter(dt, 1.0), nom, dt), y2p_dt, 1.0e-12);
  }

  // Rpi -> 0 limit: both near and far taps collapse to 1-pole Rd*(C_near + C_far).
  for (bool near_tap : {false, true}) {
    const TwoPoleTransfer short_tf =
        computeTwoStageLadder(r1, c_near, 1.0e-9, c_far, near_tap);
    for (double vth : {0.20, 0.50, 0.80}) {
      EXPECT_NEAR(solveTwoPoleRampCrossing(short_tf, dt, vth),
                  solveSinglePoleRampCrossing(tau_tot, dt, vth), 1.0e-15);
    }
    for (double t : {0.5 * dt, dt, 2.0 * dt}) {
      EXPECT_NEAR(yRampTwoPole(t, short_tf, dt), yRampSinglePole(t, tau_tot, dt),
                  1.0e-12);
    }
  }

  // Rpi -> inf limit: near tap collapses to 1-pole Rd * C_near (C_far shielded).
  const double tau_near = r1 * c_near;
  const TwoPoleTransfer open_tf =
      computeTwoStageLadder(r1, c_near, 1.0e14, c_far, true);
  for (double vth : {0.20, 0.50, 0.80}) {
    EXPECT_NEAR(solveTwoPoleRampCrossing(open_tf, dt, vth),
                solveSinglePoleRampCrossing(tau_near, dt, vth), 1.0e-14);
  }
  for (double t : {0.5 * dt, dt, 2.0 * dt}) {
    EXPECT_NEAR(yRampTwoPole(t, open_tf, dt), yRampSinglePole(t, tau_near, dt),
                1.0e-10);
  }

  // Overflow-free evaluation at extreme p * dt = 1e6.
  const TwoPoleTransfer fast_tf =
      computeTwoStageLadder(1.0, 1.0e-18, 1.0, 1.0e-18, false);
  const double y_sat = yRampTwoPole(1.0e-6, fast_tf, 1.0e-6);
  EXPECT_TRUE(std::isfinite(y_sat));
  EXPECT_NEAR(y_sat, 1.0, 1.0e-5);
}

// Exercises DmpCeffDelayCalc::setCeffAlgorithm regime dispatch across DmpCap,
// DmpZeroC2, and DmpPi, verifying exact behavior in capacitive regimes and
// smooth continuity across the 1e-2 Rd and 1e-3 Pi-ratio cutoffs.
TEST_F(DmpAccuracyTest,
       RegimeSelectionAndBoundaryContinuity)
{
  // BUF_RD0: constant delay -> Rd == 0 (< 1e-2 -> DmpCap).
  ASSERT_NE(readLibertyText(
                makeLinearNldmLibText("reg_rd0", "BUF_RD0", 50.0, 50.0, 20.0, 20.0,
                                      80.0, 80.0, 1.0, 1.0, {1.0, 100.0},
                                      {1.0, 200.0}, {15.0, 15.0}, {30.0, 30.0}),
                "rd0.lib"),
            nullptr);
  // BUF_TINY_RD: tiny slope -> Rd ~ 0.05 Ohm (> 1e-2).
  const double tiny_d200 = 15.0 + 0.05 * 200.0 * 1.0e-3 * std::log(2.0);
  ASSERT_NE(readLibertyText(
                makeLinearNldmLibText("reg_tiny_rd", "BUF_TINY_RD", 50.0, 50.0, 20.0,
                                      20.0, 80.0, 80.0, 1.0, 1.0, {1.0, 100.0},
                                      {1.0, 200.0}, {15.0, tiny_d200}, {30.0, 30.0}),
                "tiny.lib"),
            nullptr);
  // BUF_LIN: Rd ~ 500 Ohm (fixed) / ~240 Ohm (master), well above 1e-2.
  const double slope_ps_per_ff = 0.5 * std::log(2.0);
  ASSERT_NE(
      readLibertyText(makeLinearNldmLibText(
                          "reg_lin", "BUF_LIN", 50.0, 50.0, 20.0, 20.0, 80.0, 80.0,
                          1.0, 1.0, {1.0, 100.0}, {0.0, 200.0},
                          {10.0, 10.0 + 200.0 * slope_ps_per_ff}, {30.0, 80.0}),
                      "lin.lib"),
      nullptr);

  loadVerilogAndLink(
      "module top(in1, out1, out2, out3);\n"
      "  input in1; output out1, out2, out3; wire n1, n2, n3;\n"
      "  BUF_RD0 u_rd0(.A(in1), .Y(n1));\n"
      "  BUF_RD0 u_rd0_l(.A(n1), .Y(out1));\n"
      "  BUF_TINY_RD u_tiny(.A(in1), .Y(n2));\n"
      "  BUF_TINY_RD u_tiny_l(.A(n2), .Y(out2));\n"
      "  BUF_LIN u_lin(.A(in1), .Y(n3));\n"
      "  BUF_LIN u_lin_l(.A(n3), .Y(out3));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);
  sta_->setArcDelayCalc("dmp_ceff_elmore");

  // 1. Rd < 1e-2 -> DmpCap vs tiny Rd > 1e-2 continuity.
  Pin *rd0_y = findPin("u_rd0/Y");
  Pin *rd0l_a = findPin("u_rd0_l/A");
  Pin *tiny_y = findPin("u_tiny/Y");
  Pin *tinyl_a = findPin("u_tiny_l/A");
  setPiElmoreModel(rd0_y, rd0l_a, 10.0e-15, 2000.0, 90.0e-15, 180.0e-12);
  setPiElmoreModel(tiny_y, tinyl_a, 10.0e-15, 0.1, 90.0e-15, 0.009e-12);
  sta_->findDelays();
  const auto m_rd0 = measureStage(findPin("u_rd0/A"), rd0_y, rd0l_a);
  const auto m_tiny = measureStage(findPin("u_tiny/A"), tiny_y, tinyl_a);
  EXPECT_NEAR(m_rd0.gate_delay, 15.0e-12, 0.01e-12);
  EXPECT_NEAR(m_rd0.drvr_slew, 30.0e-12, 0.01e-12);
  EXPECT_NEAR(m_rd0.wire_delay, 180.0e-12, 0.01e-12);
  EXPECT_NEAR(m_rd0.load_slew, 30.0e-12, 0.01e-12);
  EXPECT_NEAR(m_tiny.gate_delay, m_rd0.gate_delay, 0.05e-12);
  EXPECT_NEAR(m_tiny.drvr_slew, m_rd0.drvr_slew, 0.05e-12);

  Pin *lin_a = findPin("u_lin/A");
  Pin *lin_y = findPin("u_lin/Y");
  Pin *linl_a = findPin("u_lin_l/A");
  auto eval_lin = [&](double c2, double rpi, double c1, double elmore) {
    setPiElmoreModel(lin_y, linl_a, c2, rpi, c1, elmore);
    sta_->findDelays();
    return measureStage(lin_a, lin_y, linl_a);
  };

  // 2. rpi == 0 and rpi crossing rd * 1e-3 (DmpCap <-> DmpPi).
  const auto m_rpi0 = eval_lin(20.0e-15, 0.0, 80.0e-15, 1.0e-12);
  const auto m_rpi_below = eval_lin(20.0e-15, 0.1, 80.0e-15, 1.0e-12);
  const auto m_rpi_above = eval_lin(20.0e-15, 1.0, 80.0e-15, 1.0e-12);
  const auto m_rpi_above2 = eval_lin(20.0e-15, 2.0, 80.0e-15, 1.0e-12);
  const double expected_100ff_gd = (10.0 + 100.0 * slope_ps_per_ff) * 1.0e-12;
  EXPECT_NEAR(m_rpi0.gate_delay, expected_100ff_gd, 0.01e-12);
  EXPECT_NEAR(m_rpi0.wire_delay, 1.0e-12, 0.01e-12);
  EXPECT_NEAR(m_rpi_below.gate_delay, m_rpi0.gate_delay, 0.01e-12);
  EXPECT_NEAR(m_rpi_below.drvr_slew, m_rpi0.drvr_slew, 0.01e-12);
  EXPECT_NEAR(m_rpi_above.gate_delay, m_rpi_below.gate_delay, 0.25e-12);
  EXPECT_NEAR(m_rpi_above2.gate_delay, m_rpi_above.gate_delay, 0.25e-12);
  EXPECT_NEAR(m_rpi_above2.drvr_slew, m_rpi_above.drvr_slew, 0.25e-12);

  // 3. c1 == 0 and c1 / c2 crossing 1e-3 (DmpCap <-> DmpPi).
  const auto m_c1_0 = eval_lin(100.0e-15, 500.0, 0.0, 0.0);
  const auto m_c1_below = eval_lin(100.0e-15, 500.0, 0.05e-15, 0.025e-12);
  const auto m_c1_above = eval_lin(100.0e-15, 500.0, 0.20e-15, 0.10e-12);
  const auto m_c1_above2 = eval_lin(100.0e-15, 500.0, 0.40e-15, 0.20e-12);
  EXPECT_NEAR(m_c1_0.gate_delay, expected_100ff_gd, 0.01e-12);
  EXPECT_NEAR(m_c1_below.gate_delay, m_c1_0.gate_delay, 0.05e-12);
  EXPECT_NEAR(m_c1_above.gate_delay, m_c1_below.gate_delay, 0.50e-12);
  EXPECT_NEAR(m_c1_above2.gate_delay, m_c1_above.gate_delay, 0.50e-12);
  EXPECT_NEAR(m_c1_above2.drvr_slew, m_c1_above.drvr_slew, 0.50e-12);

  // 4. c2 == 0 (DmpZeroC2) and small c2 > 1e-3 * c1 (DmpPi).
  const auto m_c2_0 = eval_lin(0.0, 500.0, 100.0e-15, 50.0e-12);
  const auto m_c2_above = eval_lin(0.20e-15, 500.0, 100.0e-15, 50.0e-12);
  const auto m_c2_above2 = eval_lin(0.40e-15, 500.0, 100.0e-15, 50.0e-12);
  EXPECT_GT(m_c2_0.gate_delay, 0.0);
  EXPECT_GT(m_c2_0.drvr_slew, 0.0);
  EXPECT_GT(m_c2_0.wire_delay, 0.0);
  EXPECT_GT(m_c2_0.load_slew, m_c2_0.drvr_slew);
  EXPECT_NEAR(m_c2_above.wire_delay, m_c2_0.wire_delay, 1.0e-12);
  EXPECT_NEAR(m_c2_above2.gate_delay, m_c2_above.gate_delay, 0.50e-12);
  EXPECT_NEAR(m_c2_above2.drvr_slew, m_c2_above.drvr_slew, 0.50e-12);
  EXPECT_NEAR(m_c2_above2.wire_delay, m_c2_above.wire_delay, 0.50e-12);
  EXPECT_NEAR(m_c2_above2.load_slew, m_c2_above.load_slew, 0.50e-12);
}

// Verifies physical Ceff bounds delay(C2) <= gate_delay <= delay(C1+C2) and
// strict resistive-shielding monotonicity across Rpi, C1, C2, and elmore.
TEST_F(DmpAccuracyTest,
       PhysicalCeffBoundsAndShieldingMonotonicity)
{
  ASSERT_NE(readLibertyText(makeCausalOnePoleLibText(
                "mono_lib", "BUF", 800.0, 5.0e-12, 15.0e-12, 0.50, 0.20, 0.80, 1.0,
                {1.0, 5.0, 10.0, 25.0, 50.0, 100.0, 150.0, 250.0})),
            nullptr);
  loadVerilogAndLink(
      "module top(in1, out1);\n"
      "  input in1; output out1; wire n1;\n"
      "  BUF u1(.A(in1), .Y(n1));\n"
      "  BUF u2(.A(n1), .Y(out1));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);
  sta_->setArcDelayCalc("dmp_ceff_elmore");

  Pin *u1_a = findPin("u1/A");
  Pin *u1_y = findPin("u1/Y");
  Pin *u2_a = findPin("u2/A");
  auto eval_pi = [&](double cap2, double rpi, double cap1, double elmore) {
    setPiElmoreModel(u1_y, u2_a, cap2, rpi, cap1, elmore);
    sta_->findDelays();
    return measureStage(u1_a, u1_y, u2_a);
  };

  const double c2 = 10.0e-15;
  const double c1 = 90.0e-15;
  const StageMetrics m_c2 = eval_pi(c2, 0.0, 0.0, 0.0);
  const StageMetrics m_tot = eval_pi(c1 + c2, 0.0, 0.0, 0.0);

  // Rpi sweep: bounds and strict monotonic shielding from C1+C2 down to C2.
  StageMetrics prev_rpi = eval_pi(c2, 0.0, c1, 45.0e-12);
  EXPECT_NEAR(prev_rpi.gate_delay, m_tot.gate_delay, 0.01e-12);
  EXPECT_NEAR(prev_rpi.drvr_slew, m_tot.drvr_slew, 0.01e-12);
  for (double rpi : {50.0, 200.0, 1000.0, 5000.0, 50000.0, 1.0e6}) {
    const StageMetrics m = eval_pi(c2, rpi, c1, 45.0e-12);
    EXPECT_GT(m.gate_delay, m_c2.gate_delay);
    EXPECT_LT(m.gate_delay, m_tot.gate_delay);
    EXPECT_LT(m.gate_delay, prev_rpi.gate_delay);
    prev_rpi = m;
  }
  EXPECT_NEAR(prev_rpi.gate_delay, m_c2.gate_delay, 0.50e-12);

  // C1 and C2 monotonicity at fixed Rpi = 500 Ohm.
  StageMetrics prev_c1 = eval_pi(10.0e-15, 500.0, 20.0e-15, 30.0e-12);
  for (double cap1 : {60.0e-15, 140.0e-15}) {
    const StageMetrics m = eval_pi(10.0e-15, 500.0, cap1, 30.0e-12);
    EXPECT_GT(m.gate_delay, prev_c1.gate_delay);
    EXPECT_GT(m.drvr_slew, prev_c1.drvr_slew);
    prev_c1 = m;
  }

  StageMetrics prev_c2 = eval_pi(5.0e-15, 500.0, 80.0e-15, 40.0e-12);
  for (double cap2 : {20.0e-15, 50.0e-15}) {
    const StageMetrics m = eval_pi(cap2, 500.0, 80.0e-15, 40.0e-12);
    EXPECT_GT(m.gate_delay, prev_c2.gate_delay);
    EXPECT_GT(m.drvr_slew, prev_c2.drvr_slew);
    prev_c2 = m;
  }

  // Elmore sweep: wire_delay and load_slew strictly increase; driver invariant.
  StageMetrics prev_elm = eval_pi(10.0e-15, 500.0, 90.0e-15, 10.0e-12);
  EXPECT_GT(prev_elm.load_slew, prev_elm.drvr_slew);
  for (double elmore : {50.0e-12, 150.0e-12, 400.0e-12}) {
    const StageMetrics m = eval_pi(10.0e-15, 500.0, 90.0e-15, elmore);
    EXPECT_GT(m.load_slew, m.drvr_slew);
    EXPECT_NEAR(m.gate_delay, prev_elm.gate_delay, 1.0e-15);
    EXPECT_NEAR(m.drvr_slew, prev_elm.drvr_slew, 1.0e-15);
    EXPECT_GT(m.wire_delay, prev_elm.wire_delay);
    EXPECT_GT(m.load_slew, prev_elm.load_slew);
    prev_elm = m;
  }
}

// Verifies 2D nonlinear NLDM interpolation/extrapolation across (in_slew, Ceff)
// and single-library PVT scaling via operating_conditions and per-instance setPvt,
// including consistent scaling of effective driver resistance Rd under DmpPi.
TEST_F(DmpAccuracyTest,
       TwoDimNonlinearNldmAndPvtScaling)
{
  const std::vector<double> slews_ps = {5.0, 20.0, 80.0};
  const std::vector<double> caps_ff = {10.0, 40.0, 160.0};
  std::vector<std::vector<double>> d_grid(slews_ps.size(),
                                          std::vector<double>(caps_ff.size()));
  std::vector<std::vector<double>> s_grid = d_grid;
  for (size_t i = 0; i < slews_ps.size(); ++i) {
    for (size_t j = 0; j < caps_ff.size(); ++j) {
      const double s = slews_ps[i];
      const double c = caps_ff[j];
      d_grid[i][j] =
          8.0 + 0.15 * s + 0.001 * s * s + 0.40 * c + 0.0015 * c * c + 0.002 * s * c;
      s_grid[i][j] = 4.0 + 0.30 * s + 0.35 * c + 0.001 * s * c;
    }
  }

  auto format_grid = [](const std::vector<std::vector<double>> &grid) {
    std::ostringstream oss;
    for (size_t i = 0; i < grid.size(); ++i) {
      oss << (i ? ", \"" : "\"") << formatFloatRow(grid[i]) << "\"";
    }
    return oss.str();
  };
  const std::string d_vals = format_grid(d_grid);
  const std::string s_vals = format_grid(s_grid);

  std::ostringstream lib_oss;
  lib_oss
      << "library (nldm_2d_pvt) {\n"
         "  delay_model : table_lookup;\n"
         "  time_unit : \"1ps\";\n"
         "  voltage_unit : \"1V\";\n"
         "  current_unit : \"1mA\";\n"
         "  pulling_resistance_unit : \"1kohm\";\n"
         "  leakage_power_unit : \"1nW\";\n"
         "  capacitive_load_unit (1, ff);\n"
         "  nom_process : 1.0; nom_temperature : 25.0; nom_voltage : 0.8;\n"
         "  k_process_cell_rise : 0.25; k_volt_cell_rise : -0.60;\n"
         "  k_temp_cell_rise : 0.002;\n"
         "  k_process_cell_fall : 0.25; k_volt_cell_fall : -0.60;\n"
         "  k_temp_cell_fall : 0.002;\n"
         "  k_process_rise_transition : 0.20; k_volt_rise_transition : -0.50;\n"
         "  k_temp_rise_transition : 0.001;\n"
         "  k_process_fall_transition : 0.20; k_volt_fall_transition : -0.50;\n"
         "  k_temp_fall_transition : 0.001;\n"
         "  operating_conditions (slow_oc) {\n"
         "    process : 1.2; voltage : 0.72; temperature : 105.0;\n"
         "    tree_type : balanced_tree;\n"
         "  }\n"
         "  input_threshold_pct_rise : 50.0; input_threshold_pct_fall : 50.0;\n"
         "  output_threshold_pct_rise : 50.0; output_threshold_pct_fall : 50.0;\n"
         "  slew_lower_threshold_pct_rise : 20.0;\n"
         "  slew_lower_threshold_pct_fall : 20.0;\n"
         "  slew_upper_threshold_pct_rise : 80.0;\n"
         "  slew_upper_threshold_pct_fall : 80.0;\n"
         "  slew_derate_from_library : 1.0;\n"
         "  lu_table_template (t2) {\n"
         "    variable_1 : input_net_transition;\n"
         "    variable_2 : total_output_net_capacitance;\n"
         "    index_1 (\""
      << formatFloatRow(slews_ps)
      << "\");\n"
         "    index_2 (\""
      << formatFloatRow(caps_ff)
      << "\");\n"
         "  }\n"
         "  cell (BUF2D) {\n"
         "    pin (A) { direction : input; capacitance : 2.0; }\n"
         "    pin (Y) {\n"
         "      direction : output; function : \"A\";\n"
         "      timing () {\n"
         "        related_pin : \"A\"; timing_sense : positive_unate;\n"
         "        cell_rise (t2) { values ("
      << d_vals
      << "); }\n"
         "        cell_fall (t2) { values ("
      << d_vals
      << "); }\n"
         "        rise_transition (t2) { values ("
      << s_vals
      << "); }\n"
         "        fall_transition (t2) { values ("
      << s_vals
      << "); }\n"
         "      }\n"
         "    }\n"
         "  }\n"
         "}\n";

  LibertyLibrary *lib = readLibertyText(lib_oss.str(), "nldm2d.lib");
  ASSERT_NE(lib, nullptr);

  loadVerilogAndLink(
      "module top(in1, out1, out2, out3);\n"
      "  input in1; output out1, out2, out3; wire n1, n2, n3;\n"
      "  BUF2D u_slow(.A(in1), .Y(n1));\n"
      "  BUF2D u_slow_l(.A(n1), .Y(out1));\n"
      "  BUF2D u_fast(.A(in1), .Y(n2));\n"
      "  BUF2D u_fast_l(.A(n2), .Y(out2));\n"
      "  BUF2D u_nom(.A(in1), .Y(n3));\n"
      "  BUF2D u_nom_l(.A(n3), .Y(out3));\n"
      "endmodule\n");
  sta_->setArcDelayCalc("dmp_ceff_elmore");

  struct Stage
  {
    Instance *inst;
    Pin *a;
    Pin *y;
    Pin *load_a;
  };
  auto get_stage = [&](const std::string &name) -> Stage {
    return {
        sta_->network()->findInstance(name),
        findPin(name + "/A"),
        findPin(name + "/Y"),
        findPin(name + "_l/A"),
    };
  };
  const Stage slow = get_stage("u_slow");
  const Stage fast = get_stage("u_fast");
  const Stage nom = get_stage("u_nom");

  auto ref_delay_s = [&](double slew_ps, double cap_ff) {
    return bilinearEval2d(slews_ps, caps_ff, d_grid, slew_ps, cap_ff) * 1.0e-12;
  };
  auto ref_slew_s = [&](double slew_ps, double cap_ff) {
    return bilinearEval2d(slews_ps, caps_ff, s_grid, slew_ps, cap_ff) * 1.0e-12;
  };

  // 1. Interior & extrapolated 2D bilinear interpolation at nominal PVT.
  setInputTransition("in1", 35.0e-12);
  setPiElmoreModel(nom.y, nom.load_a, 70.0e-15, 0.0, 0.0, 0.0);
  sta_->findDelays();
  const StageMetrics m_int = measureStage(nom.a, nom.y, nom.load_a);
  EXPECT_NEAR(m_int.gate_delay, ref_delay_s(35.0, 70.0), 0.01e-12);
  EXPECT_NEAR(m_int.drvr_slew, ref_slew_s(35.0, 70.0), 0.01e-12);

  setInputTransition("in1", 110.0e-12);
  setPiElmoreModel(nom.y, nom.load_a, 200.0e-15, 0.0, 0.0, 0.0);
  sta_->findDelays();
  const StageMetrics m_ext = measureStage(nom.a, nom.y, nom.load_a);
  EXPECT_NEAR(m_ext.gate_delay, ref_delay_s(110.0, 200.0), 0.01e-12);
  EXPECT_NEAR(m_ext.drvr_slew, ref_slew_s(110.0, 200.0), 0.01e-12);

  // 2. Single-library PVT scaling via setOperatingConditions + instance setPvt.
  OperatingConditions *slow_oc = lib->findOperatingConditions("slow_oc");
  ASSERT_NE(slow_oc, nullptr);
  sta_->setOperatingConditions(slow_oc, MinMaxAll::all(), sta_->cmdSdc());
  sta_->setPvt(nom.inst, MinMaxAll::all(), 1.0f, 0.8f, 25.0f, sta_->cmdSdc());
  sta_->setPvt(fast.inst, MinMaxAll::all(), 0.9f, 0.88f, 0.0f, sta_->cmdSdc());

  setInputTransition("in1", 35.0e-12);
  for (const Stage &s : {slow, fast, nom}) {
    setPiElmoreModel(s.y, s.load_a, 70.0e-15, 0.0, 0.0, 0.0);
  }
  sta_->findDelays();

  auto pvt_scale = [](double p, double v, double t, double kp, double kv,
                      double kt) {
    return (1.0 + (p - 1.0) * kp) * (1.0 + (t - 25.0) * kt) * (1.0 + (v - 0.8) * kv);
  };
  const double base_d = ref_delay_s(35.0, 70.0);
  const double base_s = ref_slew_s(35.0, 70.0);
  const double slow_d_scale = pvt_scale(1.2, 0.72, 105.0, 0.25, -0.60, 0.002);
  const double slow_s_scale = pvt_scale(1.2, 0.72, 105.0, 0.20, -0.50, 0.001);
  const double fast_d_scale = pvt_scale(0.9, 0.88, 0.0, 0.25, -0.60, 0.002);
  const double fast_s_scale = pvt_scale(0.9, 0.88, 0.0, 0.20, -0.50, 0.001);

  const StageMetrics m_slow_cap = measureStage(slow.a, slow.y, slow.load_a);
  const StageMetrics m_fast_cap = measureStage(fast.a, fast.y, fast.load_a);
  EXPECT_NEAR(m_slow_cap.gate_delay, base_d * slow_d_scale, 0.01e-12);
  EXPECT_NEAR(m_slow_cap.drvr_slew, base_s * slow_s_scale, 0.01e-12);
  EXPECT_NEAR(m_fast_cap.gate_delay, base_d * fast_d_scale, 0.01e-12);
  EXPECT_NEAR(m_fast_cap.drvr_slew, base_s * fast_s_scale, 0.01e-12);

  // 3. Under DmpPi, slower PVT increases delay and slew while Ceff stays shielded.
  for (const Stage &s : {slow, nom, fast}) {
    setPiElmoreModel(s.y, s.load_a, 10.0e-15, 1500.0, 90.0e-15, 135.0e-12);
  }
  sta_->findDelays();

  const StageMetrics m_slow_pi = measureStage(slow.a, slow.y, slow.load_a);
  const StageMetrics m_nom_pi = measureStage(nom.a, nom.y, nom.load_a);
  const StageMetrics m_fast_pi = measureStage(fast.a, fast.y, fast.load_a);
  EXPECT_LT(m_fast_pi.gate_delay, m_nom_pi.gate_delay);
  EXPECT_LT(m_nom_pi.gate_delay, m_slow_pi.gate_delay);
  EXPECT_LT(m_fast_pi.drvr_slew, m_nom_pi.drvr_slew);
  EXPECT_LT(m_nom_pi.drvr_slew, m_slow_pi.drvr_slew);

  auto get_ceff_ff = [&](const Stage &s) {
    Edge *e = nullptr;
    const TimingArc *arc = nullptr;
    sta_->graph()->gateEdgeArc(s.a, RiseFall::rise(), s.y, RiseFall::rise(), e, arc);
    const std::string rep = sta_->reportDelayCalc(
        e, const_cast<TimingArc *>(arc), sta_->cmdScene(), MinMax::max(), 6);
    return extractMarkedValue(rep, "Ceff=");
  };
  const double ceff_slow = get_ceff_ff(slow);
  const double ceff_nom = get_ceff_ff(nom);
  const double ceff_fast = get_ceff_ff(fast);
  EXPECT_GT(ceff_fast, 10.0);
  EXPECT_LT(ceff_fast, 100.0);
  EXPECT_GT(ceff_nom, 10.0);
  EXPECT_LT(ceff_nom, 100.0);
  EXPECT_GT(ceff_slow, 10.0);
  EXPECT_LT(ceff_slow, 100.0);
  EXPECT_NEAR(m_slow_pi.gate_delay, ref_delay_s(35.0, ceff_slow) * slow_d_scale,
              0.01e-12);
  EXPECT_NEAR(m_nom_pi.gate_delay, ref_delay_s(35.0, ceff_nom), 0.01e-12);
  EXPECT_NEAR(m_fast_pi.gate_delay, ref_delay_s(35.0, ceff_fast) * fast_d_scale,
              0.01e-12);
}

// Tests negative-unate INV arcs (rise->fall and fall->rise) with asymmetric
// rise/fall NLDM tables, asymmetric input slews, and asymmetric rise/fall
// PiElmore + RC ladder parasitics under dmp_ceff_elmore and dmp_ceff_two_pole.
TEST_F(DmpAccuracyTest,
       NegativeUnateInverterAsymmetricRiseFall)
{
  ASSERT_NE(readLibertyText(R"(
library (inv_asym) {
  delay_model : table_lookup;
  time_unit : "1ps";
  voltage_unit : "1V";
  current_unit : "1mA";
  pulling_resistance_unit : "1kohm";
  leakage_power_unit : "1nW";
  capacitive_load_unit (1, ff);
  nom_process : 1.0; nom_temperature : 25.0; nom_voltage : 0.8;
  input_threshold_pct_rise : 50.0;      input_threshold_pct_fall : 50.0;
  output_threshold_pct_rise : 50.0;     output_threshold_pct_fall : 50.0;
  slew_lower_threshold_pct_rise : 20.0; slew_lower_threshold_pct_fall : 20.0;
  slew_upper_threshold_pct_rise : 80.0; slew_upper_threshold_pct_fall : 80.0;
  slew_derate_from_library : 1.0;

  lu_table_template (t2) {
    variable_1 : input_net_transition;
    variable_2 : total_output_net_capacitance;
    index_1 ("10.0, 60.0");
    index_2 ("10.0, 110.0");
  }

  cell (INV) {
    pin (A) { direction : input; rise_capacitance : 3.0; fall_capacitance : 1.0; }
    pin (Y) {
      direction : output;
      function : "!A";
      timing () {
        related_pin : "A";
        timing_sense : negative_unate;
        cell_rise (t2) { values ("22.0, 122.0", "27.0, 127.0"); }
        cell_fall (t2) { values ("11.0, 51.0", "13.0, 53.0"); }
        rise_transition (t2) { values ("14.0, 114.0", "24.0, 124.0"); }
        fall_transition (t2) { values ("7.0, 47.0", "12.0, 52.0"); }
      }
    }
  }
}
)"),
            nullptr);
  loadVerilogAndLink(
      "module top(in1, out1, out2);\n"
      "  input in1; output out1, out2; wire n1, n2;\n"
      "  INV u1(.A(in1), .Y(n1));\n"
      "  INV u2(.A(n1), .Y(out1));\n"
      "  INV u_cap(.A(in1), .Y(n2));\n"
      "  INV u_cap_l(.A(n2), .Y(out2));\n"
      "endmodule\n");

  Network *net = sta_->network();
  Port *in1_port = net->findPort(net->cell(net->topInstance()), "in1");
  sta_->setInputSlew(in1_port, RiseFallBoth::rise(), MinMaxAll::all(), 10.0e-12f,
                     sta_->cmdSdc());
  sta_->setInputSlew(in1_port, RiseFallBoth::fall(), MinMaxAll::all(), 50.0e-12f,
                     sta_->cmdSdc());

  Pin *u1_a = findPin("u1/A");
  Pin *u1_y = findPin("u1/Y");
  Pin *u2_a = findPin("u2/A");
  Pin *u2_y = findPin("u2/Y");
  Pin *ucap_a = findPin("u_cap/A");
  Pin *ucap_y = findPin("u_cap/Y");
  Pin *ucapl_a = findPin("u_cap_l/A");

  sta_->makePiElmore(u1_y, RiseFall::rise(), MinMaxAll::all(), 10.0e-15f, 400.0f,
                     40.0e-15f);
  sta_->setElmore(u1_y, u2_a, RiseFall::rise(), MinMaxAll::all(), 16.0e-12f);
  sta_->makePiElmore(u1_y, RiseFall::fall(), MinMaxAll::all(), 10.0e-15f, 1200.0f,
                     40.0e-15f);
  sta_->setElmore(u1_y, u2_a, RiseFall::fall(), MinMaxAll::all(), 48.0e-12f);
  setPiElmoreModel(ucap_y, ucapl_a, 20.0e-15, 0.0, 0.0, 0.0);

  sta_->setArcDelayCalc("dmp_ceff_elmore");
  sta_->findDelays();

  // Exact capacitive lookup checks R/F inversion: rise->fall uses 10 ps,
  // fall->rise uses 50 ps.
  const StageMetrics cap_rf =
      measureStageArc(ucap_a, RiseFall::rise(), ucap_y, RiseFall::fall(), ucapl_a);
  const StageMetrics cap_fr =
      measureStageArc(ucap_a, RiseFall::fall(), ucap_y, RiseFall::rise(), ucapl_a);
  EXPECT_NEAR(cap_rf.gate_delay, 15.0e-12, 0.01e-12);
  EXPECT_NEAR(cap_rf.drvr_slew, 11.0e-12, 0.01e-12);
  EXPECT_NEAR(cap_fr.gate_delay, 36.0e-12, 0.01e-12);
  EXPECT_NEAR(cap_fr.drvr_slew, 32.0e-12, 0.01e-12);

  const StageMetrics u1_rf =
      measureStageArc(u1_a, RiseFall::rise(), u1_y, RiseFall::fall(), u2_a);
  const StageMetrics u1_fr =
      measureStageArc(u1_a, RiseFall::fall(), u1_y, RiseFall::rise(), u2_a);
  EXPECT_GT(u1_rf.wire_delay, u1_fr.wire_delay);
  EXPECT_GT(u1_fr.gate_delay, u1_rf.gate_delay);

  // Perturbing only rise input slew must leave u1 fall->rise bit-exact.
  sta_->setInputSlew(in1_port, RiseFallBoth::rise(), MinMaxAll::all(), 40.0e-12f,
                     sta_->cmdSdc());
  sta_->findDelays();
  const StageMetrics u1_rf_2 =
      measureStageArc(u1_a, RiseFall::rise(), u1_y, RiseFall::fall(), u2_a);
  const StageMetrics u1_fr_2 =
      measureStageArc(u1_a, RiseFall::fall(), u1_y, RiseFall::rise(), u2_a);
  EXPECT_GT(u1_rf_2.gate_delay, u1_rf.gate_delay);
  EXPECT_DOUBLE_EQ(u1_fr_2.gate_delay, u1_fr.gate_delay);
  EXPECT_DOUBLE_EQ(u1_fr_2.drvr_slew, u1_fr.drvr_slew);

  // Verify u2 fall->rise consumes u1's propagated fall load_slew under two-pole RC.
  sta_->deleteParasitics();
  buildRcLadderParasitic(net->net(u1_y), {u1_y, u2_a}, {500.0}, {10.0e-15, 0.0},
                         false);
  sta_->setArcDelayCalc("dmp_ceff_two_pole");
  sta_->findDelays();
  const StageMetrics tp_u1_rf =
      measureStageArc(u1_a, RiseFall::rise(), u1_y, RiseFall::fall(), u2_a);
  const StageMetrics tp_u2_fr = measureStageArc(u2_a, RiseFall::fall(), u2_y,
                                                RiseFall::rise(), findPin("out1"));
  // cell_rise(in_slew=tp_u1_rf.load_slew, cap=0) -> 12 + 0.1 * (slew - 10).
  const double expected_u2_gd_fr =
      (12.0 + 0.1 * (tp_u1_rf.load_slew * 1.0e12 - 10.0)) * 1.0e-12;
  EXPECT_NEAR(tp_u2_fr.gate_delay, expected_u2_gd_fr, 0.02e-12);
}

// Verifies dmp_ceff_two_pole with a resistive Rd > 0 driver (DmpPi shielding active)
// driving a 2-stage RC ladder, checking gate_delay/drvr_slew parity with
// dmp_ceff_elmore, active shielding bounds, and 2-pole wire_delay/load_slew.
TEST_F(DmpAccuracyTest,
       ResistiveDriverTwoPoleMultiStageIntegration)
{
  const std::string lib_text = makeCausalOnePoleLibText(
      "tp_rd_lib", "BUF", 800.0, 5.0e-12, 20.0e-12, 0.50, 0.20, 0.80, 1.0,
      {1.0, 10.0, 25.0, 50.0, 100.0, 200.0});
  ASSERT_NE(readLibertyText(lib_text, "tp_rd.lib"), nullptr);
  loadVerilogAndLink(
      "module top(in1, out1, out2, out3, out4);\n"
      "  input in1; output out1, out2, out3, out4;\n"
      "  wire n_rc, n_c2, n_tot;\n"
      "  BUF u_drv(.A(in1), .Y(n_rc));\n"
      "  BUF u_near(.A(n_rc), .Y(out1));\n"
      "  BUF u_far(.A(n_rc), .Y(out2));\n"
      "  BUF u_c2(.A(in1), .Y(n_c2));\n"
      "  BUF u_c2_l(.A(n_c2), .Y(out3));\n"
      "  BUF u_tot(.A(in1), .Y(n_tot));\n"
      "  BUF u_tot_l(.A(n_tot), .Y(out4));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);

  Pin *drv_a = findPin("u_drv/A");
  Pin *drv_y = findPin("u_drv/Y");
  Pin *near_a = findPin("u_near/A");
  Pin *far_a = findPin("u_far/A");
  Pin *c2_y = findPin("u_c2/Y");
  Pin *c2l_a = findPin("u_c2_l/A");
  Pin *tot_y = findPin("u_tot/Y");
  Pin *totl_a = findPin("u_tot_l/A");

  const double r1 = 400.0;
  const double c_near = 15.0e-15;
  const double r2 = 2000.0;
  const double c_far = 85.0e-15;
  Net *drv_net = sta_->network()->net(drv_y);
  auto build_ladder = [&]() {
    buildRcLadderParasitic(drv_net, {drv_y, near_a, far_a}, {r1, r2},
                           {0.0, c_near, c_far}, true);
  };

  build_ladder();
  setPiElmoreModel(c2_y, c2l_a, 1.0e-15, 0.0, 0.0, 0.0);
  setPiElmoreModel(tot_y, totl_a, c_near + c_far, 0.0, 0.0, 0.0);
  sta_->setArcDelayCalc("dmp_ceff_two_pole");
  sta_->findDelays();
  const StageMetrics tp_near = measureStage(drv_a, drv_y, near_a);
  const StageMetrics tp_far = measureStage(drv_a, drv_y, far_a);
  const StageMetrics m_c2 = measureStage(findPin("u_c2/A"), c2_y, c2l_a);
  const StageMetrics m_tot = measureStage(findPin("u_tot/A"), tot_y, totl_a);

  // Reset parasitics so dmp_ceff_elmore sees the unreduced RC ladder.
  sta_->deleteParasitics();
  build_ladder();
  sta_->setArcDelayCalc("dmp_ceff_elmore");
  sta_->findDelays();
  const StageMetrics el_far = measureStage(drv_a, drv_y, far_a);

  // Driver DmpPi solution matches dmp_ceff_elmore and obeys shielding bounds.
  EXPECT_NEAR(tp_far.gate_delay, el_far.gate_delay, 0.01e-12);
  EXPECT_NEAR(tp_far.drvr_slew, el_far.drvr_slew, 0.01e-12);
  EXPECT_GT(tp_far.gate_delay, m_c2.gate_delay + 5.0e-12);
  EXPECT_LT(tp_far.gate_delay, m_tot.gate_delay - 15.0e-12);

  // Two-pole RC response matches far-tap ODE for the computed driver ramp.
  EXPECT_GT(tp_far.wire_delay, 0.0);
  EXPECT_GT(tp_far.wire_delay, tp_near.wire_delay);
  EXPECT_GT(tp_far.load_slew, tp_near.load_slew);
  EXPECT_GT(tp_far.load_slew, tp_far.drvr_slew);

  const double tt_drv = tp_far.drvr_slew / 0.6;
  const TwoPoleTransfer tf_far = computeTwoStageLadder(r1, c_near, r2, c_far, false);
  EXPECT_NEAR(tp_far.wire_delay,
              solveTwoPoleRampCrossing(tf_far, tt_drv, 0.50) - 0.5 * tt_drv,
              1.0e-12);
  EXPECT_NEAR(tp_far.load_slew,
              solveTwoPoleRampCrossing(tf_far, tt_drv, 0.80)
                  - solveTwoPoleRampCrossing(tf_far, tt_drv, 0.20),
              1.0e-12);
}

// Tests an asymmetric Y-branched RC tree driven by a gate: fast-stub bypass
// (elmore < 1e-3 * drvr_slew), resistive far branch, and cross-library receiver
// thresholdAdjust under both dmp_ceff_elmore and dmp_ceff_two_pole.
TEST_F(DmpAccuracyTest,
       MultiSinkAsymmetricBranchedTreeAndFastStubBypass)
{
  auto make_lib = [](std::string_view name, std::string_view cell, double vth_r,
                     double vth_f, double vl, double vh, double derate) {
    return makeLinearNldmLibText(name, cell, vth_r, vth_f, vl, vl, vh, vh, derate,
                                 2.0, {1.0, 100.0}, {1.0, 200.0}, {10.0, 109.5},
                                 {60.0, 60.0});
  };
  ASSERT_NE(
      readLibertyText(make_lib("ytree_drv", "DRV", 50.0, 50.0, 20.0, 80.0, 1.0),
                      "ytree_drv.lib"),
      nullptr);
  // Receiver with different vth = 65%, 30%-70% slew, slew_derate = 0.5.
  ASSERT_NE(
      readLibertyText(make_lib("ytree_rcv", "RCV", 65.0, 35.0, 30.0, 70.0, 0.5),
                      "ytree_rcv.lib"),
      nullptr);

  loadVerilogAndLink(
      "module top(in1, out1, out2, out3, out4);\n"
      "  input in1; output out1, out2, out3, out4; wire n1;\n"
      "  DRV u_drv(.A(in1), .Y(n1));\n"
      "  DRV u_fast(.A(n1), .Y(out1));\n"
      "  DRV u_far(.A(n1), .Y(out2));\n"
      "  DRV u_med(.A(n1), .Y(out3));\n"
      "  RCV u_rcv(.A(n1), .Y(out4));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);

  Pin *drv_a = findPin("u_drv/A");
  Pin *drv_y = findPin("u_drv/Y");
  Pin *fast_a = findPin("u_fast/A");
  Pin *far_a = findPin("u_far/A");
  Pin *med_a = findPin("u_med/A");
  Pin *rcv_a = findPin("u_rcv/A");

  Network *net = sta_->network();
  Scene *scene = sta_->cmdScene();
  const Net *dnet = net->net(drv_y);
  auto check_y_tree = [&](const char *calc_name) {
    SCOPED_TRACE(calc_name);
    sta_->deleteParasitics();
    for (const MinMax *mm : MinMax::range()) {
      Parasitics *p = scene->parasitics(mm);
      Parasitic *pnet = sta_->makeParasiticNetwork(dnet, true, scene, mm);
      ParasiticNode *n_drv = p->ensureParasiticNode(pnet, drv_y, net);
      ParasiticNode *n_split = p->ensureParasiticNode(pnet, dnet, 10, net);
      ParasiticNode *n_fast = p->ensureParasiticNode(pnet, fast_a, net);
      ParasiticNode *n_mid = p->ensureParasiticNode(pnet, dnet, 11, net);
      ParasiticNode *n_far = p->ensureParasiticNode(pnet, far_a, net);
      ParasiticNode *n_med = p->ensureParasiticNode(pnet, med_a, net);
      ParasiticNode *n_rcv = p->ensureParasiticNode(pnet, rcv_a, net);

      p->incrCap(n_split, 5.0e-15f);
      p->incrCap(n_fast, 2.0e-15f);
      p->incrCap(n_mid, 20.0e-15f);
      p->incrCap(n_far, 60.0e-15f);
      p->incrCap(n_med, 20.0e-15f);
      p->incrCap(n_rcv, 20.0e-15f);

      // Trunk has 0.1 Ohm -> fast branch elmore = 0.1*127f + 0.5*2f = 0.0137 ps
      // (< 0.060 ps = 1e-3 * drvr_slew).
      p->makeResistor(pnet, 1, 0.1f, n_drv, n_split);
      p->makeResistor(pnet, 2, 0.5f, n_split, n_fast);
      // Far resistive branch.
      p->makeResistor(pnet, 3, 1000.0f, n_split, n_mid);
      p->makeResistor(pnet, 4, 2000.0f, n_mid, n_far);
      // Symmetric medium branches for DRV vs RCV thresholdAdjust comparison.
      p->makeResistor(pnet, 5, 1000.0f, n_split, n_med);
      p->makeResistor(pnet, 6, 1000.0f, n_split, n_rcv);
    }

    sta_->setArcDelayCalc(calc_name);
    sta_->findDelays();

    const StageMetrics m_fast = measureStage(drv_a, drv_y, fast_a);
    const StageMetrics m_far = measureStage(drv_a, drv_y, far_a);
    const StageMetrics m_med = measureStage(drv_a, drv_y, med_a);
    const StageMetrics m_rcv = measureStage(drv_a, drv_y, rcv_a);

    EXPECT_DOUBLE_EQ(m_fast.gate_delay, m_far.gate_delay);
    EXPECT_DOUBLE_EQ(m_fast.drvr_slew, m_far.drvr_slew);
    EXPECT_NEAR(m_fast.wire_delay, 0.1 * 127.0e-15 + 0.5 * 2.0e-15, 0.02e-12);
    EXPECT_NEAR(m_fast.load_slew, m_fast.drvr_slew, 0.02e-12);
    EXPECT_GT(m_far.wire_delay, 100.0e-12);
    EXPECT_GT(m_far.load_slew, m_far.drvr_slew + 100.0e-12);

    // Cross-library thresholdAdjust: same RC branch, RCV shifts vth 50->65%
    // and scales 20-80% (derate=1.0) -> 30-70% (derate=0.5).
    const double expected_rcv_wd =
        m_med.wire_delay + m_med.load_slew * ((0.65 - 0.50) / 0.60);
    const double expected_rcv_ls = m_med.load_slew * ((0.40 / 0.5) / 0.60);
    EXPECT_NEAR(m_rcv.wire_delay, expected_rcv_wd, 0.02e-12);
    EXPECT_NEAR(m_rcv.load_slew, expected_rcv_ls, 0.05e-12);
    return m_med;
  };

  const StageMetrics med_elmore = check_y_tree("dmp_ceff_elmore");
  const StageMetrics med_two_pole = check_y_tree("dmp_ceff_two_pole");
  EXPECT_NEAR(med_two_pole.gate_delay, med_elmore.gate_delay, 0.01e-12);
  EXPECT_NEAR(med_two_pole.drvr_slew, med_elmore.drvr_slew, 0.01e-12);
}

// Tests Liberty wire_load Pi-Elmore estimation (worst_case_tree and balanced_tree)
// without SPEF/explicit parasitics, and verifies set_load (setNetWireCap)
// precedence overrides wireload and explicit PiElmore lookup in both DMP engines.
TEST_F(DmpAccuracyTest,
       WireloadEstimationAndSetLoadPrecedence)
{
  LibertyLibrary *lib = readLibertyText(R"(
library (wl_lib) {
  delay_model : table_lookup;
  time_unit : "1ps";
  voltage_unit : "1V";
  current_unit : "1mA";
  pulling_resistance_unit : "1kohm";
  leakage_power_unit : "1nW";
  capacitive_load_unit (1, ff);
  nom_process : 1.0; nom_temperature : 25.0; nom_voltage : 0.8;
  input_threshold_pct_rise : 50.0; input_threshold_pct_fall : 50.0;
  output_threshold_pct_rise : 50.0; output_threshold_pct_fall : 50.0;
  slew_lower_threshold_pct_rise : 20.0; slew_lower_threshold_pct_fall : 20.0;
  slew_upper_threshold_pct_rise : 80.0; slew_upper_threshold_pct_fall : 80.0;
  slew_derate_from_library : 1.0;

  wire_load ("WL_TEST") {
    capacitance : 2.0;
    resistance : 0.5;
    slope : 10.0;
    fanout_length(1, 10.0);
    fanout_length(2, 25.0);
  }
  operating_conditions (oc_worst) {
    process : 1.0; voltage : 0.8; temperature : 25.0;
    tree_type : worst_case_tree;
  }
  operating_conditions (oc_bal) {
    process : 1.0; voltage : 0.8; temperature : 25.0;
    tree_type : balanced_tree;
  }
  lu_table_template (t2) {
    variable_1 : input_net_transition;
    variable_2 : total_output_net_capacitance;
    index_1 ("1.0, 100.0");
    index_2 ("0.0, 200.0");
  }
  cell (BUF) {
    pin (A) { direction : input; capacitance : 5.0; }
    pin (Y) {
      direction : output; capacitance : 2.0; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        cell_rise (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        cell_fall (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        rise_transition (t2) { values ("20.0, 220.0", "20.0, 220.0"); }
        fall_transition (t2) { values ("20.0, 220.0", "20.0, 220.0"); }
      }
    }
  }
}
)",
                                        "wl.lib");
  ASSERT_NE(lib, nullptr);

  loadVerilogAndLink(
      "module top(in1, out1);\n"
      "  input in1; output out1; wire n1;\n"
      "  BUF u1(.A(in1), .Y(n1));\n"
      "  BUF u2(.A(n1), .Y(out1));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);

  Pin *u1_a = findPin("u1/A");
  Pin *u1_y = findPin("u1/Y");
  Pin *u2_a = findPin("u2/A");

  // makeWireload returns the existing parsed entry; findWireload returns const.
  Wireload *wl = lib->makeWireload("WL_TEST");
  ASSERT_NE(wl, nullptr);
  sta_->setWireload(wl, MinMaxAll::all(), sta_->cmdSdc());

  // 1. worst_case_tree wireload in both DMP engines.
  // Fanout 1 -> length 10 -> wire_cap = 20 fF, wire_res = 5 kOhm.
  OperatingConditions *oc_worst = lib->findOperatingConditions("oc_worst");
  ASSERT_NE(oc_worst, nullptr);
  sta_->setOperatingConditions(oc_worst, MinMaxAll::all(), sta_->cmdSdc());

  for (const char *calc : {"dmp_ceff_elmore", "dmp_ceff_two_pole"}) {
    SCOPED_TRACE(calc);
    sta_->deleteParasitics();
    sta_->setArcDelayCalc(calc);
    sta_->findDelays();

    float c2 = 0.0f, rpi = 0.0f, c1 = 0.0f;
    bool pi_exists = false;
    sta_->findPiElmore(u1_y, RiseFall::rise(), MinMax::max(), c2, rpi, c1,
                       pi_exists);
    EXPECT_TRUE(pi_exists);
    EXPECT_NEAR(c2, 2.0e-15, 0.01e-15);
    EXPECT_NEAR(rpi, 5000.0, 1.0);
    EXPECT_NEAR(c1, 25.0e-15, 0.01e-15);

    const StageMetrics m_wl = measureStage(u1_a, u1_y, u2_a);
    // Shielding keeps gate_delay strictly between C2 (2 fF -> 12 ps) and
    // unshielded total cap (2 + 25 = 27 fF -> 37 ps).
    EXPECT_GT(m_wl.gate_delay, 12.0e-12);
    EXPECT_LT(m_wl.gate_delay, 37.0e-12);
    if (std::string_view(calc) == "dmp_ceff_elmore") {
      EXPECT_GT(m_wl.wire_delay, 20.0e-12);
      EXPECT_GT(m_wl.load_slew, m_wl.drvr_slew + 20.0e-12);
    }
  }

  // 2. balanced_tree wireload mode.
  OperatingConditions *oc_bal = lib->findOperatingConditions("oc_bal");
  ASSERT_NE(oc_bal, nullptr);
  sta_->deleteParasitics();
  sta_->setOperatingConditions(oc_bal, MinMaxAll::all(), sta_->cmdSdc());
  sta_->setArcDelayCalc("dmp_ceff_elmore");
  sta_->findDelays();
  const StageMetrics m_bal = measureStage(u1_a, u1_y, u2_a);
  EXPECT_GT(m_bal.wire_delay, 20.0e-12);
  EXPECT_GT(m_bal.load_slew, m_bal.drvr_slew + 20.0e-12);

  // 3. set_load (setNetWireCap) overrides both wireload and explicit PiElmore.
  Net *n1 = sta_->network()->net(u1_y);
  ASSERT_NE(n1, nullptr);
  setPiElmoreModel(u1_y, u2_a, 10.0e-15, 2000.0, 90.0e-15, 180.0e-12);
  sta_->setNetWireCap(n1, false, MinMaxAll::all(), 50.0e-15f, sta_->cmdSdc());
  sta_->setArcDelayCalc("dmp_ceff_elmore");
  sta_->findDelays();
  const StageMetrics m_setload = measureStage(u1_a, u1_y, u2_a);
  // Total load cap = drvr(2 fF) + load(5 fF) + wire(50 fF) = 57 fF -> 67 ps.
  EXPECT_NEAR(m_setload.gate_delay, 67.0e-12, 0.01e-12);
  EXPECT_NEAR(m_setload.drvr_slew, 77.0e-12, 0.01e-12);
  EXPECT_NEAR(m_setload.wire_delay, 0.0, 0.01e-12);
  EXPECT_NEAR(m_setload.load_slew, m_setload.drvr_slew, 0.01e-12);
}

// Tests 1D NLDM tables (cap-only and slew-only) and non-table linear cells
// (delay_model : generic_cmos -> LumpedCapDelayCalc fallback) under DMP.
TEST_F(DmpAccuracyTest,
       OneDimNldmTablesAndNonTableFallback)
{
  const std::string lib_attrs = R"(
  time_unit : "1ps"; voltage_unit : "1V"; current_unit : "1mA";
  pulling_resistance_unit : "1kohm"; leakage_power_unit : "1nW";
  capacitive_load_unit (1, ff);
  nom_process : 1.0; nom_temperature : 25.0; nom_voltage : 0.8;
  input_threshold_pct_rise : 50.0; input_threshold_pct_fall : 50.0;
  output_threshold_pct_rise : 50.0; output_threshold_pct_fall : 50.0;
  slew_lower_threshold_pct_rise : 20.0; slew_lower_threshold_pct_fall : 20.0;
  slew_upper_threshold_pct_rise : 80.0; slew_upper_threshold_pct_fall : 80.0;
  slew_derate_from_library : 1.0;
)";
  const std::string lib_1d =
      "library (nldm_1d) {\n  delay_model : table_lookup;" + lib_attrs + R"(
  lu_table_template (t_cap) {
    variable_1 : total_output_net_capacitance;
    index_1 ("0.0, 100.0");
  }
  lu_table_template (t_slew) {
    variable_1 : input_net_transition;
    index_1 ("10.0, 110.0");
  }
  cell (BUF_1D_CAP) {
    pin (A) { direction : input; capacitance : 2.0; }
    pin (Y) {
      direction : output; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        cell_rise (t_cap) { values ("10.0, 110.0"); }
        cell_fall (t_cap) { values ("10.0, 110.0"); }
        rise_transition (t_cap) { values ("20.0, 120.0"); }
        fall_transition (t_cap) { values ("20.0, 120.0"); }
      }
    }
  }
  cell (BUF_1D_SLEW) {
    pin (A) { direction : input; capacitance : 2.0; }
    pin (Y) {
      direction : output; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        cell_rise (t_slew) { values ("15.0, 45.0"); }
        cell_fall (t_slew) { values ("15.0, 45.0"); }
        rise_transition (t_slew) { values ("12.0, 32.0"); }
        fall_transition (t_slew) { values ("12.0, 32.0"); }
      }
    }
  }
}
)";
  const std::string lib_linear =
      "library (cmos_lin) {\n  delay_model : generic_cmos;" + lib_attrs + R"(
  cell (BUF_LIN_CMOS) {
    pin (A) { direction : input; capacitance : 5.0; }
    pin (Y) {
      direction : output; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        intrinsic_rise : 14.0; intrinsic_fall : 14.0;
        rise_resistance : 0.4; fall_resistance : 0.4;
      }
    }
  }
}
)";
  ASSERT_NE(readLibertyText(lib_1d, "nldm1d.lib"), nullptr);
  ASSERT_NE(readLibertyText(lib_linear, "lin_cmos.lib"), nullptr);

  loadVerilogAndLink(
      "module top(in1, out1, out2, out3);\n"
      "  input in1; output out1, out2, out3; wire n1, n2, n3;\n"
      "  BUF_1D_CAP u_cap(.A(in1), .Y(n1));\n"
      "  BUF_1D_CAP u_cap_l(.A(n1), .Y(out1));\n"
      "  BUF_1D_SLEW u_slew(.A(in1), .Y(n2));\n"
      "  BUF_1D_SLEW u_slew_l(.A(n2), .Y(out2));\n"
      "  BUF_LIN_CMOS u_lin(.A(in1), .Y(n3));\n"
      "  BUF_LIN_CMOS u_lin_l(.A(n3), .Y(out3));\n"
      "endmodule\n");
  setInputTransition("in1", 30.0e-12);

  Pin *cap_a = findPin("u_cap/A");
  Pin *cap_y = findPin("u_cap/Y");
  Pin *capl_a = findPin("u_cap_l/A");
  Pin *slew_a = findPin("u_slew/A");
  Pin *slew_y = findPin("u_slew/Y");
  Pin *slewl_a = findPin("u_slew_l/A");
  Pin *lin_a = findPin("u_lin/A");
  Pin *lin_y = findPin("u_lin/Y");
  Pin *linl_a = findPin("u_lin_l/A");

  setPiElmoreModel(cap_y, capl_a, 10.0e-15, 2000.0, 90.0e-15, 180.0e-12);
  setPiElmoreModel(slew_y, slewl_a, 10.0e-15, 2000.0, 90.0e-15, 180.0e-12);
  setPiElmoreModel(lin_y, linl_a, 10.0e-15, 500.0, 40.0e-15, 20.0e-12);

  sta_->setArcDelayCalc("dmp_ceff_elmore");
  sta_->findDelays();

  // 1D cap-only table activates DmpPi shielding:
  // delay(C2=10fF)=20 ps < gate_delay < delay(C2+C1=100fF)=110 ps.
  const StageMetrics m_cap = measureStage(cap_a, cap_y, capl_a);
  EXPECT_GT(m_cap.gate_delay, 20.0e-12);
  EXPECT_LT(m_cap.gate_delay, 110.0e-12);
  EXPECT_GT(m_cap.wire_delay, 100.0e-12);
  EXPECT_GT(m_cap.load_slew, m_cap.drvr_slew + 100.0e-12);

  // 1D slew-only table has Rd == 0 -> DmpCap:
  // interpolates in_slew=30 ps -> gate_delay=21 ps, drvr_slew=16 ps.
  const StageMetrics m_slew = measureStage(slew_a, slew_y, slewl_a);
  EXPECT_NEAR(m_slew.gate_delay, 21.0e-12, 0.01e-12);
  EXPECT_NEAR(m_slew.drvr_slew, 16.0e-12, 0.01e-12);
  EXPECT_NEAR(m_slew.wire_delay, 180.0e-12, 0.01e-12);
  EXPECT_NEAR(m_slew.load_slew, 16.0e-12, 0.01e-12);

  // Linear generic_cmos (table_model == nullptr) falls back to LumpedCapDelayCalc:
  // load_cap = c2 + c1 = 50 fF -> 14 + 0.4 * 50 = 34 ps, 0 slew, 0 wire delay.
  for (const char *calc : {"dmp_ceff_elmore", "dmp_ceff_two_pole"}) {
    SCOPED_TRACE(calc);
    sta_->setArcDelayCalc(calc);
    sta_->findDelays();
    const StageMetrics m_lin = measureStage(lin_a, lin_y, linl_a);
    EXPECT_NEAR(m_lin.gate_delay, 34.0e-12, 0.01e-12);
    EXPECT_NEAR(m_lin.drvr_slew, 0.0, 0.01e-12);
    EXPECT_NEAR(m_lin.wire_delay, 0.0, 0.01e-12);
    EXPECT_NEAR(m_lin.load_slew, 0.0, 0.01e-12);
  }
}

// Verifies DmpCeffDelayCalc::reportGateDelay / Sta::reportDelayCalc outputs
// consistent Pi-model Ceff, table lookup, and driver waveform slew strings
// both with an active PiElmore parasitic and without parasitics (c_eff = load_cap).
TEST_F(DmpAccuracyTest,
       ReportDelayCalcNumericConsistency)
{
  ASSERT_NE(readLibertyText(makeLinearNldmLibText(
                "rep_lib", "BUF", 50.0, 50.0, 20.0, 20.0, 80.0, 80.0, 1.0, 5.0,
                {1.0, 100.0}, {0.0, 200.0}, {10.0, 210.0}, {10.0, 110.0})),
            nullptr);
  loadVerilogAndLink(
      "module top(in1, out1, out2);\n"
      "  input in1; output out1, out2; wire n1, n2;\n"
      "  BUF u1(.A(in1), .Y(n1));\n"
      "  BUF u1_l(.A(n1), .Y(out1));\n"
      "  BUF u2(.A(in1), .Y(n2));\n"
      "  BUF u2_l(.A(n2), .Y(out2));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);
  sta_->setArcDelayCalc("dmp_ceff_elmore");

  Pin *u1_a = findPin("u1/A");
  Pin *u1_y = findPin("u1/Y");
  Pin *u1l_a = findPin("u1_l/A");
  Pin *u2_a = findPin("u2/A");
  Pin *u2_y = findPin("u2/Y");
  Pin *u2l_a = findPin("u2_l/A");

  // u1 has a PiElmore parasitic; u2 has no parasitic (lumped pin cap = 5 fF).
  setPiElmoreModel(u1_y, u1l_a, 10.0e-15, 1.0e3, 90.0e-15, 90.0e-12);
  sta_->findDelays();

  auto report_rise = [&](const Pin *in_pin, const Pin *drvr_pin) -> std::string {
    Edge *edge = nullptr;
    const TimingArc *arc = nullptr;
    sta_->graph()->gateEdgeArc(in_pin, RiseFall::rise(), drvr_pin, RiseFall::rise(),
                               edge, arc);
    EXPECT_NE(edge, nullptr);
    EXPECT_NE(arc, nullptr);
    if (!edge || !arc) {
      return {};
    }
    return sta_->reportDelayCalc(edge, const_cast<TimingArc *>(arc),
                                 sta_->cmdScene(), MinMax::max(), 4);
  };

  const StageMetrics m1 = measureStage(u1_a, u1_y, u1l_a);
  const std::string rep_pi = report_rise(u1_a, u1_y);
  EXPECT_NE(rep_pi.find("Pi model C2=10.0000 Rpi=1.0000 C1=90.0000, Ceff="),
            std::string::npos);
  const double reported_ceff_ff = extractMarkedValue(rep_pi, "Ceff=");
  const double implied_ceff_ff = m1.gate_delay * 1.0e12 - 10.0;
  EXPECT_GT(reported_ceff_ff, 10.0);
  EXPECT_LT(reported_ceff_ff, 100.0);
  EXPECT_NEAR(reported_ceff_ff, implied_ceff_ff, 0.01);
  EXPECT_NEAR(extractMarkedValue(rep_pi, "Driver waveform slew = "),
              m1.drvr_slew * 1.0e12, 0.01);

  const StageMetrics m2 = measureStage(u2_a, u2_y, u2l_a);
  const std::string rep_nopi = report_rise(u2_a, u2_y);
  EXPECT_EQ(rep_nopi.find("Pi model"), std::string::npos);
  EXPECT_NE(rep_nopi.find("total_output_net_capacitance = 5.0000"),
            std::string::npos);
  EXPECT_NEAR(extractMarkedValue(rep_nopi, "Delay = "), m2.gate_delay * 1.0e12,
              0.01);
  EXPECT_NEAR(extractMarkedValue(rep_nopi, "Driver waveform slew = "),
              m2.drvr_slew * 1.0e12, 0.01);
}

// Tests incremental parasitic replacement/deletion (makePiElmore + deleteParasitics)
// and verifies POCV/LVF cell parameters (normal and skew_normal) are evaluated
// at effective capacitance Ceff rather than total capacitance C1 + C2.
TEST_F(DmpAccuracyTest,
       IncrementalParasiticMutationAndPocvCeffEvaluation)
{
  const std::string lib_text = R"(
library (pocv_ceff) {
  delay_model : table_lookup;
  time_unit : "1ps"; voltage_unit : "1V"; current_unit : "1mA";
  pulling_resistance_unit : "1kohm"; leakage_power_unit : "1nW";
  capacitive_load_unit (1, ff);
  nom_process : 1.0; nom_temperature : 25.0; nom_voltage : 0.8;
  input_threshold_pct_rise : 50.0; input_threshold_pct_fall : 50.0;
  output_threshold_pct_rise : 50.0; output_threshold_pct_fall : 50.0;
  slew_lower_threshold_pct_rise : 20.0; slew_lower_threshold_pct_fall : 20.0;
  slew_upper_threshold_pct_rise : 80.0; slew_upper_threshold_pct_fall : 80.0;
  slew_derate_from_library : 1.0;
  lu_table_template (t2) {
    variable_1 : input_net_transition;
    variable_2 : total_output_net_capacitance;
    index_1 ("1.0, 100.0"); index_2 ("0.0, 200.0");
  }
  cell (BUF) {
    pin (A) { direction : input; capacitance : 4.0; }
    pin (Y) {
      direction : output; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        cell_rise (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        cell_fall (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        rise_transition (t2) { values ("10.0, 110.0", "10.0, 110.0"); }
        fall_transition (t2) { values ("10.0, 110.0", "10.0, 110.0"); }
        ocv_sigma_cell_rise (t2) { sigma_type : early_and_late; values ("1.0, 21.0", "1.0, 21.0"); }
        ocv_sigma_cell_fall (t2) { sigma_type : early_and_late; values ("1.0, 21.0", "1.0, 21.0"); }
        ocv_sigma_rise_transition (t2) { sigma_type : early_and_late; values ("0.5, 10.5", "0.5, 10.5"); }
        ocv_sigma_fall_transition (t2) { sigma_type : early_and_late; values ("0.5, 10.5", "0.5, 10.5"); }
      }
    }
  }
  cell (BUF_LVF) {
    pin (A) { direction : input; capacitance : 4.0; }
    pin (Y) {
      direction : output; function : "A";
      timing () {
        related_pin : "A"; timing_sense : positive_unate;
        cell_rise (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        cell_fall (t2) { values ("10.0, 210.0", "10.0, 210.0"); }
        rise_transition (t2) { values ("10.0, 110.0", "10.0, 110.0"); }
        fall_transition (t2) { values ("10.0, 110.0", "10.0, 110.0"); }
        ocv_std_dev_cell_rise (t2) { values ("2.0, 42.0", "2.0, 42.0"); }
        ocv_std_dev_cell_fall (t2) { values ("2.0, 42.0", "2.0, 42.0"); }
        ocv_mean_shift_cell_rise (t2) { values ("0.2, 4.2", "0.2, 4.2"); }
        ocv_mean_shift_cell_fall (t2) { values ("0.2, 4.2", "0.2, 4.2"); }
        ocv_skewness_cell_rise (t2) { values ("0.1, 0.9", "0.1, 0.9"); }
        ocv_skewness_cell_fall (t2) { values ("0.1, 0.9", "0.1, 0.9"); }
        ocv_std_dev_rise_transition (t2) { values ("1.0, 21.0", "1.0, 21.0"); }
        ocv_std_dev_fall_transition (t2) { values ("1.0, 21.0", "1.0, 21.0"); }
        ocv_mean_shift_rise_transition (t2) { values ("0.1, 2.1", "0.1, 2.1"); }
        ocv_mean_shift_fall_transition (t2) { values ("0.1, 2.1", "0.1, 2.1"); }
        ocv_skewness_rise_transition (t2) { values ("0.05, 0.45", "0.05, 0.45"); }
        ocv_skewness_fall_transition (t2) { values ("0.05, 0.45", "0.05, 0.45"); }
      }
    }
  }
}
)";
  ASSERT_NE(readLibertyText(lib_text, "pocv.lib"), nullptr);
  loadVerilogAndLink(
      "module top(in1, out1, out2);\n"
      "  input in1; output out1, out2; wire n1, n2;\n"
      "  BUF u1(.A(in1), .Y(n1));\n"
      "  BUF u2(.A(n1), .Y(out1));\n"
      "  BUF_LVF u3(.A(in1), .Y(n2));\n"
      "  BUF_LVF u4(.A(n2), .Y(out2));\n"
      "endmodule\n");
  setInputTransition("in1", 10.0e-12);
  sta_->setArcDelayCalc("dmp_ceff_elmore");

  Pin *u1_a = findPin("u1/A");
  Pin *u1_y = findPin("u1/Y");
  Pin *u2_a = findPin("u2/A");
  Pin *u3_a = findPin("u3/A");
  Pin *u3_y = findPin("u3/Y");
  Pin *u4_a = findPin("u4/A");

  // 1. Incremental parasitic replacement and deletion.
  setPiElmoreModel(u1_y, u2_a, 10.0e-15, 500.0, 90.0e-15, 45.0e-12);
  sta_->findDelays();
  const StageMetrics m_low_r = measureStage(u1_a, u1_y, u2_a);

  setPiElmoreModel(u1_y, u2_a, 10.0e-15, 5000.0, 90.0e-15, 450.0e-12);
  sta_->findDelays();
  const StageMetrics m_high_r = measureStage(u1_a, u1_y, u2_a);
  EXPECT_LT(m_high_r.gate_delay, m_low_r.gate_delay);
  EXPECT_GT(m_high_r.wire_delay, m_low_r.wire_delay + 250.0e-12);
  EXPECT_GT(m_high_r.load_slew, m_low_r.load_slew + 400.0e-12);

  sta_->deleteParasitics();
  sta_->findDelays();
  const StageMetrics m_cleared = measureStage(u1_a, u1_y, u2_a);
  // Pin cap = 4 fF -> gate_delay = 14 ps, drvr_slew = 12 ps, 0 wire delay.
  EXPECT_NEAR(m_cleared.gate_delay, 14.0e-12, 0.01e-12);
  EXPECT_NEAR(m_cleared.drvr_slew, 12.0e-12, 0.01e-12);
  EXPECT_NEAR(m_cleared.wire_delay, 0.0, 0.01e-12);
  EXPECT_NEAR(m_cleared.load_slew, m_cleared.drvr_slew, 0.01e-12);

  // 2. POCV normal (ocv_sigma_*) and skew_normal (LVF moments) evaluate at
  // shielded Ceff rather than C1 + C2.
  setPiElmoreModel(u1_y, u2_a, 10.0e-15, 5000.0, 90.0e-15, 450.0e-12);
  setPiElmoreModel(u3_y, u4_a, 10.0e-15, 5000.0, 90.0e-15, 450.0e-12);

  Graph *graph = sta_->graph();
  const DcalcAPIndex ap = sta_->cmdScene()->dcalcAnalysisPtIndex(MinMax::max());
  struct GateRiseStats
  {
    ArcDelay delay;
    Slew slew;
  };
  auto read_gate_rise = [&](const Pin *in_pin,
                            const Pin *drvr_pin) -> GateRiseStats {
    Edge *edge = nullptr;
    const TimingArc *arc = nullptr;
    graph->gateEdgeArc(in_pin, RiseFall::rise(), drvr_pin, RiseFall::rise(), edge,
                       arc);
    EXPECT_NE(edge, nullptr);
    EXPECT_NE(arc, nullptr);
    Vertex *vtx = graph->pinDrvrVertex(drvr_pin);
    EXPECT_NE(vtx, nullptr);
    return {(edge && arc) ? graph->arcDelay(edge, arc, ap) : ArcDelay{},
            vtx ? graph->slew(vtx, RiseFall::rise(), ap) : Slew{}};
  };

  sta_->setPocvMode(PocvMode::normal);
  sta_->setPocvQuantile(3.0f);
  sta_->delaysInvalid();
  sta_->findDelays();

  const auto [gd_norm, ds_norm] = read_gate_rise(u1_a, u1_y);
  // cell_rise(cap) = 10 + cap_ff ps -> exact Ceff in fF from mean delay.
  const double ceff_ff = gd_norm.mean() * 1.0e12 - 10.0;
  EXPECT_GT(ceff_ff, 10.0);
  EXPECT_LT(ceff_ff, 50.0);
  EXPECT_NEAR(gd_norm.stdDev(), (1.0 + 0.1 * ceff_ff) * 1.0e-12, 0.01e-12);
  EXPECT_NEAR(ds_norm.stdDev(), (0.5 + 0.05 * ceff_ff) * 1.0e-12, 0.01e-12);
  // Strictly less than unshielded sigma(C1 + C2 = 100 fF) = 11.0 ps.
  EXPECT_LT(gd_norm.stdDev(), 6.0e-12);

  sta_->setPocvMode(PocvMode::skew_normal);
  sta_->delaysInvalid();
  sta_->findDelays();

  const auto [gd_skew, ds_skew] = read_gate_rise(u3_a, u3_y);
  EXPECT_NEAR(gd_skew.stdDev(), (2.0 + 0.2 * ceff_ff) * 1.0e-12, 0.01e-12);
  EXPECT_NEAR(gd_skew.meanShift(), (0.2 + 0.02 * ceff_ff) * 1.0e-12, 0.01e-12);
  EXPECT_NEAR(gd_skew.skewness(), 0.1 + 0.004 * ceff_ff, 1.0e-3);
  EXPECT_NEAR(ds_skew.stdDev(), (1.0 + 0.1 * ceff_ff) * 1.0e-12, 0.01e-12);
  EXPECT_NEAR(ds_skew.meanShift(), (0.1 + 0.01 * ceff_ff) * 1.0e-12, 0.01e-12);
  EXPECT_NEAR(ds_skew.skewness(), 0.05 + 0.002 * ceff_ff, 1.0e-3);
}

}  // namespace
}  // namespace sta
