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

// "Performance Computation for Precharacterized CMOS Gates with RC Loads",
// Florentin Dartu, Noel Menezes and Lawrence Pileggi, IEEE Transactions
// on Computer-Aided Design of Integrated Circuits and Systems, Vol 15, No 5,
// May 1996, pg 544-553.
//
// The only real change from the paper is that Vl, the measured low
// slew voltage is matched instead of y20 in eqn 12.

#include <algorithm> // abs, min
#include <math.h>    // sqrt
#include "Machine.hh"
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
#include "DmpCeff.hh"

namespace sta {

using std::abs;
using std::min;

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
gateModelRd(const LibertyCell *cell,
	    GateTableModel *gate_model,
	    double in_slew,
	    double c2,
	    double c1,
	    float related_out_cap,
	    const Pvt *pvt,
	    bool pocv_enabled);
static bool
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
	 int max_iter,
	 const char *&error);
static bool
newtonRaphson(const int max_iter,
	      double x[],
	      const int n,
	      const double x_tol,
	      // eval(state) is called to fill fvec and fjac.
	      // Returns false if fails.
	      bool (*eval)(void *state),
	      void *state,
	      // Temporaries supplied by caller.
	      double *fvec,
	      double **fjac,
	      int *index,
	      double *p,
	      double *scale,
	      const char *&error);
static void
luSolve(double **a,
	const int size,
	const int *index,
	double b[]);
static bool
luDecomp(double **a,
	 const int size,
	 int *index,
	 double *scale,
	 const char *&error);

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
		    double c1) = 0;
  virtual void gateDelaySlew(double &delay,
			     double &slew) = 0;
  virtual void loadDelaySlew(const Pin *load_pin,
			     double elmore,
			     ArcDelay &delay,
			     Slew &slew);
  double ceff() { return ceff_; }

  // Given x_ as a vector of input parameters, fill fvec_ with the
  // equations evaluated at x_ and fjac_ with the jabobian evaluated at x_.
  virtual bool evalDmpEqns() = 0;
  // Output response to vs(t) ramp driving pi model load.
  double vo(double t);
  double dVoDt(double t);
  // Load responce to driver waveform.
  double vl(double t);
  double dVlDt(double t);
  double vCross() { return v_cross_; }

protected:
  void init(const LibertyLibrary *library,
	    const LibertyCell *drvr_cell,
	    const Pvt *pvt,
	    const GateTableModel *gate_model,
	    const RiseFall *rf,
	    double rd,
	    double in_slew,
	    float related_out_cap);
  // Find driver parameters t0, delta_t, Ceff.
  bool findDriverParams(double &ceff);
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
  bool findDriverDelaySlew(double &delay,
			   double &slew);
  bool findVoCrossing(double vth,
		      double &t);
  void showVo();
  bool findVlCrossing(double vth,
		      double &t);
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

  // Gate slew used to check load delay.
  double gate_slew_;
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
	     float related_out_cap)
{
  drvr_library_ = drvr_library;
  drvr_cell_ = drvr_cell;
  pvt_ = pvt;
  gate_model_ = gate_model;
  rd_ = rd;
  in_slew_ = in_slew;
  related_out_cap_ = related_out_cap;
  driver_valid_ = false;
  vth_ = drvr_library->outputThreshold(rf);
  vl_ = drvr_library->slewLowerThreshold(rf);
  vh_ = drvr_library->slewUpperThreshold(rf);
  slew_derate_ = drvr_library->slewDerateFromLibrary();
}

// Find Ceff, delta_t and t0 for the driver.
// Caller must initialize/retrieve x_[DmpParam::ceff] because
// order 2 eqns don't have a ceff eqn.
// Return true if successful.
bool
DmpAlg::findDriverParams(double &ceff)
{
  double t_vth, t_vl, slew;
  gateDelays(ceff, t_vth, t_vl, slew);
  double dt = slew / (vh_ - vl_);
  double t0 = t_vth + log(1.0 - vth_) * rd_ * ceff - vth_ * dt;
  x_[DmpParam::dt] = dt;
  x_[DmpParam::t0] = t0;
  const char *nr_error;
  if (newtonRaphson(100, x_, nr_order_, driver_param_tol, evalDmpEqnsState,
		    this, fvec_, fjac_, index_, p_, scale_, nr_error)) {
    t0_ = x_[DmpParam::t0];
    dt_ = x_[DmpParam::dt];
    debugPrint3(debug_, "delay_calc", 3, "    t0 = %s dt = %s ceff = %s\n",
		units_->timeUnit()->asString(t0_),
		units_->timeUnit()->asString(dt_),
		units_->capacitanceUnit()->asString(x_[DmpParam::ceff]));
    if (debug_->check("delay_calc", 4))
      showVo();
    return true;
  }
  else {
    fail(nr_error);
    return false;
  }
}

