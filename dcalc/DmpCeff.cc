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

// "Performance Computation for Precharacterized CMOS Gates with RC Loads",
// Florentin Dartu, Noel Menezes and Lawrence Pileggi, IEEE Transactions
// on Computer-Aided Design of Integrated Circuits and Systems, Vol 15, No 5,
// May 1996, pg 544-553.
//
// The only real change from the paper is that Vl, the measured low
// slew voltage is matched instead of y20 in eqn 12.

#include "DmpCeff.hh"

#include <algorithm> // abs, min
#include <cmath>    // sqrt, log

#include "Report.hh"
#include "Debug.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "ArcDelayCalc.hh"

namespace sta {

using std::abs;
using std::min;
using std::max;
using std::sqrt;
using std::log;
using std::isnan;

// Tolerance (as a scale of value) for driver parameters (Ceff, delta t, t0).
static const double driver_param_tol = .01;
// Waveform threshold crossing time tolerance (1.0 = 100%).
static const double vth_time_tol = .01;
// A small number used by luDecomp.
static const double tiny_double = 1.0e-20;
// Max iterations for findRoot.
static const int find_root_max_iter = 20;

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
  DmpError(const char *what);
  virtual ~DmpError() {}
  virtual const char *what() const noexcept { return what_; }

private:
  const char *what_;
};

static double
gateModelRd(const LibertyCell *cell,
	    const GateTableModel *gate_model,
	    const RiseFall *rf,
	    double in_slew,
	    double c2,
	    double c1,
	    float related_out_cap,
	    const Pvt *pvt,
	    bool pocv_enabled);
static void
evalDmpEqnsState(void *state);
static void
evalVoEqns(void *state,
	   double x,
	   double &y,
	   double &dy);
static void
evalVlEqns(void *state,
	   double x,
	   double &y,
	   double &dy);

static double
findRoot(void (*func)(void *state, double x, double &y, double &dy),
	 void *state,
	 double x1,
	 double x2,
	 double x_tol,
	 int max_iter);
static void
newtonRaphson(const int max_iter,
	      double x[],
	      const int n,
	      const double x_tol,
	      // eval(state) is called to fill fvec and fjac.
	      // Returns false if fails.
	      void (*eval)(void *state),
	      void *state,
	      // Temporaries supplied by caller.
	      double *fvec,
	      double **fjac,
	      int *index,
	      double *p,
	      double *scale);
static void
luSolve(double **a,
	const int size,
	const int *index,
	double b[]);
static void
luDecomp(double **a,
	 const int size,
	 int *index,
	 double *scale);

////////////////////////////////////////////////////////////////

// Base class for Dartu/Menezes/Pileggi algorithm.
// Derived classes handle different cases of zero values in the Pi model.
class DmpAlg : public StaState
{
public:
  DmpAlg(int nr_order, StaState *sta);
  virtual ~DmpAlg();
  virtual const char *name() = 0;
  // Set driver model and pi model parameters for delay calculation.
  virtual void init(const LibertyLibrary *library,
		    const LibertyCell *drvr_cell,
		    const Pvt *pvt,
		    const GateTableModel *gate_model,
		    const RiseFall *rf,
		    double rd,
		    double in_slew,
		    float related_out_cap,
		    double c2,
		    double rpi,
		    double c1);
  virtual void gateDelaySlew(double &delay,
			     double &slew) = 0;
  virtual void loadDelaySlew(const Pin *load_pin,
			     double elmore,
			     ArcDelay &delay,
			     Slew &slew);
  double ceff() { return ceff_; }

  // Given x_ as a vector of input parameters, fill fvec_ with the
  // equations evaluated at x_ and fjac_ with the jabobian evaluated at x_.
  virtual void evalDmpEqns() = 0;
  // Output response to vs(t) ramp driving pi model load.
  double vo(double t);
  double dVoDt(double t);
  // Load responce to driver waveform.
  double vl(double t);
  double dVlDt(double t);
  double vCross() { return v_cross_; }

protected:
  // Find driver parameters t0, delta_t, Ceff.
  void findDriverParams(double ceff);
  void gateCapDelaySlew(double cl,
			double &delay,
			double &slew);
  void gateDelays(double ceff,
		  double &t_vth,
		  double &t_vl,
		  double &slew);
  virtual double dv0dt(double t) = 0;
  // Partial derivatives of y(t) (jacobian).
  void dy(double t,
	  double t0,
	  double dt,
	  double cl,
	  double &dydt0,
	  double &dyddt,
	  double &dydcl);
  double y0dt(double t,
	      double cl);
  double y0dcl(double t,
	       double cl);
  void showX();
  void showFvec();
  void showJacobian();
  void findDriverDelaySlew(double &delay,
			   double &slew);
  double findVoCrossing(double vth);
  void showVo();
  double findVlCrossing(double vth);
  void showVl();
  void fail(const char *reason);

  // Output response to vs(t) ramp driving capacitive load.
  double y(double t,
	   double t0,
	   double dt,
	   double cl);
  // Output response to unit ramp driving capacitive load.
  double y0(double t,
	    double cl);
  // Output response to unit ramp driving pi model load.
  virtual double v0(double t) = 0;
  // Upper bound on time that vo crosses vh.
  virtual double voCrossingUpperBound() = 0;
  // Load responce to driver unit ramp.
  virtual double vl0(double t) = 0;
  // Upper bound on time that vl crosses vh.
  double vlCrossingUpperBound();

  // Inputs to the delay calculator.
  const LibertyCell *drvr_cell_;
  const LibertyLibrary *drvr_library_;
  const Pvt *pvt_;
  const GateTableModel *gate_model_;
  double in_slew_;
  float related_out_cap_;
  double c2_;
  double rpi_;
  double c1_;

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
  double *x_;
  double *fvec_;
  double **fjac_;
  double *scale_;
  double *p_;
  int *index_;