static bool
evalDmpEqnsState(void *state)
{
  DmpAlg *alg = reinterpret_cast<DmpAlg *>(state);
  return alg->evalDmpEqns();
}

void
DmpAlg::gateCapDelaySlew(double ceff,
			 double &delay,
			 double &slew)
{
  ArcDelay model_delay;
  Slew model_slew;
  gate_model_->gateDelay(drvr_cell_, pvt_,
			 static_cast<float>(in_slew_),
			 static_cast<float>(ceff),
			 related_out_cap_,
			 pocv_enabled_,
			 model_delay, model_slew);
  delay = delayAsFloat(model_delay);
  slew = delayAsFloat(model_slew);
}

void
DmpAlg::gateDelays(double ceff,
		   double &t_vth,
		   double &t_vl,
		   double &slew)
{
  gateCapDelaySlew(ceff, t_vth, slew);
  // Gate table slew is reported.  Convert to measured.
  slew *= slew_derate_;
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
  return t - rd_ * cl * (1.0 - exp(-t / (rd_ * cl)));
}

void
DmpAlg::dy(double t,
	   double t0,
	   double dt,
	   double cl,
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
  return 1.0 - exp(-t / (rd_ * cl));
}

double
DmpAlg::y0dcl(double t,
	      double cl)
{
  return rd_ * ((1.0 + t / (rd_ * cl)) * exp(-t / (rd_ * cl)) - 1);
}

void
DmpAlg::showX()
{
  for (int i = 0; i < nr_order_; i++)
    debug_->print("%4s %12.3e\n", dmp_param_index_strings[i], x_[i]);
}

void
DmpAlg::showFvec()
{
  for (int i = 0; i < nr_order_; i++)
    debug_->print("%4s %12.3e\n", dmp_func_index_strings[i], fvec_[i]);
}

void
DmpAlg::showJacobian()
{
  debug_->print("    ");
  for (int j = 0; j < nr_order_; j++)
    debug_->print("%12s", dmp_param_index_strings[j]);
  debug_->print("\n");
  for (int i = 0; i < nr_order_; i++) {
    debug_->print("%4s ", dmp_func_index_strings[i]);
    for (int j = 0; j < nr_order_; j++)
      debug_->print("%12.3e ", fjac_[i][j]);
    debug_->print("\n");
  }
}

// Return true if successful.
bool
DmpAlg::findDriverDelaySlew(double &delay,
			    double &slew)
{
  double tl, th;
  if (findVoCrossing(vth_, delay)
      && findVoCrossing(vl_, tl)
      && findVoCrossing(vh_, th)) {
    slew = (th - tl) / slew_derate_;
    return true;
  }
  else
    return false;
}

// Find t such that vo(t)=v.
// Return true if successful.
bool
DmpAlg::findVoCrossing(double vth,
		       double &t)
{
  v_cross_ = vth;
  double ub = voCrossingUpperBound();
  const char *error;
  t = findRoot(evalVoEqns, this, t0_, ub, vth_time_tol, find_root_max_iter,
	       error);
  if (error)
    fail(error);
  return (error == 0);
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
  debug_->print("  t    vo(t)\n");
  double ub = voCrossingUpperBound();
  for (double t = t0_; t < t0_ + ub; t += dt_ / 10.0)
    debug_->print(" %g %g\n", t, vo(t));
  // debug_->print(" %.3g %.3g", t-t0_, vo(t)*3);
  // debug_->print("\n");
}