  // Driver slew used to check load delay.
  double drvr_slew_;
  double vo_delay_;
  // True if the driver parameters are valid for finding the load delays.
  bool driver_valid_;
  // Load rspf elmore delay.
  double elmore_;
  double p3_;

private:
  virtual double dvl0dt(double t) = 0;

  // Implicit argument passed to evalVoEqns, evalVlEqns.
  double v_cross_;
};

DmpAlg::DmpAlg(int nr_order,
	       StaState *sta):
  StaState(sta),
  c2_(0.0),
  rpi_(0.0),
  c1_(0.0),
  nr_order_(nr_order)
{
  x_ = new double[nr_order_];
  fvec_ = new double[nr_order_];
  scale_ = new double[nr_order_];
  p_ = new double[nr_order_];
  fjac_ = new double*[nr_order_];
  for (int i = 0; i < nr_order_; i++)
    fjac_[i] = new double[nr_order_];
  index_ = new int[nr_order_];
}

DmpAlg::~DmpAlg()
{
  delete [] x_;
  delete [] fvec_;
  delete [] scale_;
  delete [] p_;
  for (int i = 0; i < nr_order_; i++)
    delete [] fjac_[i];
  delete [] fjac_;
  delete [] index_;
}

void
DmpAlg::init(const LibertyLibrary *drvr_library,
	     const LibertyCell *drvr_cell,
	     const Pvt *pvt,
	     const GateTableModel *gate_model,
	     const RiseFall *rf,
	     double rd,
	     double in_slew,
	     float related_out_cap,
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
  related_out_cap_ = related_out_cap;
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
  double t_vth, t_vl, slew;
  gateDelays(ceff, t_vth, t_vl, slew);
  // Scale slew to 0-100%
  double dt = slew / (vh_ - vl_);
  double t0 = t_vth + log(1.0 - vth_) * rd_ * ceff - vth_ * dt;
  x_[DmpParam::dt] = dt;
  x_[DmpParam::t0] = t0;
  newtonRaphson(100, x_, nr_order_, driver_param_tol, evalDmpEqnsState,
		this, fvec_, fjac_, index_, p_, scale_);
  t0_ = x_[DmpParam::t0];
  dt_ = x_[DmpParam::dt];
  debugPrint(debug_, "dmp_ceff", 3, "    t0 = %s dt = %s ceff = %s",
             units_->timeUnit()->asString(t0_),
             units_->timeUnit()->asString(dt_),
             units_->capacitanceUnit()->asString(x_[DmpParam::ceff]));
  if (debug_->check("dmp_ceff", 4))
    showVo();
}

static void
evalDmpEqnsState(void *state)
{
  DmpAlg *alg = reinterpret_cast<DmpAlg *>(state);
  alg->evalDmpEqns();
}

void
DmpAlg::gateCapDelaySlew(double ceff,
			 double &delay,
			 double &slew)
{
  ArcDelay model_delay;
  Slew model_slew;
  gate_model_->gateDelay(drvr_cell_, pvt_, in_slew_, ceff, related_out_cap_,
			 pocv_enabled_, model_delay, model_slew);
  delay = delayAsFloat(model_delay);
  slew = delayAsFloat(model_slew);
}

void
DmpAlg::gateDelays(double ceff,
		   double &t_vth,
		   double &t_vl,
		   double &slew)
{
  double table_slew;
  gateCapDelaySlew(ceff, t_vth, table_slew);
  // Convert reported/table slew to measured slew.
  slew = table_slew * slew_derate_;
  t_vl = t_vth - slew * (vth_ - vl_) / (vh_ - vl_);
}

double
DmpAlg::y(double t,
	  double t0,
	  double dt,
	  double cl)
{
  double t1 = t - t0;
  if (t1 <= 0.0)
    return 0.0;
  else if (t1 <= dt)
    return y0(t1, cl) / dt;
  else
    return (y0(t1, cl) - y0(t1 - dt, cl)) / dt;
}

double
DmpAlg::y0(double t,
	   double cl)
{
  return t - rd_ * cl * (1.0 - exp2(-t / (rd_ * cl)));
}

void
DmpAlg::dy(double t,
	   double t0,
	   double dt,
	   double cl,
	   // Return values.
	   double &dydt0,
	   double &dyddt,
	   double &dydcl)
{
  double t1 = t - t0;
  if (t1 <= 0.0)
    dydt0 = dyddt = dydcl = 0.0;
  else if (t1 <= dt) {
    dydt0 = -y0dt(t1, cl) / dt;
    dyddt = -y0(t1, cl) / (dt * dt);
    dydcl = y0dcl(t1, cl) / dt;
  }
  else {
    dydt0 = -(y0dt(t1, cl) - y0dt(t1 - dt, cl)) / dt;
    dyddt = -(y0(t1, cl) + y0(t1 - dt, cl)) / (dt * dt)
      + y0dt(t1 - dt, cl) / dt;
    dydcl = (y0dcl(t1, cl) - y0dcl(t1 - dt, cl)) / dt;
  }
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
    report_->reportLine("%4s %12.3e", dmp_param_index_strings[i], x_[i]);
}

void
DmpAlg::showFvec()
{
  for (int i = 0; i < nr_order_; i++)
    report_->reportLine("%4s %12.3e", dmp_func_index_strings[i], fvec_[i]);
}

void
DmpAlg::showJacobian()
{
  string line = "    ";
  for (int j = 0; j < nr_order_; j++)
    line += stdstrPrint("%12s", dmp_param_index_strings[j]);
  report_->reportLineString(line);
  line.clear();
  for (int i = 0; i < nr_order_; i++) {
    line += stdstrPrint("%4s ", dmp_func_index_strings[i]);
    for (int j = 0; j < nr_order_; j++)
      line += stdstrPrint("%12.3e ", fjac_[i][j]);
    report_->reportLineString(line);
  }
}

void
DmpAlg::findDriverDelaySlew(double &delay,
			    double &slew)
{
  delay = findVoCrossing(vth_);
  double tl = findVoCrossing(vl_);
  double th = findVoCrossing(vh_);
  // Convert measured slew to table slew.
  slew = (th - tl) / slew_derate_;
}

// Find t such that vo(t)=v.
double
DmpAlg::findVoCrossing(double vth)
{
  v_cross_ = vth;
  double ub = voCrossingUpperBound();
  return findRoot(evalVoEqns, this, t0_, ub, vth_time_tol, find_root_max_iter);
}

static void
evalVoEqns(void *state,
	   double x,
	   double &y,
	   double &dy)
{
  DmpAlg *pi_ceff = reinterpret_cast<DmpAlg *>(state);
  y = pi_ceff->vo(x) - pi_ceff->vCross();
  dy = pi_ceff->dVoDt(x);
}

double
DmpAlg::vo(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0.0)
    return 0.0;
  else if (t1 <= dt_)
    return v0(t1) / dt_;
  else
    return (v0(t1) - v0(t1 - dt_)) / dt_;
}

double
DmpAlg::dVoDt(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0)
    return 0.0;
  else if (t1 <= dt_)
    return dv0dt(t1) / dt_;
  else
    return (dv0dt(t1) - dv0dt(t1 - dt_)) / dt_;
}

void
DmpAlg::showVo()
{
  report_->reportLine("  t    vo(t)");
  double ub = voCrossingUpperBound();
  for (double t = t0_; t < t0_ + ub; t += dt_ / 10.0)
    report_->reportLine(" %g %g", t, vo(t));
}

void
DmpAlg::loadDelaySlew(const Pin *,
		      double elmore,
		      ArcDelay &delay,
		      Slew &slew)
{
  if (!driver_valid_
      || elmore == 0.0
      // Elmore delay is small compared to driver slew.
      || elmore < drvr_slew_ * 1e-3) {
    delay = elmore;
    slew = drvr_slew_;
  }
  else {
    // Use the driver thresholds and rely on thresholdAdjust to
    // convert the delay and slew to the load's thresholds.
    try {
      if (debug_->check("dmp_ceff", 4))
	showVl();
      elmore_ = elmore;
      p3_ = 1.0 / elmore;
      double load_delay = findVlCrossing(vth_);
      double tl = findVlCrossing(vl_);
      double th = findVlCrossing(vh_);
      // Measure delay from Vo, the load dependent source excitation.
      double delay1 = load_delay - vo_delay_;
      // Convert measured slew to reported/table slew.
      double slew1 = (th - tl) / slew_derate_;
      if (delay1 < 0.0) {
	// Only report a problem if the difference is significant.
	if (-delay1 > vth_time_tol * vo_delay_)
	  fail("load delay less than zero");
	// Use elmore delay.
	delay1 = elmore;
      }
      if (slew1 < drvr_slew_) {
	// Only report a problem if the difference is significant.
	if ((drvr_slew_ - slew1) > vth_time_tol * drvr_slew_)
	  fail("load slew less than driver slew");
	slew1 = drvr_slew_;
      }
      delay = delay1;
      slew = slew1;
    }
    catch (DmpError &error) {
      fail(error.what());
      delay = elmore_;
      slew = drvr_slew_;
    }
  }
}

// Find t such that vl(t)=v.
// Return true if successful.
double
DmpAlg::findVlCrossing(double vth)
{
  v_cross_ = vth;
  double ub = vlCrossingUpperBound();
  return findRoot(evalVlEqns, this, t0_, ub, vth_time_tol, find_root_max_iter);
}

double
DmpAlg::vlCrossingUpperBound()
{
  return voCrossingUpperBound() + elmore_ * 2.0;
}

static void
evalVlEqns(void *state,
	   double x,
	   double &y,
	   double &dy)
{
  DmpAlg *pi_ceff = reinterpret_cast<DmpAlg *>(state);
  y = pi_ceff->vl(x) - pi_ceff->vCross();
  dy = pi_ceff->dVlDt(x);
}

double
DmpAlg::vl(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0)
    return 0.0;
  else if (t1 <= dt_)
    return vl0(t1) / dt_;
  else
    return (vl0(t1) - vl0(t1 - dt_)) / dt_;
}

double
DmpAlg::dVlDt(double t)
{
  double t1 = t - t0_;
  if (t1 <= 0)
    return 0.0;
  else if (t1 <= dt_)
    return dvl0dt(t1) / dt_;
  else
    return (dvl0dt(t1) - dvl0dt(t1 - dt_)) / dt_;
}

void
DmpAlg::showVl()
{
  report_->reportLine("  t    vl(t)");
  double ub = vlCrossingUpperBound();
  for (double t = t0_; t < t0_ + ub * 2.0; t += ub / 10.0)
    report_->reportLine(" %g %g", t, vl(t));
}

void
DmpAlg::fail(const char *reason)
{
  // Report failures with a unique debug flag.
  if (debug_->check("dmp_ceff", 1) || debug_->check("dcalc_error", 1))
    report_->reportLine("delay_calc: DMP failed - %s c2=%s rpi=%s c1=%s rd=%s",
                        reason,
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
  virtual const char *name() { return "cap"; }
  virtual void init(const LibertyLibrary *library,
		    const LibertyCell *drvr_cell,
		    const Pvt *pvt,
		    const GateTableModel *gate_model,
		    const RiseFall *rf,
		    double rd,
		    double in_slew,
		    float related_out_cap,
		    double c2,
		    double rpi,
		    double c1);
  virtual void gateDelaySlew(double &delay,
			     double &slew);
  virtual void loadDelaySlew(const Pin *,
			     double elmore,
			     ArcDelay &delay,
			     Slew &slew);
  virtual void evalDmpEqns();
  virtual double voCrossingUpperBound();

private:
  virtual double v0(double t);
  virtual double dv0dt(double t);
  virtual double vl0(double t);
  virtual double dvl0dt(double t);
};

DmpCap::DmpCap(StaState *sta):
  DmpAlg(1, sta)
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
	     float related_out_cap,
	     double c2,
	     double rpi,
	     double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP cap");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf,
	       rd, in_slew, related_out_cap, c2, rpi, c1);
  ceff_ = c1 + c2;
}