void
DmpAlg::loadDelaySlew(const Pin *,
		      double elmore,
		      ArcDelay &delay,
		      Slew &slew)
{
  if (elmore == 0.0) {
    delay = 0.0;
    slew = static_cast<float>(gate_slew_);
  }
  else if (elmore < gate_slew_ * 1e-3) {
    // Elmore delay is small compared to driver slew.
    delay = static_cast<float>(elmore);
    slew = static_cast<float>(gate_slew_);
  }
  else {
    elmore_ = elmore;
    p3_ = 1.0 / elmore;
    if (driver_valid_
	&& debug_->check("delay_calc", 4))
      showVl();
    // Use the driver thresholds and rely on thresholdAdjust to
    // convert the delay and slew to the load's thresholds.
    double tl, th, load_delay;
    if (driver_valid_
	&& findVlCrossing(vth_, load_delay)
	&& findVlCrossing(vl_, tl)
	&& findVlCrossing(vh_, th)) {
      // Measure delay from Vo, the load dependent source excitation.
      double delay1 = load_delay - vo_delay_;
      double slew1 = (th - tl) / slew_derate_;
      if (delay1 < 0.0) {
	// Only report a problem if the difference is significant.
	if (-delay1 > vth_time_tol * vo_delay_)
	  fail("load delay less than zero\n");
	// Use elmore delay.
	delay1 = 1.0 / p3_;
      }
      if (slew1 < gate_slew_) {
	// Only report a problem if the difference is significant.
	if ((gate_slew_ - slew1) > vth_time_tol * gate_slew_)
	  fail("load slew less than driver slew\n");
	slew1 = static_cast<float>(gate_slew_);
      }
      delay = static_cast<float>(delay1);
      slew = static_cast<float>(slew1);
    }
    else {
      // Failed - use elmore delay and driver slew.
      delay = static_cast<float>(elmore_);
      slew = static_cast<float>(gate_slew_);
    }
  }
}

// Find t such that vl(t)=v.
// Return true if successful.
bool
DmpAlg::findVlCrossing(double vth,
		       double &t)
{
  v_cross_ = vth;
  double ub = vlCrossingUpperBound();
  const char *error;
  t = findRoot(evalVlEqns, this, t0_, ub, vth_time_tol, find_root_max_iter,
	       error);
  if (error)
    fail("findVlCrossing: Vl(t) did not cross threshold\n");
  return (error == 0);
}

double
DmpAlg::vlCrossingUpperBound()
{
  return voCrossingUpperBound() + elmore_;
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
  debug_->print("  t    vl(t)\n");
  double ub = vlCrossingUpperBound();
  for (double t = t0_; t < t0_ + ub * 2.0; t += ub / 10.0)
    debug_->print(" %g %g\n", t, vl(t));
}

void
DmpAlg::fail(const char *reason)
{
  // Allow only failures to be reported with a unique debug flag.
  if (debug_->check("delay_calc", 1) || debug_->check("delay_calc_dmp", 1))
    debug_->print("delay_calc: DMP failed - %s", reason);
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
  virtual bool evalDmpEqns();
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
	     double,
	     double c1)
{
  debugPrint0(debug_, "delay_calc", 3, "Using DMP cap\n");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf,
	       rd, in_slew, related_out_cap);
  ceff_ = c1 + c2;
}

void
DmpCap::gateDelaySlew(double &delay,
		      double &slew)
{
  debugPrint1(debug_, "delay_calc", 3, "    ceff = %s\n",
	      units_->capacitanceUnit()->asString(ceff_));
  gateCapDelaySlew(ceff_, delay, slew);
  gate_slew_ = slew;
}

void
DmpCap::loadDelaySlew(const Pin *,
		      double elmore,
		      ArcDelay &delay,
		      Slew &slew)
{
  delay = static_cast<float>(elmore);
  slew = static_cast<float>(gate_slew_);
}