void
DmpCap::gateDelaySlew(double &delay,
		      double &slew)
{
  debugPrint(debug_, "dmp_ceff", 3, "    ceff = %s",
             units_->capacitanceUnit()->asString(ceff_));
  gateCapDelaySlew(ceff_, delay, slew);
  drvr_slew_ = slew;
}

void
DmpCap::loadDelaySlew(const Pin *,
		      double elmore,
		      ArcDelay &delay,
		      Slew &slew)
{
  delay = elmore;
  slew = drvr_slew_;
}

void
DmpCap::evalDmpEqns()
{
}

double
DmpCap::v0(double)
{
  return 0.0;
}

double
DmpCap::dv0dt(double)
{
  return 0.0;
}

double
DmpCap::voCrossingUpperBound()
{
  return 0.0;
}

double
DmpCap::vl0(double)
{
  return 0.0;
}

double
DmpCap::dvl0dt(double)
{
  return 0.0;
}

////////////////////////////////////////////////////////////////

// No non-zero pi model parameters, two poles, one zero
class DmpPi : public DmpAlg
{
public:
  DmpPi(StaState *sta);
  virtual const char *name() { return "Pi"; }
  virtual void init(const LibertyLibrary *library,
		    const LibertyCell *drvr_cell,
		    const Pvt *pvt,
		    const GateTableModel *gate_model,
		    const RiseFall *rf,
		    double rd,
		    double in_slew,
		    float related_out_cap,
		    double c2,
		    double rpi,
		    double c1);
  virtual void gateDelaySlew(double &delay,
			     double &slew);
  virtual void evalDmpEqns();
  virtual double voCrossingUpperBound();

private:
  void findDriverParamsPi();
  virtual double v0(double t);
  virtual double dv0dt(double t);
  double ipiIceff(double t0,
		  double dt,
		  double ceff_time,
		  double ceff);
  virtual double vl0(double t);
  virtual double dvl0dt(double t);

  // Poles/zero.
  double p1_;
  double p2_;
  double z1_;
  // Residues.
  double k0_;
  double k1_;
  double k2_;
  double k3_;
  double k4_;
  // Ipi coefficients.
  double A_;
  double B_;
  double D_;
};

DmpPi::DmpPi(StaState *sta) :
  DmpAlg(3, sta),
  p1_(0.0),
  p2_(0.0),
  z1_(0.0),
  k0_(0.0),
  k1_(0.0),
  k2_(0.0),
  k3_(0.0),
  k4_(0.0),
  A_(0.0),
  B_(0.0),
  D_(0.0)
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
	    float related_out_cap,
	    double c2,
	    double rpi,
	    double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP Pi");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd,
	       in_slew, related_out_cap, c2, rpi, c1);

  // Find poles/zeros.
  z1_ = 1.0 / (rpi_ * c1_);
  k0_ = 1.0 / (rd_ * c2_);
  double a = rpi_ * rd_ * c1_ * c2_;
  double b = rd_ * (c1_ + c2_) + rpi_ * c1_;
  double sqrt_ = sqrt(b * b - 4 * a);
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

void
DmpPi::gateDelaySlew(double &delay,
		     double &slew)
{
  driver_valid_ = false;
  try {
    findDriverParamsPi();
    ceff_ = x_[DmpParam::ceff];
    double table_delay, table_slew;
    gateCapDelaySlew(ceff_, table_delay, table_slew);
    delay = table_delay;
    //slew = table_slew;
    try {
      double vo_delay, vo_slew;
      findDriverDelaySlew(vo_delay, vo_slew);
      driver_valid_ = true;
      // Save Vo delay to measure load wire delay waveform.
      vo_delay_ = vo_delay;
      //delay = vo_delay;
      slew = vo_slew;
    }
    catch (DmpError &error) {
      fail(error.what());
      // Fall back to table slew.
      slew = table_slew;
    }
  }
  catch (DmpError &error) {
    fail(error.what());
    // Driver calculation failed - use Ceff=c1+c2.
    ceff_ = c1_ + c2_;
    gateCapDelaySlew(ceff_, delay, slew);
  }
  drvr_slew_ = slew;
}

void
DmpPi::findDriverParamsPi()
{
  try {
    findDriverParams(c2_ + c1_);
  }
  catch (DmpError &) {
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

  double t_vth, t_vl, slew;
  gateDelays(ceff, t_vth, t_vl, slew);
  if (slew == 0.0)
    throw DmpError("eqn eval failed: slew = 0");

  double ceff_time = slew / (vh_ - vl_);
  if (ceff_time > 1.4 * dt)
    ceff_time = 1.4 * dt;

  if (dt <= 0.0)
    throw DmpError("eqn eval failed: dt < 0");

  double exp_p1_dt = exp2(-p1_ * dt);
  double exp_p2_dt = exp2(-p2_ * dt);
  double exp_dt_rd_ceff = exp2(-dt / (rd_ * ceff));

  double y50 = y(t_vth, t0, dt, ceff);
  // Match Vl.
  double y20 = y(t_vl, t0, dt, ceff);
  fvec_[DmpFunc::ipi] = ipiIceff(t0, dt, ceff_time, ceff);
  fvec_[DmpFunc::y50] = y50 - vth_;
  fvec_[DmpFunc::y20] = y20 - vl_;
  fjac_[DmpFunc::ipi][DmpParam::t0] = 0.0;
  fjac_[DmpFunc::ipi][DmpParam::dt] =
    (-A_ * dt + B_ * dt * exp_p1_dt - (2 * B_ / p1_) * (1.0 - exp_p1_dt)
     + D_ * dt * exp_p2_dt - (2 * D_ / p2_) * (1.0 - exp_p2_dt)
     + rd_ * ceff * (dt + dt * exp_dt_rd_ceff
		     - 2 * rd_ * ceff * (1.0 - exp_dt_rd_ceff)))
    / (rd_ * dt * dt * dt);
  fjac_[DmpFunc::ipi][DmpParam::ceff] =
    (2 * rd_ * ceff - dt - (2 * rd_ * ceff + dt) * exp2(-dt / (rd_ * ceff)))
    / (dt * dt);

  dy(t_vl, t0, dt, ceff,
     fjac_[DmpFunc::y20][DmpParam::t0],
     fjac_[DmpFunc::y20][DmpParam::dt],
     fjac_[DmpFunc::y20][DmpParam::ceff]);

  dy(t_vth, t0, dt, ceff,
     fjac_[DmpFunc::y50][DmpParam::t0],
     fjac_[DmpFunc::y50][DmpParam::dt],
     fjac_[DmpFunc::y50][DmpParam::ceff]);

  if (debug_->check("dmp_ceff", 4)) {
    showX();
    showFvec();
    showJacobian();
    report_->reportLine(".................");
  }
}

// Eqn 13, Eqn 14.
double
DmpPi::ipiIceff(double, double dt,
		double ceff_time,
		double ceff)
{
  double exp_p1_dt = exp2(-p1_ * ceff_time);
  double exp_p2_dt = exp2(-p2_ * ceff_time);
  double exp_dt_rd_ceff = exp2(-ceff_time / (rd_ * ceff));
  double ipi = (A_ * ceff_time + (B_ / p1_) * (1.0 - exp_p1_dt)
	       + (D_ / p2_) * (1.0 - exp_p2_dt))
    / (rd_ * ceff_time * dt);
  double iceff = (rd_ * ceff * ceff_time - (rd_ * ceff) * (rd_ * ceff)
		  * (1.0 - exp_dt_rd_ceff))
    / (rd_ * ceff_time * dt);
  return ipi - iceff;
}

double
DmpPi::v0(double t)
{
  return k0_ * (k1_ + k2_ * t + k3_ * exp2(-p1_ * t) + k4_ * exp2(-p2_ * t));
}

double
DmpPi::dv0dt(double t)
{
  return k0_ * (k2_ - k3_ * p1_ * exp2(-p1_ * t) - k4_ * p2_ * exp2(-p2_ * t));
}

double
DmpPi::vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D4 = -p3_ * k0_ * k4_ / (p2_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_)
		    + p3_ * k4_ / (p2_ - p3_));
  return D1 + t + D3 * exp2(-p1_ * t) + D4 * exp2(-p2_ * t) + D5 * exp2(-p3_ * t);
}

double
DmpPi::voCrossingUpperBound()
{
  return t0_ + dt_ + (c1_ + c2_) * (rd_ + rpi_) * 2.0;
}

double
DmpPi::dvl0dt(double t)
{
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D4 = -p3_ * k0_ * k4_ / (p2_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_)
		    + p3_ * k4_ / (p2_ - p3_));
  return 1.0 - D3 * p1_ * exp2(-p1_ * t) - D4 * p2_ * exp2(-p2_ * t)
    - D5 * p3_ * exp2(-p3_ * t);
}

////////////////////////////////////////////////////////////////

// Capacitive load, so Ceff is known.
// Solve for t0, delta t.
class DmpOnePole : public DmpAlg
{
public:
  DmpOnePole(StaState *sta);
  virtual void evalDmpEqns();
  virtual double voCrossingUpperBound();
};

DmpOnePole::DmpOnePole(StaState *sta) :
  DmpAlg(2, sta)
{
}

void
DmpOnePole::evalDmpEqns()
{
  double t0 = x_[DmpParam::t0];
  double dt = x_[DmpParam::dt];

  double t_vth, t_vl, ignore1, ignore2;
  gateDelays(ceff_, t_vth, t_vl, ignore1);

  if (dt <= 0.0)
    dt = x_[DmpParam::dt] = (t_vl - t_vth) / 100;

  fvec_[DmpFunc::y50] = y(t_vth, t0, dt, ceff_) - vth_;
  fvec_[DmpFunc::y20] = y(t_vl, t0, dt, ceff_) - vl_;

  if (debug_->check("dmp_ceff", 4)) {
    showX();
    showFvec();
  }

  dy(t_vl, t0, dt, ceff_,
     fjac_[DmpFunc::y20][DmpParam::t0],
     fjac_[DmpFunc::y20][DmpParam::dt],
     ignore2);

  dy(t_vth, t0, dt, ceff_,
     fjac_[DmpFunc::y50][DmpParam::t0],
     fjac_[DmpFunc::y50][DmpParam::dt],
     ignore2);

  if (debug_->check("dmp_ceff", 4)) {
    showJacobian();
    report_->reportLine(".................");
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
  virtual const char *name() { return "c2=0"; }
  virtual void init(const LibertyLibrary *drvr_library,
		    const LibertyCell *drvr_cell,
		    const Pvt *pvt,
		    const GateTableModel *gate_model,
		    const RiseFall *rf,
		    double rd,
		    double in_slew,
		    float related_out_cap,
		    double c2,
		    double rpi,
		    double c1);
  virtual void gateDelaySlew(double &delay,
			     double &slew);

private:
  virtual double v0(double t);
  virtual double dv0dt(double t);
  virtual double vl0(double t);
  virtual double dvl0dt(double t);
  virtual double voCrossingUpperBound();

  // Pole/zero.
  double p1_;
  double z1_;
  // Residues.
  double k0_;
  double k1_;
  double k2_;
  double k3_;
};

DmpZeroC2::DmpZeroC2(StaState *sta) :
  DmpOnePole(sta),
  p1_(0.0),
  z1_(0.0),
  k0_(0.0),
  k1_(0.0),
  k2_(0.0),
  k3_(0.0)
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
		float related_out_cap,
		double c2,
		double rpi,
		double c1)
{
  debugPrint(debug_, "dmp_ceff", 3, "Using DMP C2=0");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd,
	       in_slew, related_out_cap, c2, rpi, c1);
  ceff_ = c1;

  z1_ = 1.0 / (rpi_ * c1_);
  p1_ = 1.0 / (c1_ * (rd_ + rpi_));

  k0_ = p1_ / z1_;
  k2_ = 1.0 / k0_;
  k1_ = (p1_ - z1_) / (p1_ * p1_);
  k3_ = -k1_;
}