bool
DmpCap::evalDmpEqns()
{
  return true;
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
  virtual bool evalDmpEqns();
  virtual double voCrossingUpperBound();

private:
  bool findDriverParamsPi();
  virtual double v0(double t);
  virtual double dv0dt(double t);
  double ipiIceff(double t0,
		  double dt,
		  double ceff_time,
		  double ceff);
  virtual double vl0(double t);
  virtual double dvl0dt(double t);

  // Pi model.
  double c1_;
  double c2_;
  double rpi_;
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
  c1_(0.0),
  c2_(0.0),
  rpi_(0.0),
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
  debugPrint0(debug_, "delay_calc", 3, "Using DMP Pi\n");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd,
	       in_slew, related_out_cap);
  c1_ = c1;
  c2_ = c2;
  rpi_ = rpi;

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
  if (findDriverParamsPi()) {
    ceff_ = x_[DmpParam::ceff];
    driver_valid_ = true;
    double table_slew;
    // Table gate delays are more accurate than using Vo waveform delay
    // (-12% max, -5% avg vs 26% max, -7% avg).
    gateCapDelaySlew(ceff_, delay, table_slew);
    // Vo slew is more accurate than table
    // (-8% max, -3% avg vs -32% max, -12% avg).
    // Need Vo delay to measure load wire delay waveform.
    if (!findDriverDelaySlew(vo_delay_, slew))
      // Fall back to table slew if findDriverDelaySlew fails.
      slew = table_slew;
  }
  else {
    // Driver calculation failed - use Ceff=c1+c2.
    ceff_ = c1_ + c2_;
    gateCapDelaySlew(ceff_, delay, slew);
  }
  // Save for wire delay calc.
  gate_slew_ = slew;
}

bool
DmpPi::findDriverParamsPi()
{
  double ceff = c1_ + c2_;
  x_[DmpParam::ceff] = ceff;
  return findDriverParams(x_[DmpParam::ceff]);
}

// Given x_ as a vector of input parameters, fill fvec_ with the
// equations evaluated at x_ and fjac_ with the jacobian evaluated at x_.
bool
DmpPi::evalDmpEqns()
{
  double t0 = x_[DmpParam::t0];
  double dt = x_[DmpParam::dt];
  double ceff = x_[DmpParam::ceff];

  if (ceff > (c1_ + c2_) || ceff < 0.0)
    return false;

  double t_vth, t_vl, slew;
  gateDelays(ceff, t_vth, t_vl, slew);
  double ceff_time = slew / (vh_ - vl_);
  if (ceff_time > 1.4 * dt)
    ceff_time = 1.4 * dt;

  if (dt <= 0.0)
    return false;

  double exp_p1_dt = exp(-p1_ * dt);
  double exp_p2_dt = exp(-p2_ * dt);
  double exp_dt_rd_ceff = exp(-dt / (rd_ * ceff));

  // y50 in the paper.
  double y_t_vth = y(t_vth, t0, dt, ceff);
  // y20 in the paper. Match Vl.
  double y_t_vl = y(t_vl, t0, dt, ceff);
  fvec_[DmpFunc::ipi] = ipiIceff(t0, dt, ceff_time, ceff);
  fvec_[DmpFunc::y50] = y_t_vth - vth_;
  fvec_[DmpFunc::y20] = y_t_vl - vl_;
  fjac_[DmpFunc::ipi][DmpParam::t0] = 0.0;
  fjac_[DmpFunc::ipi][DmpParam::dt] =
    (-A_ * dt + B_ * dt * exp_p1_dt - (2 * B_ / p1_) * (1.0 - exp_p1_dt)
     + D_ * dt * exp_p2_dt - (2 * D_ / p2_) * (1.0 - exp_p2_dt)
     + rd_ * ceff * (dt + dt * exp_dt_rd_ceff
		     - 2 * rd_ * ceff * (1.0 - exp_dt_rd_ceff)))
    / (rd_ * dt * dt * dt);
  fjac_[DmpFunc::ipi][DmpParam::ceff] =
    (2 * rd_ * ceff - dt - (2 * rd_ * ceff + dt) * exp(-dt / (rd_ * ceff)))
    / (dt * dt);

  dy(t_vl, t0, dt, ceff,
     fjac_[DmpFunc::y20][DmpParam::t0],
     fjac_[DmpFunc::y20][DmpParam::dt],
     fjac_[DmpFunc::y20][DmpParam::ceff]);

  dy(t_vth, t0, dt, ceff,
     fjac_[DmpFunc::y50][DmpParam::t0],
     fjac_[DmpFunc::y50][DmpParam::dt],
     fjac_[DmpFunc::y50][DmpParam::ceff]);

  if (debug_->check("delay_calc", 4)) {
    showX();
    showFvec();
    showJacobian();
    debug_->print(".................\n");
  }
  return true;
}