void
DmpZeroC2::gateDelaySlew(double &delay,
			 double &slew)
{
  try {
    findDriverParams(c1_);
    ceff_ = c1_;
    findDriverDelaySlew(delay, slew);
    driver_valid_ = true;
    vo_delay_ = delay;
  }
  catch (DmpError &error) {
    fail(error.what());
    // Fall back to table slew.
    driver_valid_ = false;
    ceff_ = c1_;
    gateCapDelaySlew(ceff_, delay, slew);
  }
  drvr_slew_ = slew;
}

double
DmpZeroC2::v0(double t)
{
  return k0_ * (k1_ + k2_ * t + k3_ * exp2(-p1_ * t));
}

double
DmpZeroC2::dv0dt(double t)
{
  return k0_ * (k2_ - k3_ * p1_ * exp2(-p1_ * t));
}

double
DmpZeroC2::vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_));
  return D1 + t + D3 * exp2(-p1_ * t) + D5 * exp2(-p3_ * t);
}

double
DmpZeroC2::dvl0dt(double t)
{
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_));
  return 1.0 - D3 * p1_ * exp2(-p1_ * t) - D5 * p3_ * exp2(-p3_ * t);
}

double
DmpZeroC2::voCrossingUpperBound()
{
  return t0_ + dt_ + c1_ * (rd_ + rpi_) * 2.0;
}

////////////////////////////////////////////////////////////////

// Find the root of a function between x1 and x2 using a combination
// of Newton-Raphson and bisection search.
// x_tol is a percentage that change in x must be less than (1.0 = 100%).
// error is non-null if a problem occurs.
static double
findRoot(void (*func)(void *state, double x, double &y, double &dy),
	 void *state,
	 double x1,
	 double x2,
	 double x_tol,
	 int max_iter)
{
  double y1, y2, dy;
  func(state, x1, y1, dy);
  func(state, x2, y2, dy);

  if ((y1 > 0.0 && y2 > 0.0) || (y1 < 0.0 && y2 < 0.0))
    throw DmpError("findRoot: initial bounds do not surround a root");

  if (y1 == 0.0)
    return x1;

  if (y2 == 0.0)
    return x2;

  if (y1 > 0.0) {
    // Swap x1/x2 so func(x1) < 0.
    double tmp = x1;
    x1 = x2;
    x2 = tmp;
  }
  double root = (x1 + x2) * 0.5;
  double dx_prev = abs(x2 - x1);
  double dx = dx_prev;
  double y;
  func(state, root, y, dy);
  for (int iter = 0; iter < max_iter; iter++) {
    // Newton/raphson out of range.
    if ((((root - x2) * dy - y) * ((root - x1) * dy - y) > 0.0)
	// Not decreasing fast enough.
	|| (abs(2.0 * y) > abs(dx_prev * dy))) {
      // Bisect x1/x2 interval.
      dx_prev = dx;
      dx = (x2 - x1) * 0.5;
      root = x1 + dx;
    }
    else {
      dx_prev = dx;
      dx = y / dy;
      root -= dx;
    }
    if (abs(dx) <= x_tol * abs(root))
      // Converged.
      return root;

    func(state, root, y, dy);
    if (y < 0.0)
      x1 = root;
    else
      x2 = root;
  }
  throw DmpError("findRoot: max iterations exceeded");
}

// Newton-Raphson iteration to find zeros of a function.
// x_tol is percentage that all changes in x must be less than (1.0 = 100%).
// Eval(state) is called to fill fvec and fjac (returns false if fails).
// Return error msg on failure.
static void
newtonRaphson(const int max_iter,
	      double x[],
	      const int size,
	      const double x_tol,
	      void (*eval)(void *state),
	      void *state,
	      // Temporaries supplied by caller.
	      double *fvec,
	      double **fjac,
	      int *index,
	      double *p,
	      double *scale)
{
  for (int k = 0; k < max_iter; k++) {
    eval(state);
    for (int i = 0; i < size; i++)
      // Right-hand side of linear equations.
      p[i] = -fvec[i];
    luDecomp(fjac, size, index, scale);
    luSolve(fjac, size, index, p);

    bool all_under_x_tol = true;
    for (int i = 0; i < size; i++) {
      if (abs(p[i]) > abs(x[i]) * x_tol)
	all_under_x_tol = false;
      x[i] += p[i];
    }
    if (all_under_x_tol)
      return;
  }
  throw DmpError("Newton-Raphson max iterations exceeded");
}

// luDecomp, luSolve based on MatClass from C. R. Birchenhall,
// University of Manchester
// ftp://ftp.mcc.ac.uk/pub/matclass/libmat.tar.Z

// Crout's Method of LU decomposition of square matrix, with implicit
// partial pivoting.  A is overwritten. U is explicit in the upper
// triangle and L is in multiplier form in the subdiagionals i.e. subdiag
// a[i,j] is the multiplier used to eliminate the [i,j] term.
//
// Replaces a[0..size-1][0..size-1] by the LU decomposition.
// index[0..size-1] is an output vector of the row permutations.
// Return error msg on failure.
void
luDecomp(double **a,
	 const int size,
	 int *index,
	 // Temporary supplied by caller.
	 // scale stores the implicit scaling of each row.
	 double *scale)
{
  // Find implicit scaling factors.
  for (int i = 0; i < size; i++) {
    double big = 0.0;
    for (int j = 0; j < size; j++) {
      double temp = abs(a[i][j]);
      if (temp > big)
	big = temp;
    }
    if (big == 0.0)
      throw DmpError("LU decomposition: no non-zero row element");
    scale[i] = 1.0 / big;
  }
  int size_1 = size - 1;
  for (int j = 0; j < size; j++) {
    // Run down jth column from top to diag, to form the elements of U.
    for (int i = 0; i < j; i++) {
      double sum = a[i][j];
      for (int k = 0; k < i; k++)
	sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
    }
    // Run down jth subdiag to form the residuals after the elimination
    // of the first j-1 subdiags.  These residuals divided by the
    // appropriate diagonal term will become the multipliers in the
    // elimination of the jth. subdiag. Find index of largest scaled
    // term in imax.
    double big = 0.0;
    int imax = 0;
    for (int i = j; i < size; i++) {
      double sum = a[i][j];
      for (int k = 0; k < j; k++)
	sum -= a[i][k] * a[k][j];
      a[i][j] = sum;
      double dum = scale[i] * abs(sum);
      if (dum >= big) {
	big = dum;
	imax = i;
      }
    }
    // Permute current row with imax.
    if (j != imax) {
      // Yes, do so...
      for (int k = 0; k < size; k++) {
	double dum = a[imax][k];
	a[imax][k] = a[j][k];
	a[j][k] = dum;
      }
      scale[imax] = scale[j];
    }
    index[j] = imax;
    // If diag term is not zero divide subdiag to form multipliers.
    if (a[j][j] == 0.0)
      a[j][j] = tiny_double;
    if (j != size_1) {
      double pivot = 1.0 / a[j][j];
      for (int i = j + 1; i < size; i++)
	a[i][j] *= pivot;
    }
  }
}

// Solves the set of size linear equations a*x=b, assuming A is LU form
// but assume b has not been transformed.
//  a[0..size-1] is LU decomposition
// Returns the solution vector x in b.
// a and index are not modified.
void
luSolve(double **a,
	const int size,
	const int *index,
	double b[])
{
  // Transform b allowing for leading zeros.
  int non_zero = -1;
  for (int i = 0; i < size; i++) {
    int iperm = index[i];
    double sum = b[iperm];
    b[iperm] = b[i];
    if (non_zero != -1) {
      for (int j = non_zero; j <= i - 1; j++)
	sum -= a[i][j] * b[j];
    }
    else {
      if (sum != 0.0)
	non_zero = i;
    }
    b[i] = sum;
  }
  // Backsubstitution.
  for (int i = size - 1; i >= 0; i--) {
    double sum = b[i];
    for (int j = i + 1; j < size; j++)
      sum -= a[i][j] * b[j];
    b[i] = sum / a[i][i];
  }
}

#if 0
// Solve:
//  x + y = 5
//  x - y = 1
// x = 3
// y = 2
void
testLuDecomp1()
{
  double a0[2] = {1,  1};
  double a1[2] = {1, -1};
  double *a[2] = {a0, a1};
  int index[2];
  double b[2] = {5, 1};
  double scale[2];
  luDecomp(a, 2, index, scale);
  luSolve(a, 2, index, b);
  printf("x = %f y= %f\n", b[0], b[1]);
}

// Solve
//   x + 2y =  3
//  3x - 4y = 19
// x = 5
// y = -1
void
testLuDecomp2()
{
  double a0[2] = {1,  2};
  double a1[2] = {3, -4};
  double *a[2] = {a0, a1};
  int index[2];
  double b[2] = {3, 19};
  double scale[2];
  luDecomp(a, 2, index, scale);
  luSolve(a, 2, index, b);
  printf("x = %f y= %f\n", b[0], b[1]);
}
#endif

////////////////////////////////////////////////////////////////

bool DmpCeffDelayCalc::unsuppored_model_warned_ = false;

DmpCeffDelayCalc::DmpCeffDelayCalc(StaState *sta) :
  RCDelayCalc(sta),
  dmp_cap_(new DmpCap(sta)),
  dmp_pi_(new DmpPi(sta)),
  dmp_zero_c2_(new DmpZeroC2(sta)),
  dmp_alg_(nullptr)
{
}

DmpCeffDelayCalc::~DmpCeffDelayCalc()
{
  delete dmp_cap_;
  delete dmp_pi_;
  delete dmp_zero_c2_;
}

void
DmpCeffDelayCalc::inputPortDelay(const Pin *port_pin,
				 float in_slew,
				 const RiseFall *rf,
				 const Parasitic *parasitic,
				 const DcalcAnalysisPt *dcalc_ap)
{
  dmp_alg_ = nullptr;
  RCDelayCalc::inputPortDelay(port_pin, in_slew, rf, parasitic, dcalc_ap);
}