// Eqn 13, Eqn 14.
double
DmpPi::ipiIceff(double, double dt,
		double ceff_time,
		double ceff)
{
  double exp_p1_dt = exp(-p1_ * ceff_time);
  double exp_p2_dt = exp(-p2_ * ceff_time);
  double exp_dt_rd_ceff = exp(-ceff_time / (rd_ * ceff));
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
  return k0_ * (k1_ + k2_ * t + k3_ * exp(-p1_ * t) + k4_ * exp(-p2_ * t));
}

double
DmpPi::dv0dt(double t)
{
  return k0_ * (k2_ - k3_ * p1_ * exp(-p1_ * t) - k4_ * p2_ * exp(-p2_ * t));
}

double
DmpPi::vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D4 = -p3_ * k0_ * k4_ / (p2_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_)
		    + p3_ * k4_ / (p2_ - p3_));
  return D1 + t + D3 * exp(-p1_ * t) + D4 * exp(-p2_ * t) + D5 * exp(-p3_ * t);
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
  return 1.0 - D3 * p1_ * exp(-p1_ * t) - D4 * p2_ * exp(-p2_ * t)
    - D5 * p3_ * exp(-p3_ * t);
}

////////////////////////////////////////////////////////////////

// Capacitive load, so Ceff is known.
// Solve for t0, delta t.
class DmpOnePole : public DmpAlg
{
public:
  DmpOnePole(StaState *sta);
  virtual bool evalDmpEqns();
  virtual double voCrossingUpperBound();
};

DmpOnePole::DmpOnePole(StaState *sta) :
  DmpAlg(2, sta)
{
}

bool
DmpOnePole::evalDmpEqns()
{
  double t0 = x_[DmpParam::t0];
  double dt = x_[DmpParam::dt];

  double t_vth, t_vl, ignore, dummy;
  gateDelays(ceff_, t_vth, t_vl, ignore);

  if (dt <= 0.0)
    dt = x_[DmpParam::dt] = (t_vl - t_vth) / 100;

  fvec_[DmpFunc::y50] = y(t_vth, t0, dt, ceff_) - vth_;
  fvec_[DmpFunc::y20] = y(t_vl, t0, dt, ceff_) - vl_;

  if (debug_->check("delay_calc", 4)) {
    showX();
    showFvec();
  }

  dy(t_vl, t0, dt, ceff_,
     fjac_[DmpFunc::y20][DmpParam::t0],
     fjac_[DmpFunc::y20][DmpParam::dt],
     dummy);

  dy(t_vth, t0, dt, ceff_,
     fjac_[DmpFunc::y50][DmpParam::t0],
     fjac_[DmpFunc::y50][DmpParam::dt],
     dummy);

  if (debug_->check("delay_calc", 4)) {
    showJacobian();
    debug_->print(".................\n");
  }
  return true;
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

  double c1_;
  double rpi_;
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
  c1_(0.0),
  rpi_(0.0),
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
		double,
		double rpi,
		double c1)
{
  debugPrint0(debug_, "delay_calc", 3, "Using DMP C2=0\n");
  DmpAlg::init(drvr_library, drvr_cell, pvt, gate_model, rf, rd,
	       in_slew, related_out_cap);
  ceff_ = c1_ = c1;
  rpi_ = rpi;

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
  driver_valid_ = findDriverParams(c1_);
  ceff_ = c1_;
  if (!findDriverDelaySlew(delay, slew))
    // Fall back to table slew if findDriverDelaySlew fails.
    gateCapDelaySlew(ceff_, delay, slew);
  vo_delay_ = delay;
  gate_slew_ = slew;
}

double
DmpZeroC2::v0(double t)
{
  return k0_ * (k1_ + k2_ * t + k3_ * exp(-p1_ * t));
}

double
DmpZeroC2::dv0dt(double t)
{
  return k0_ * (k2_ - k3_ * p1_ * exp(-p1_ * t));
}

double
DmpZeroC2::vl0(double t)
{
  double D1 = k0_ * (k1_ - k2_ / p3_);
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_));
  return D1 + t + D3 * exp(-p1_ * t) + D5 * exp(-p3_ * t);
}

double
DmpZeroC2::dvl0dt(double t)
{
  double D3 = -p3_ * k0_ * k3_ / (p1_ - p3_);
  double D5 = k0_ * (k2_ / p3_ - k1_ + p3_ * k3_ / (p1_ - p3_));
  return 1.0 - D3 * p1_ * exp(-p1_ * t) - D5 * p3_ * exp(-p3_ * t);
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
	 int max_iter,
	 const char *&error)
{
  double y1, y2, dy;
  func(state, x1, y1, dy);
  func(state, x2, y2, dy);

  if ((y1 > 0.0 && y2 > 0.0) || (y1 < 0.0 && y2 < 0.0)) {
    error = "findRoot: initial bounds do not surround a root\n";
    return 0.0;
  }

  error = 0;
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
  error = "findRoot: max iterations exceeded\n";
  return 0.0;
}

// Newton-Raphson iteration to find zeros of a function.
// x_tol is percentage that all changes in x must be less than (1.0 = 100%).
// Eval(state) is called to fill fvec and fjac (returns false if fails).
// Return true if successful.
static bool
newtonRaphson(const int max_iter,
	      double x[],
	      const int size,
	      const double x_tol,
	      bool (*eval)(void *state),
	      void *state,
	      // Temporaries supplied by caller.
	      double *fvec,
	      double **fjac,
	      int *index,
	      double *p,
	      double *scale,
	      const char *&error)
{
  for (int k = 0; k < max_iter; k++) {
    if (!eval(state)) {
      error = "Newton-Raphson eval failed.\n";
      return false;
    }

    for (int i = 0; i < size; i++)
      // Right-hand side of linear equations.
      p[i] = -fvec[i];
    const char *lu_error;
    if (luDecomp(fjac, size, index, scale, lu_error)) {
      luSolve(fjac, size, index, p);

      bool all_under_x_tol = true;
      for (int i = 0; i < size; i++) {
	if (abs(p[i]) > abs(x[i]) * x_tol)
	  all_under_x_tol = false;
	x[i] += p[i];
      }
      if (all_under_x_tol)
	return true;
    }
    else {
      error = lu_error;
      return false;
    }
  }
  error = "Newton-Raphson max iterations exceeded.\n";
  return false;
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
// Return true if successful.
bool
luDecomp(double **a,
	 const int size,
	 int *index,
	 // Temporary supplied by caller.
	 // scale stores the implicit scaling of each row.
	 double *scale,
	 const char *&error)
{
  // Find implicit scaling factors.
  for (int i = 0; i < size; i++) {
    double big = 0.0;
    for (int j = 0; j < size; j++) {
      double temp = abs(a[i][j]);
      if (temp > big)
	big = temp;
    }
    if (big == 0.0) {
      error = "LU decomposition: no non-zero row element.\n";
      return false;
    }
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
  return true;
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
				 Parasitic *parasitic,
				 const DcalcAnalysisPt *dcalc_ap)
{
  dmp_alg_ = nullptr;
  input_port_ = true;
  RCDelayCalc::inputPortDelay(port_pin, in_slew, rf, parasitic, dcalc_ap);
}

void
DmpCeffDelayCalc::gateDelay(const LibertyCell *drvr_cell,
			    TimingArc *arc,
			    const Slew &in_slew,
			    float load_cap,
			    Parasitic *drvr_parasitic,
			    float related_out_cap,
			    const Pvt *pvt,
			    const DcalcAnalysisPt *dcalc_ap,
			    // Return values.
			    ArcDelay &gate_delay,
			    Slew &drvr_slew)
{
  input_port_ = false;
  drvr_rf_ = arc->toTrans()->asRiseFall();
  drvr_library_ = drvr_cell->libertyLibrary();
  drvr_parasitic_ = drvr_parasitic;
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  GateTableModel *table_model = dynamic_cast<GateTableModel*>(model);
  if (table_model && drvr_parasitic) {
    float in_slew1 = delayAsFloat(in_slew);
    float c2, rpi, c1;
    parasitics_->piModel(drvr_parasitic, c2, rpi, c1);
    setCeffAlgorithm(drvr_library_, drvr_cell, pvt, table_model,
		     in_slew1, related_out_cap,
		     c2, rpi, c1);
    double dmp_gate_delay, dmp_drvr_slew;
    gateDelaySlew(dmp_gate_delay, dmp_drvr_slew);
    gate_delay = static_cast<float>(dmp_gate_delay);
    drvr_slew = static_cast<float>(dmp_drvr_slew);
  }
  else {
    LumpedCapDelayCalc::gateDelay(drvr_cell, arc, in_slew, load_cap,
				  drvr_parasitic, related_out_cap, pvt,
				  dcalc_ap, gate_delay, drvr_slew);
    if (drvr_parasitic
	&& !unsuppored_model_warned_) {
      unsuppored_model_warned_ = true;
      report_->warn("cell %s delay model not supported on SPF parasitics by DMP delay calculator\n",
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
				   GateTableModel *gate_model,
				   double in_slew,
				   float related_out_cap,
				   double c2,
				   double rpi,
				   double c1)
{
  double rd = gate_model
    ? gateModelRd(drvr_cell, gate_model, in_slew, c2, c1,
		  related_out_cap, pvt, pocv_enabled_)
    : 0.0;
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
  dmp_alg_->init(drvr_library, drvr_cell, pvt, gate_model,
		 drvr_rf_, rd, in_slew, related_out_cap, c2, rpi, c1);
  debugPrint6(debug_, "delay_calc", 3,
	      "    DMP in_slew = %s c2 = %s rpi = %s c1 = %s Rd = %s (%s alg)\n",
	      units_->timeUnit()->asString(in_slew),
	      units_->capacitanceUnit()->asString(c2),
	      units_->resistanceUnit()->asString(rpi),
	      units_->capacitanceUnit()->asString(c1),
	      units_->resistanceUnit()->asString(rd),
	      dmp_alg_->name());
}

float
DmpCeffDelayCalc::ceff(const LibertyCell *drvr_cell,
		       TimingArc *arc,
		       const Slew &in_slew,
		       float load_cap,
		       Parasitic *drvr_parasitic,
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
    return static_cast<float>(dmp_alg_->ceff());
  else
    return load_cap;
}

void
DmpCeffDelayCalc::reportGateDelay(const LibertyCell *drvr_cell,
				  TimingArc *arc,
				  const Slew &in_slew,
				  float load_cap,
				  Parasitic *drvr_parasitic,
				  float related_out_cap,
				  const Pvt *pvt,
				  const DcalcAnalysisPt *dcalc_ap,
				  int digits,
				  string *result)
{
  ArcDelay gate_delay;
  Slew drvr_slew;
  gateDelay(drvr_cell, arc, in_slew, load_cap,
	    drvr_parasitic, related_out_cap, pvt, dcalc_ap,
	    gate_delay, drvr_slew);
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  float c_eff = 0.0;
  if (drvr_parasitic_ && dmp_alg_) {
    c_eff = dmp_alg_->ceff();
    const LibertyLibrary *drvr_library = drvr_cell->libertyLibrary();
    const Units *units = drvr_library->units();
    const Unit *cap_unit = units->capacitanceUnit();
    const Unit *res_unit = units->resistanceUnit();
    float c2, rpi, c1;
    parasitics_->piModel(drvr_parasitic_, c2, rpi, c1);
    *result += "Pi model C2=";
    *result += cap_unit->asString(c2, digits);
    *result += " Rpi=";
    *result += res_unit->asString(rpi, digits);
    *result += " C1=";
    *result += cap_unit->asString(c1, digits);
    *result += ", Ceff=";
    *result += cap_unit->asString(c_eff, digits);
    *result += '\n';
  }
  else
    c_eff = load_cap;
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    model->reportGateDelay(drvr_cell, pvt, in_slew1, c_eff,
			   related_out_cap, pocv_enabled_,
			   digits, result);
  }
}

static double
gateModelRd(const LibertyCell *cell,
	    GateTableModel *gate_model,
	    double in_slew,
	    double c2,
	    double c1,
	    float related_out_cap,
	    const Pvt *pvt,
	    bool pocv_enabled)
{
  float cap1 = static_cast<float>((c1 + c2) * .75);
  float cap2 = cap1 * 1.1F;
  float in_slew1 = static_cast<float>(in_slew);
  ArcDelay d1, d2;
  Slew s1, s2;
  gate_model->gateDelay(cell, pvt, in_slew1, cap1, related_out_cap, pocv_enabled,
			d1, s1);
  gate_model->gateDelay(cell, pvt, in_slew1, cap2, related_out_cap, pocv_enabled,
			d2, s2);
  return abs(delayAsFloat(d1) - delayAsFloat(d2)) / (cap2 - cap1);
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

} // namespace