void
DmpCeffDelayCalc::gateDelay(const LibertyCell *drvr_cell,
			    const TimingArc *arc,
			    const Slew &in_slew,
			    float load_cap,
			    const Parasitic *drvr_parasitic,
			    float related_out_cap,
			    const Pvt *pvt,
			    const DcalcAnalysisPt *dcalc_ap,
			    // Return values.
			    ArcDelay &gate_delay,
			    Slew &drvr_slew)
{
  input_port_ = false;
  drvr_rf_ = arc->toEdge()->asRiseFall();
  drvr_library_ = drvr_cell->libertyLibrary();
  drvr_parasitic_ = drvr_parasitic;
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  GateTableModel *table_model = dynamic_cast<GateTableModel*>(model);
  if (table_model && drvr_parasitic) {
    float in_slew1 = delayAsFloat(in_slew);
    float c2, rpi, c1;
    parasitics_->piModel(drvr_parasitic, c2, rpi, c1);
    if (isnan(c2) || isnan(c1) || isnan(rpi))
      report_->error(618, "parasitic Pi model has NaNs.");
    setCeffAlgorithm(drvr_library_, drvr_cell, pvt, table_model,
		     drvr_rf_, in_slew1, related_out_cap,
		     c2, rpi, c1);
    double dmp_gate_delay, dmp_drvr_slew;
    gateDelaySlew(dmp_gate_delay, dmp_drvr_slew);
    gate_delay = dmp_gate_delay;
    drvr_slew = dmp_drvr_slew;
  }
  else {
    LumpedCapDelayCalc::gateDelay(drvr_cell, arc, in_slew, load_cap,
				  drvr_parasitic, related_out_cap, pvt,
				  dcalc_ap, gate_delay, drvr_slew);
    if (drvr_parasitic
	&& !unsuppored_model_warned_) {
      unsuppored_model_warned_ = true;
      report_->warn(1, "cell %s delay model not supported on SPF parasitics by DMP delay calculator",
		    drvr_cell->name());
    }
  }
  drvr_slew_ = drvr_slew;
  multi_drvr_slew_factor_ = 1.0F;
}

void
DmpCeffDelayCalc::setCeffAlgorithm(const LibertyLibrary *drvr_library,
				   const LibertyCell *drvr_cell,
				   const Pvt *pvt,
				   const GateTableModel *gate_model,
				   const RiseFall *rf,
				   double in_slew,
				   float related_out_cap,
				   double c2,
				   double rpi,
				   double c1)
{
  double rd = 0.0;
  if (gate_model) {
    rd = gateModelRd(drvr_cell, gate_model, rf, in_slew, c2, c1,
		     related_out_cap, pvt, pocv_enabled_);
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
  dmp_alg_->init(drvr_library, drvr_cell, pvt, gate_model,
		 drvr_rf_, rd, in_slew, related_out_cap, c2, rpi, c1);
  debugPrint(debug_, "dmp_ceff", 3,
             "    DMP in_slew = %s c2 = %s rpi = %s c1 = %s Rd = %s (%s alg)",
             units_->timeUnit()->asString(in_slew),
             units_->capacitanceUnit()->asString(c2),
             units_->resistanceUnit()->asString(rpi),
             units_->capacitanceUnit()->asString(c1),
             units_->resistanceUnit()->asString(rd),
             dmp_alg_->name());
}

float
DmpCeffDelayCalc::ceff(const LibertyCell *drvr_cell,
		       const TimingArc *arc,
		       const Slew &in_slew,
		       float load_cap,
		       const Parasitic *drvr_parasitic,
		       float related_out_cap,
		       const Pvt *pvt,
		       const DcalcAnalysisPt *dcalc_ap)
{
  ArcDelay gate_delay;
  Slew drvr_slew;
  gateDelay(drvr_cell, arc, in_slew, load_cap,
	    drvr_parasitic, related_out_cap, pvt, dcalc_ap,
	    gate_delay, drvr_slew);
  if (dmp_alg_)
    return dmp_alg_->ceff();
  else
    return load_cap;
}

string
DmpCeffDelayCalc::reportGateDelay(const LibertyCell *drvr_cell,
				  const TimingArc *arc,
				  const Slew &in_slew,
				  float load_cap,
				  const Parasitic *drvr_parasitic,
				  float related_out_cap,
				  const Pvt *pvt,
				  const DcalcAnalysisPt *dcalc_ap,
				  int digits)
{
  ArcDelay gate_delay;
  Slew drvr_slew;
  gateDelay(drvr_cell, arc, in_slew, load_cap,
	    drvr_parasitic, related_out_cap, pvt, dcalc_ap,
	    gate_delay, drvr_slew);
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  float c_eff = 0.0;
  string result;
  if (drvr_parasitic_ && dmp_alg_) {
    c_eff = dmp_alg_->ceff();
    const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
    const Units *units = drvr_library->units();
    const Unit *cap_unit = units->capacitanceUnit();
    const Unit *res_unit = units->resistanceUnit();
    float c2, rpi, c1;
    parasitics_->piModel(drvr_parasitic_, c2, rpi, c1);
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
    result += model->reportGateDelay(drvr_cell, pvt, in_slew1, c_eff,
                                     related_out_cap, pocv_enabled_, digits);
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
	    float related_out_cap,
	    const Pvt *pvt,
	    bool pocv_enabled)
{
  float cap1 = c1 + c2;
  float cap2 = cap1 + 1e-15;
  ArcDelay d1, d2;
  Slew s1, s2;
  gate_model->gateDelay(cell, pvt, in_slew, cap1, related_out_cap, pocv_enabled,
			d1, s1);
  gate_model->gateDelay(cell, pvt, in_slew, cap2, related_out_cap, pocv_enabled,
			d2, s2);
  double vth = cell->libertyLibrary()->outputThreshold(rf);
  float rd = -log(vth) * abs(delayAsFloat(d1) - delayAsFloat(d2)) / (cap2 - cap1);
  return rd;
}

void
DmpCeffDelayCalc::gateDelaySlew(double &delay,
				double &slew)
{
  dmp_alg_->gateDelaySlew(delay, slew);
}

void
DmpCeffDelayCalc::loadDelaySlew(const Pin *load_pin,
				double elmore,
				ArcDelay &delay,
				Slew &slew)
{
  if (dmp_alg_)
    dmp_alg_->loadDelaySlew(load_pin, elmore, delay, slew);
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

DmpError::DmpError(const char *what) :
  what_(what)
{
  //printf("DmpError %s\n", what);
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

} // namespace
