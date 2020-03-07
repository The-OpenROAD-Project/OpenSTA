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

// (c) 2018 Nefelus, Inc.
//
// Author: W. Scott

#include <stdio.h>
#include <cmath> // abs
#include "Machine.hh"
#include "Report.hh"
#include "Debug.hh"
#include "Units.hh"
#include "Liberty.hh"
#include "TimingModel.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Parasitics.hh"
#include "Sdc.hh"
#include "DcalcAnalysisPt.hh"
#include "DelayCalc.hh"
#include "ArcDelayCalc.hh"
#include "RCDelayCalc.hh"
#include "GraphDelayCalc.hh"
#include "Arnoldi.hh"
#include "ArnoldiReduce.hh"
#include "ArnoldiDelayCalc.hh"

namespace sta {

// wireload8 is n^2
// do not delete arnoldi parasitics
// handle rspf parasitics?

// mv static functions to ArnoldiDelayCalc
// need slew only lookup for
//  ra_delay
//  ra_get_r
//  ra_get_s

using std::abs;

struct delay_work;
struct delay_c;

////////////////////////////////////////////////////////////////

static delay_work *delay_work_create();
static void
delay_work_destroy(delay_work *D);
static double *
delay_work_get_residues(delay_work *D,
			int term_index);

static bool
tridiagEV(int n,double *d,double *e,double *p,double **v);

//////////////////////////////////////////////////////////////

struct delay_c
{
  double slew_derate;
  double vlo;
  double vhi;
  double vlg;
  double smin;
  double x1;
  double y1;
  double vmid;  // falling convention, should be >= 0.5
};

// workspace for pole-residue -> delay calculations
// delay_work
// max order is 32
struct delay_work
{
  double slew_derate;
  double slew_factor;   // (0,1.0] table_slew = slew_factor * full_slew
  delay_c cV[2];
  delay_c *c;

  double lo_thresh;
  double hi_thresh;

  int nmax;
  double poles[32];     // 1/tau
  double **resi;        // resi[jrec][h]  h=0,..order
  double *v[32];
  double *w[32];
  double aa[32];
};

////////////////////////////////////////////////////////////////

class ArnoldiDelayCalc : public RCDelayCalc
{
public:
  ArnoldiDelayCalc(StaState *sta);
  virtual ~ArnoldiDelayCalc();
  virtual ArcDelayCalc *copy();
  virtual Parasitic *findParasitic(const Pin *drvr_pin,
				   const RiseFall *rf,
				   const DcalcAnalysisPt *dcalc_ap);
  virtual void gateDelay(const LibertyCell *drvr_cell,
			 TimingArc *arc,
			 const Slew &in_slew,
			 // Pass in load_cap or drvr_parasitic.
			 float load_cap,
			 Parasitic *drvr_parasitic,
			 float related_out_cap,
			 const Pvt *pvt,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 ArcDelay &gate_delay,
			 Slew &drvr_slew);
  virtual void loadDelay(const Pin *load_pin,
			 // Return values.
			 ArcDelay &wire_delay,
			 Slew &load_slew);
  virtual void inputPortDelay(const Pin *port_pin,
			      float in_slew,
			      const RiseFall *rf,
			      Parasitic *parasitic,
			      const DcalcAnalysisPt *dcalc_ap);
  virtual void reportGateDelay(const LibertyCell *drvr_cell,
			       TimingArc *arc,
			       const Slew &in_slew,
			       float load_cap,
			       Parasitic *,
			       float related_out_cap,
			       const Pvt *pvt,
			       const DcalcAnalysisPt *dcalc_ap,
			       int digits,
			       string *result);
  void delay_work_set_thresholds(delay_work *D,
				 double lo,
				 double hi,
				 bool rising,
				 double derate);

private:
  void gateDelaySlew(const LibertyCell *drvr_cell,
		     GateTableModel *table_model,
		     const Slew &in_slew,
		     float related_out_cap,
		     const Pvt *pvt,
		     // Return values.
		     ArcDelay &gate_delay,
		     Slew &drvr_slew);
  void ar1_ceff_delay(delay_work *D,timing_table *tab, arnoldi1 *mod,
		      double *delays, double *slews);
  double ra_rdelay_1(timing_table *tab,
		     double ctot);
  double ra_get_r(delay_work *D,
		  timing_table *tab,
		  double rdelay,
		  double ctot);
  double ra_get_s(delay_work *D,
		  timing_table *tab,
		  double r,
		  double c);
  void ra_solve_for_s(delay_work *D,
		      double p,
		      double tlohi,
		      double &s);
  // from poles and residues, solve for t20,t50,t80
  void pr_solve1(double s,
		 int order,
		 double *p,
		 double *rr,
		 double v1,
		 double *t1);
  void pr_solve3(double s,
		 int order,
		 double *p,
		 double *rr,
		 double vhi,
		 double *thi,
		 double vmid,
		 double *tmid,
		 double vlo,
		 double *tlo);

  //
  // routines for linear drive model and ceff
  //
 double pr_ceff(double s,
		double rdrive,
		int order,
		double *p,
		double *rr,
		double ceff_time);
  double ra_solve_for_t(double p,
			double s,
			double v);
  void ra_solve_for_pt(double ps,
		       double v,
		       double *pt,
		       double *d);
  void ra_calc_c(double lo,
		 double hi,
		 double *c_smin,
		 double *c_x1,
		 double *c_y1);

  rcmodel *rcmodel_;
  int _pinNmax;
  double *_delayV;
  double *_slewV;
  int pin_n_;
  bool input_port_;
  ArnoldiReduce *reduce_;
  delay_work *delay_work_;
};

ArcDelayCalc *
makeArnoldiDelayCalc(StaState *sta)
{
  return new ArnoldiDelayCalc(sta);
}

ArnoldiDelayCalc::ArnoldiDelayCalc(StaState *sta) :
  RCDelayCalc(sta),
  reduce_(new ArnoldiReduce(sta)),
  delay_work_(delay_work_create())
{
  _pinNmax = 1024;
  _delayV = (double*)malloc(_pinNmax * sizeof(double));
  _slewV = (double*)malloc(_pinNmax * sizeof(double));
}

ArcDelayCalc *
ArnoldiDelayCalc::copy()
{
  return new ArnoldiDelayCalc(this);
}

ArnoldiDelayCalc::~ArnoldiDelayCalc()
{
  delay_work_destroy(delay_work_);
  free(_delayV);
  free(_slewV);
  delete reduce_;
}

Parasitic *
ArnoldiDelayCalc::findParasitic(const Pin *drvr_pin,
				const RiseFall *drvr_rf,
				const DcalcAnalysisPt *dcalc_ap)
{
  // set_load has precidence over parasitics.
  if (!sdc_->drvrPinHasWireCap(drvr_pin)) {
    const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
    Parasitic *parasitic_network =
      parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
    bool delete_parasitic_network = false;

    const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
    const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
    const Corner *corner = dcalc_ap->corner();
    if (parasitic_network == nullptr) {
      Wireload *wireload = sdc_->wireloadDefaulted(cnst_min_max);
      if (wireload) {
	float pin_cap, wire_cap, fanout;
	bool has_wire_cap;
	graph_delay_calc_->netCaps(drvr_pin, drvr_rf, dcalc_ap,
				   pin_cap, wire_cap, fanout, has_wire_cap);
	parasitic_network = parasitics_->makeWireloadNetwork(drvr_pin, wireload,
							     fanout, op_cond,
							     parasitic_ap);
	delete_parasitic_network = true;
      }
    }
    
    if (parasitic_network) {
      Parasitic *parasitic =
	reduce_->reduceToArnoldi(parasitic_network,
				 drvr_pin,
				 parasitic_ap->couplingCapFactor(),
				 drvr_rf, op_cond, corner,
				 cnst_min_max, parasitic_ap);
      if (delete_parasitic_network) {
	Net *net = network_->net(drvr_pin);
	parasitics_->deleteParasiticNetwork(net, parasitic_ap);
      }
      reduced_parasitic_drvrs_.push_back(drvr_pin);
      return parasitic;
    }
  }
  return nullptr;
}

void
ArnoldiDelayCalc::inputPortDelay(const Pin *drvr_pin,
				 float in_slew,
				 const RiseFall *rf,
				 Parasitic *parasitic,
				 const DcalcAnalysisPt *dcalc_ap)
{
  RCDelayCalc::inputPortDelay(drvr_pin, in_slew, rf, parasitic, dcalc_ap);
  rcmodel_ = nullptr;
  _delayV[0] = 0.0;
  _slewV[0] = in_slew;

  int j;
  if (parasitic) {
    rcmodel_ = reinterpret_cast<rcmodel*>(parasitic);
    pin_n_ = rcmodel_->n;
    if (pin_n_ >= _pinNmax) {
      _pinNmax *= 2;
      if (pin_n_ >= _pinNmax) _pinNmax += pin_n_;
      _pinNmax *= 2;
      _delayV = (double*)realloc(_delayV,_pinNmax * sizeof(double));
      _slewV = (double*)realloc(_slewV,_pinNmax * sizeof(double));
    }
    pin_n_ = 1;

    pin_n_ = rcmodel_->n;
    double slew_derate = drvr_library_->slewDerateFromLibrary();
    double lo_thresh = drvr_library_->slewLowerThreshold(drvr_rf_);
    double hi_thresh = drvr_library_->slewUpperThreshold(drvr_rf_);
    bool rising = (drvr_rf_ == RiseFall::rise());
    delay_work_set_thresholds(delay_work_, lo_thresh, hi_thresh, rising,
			      slew_derate);
    delay_c *c = delay_work_->c;
    double c_log = c->vlg;

    for (j=1;j<pin_n_;j++) {
      double elmore = rcmodel_->elmore(j);
      _delayV[j] = 0.6931472*elmore;
      _slewV[j] = in_slew + c_log*elmore/slew_derate;
    }
  }
}

void
ArnoldiDelayCalc::gateDelay(const LibertyCell *drvr_cell,
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
  ConcreteParasitic *drvr_cparasitic =
    reinterpret_cast<ConcreteParasitic*>(drvr_parasitic);
  rcmodel_ = dynamic_cast<rcmodel*>(drvr_cparasitic);
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  GateTableModel *table_model = dynamic_cast<GateTableModel*>(model);
  if (table_model && rcmodel_)
    gateDelaySlew(drvr_cell, table_model, in_slew,
		  related_out_cap, pvt,
		  gate_delay, drvr_slew);
  else
    LumpedCapDelayCalc::gateDelay(drvr_cell, arc, in_slew, load_cap,
				  drvr_parasitic, related_out_cap, pvt,
				  dcalc_ap, gate_delay, drvr_slew);
  drvr_slew_ = drvr_slew;
  multi_drvr_slew_factor_ = 1.0F;
}

void
ArnoldiDelayCalc::gateDelaySlew(const LibertyCell *drvr_cell,
				GateTableModel *table_model,
				const Slew &in_slew,
				float related_out_cap,
				const Pvt *pvt,
				// Return values.
				ArcDelay &gate_delay,
				Slew &drvr_slew)
{
  pin_n_ = rcmodel_->n;
  if (pin_n_ >= _pinNmax) {
    _pinNmax *= 2;
    if (pin_n_ >= _pinNmax) _pinNmax += pin_n_;
    _delayV = (double*)realloc(_delayV,_pinNmax * sizeof(double));
    _slewV = (double*)realloc(_slewV,_pinNmax * sizeof(double));
  }

  pin_n_ = rcmodel_->n;
  if (table_model) {
    double slew_derate = drvr_library_->slewDerateFromLibrary();
    double lo_thresh = drvr_library_->slewLowerThreshold(drvr_rf_);
    double hi_thresh = drvr_library_->slewUpperThreshold(drvr_rf_);
    bool rising = (drvr_rf_ == RiseFall::rise());
    delay_work_set_thresholds(delay_work_, lo_thresh, hi_thresh, rising,
			      slew_derate);
    if (rcmodel_->order > 0) {
      timing_table tab;
      tab.table = table_model;
      tab.cell = drvr_cell;
      tab.pvt = pvt;
      tab.in_slew = delayAsFloat(in_slew);
      tab.relcap = related_out_cap;
      ar1_ceff_delay(delay_work_, &tab, rcmodel_,
		     _delayV, _slewV);
    }
    gate_delay = _delayV[0];
    drvr_slew = _slewV[0];
  }
}

void
ArnoldiDelayCalc::loadDelay(const Pin *load_pin,
			    // Return values.
			    ArcDelay &wire_delay,
			    Slew &load_slew)
{
  wire_delay = 0.0;
  load_slew = drvr_slew_ * multi_drvr_slew_factor_;
  if (rcmodel_) {
    // HACK
    for (int i = 0; i < rcmodel_->n; i++) {
      if (rcmodel_->pinV[i] == load_pin) {
	wire_delay = _delayV[i] - _delayV[0];
	load_slew = _slewV[i] * multi_drvr_slew_factor_;
	break;
      }
    }
  }
  thresholdAdjust(load_pin, wire_delay, load_slew);
}

void
ArnoldiDelayCalc::reportGateDelay(const LibertyCell *,
				  TimingArc *,
				  const Slew &,
				  float,
				  Parasitic *,
				  float,
				  const Pvt *,
				  const DcalcAnalysisPt *,
				  int,
				  string *)
{
}

////////////////////////////////////////////////////////////////

//
// arnoldi1.cpp
//

arnoldi1::~arnoldi1()
{
  free(d);
  free(U);
}

double
arnoldi1::elmore(int k)
{
  if (order==0) return 0.0;
  if (order==1) return d[0];
  double sqctot = 1.0/U[0][0];
  double tau = d[0] + e[0]*U[1][k]*sqctot;
  return tau;
}

delay_work *
delay_work_create()
{
  int j;
  delay_work *D = (delay_work*)malloc(sizeof(delay_work));
  D->nmax = 256;
  D->resi = (double**)malloc(D->nmax*sizeof(double*));
  D->resi[0] = (double*)malloc(D->nmax*32*sizeof(double));
  for (j=1;j<D->nmax;j++) D->resi[j] = D->resi[0] + j*32;
  D->v[0] = (double*)malloc(32*32*sizeof(double));
  for (j=1;j<32;j++) D->v[j] = D->v[0] + j*32;
  D->w[0] = (double*)malloc(32*D->nmax*sizeof(double));
  for (j=1;j<32;j++) D->w[j] = D->w[0] + j*D->nmax;
  D->lo_thresh = 0.0;
  D->hi_thresh = 0.0;
  D->slew_derate = 0.0;
  D->slew_factor = 0.0;
  for (j=0;j<2;j++) {
    D->cV[j].vlo = 0.0;
    D->cV[j].vhi = 0.0;
    D->cV[j].vlg = 0.0;
    D->cV[j].smin = 0.0;
    D->cV[j].x1 = 0.0;
    D->cV[j].y1 = 0.0;
    D->cV[j].vmid = 0.0;
  }
  D->c = D->cV;
  return D;
}

static void
delay_work_destroy(delay_work *D)
{
  free(D->resi[0]);
  free(D->resi);
  free(D->v[0]);
  free(D->w[0]);
  free(D);
}

static void
delay_work_alloc(delay_work *D,int n)
{
  if (n<=D->nmax) return;
  free(D->w[0]);
  free(D->resi[0]);
  free(D->resi);
  D->nmax *= 2;
  if (n > D->nmax) D->nmax = n;
  int j;
  D->resi = (double**)malloc(D->nmax*sizeof(double*));
  D->resi[0] = (double*)malloc(D->nmax*32*sizeof(double));
  for (j=1;j<D->nmax;j++) D->resi[j] = D->resi[0] + j*32;
  D->w[0] = (double*)malloc(32*D->nmax*sizeof(double));
  for (j=1;j<32;j++) D->w[j] = D->w[0] + j*D->nmax;
}

void
ArnoldiDelayCalc::delay_work_set_thresholds(delay_work *D,
					    double lo,
					    double hi,
					    bool rising,
					    double derate)
{
  double mid = 0.5;  // 0.0:1.0
  int i = rising?1:0;
  D->c = D->cV+ i;
  // WRONG
  bool changed = (lo != D->c->vlo || hi != D->c->vhi);
  if (changed) {
    if (!(lo>0.01 && hi<0.99)) {
      lo = 0.1;
      hi = 0.9;
      derate = 0.8;
    }
    D->c->slew_derate = derate;
    D->c->vlo = lo;
    D->c->vhi = hi;
    D->c->vmid = mid;
    D->c->vlg = log(hi/lo);
    ra_calc_c(lo,hi,
	      &(D->c->smin), &(D->c->x1),&(D->c->y1));
  }
  D->lo_thresh = D->c->vlo;
  D->hi_thresh = D->c->vhi;
  D->slew_derate = derate;
  double measured_swing = D->c->vhi - D->c->vlo;
  double reported_swing = measured_swing/D->slew_derate;
  D->slew_factor = reported_swing;
}

static double *
delay_work_get_residues(delay_work *D,int term_index)
{
  return D->resi[term_index];
}

//////////////////////////////////////////////////////////////
//
// calculate_poles_res
//

void arnoldi1::calculate_poles_res(delay_work *D,double rdrive)
{
  if (n > D->nmax) delay_work_alloc(D,n);
  double *p = D->poles;
  double **v = D->v;
  double **w = D->w;
  double *aa = D->aa;
  double **resi = D->resi;
  int h,j,k;
  double sum, dsave;

  dsave = d[0];
  d[0] += rdrive*ctot;
  if (!tridiagEV(order,d,e,p,v))
    internalError("arnoldi delay calc failed.\n");
  d[0] = dsave;

  for (h=0;h<order;h++) {
    if (p[h]<1e-14) // .01ps
      p[h]=1e-14;
    p[h] = 1.0/p[h];
  }

  for (h=0;h<order;h++) {
    for (k=0;k<n;k++) {
      sum = 0.0;
      for (j=0;j<order;j++)
        sum += v[h][j]*U[j][k];
      w[h][k] = sum;
    }
    aa[h] = sqc*v[h][0];
  }

  for (j=0;j<n;j++) {
    for (h=0;h<order;h++)
      resi[j][h] = aa[h]*w[h][j];
  }
}

////////////////////////////////////////////////////////////////

//
// tridiag.cpp
//

//
//  tridiagonal eigenvalues and eigenvectors
//   assuming all eigenvalues are positive
//
//  tridiagEV(int n,double *d,double *e,double *p,double **v)
//   d[0]..d[n-1]  diagonal elements
//   e[0]..e[n-2]  off-diagonal elements
//   p[0],..p[n-1] the eigenvalues
//   v[0],..v[n-1] the eigenvectors
//    M*v[j] = p[j]*v[j]
//
//   (M*v[j])[0] = d[0]*v[j][0]+e[0]*v[j][1]
//   (M*v[j])[k] = d[k]*v[j][k]+e[k-1]*v[j][k-1]+e[k]*v[j][k+1]  0<k<n-1
//   (M*v[j])[n-1] = d[n-1]*v[j][n-1]+e[n-2]*v[j][n-2]
//
static bool
tridiagEV(int n,double *din,double *ein,double *d,double **v)
{
  int j,k;
  for (j=0;j<n;j++) for (k=0;k<n;k++) v[j][k]=0.0;
  for (j=0;j<n;j++) v[j][j] = 1.0;

  int m,h,iter,i;
  double s,r,p,g,f,c,b;
  double e[32];
  if (n>32)
    return false;

  for (i=0;i<n;i++) d[i] = din[i];

  for (i=0;i<n-1;i++) e[i+1] = ein[i];
  e[0] = 0.0;
  for (h=n-1;h>=1;h--) {
    iter = 0;
    while (abs(e[h])>1e-18) { // 1e-6ps
      m=0;
      if (m != h) {
        if (iter++ == 20)
          return false;
        g = (d[h-1]-d[h])/(2.0*e[h]);
        r = sqrt(1.0+g*g);  // watch overflow
        g = d[m]-d[h]+e[h]/(g + (g<0?-r:r));
        s = c = 1.0;
        p = 0.0;
        for (i=m+1;i<=h;i++) {
          f = s*e[i];
          b = c*e[i];
          e[i-1] = r = sqrt(f*f+g*g);  // watch
          if (r == 0.0) {
            d[i-1] -= p;
            e[m] = 0.0;
            break;
          }
          s = f/r;
          c = g/r;
          g = d[i-1]-p;
          r = (d[i]-g)*s+2.0*c*b;
          d[i-1] = g + (p=s*r);
          g = c*r-b;
          for (k=0;k<n;k++) {
            f = v[i-1][k];
            v[i-1][k] = s*v[i][k]+c*f;
            v[i][k] = c*v[i][k]-s*f;
          }
        }
        if (r == 0.0 && i <= h) continue;
        d[h] -= p;
        e[h] = g;
        e[m] = 0.0;
      }
    }
  }

  for (i=0;i<n-1;i++) {
    k = i;
    p = d[k];
    for (j=i+1;j<n;j++)
      if (d[j] > p) { k=j; p=d[k]; }
    if (k != i) {
      d[k] = d[i];
      d[i] = p;
      for (j=0;j<n;j++) {
        p = v[i][j];
        v[i][j] = v[k][j];
        v[k][j] = p;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////

// prsolve.cpp

// get a waveform point
static void
pr_get_v(double t, double s, int order, double *p, double *rr, double *va)
{
  *va = 0.0;
  int h;
  for (h=0;h<order;h++) {
    double pt = p[h]*t;
    double ps = p[h]*s;
    double f;
    if (t<s) {
      f = 1.0-t/s + (1.0-exp(-pt))/ps;
    } else {
      f = exp(ps-pt)*(1.0-exp(-ps))/ps;
    }
    *va += rr[h]*f;
  }
}

static void
get_dv(double t, double s, int order, double *p, double *rr,
       double *va, double *dva)
{
  *va = 0.0;
  *dva = 0.0;
  int h;
  for (h=0;h<order;h++) {
    double p1 = p[h];
    double pt = p1*t;
    double ps = p1*s;
    double f,df,xtmp;
    if (t<s) {
      xtmp = (1.0-exp(-pt))/ps;
      f = 1.0-t/s + xtmp;
      df = -p1*xtmp;
    } else {
      f = exp(ps-pt)*(1.0-exp(-ps))/ps;
      df = -p1*f;
    }
    *va += rr[h]*f;
    *dva += rr[h]*df;
  }
}

static double
solve_t_bracketed(double s,int order,double *p,double *rr,
		  double val,double x1,double x2,double v1,double v2)
{
  int j;
  double df,dx,dxold,f,f2,f1;
  double temp,xh,xl,rts;
  double xacc = .001e-12; // .001ps
  f1 = v1-val;
  f2 = v2-val;
  if (f1==0.0) return x1;
  if (f2==0.0) return x2;
  rts = (f1*x2-f2*x1)/(f1-f2);
  if (f1<f2) {
    xl = x1;
    xh = x2;
    if (0.0<f1) return x1;
    if (f2<0.0) return x2;
  } else {
    xl = x2;
    xh = x1;
    if (0.0<f2) return x2;
    if (f1<0.0) return x1;
  }
  dxold = abs(x2-x1);
  dx = dxold;
  get_dv(rts,s,order,p,rr,&f,&df);
  f -= val;
  double flast = 0.0;
  for (j=1;j<10;j++) {
    if ((((rts-xh)*df-f)*((rts-xl)*df-f) >= 0.0)
        || (abs(2.0*f) > abs(dxold*df))) {
      dxold = dx;
      dx = 0.5*(xh-xl);
      if (flast*f >0.0) {
        // 2 successive bisections in same direction,
        // accelerate
        if (f<0.0) dx = 0.9348*(xh-xl);
        else dx = 0.0625*(xh-xl);
      }
      flast = f;
      rts = xl+dx;
      if (xl == rts) {
        return rts;
      }
    } else {
      dxold = dx;
      dx = f/df;
      flast = 0.0;
      temp = rts;
      rts -= dx;
      if (temp == rts) {
        return rts;
      }
    }
    if (abs(dx) < xacc) {
      return rts;
    }
    get_dv(rts,s,order,p,rr,&f,&df); f -= val;
    if (f<0.0)
      xl = rts;
    else
      xh = rts;
  }
  if (abs(f)<1e-6) // 1uV
    return rts;
  return 0.5*(xl+xh);
}

void
ArnoldiDelayCalc::pr_solve1(double s,
			    int order,
			    double *p,
			    double *rr,
			    double v1,
			    double *t1)
{
  double tmin = 0.0,tmax = 0.0,vmin = 0.0,vmax = 0.0;
  int h, h0 = 0;
  while (order>1
	 && rr[order-1]<1e-8 // 1e-8V
	 && rr[order-1]>-1e-8)
    order--;
  if (rr[0]<0.5) {
    for (h=1;h<order;h++) if (rr[h]>0.3 && rr[h]>rr[0]) { h0 = h; break; }
  }
  double p0 = p[h0];
  double ps,vs,ta,va;
  vs = 0.0;
  for (h=0;h<order;h++) {
    ps = p[h]*s;
    vs += rr[h]*(1-exp(-ps))/ps;
  }
  if (vs<v1) {
    // s dominates
    ta = 0.5*(1+v1)*s;
    pr_get_v(ta,s,order,p,rr,&va);
    if (va<v1) {
      tmax = ta; vmax = va;
      ta = v1*s;
      pr_get_v(ta,s,order,p,rr,&va);
      if (va<v1) {
        // ignoring a typical error at drive node, that comes
        // from slight inaccuracies in rr
        if (!(rr[order-1]>1.0 && p[order-1]>500.0 && va>v1-0.002))
	  debugPrint0(debug_, "arnoldi", 1, "err, pr_solve1, va<v1\n");
      }
      tmin = ta; vmin = va;
    } else {
      tmin = ta; vmin = va;
      ta = s;
      pr_get_v(ta,s,order,p,rr,&va);
      while (va>v1) {
        tmin = ta; vmin = va;
        ta *= 2.0;
        pr_get_v(ta,s,order,p,rr,&va);
      }
      if (va>v1)
	debugPrint0(debug_, "arnoldi", 1, "err, pr_solve1, va>v1\n");
      tmax = ta; vmax = va;
    }
  } else {
    // s is irrelevant
    ta = s; va = vs;
    while (va >= v1) {
      tmin = ta;
      vmin = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax = ta; vmax = va;
  }
  *t1 = solve_t_bracketed(s,order,p,rr,v1,tmin,tmax,vmin,vmax);
}

void
ArnoldiDelayCalc::pr_solve3(double s,
			    int order,
			    double *p,
			    double *rr,
			    double vhi,
			    double *thi,
			    double vmid,
			    double *tmid,
			    double vlo,
			    double *tlo)
{
  // falling, thi<tmin<tlo
  double tmin2,tmax2,vmin2,vmax2;
  double tmin5,tmax5,vmin5,vmax5;
  double tmin8,tmax8,vmin8,vmax8;
  int h, h0 = 0;
  while (order>1
	 && rr[order-1]<1e-8 // 1e-8V
	 && rr[order-1]>-1e-8)
    order--;
  if (rr[0]<0.5) {
    for (h=1;h<order;h++) if (rr[h]>0.3 && rr[h]>rr[0]) { h0 = h; break; }
  }
  double p0 = p[h0];
  if (p0>10e+9)  // 1/10ns
    p0=10e+9;
  double ps,vs,ta,va;
  vs = 0.0;
  for (h=0;h<order;h++) {
    ps = p[h]*s;
    vs += rr[h]*(1-exp(-ps))/ps;
  }
  if (vs<vlo) {
    // s dominates
    tmax8 = s; vmax8 = vs;
    ta = vhi*s;
    pr_get_v(ta,s,order,p,rr,&va);
    if (va < vmid) {
      tmax2 = tmax5 = tmin8 = ta;
      vmax2 = vmax5 = vmin8 = va;
      ta = vmid*s;
      pr_get_v(ta,s,order,p,rr,&va);
      if (va>vhi) {
        tmin2 = tmin5 = ta;
        vmin2 = vmin5 = va;
        tmin8 = ta; vmin8 = va;
        if (va<vmid) {
          tmax5 = ta; vmax5 = va;
        } else {
          tmin5 = ta; vmin5 = va;
        }
      } else {
        tmax2 = tmin5 = ta;
        vmax2 = vmin5 = va;
        ta = vlo*s;
        pr_get_v(ta,s,order,p,rr,&va);
        tmin2 = ta; vmin2 = va;
      }
    } else {
      // rare, s dominates but t=vhi*s is still above vmid
      tmin5 = tmin8 = ta;
      vmin5 = vmin8 = va;
      tmax5 = tmax8;
      vmax5 = vmax8;
      if (va > vhi) {
        tmin2 = tmin5;
        vmin2 = vmin5;
        tmax2 = tmax5;
        vmax2 = tmax5;
      } else {
        tmax2 = tmin5;
        vmax2 = vmin5;
        ta = vlo*s;
        pr_get_v(ta,s,order,p,rr,&va);
        tmin2 = ta; vmin2 = va;
      }
    }
  } else if (vs<vmid) {
    // not far from s
    tmax2 = tmax5 = tmin8 = s;
    vmax2 = vmax5 = vmin8 = vs;
    ta = s + 1.6/p0;
    pr_get_v(ta,s,order,p,rr,&va);
    while (va>vlo) {
      tmin8 = ta; vmin8 = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax8 = ta; vmax8 = va;
    ta = vmid*s;
    pr_get_v(ta,s,order,p,rr,&va);
    tmin5 = ta; vmin5 = va;
    if (va>vhi) {
      tmin2 = ta; vmin2 = va;
    } else {
      tmax2 = ta; vmax2 = va;
      ta = vlo*s;
      pr_get_v(ta,s,order,p,rr,&va);
      tmin2 = ta; vmin2 = va;
    }
  } else if (vs<vhi) {
    tmax2 = tmin5 = tmin8 = s;
    vmax2 = vmin5 = vmin8 = vs;
    ta = vlo*s;
    pr_get_v(ta,s,order,p,rr,&va);
    tmin2 = ta; vmin2 = va;
    ta = s + 0.7/p0;
    pr_get_v(ta,s,order,p,rr,&va);
    while (va>vmid) {
      tmin5 = tmin8 = ta; vmin5 = tmin8 = va;
      ta += 0.7/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax5 = ta; vmax5 = va;
    if (va < vlo) {
      tmax8 = ta; vmax8 = va;
    } else {
      tmin8 = ta; vmin8 = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
      while (va>vlo) {
        tmin8 = ta; vmin8 = va;
        ta += 1.0/p0;
        pr_get_v(ta,s,order,p,rr,&va);
      }
      tmax8 = ta; vmax8 = va;
    }
  } else {
    // s is irrelevant
    ta = s; va = vs;
    tmin2 = tmin5 = tmin8 = ta;
    vmin2 = vmin5 = vmin8 = va;
    while (va > vhi) {
      tmin2 = tmin5 = tmin8 = ta;
      vmin2 = vmin5 = vmin8 = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax2 = ta; vmax2 = va;
    if (va < vmid) {
      tmax5 = ta; vmax5 = va;
    } else while (va > vmid) {
      tmin5 = tmin8 = ta;
      vmin5 = vmin8 = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax5 = ta; vmax5 = va;
    if (va < vlo) {
      tmax8 = ta; vmax8 = va;
    } else while (va > vlo) {
      tmin8 = ta;
      vmin8 = va;
      ta += 1.0/p0;
      pr_get_v(ta,s,order,p,rr,&va);
    }
    tmax8 = ta; vmax8 = va;
  }

  *thi = solve_t_bracketed(s,order,p,rr,vhi,tmin2,tmax2,vmin2,vmax2);
  *tmid= solve_t_bracketed(s,order,p,rr,vmid,tmin5,tmax5,vmin5,vmax5);
  *tlo= solve_t_bracketed(s,order,p,rr,vlo,tmin8,tmax8,vmin8,vmax8);
}

static double
calc_integ(double p,double s,double t)
{
  // integral of f(t)-vin(t)
  double ps = p*s;
  double pt = p*t;
  double y,ept,eps;
  if (t<=s) {
    ept = (pt>40.0)?0.0:exp(-pt);
    y = ept-1.0+pt;
  } else {
    pt = pt-ps;
    ept = (pt>40.0)?0.0:exp(-pt);
    eps = (ps>40.0)?0.0:exp(-ps);
    y = ps - (1.0-eps)*ept;
  }
  y /= ps*p;
  return y;
}


double
ArnoldiDelayCalc::pr_ceff(double s,
			  double rdrive,
			  int order,
			  double *p,
			  double *rr,
			  double ceff_time)
{
  double integi = 0.0;
  double ceff, v0;
  int j;
  for (j=0;j<order;j++) {
    integi += rr[j]*calc_integ(p[j],s,ceff_time);
  }
  integi /= rdrive;
  pr_get_v(ceff_time,s,order,p,rr,&v0);
  ceff = integi/(1.0-v0);
  return ceff;
}

//////////////////////////////////////////////////////////////////

static double
ra_hinv(double y,
	Debug *debug)
{
  double x;
  if (y<1.0) {
    x = sqrt(2*y)+0.4*y;
    if (y<1e-4) return x;
  } else {
    x = y+1.0;
  }
  double ex = exp(-x);
  double f = x+ex-1.0-y;
  x += f/(ex-1.0);
  ex = exp(-x);
  f =  x+ex-1.0-y;
  x += f/(ex-1.0);
  ex = exp(-x);
  f =  x+ex-1.0-y;
  x += f/(ex-1.0);
   ex = exp(-x);
   f =  x+ex-1.0-y;
   if (f<-1e-8 || f>1e-8)
     debugPrint2(debug, "arnoldi", 1, "y f %g %g\n",y,f);
  return x;
}

double
ArnoldiDelayCalc::ra_solve_for_t(double p,
				 double s,
				 double v)
{
  double t;
  double ps = p*s;
  if (ps>30.0) {
    t = (1.0+ps*(1.0-v)) / p;
    return t;
  }
  double eps = exp(ps);
  if ((1-ps*v)*eps >= 1.0) {
    t =  log((eps-1.0)/(ps*v)) / p;
  } else {
    t =  ra_hinv((1-v)*ps, debug_)/p;
  }
  return t;
}

void
ArnoldiDelayCalc::ra_solve_for_pt(double ps,
				  double v,
				  double *pt,
				  double *d)
{
  if (ps>30.0) {
    *pt = 1.0+ps*(1.0-v);
    *d = 1.0-v;
    return;
  }
  double eps = exp(ps);
  if ((1-ps*v)*eps >= 1.0) {
    *pt =  log((eps-1.0)/(ps*v));
    *d = eps/(eps-1.0) - 1.0/ps;
  } else {
    *pt =  ra_hinv((1-v)*ps, debug_);
    *d = (1.0-v)/(*pt - (1-v)*ps);
  }
}

void
ArnoldiDelayCalc::ra_calc_c(double vlo,
			    double vhi,
			    double *c_smin,
			    double *c_x1,
			    double *c_y1)
{
  double a = log(1.0/vhi);
  *c_smin =  a + ra_hinv((1.0-vhi)/vhi - a, debug_);
  double b = log(1.0/vlo);
  double c_s1 =  b + ra_hinv((1.0-vlo)/vlo - b, debug_);
  double a1 = (exp(c_s1)-1.0)/c_s1;
  double den = log(a1/vlo) - ra_hinv((1.0-vhi)*c_s1, debug_);
  *c_x1 = (vhi-vlo)/den;
  *c_y1 = c_s1*(*c_x1);
}

////////////////////////////////////////////////////////////////

//
// ceff.cpp
//

void
ArnoldiDelayCalc::ra_solve_for_s(delay_work *D,
				 double p,
				 double tlohi,
				 double &s)
{
  delay_c *c = D->c;
  double vhi = c->vhi;
  double vlo = c->vlo;
  // s is 0-100
  // solve f(x,y)=0 with f = x*(ptlo(y/x)-pthi(y/x))-(vhi-vlo)
  // (x=0,y=1)
  // (x=x1,y=y1)  c->x1,y1
  // (x=x2,y=y2)  x2=(vhi-vlo)/log(vhi/vlo)  y2=(c->smin)*x2
  double x1 = c->x1;
  double y1 = c->y1;
  double x2 = (vhi-vlo)/c->vlg;
  double y2 = (c->smin)*x2;
  double ptlo,dlo;
  double pthi,dhi;
  double f,df,x,y;
  x = c->vlg/(p*tlohi);

  if (x <= x1) {
    y = y1 - 0.5*(x-x1);
    if (y>1.0) y=1.0;
  } else {
    y =  y1 - (x-x1)*(0.5 + 8*(x-x1));
    if (y<y2) y=y2;
  }

  ra_solve_for_pt(p*s,vlo,&ptlo,&dlo);
  ra_solve_for_pt(p*s,vhi,&pthi,&dhi);
  f = (ptlo-pthi)/p - tlohi;
  df = dlo-dhi;
  s = s - f/df;
  if (abs(f)<.001e-12) return;	// .001ps

  ra_solve_for_pt(p*s,vlo,&ptlo,&dlo);
  ra_solve_for_pt(p*s,vhi,&pthi,&dhi);
  f = (ptlo-pthi)/p - tlohi;
  df = dlo-dhi;
  s = s - f/df;
  if (abs(f)<.001e-12) return;	// .001ps

  ra_solve_for_pt(p*s,vlo,&ptlo,&dlo);
  ra_solve_for_pt(p*s,vhi,&pthi,&dhi);
  f = (ptlo-pthi)/p - tlohi;
  df = dlo-dhi;
  s = s - f/df;
  if (abs(f)<.001e-12) return;	// .001ps

  ra_solve_for_pt(p*s,vlo,&ptlo,&dlo);
  ra_solve_for_pt(p*s,vhi,&pthi,&dhi);
  f = (ptlo-pthi)/p - tlohi;
  df = dlo-dhi;
  s = s - f/df;
  if (abs(f)<.001e-12) return;	// .001ps

  ra_solve_for_pt(p*s,vlo,&ptlo,&dlo);
  ra_solve_for_pt(p*s,vhi,&pthi,&dhi);
  f = (ptlo-pthi)/p - tlohi;
  df = dlo-dhi;
  s = s - f/df;

  if (abs(f)>.5e-12) // .5ps
    debugPrint3(debug_, "arnoldi", 1,
		"ra_solve_for_s p %g tlohi %s err %s\n",
		p,
		units_->timeUnit()->asString(tlohi),
		units_->timeUnit()->asString(f));
}

/////////////////////////////////////////////////////////////////////
// method 0:
// r = a match to slew to (ctot, limited by cmin,cmax)
// if r>rdelay, lower r
// Now at any ceff (limited)
//   If slew(r,0,ceff) is too big
//     s = s_start(r,ceff), not smaller than Smin
//     accept the pessimistic output slew
//   Else
//     solve for s

// Rough translation of ra_get_r(sy_table) used by ar1_ceff_delay.
double
ArnoldiDelayCalc::ra_get_r(delay_work *D,
			   timing_table *tab,
			   double rdelay,
			   double ctot)
{
  // find the maximum r that allows a solution for s of
  // (s,r,ctot)-> output_slew
  // If this maximum is greater than rdelay, use rdelay.
  delay_c *c = D->c;
  double slew_derate = c->slew_derate;
  double c_log = c->vlg;
  float  c1;
  double tlohi,r;
  c1 = ctot;
  ArcDelay d1;
  Slew s1;
  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew,
                        c1, tab->relcap, pocv_enabled_,
			d1, s1);
  tlohi = slew_derate*delayAsFloat(s1);
  r = tlohi/(c_log*c1);
  if (rdelay>0.0 && r > rdelay)
    r = rdelay;
  // else printf("from rdelay %g to r %g\n",rdelay,r);
  return r;
}

double
ArnoldiDelayCalc::ra_get_s(delay_work *D,
			   timing_table *tab,
			   double r,
			   double c)
{
  delay_c *con = D->c;
  double slew_derate = con->slew_derate;
  double c_log = con->vlg;
  double c_smin = con->smin;
  double tlohi,smin,s;
  ArcDelay d1;
  Slew s1;
  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew,
                        c, tab->relcap, pocv_enabled_, d1, s1);
  tlohi = slew_derate*delayAsFloat(s1);
  smin = r*c*c_smin; // c_smin = ra_hinv((1-vhi)/vhi-log(vhi)) + log(vhi);
  if (c_log*r*c >= tlohi) {
    // printf("hit smin\n");
    s = smin;
  } else {
    s = smin+0.3*tlohi;
    ra_solve_for_s(D,1.0/(r*c),tlohi,s);
  }
  return s;
}

/////////////////////////////////////////////////////////////////////
// method 1:
// determine the drive resistance from change in delay versus ctot
// find the maximum r that allows a solution for s of
// (s,r,ctot)-> output_slew
// If this maximum is greater than rdelay, use rdelay.
// calculate s,r,mod -> t50_srmod,
// then   t50_srmod+t50_sy-t50_sr

double
ArnoldiDelayCalc::ra_rdelay_1(timing_table *tab,
			      double ctot)
{
  // determine the drive resistance from change in delay versus ctot
  float c1 = ctot;
  float c2 = 0.5*c1;
  if (c1==c2)
    return 0.0;
  ArcDelay d1, d2;
  Slew s1, s2;
  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew,
                        c1, tab->relcap, pocv_enabled_, d1, s1);
  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew,
                        c2, tab->relcap, pocv_enabled_, d2, s2);
  double dt50 = delayAsFloat(d1)-delayAsFloat(d2);
  if (dt50 <= 0.0)
    return 0.0;
  double rdelay = dt50/(c1-c2);
  return rdelay;
}

void
ArnoldiDelayCalc::ar1_ceff_delay(delay_work *D,
				 timing_table *tab,
				 arnoldi1 *mod,
				 double *delays,
				 double *slews)
{
  delay_c *con = D->c;
  double slew_derate = con->slew_derate;
  double vhi = con->vhi;
  double vlo = con->vlo;
  double ctot = mod->ctot;
  double ceff,tlohi,t50_sy,r,s,t50_sr,rdelay;
  ArcDelay df;
  Slew sf;

  debugPrint1(debug_, "arnoldi", 1, "\nctot=%s\n",
	      units_->capacitanceUnit()->asString(ctot));

  rdelay = ra_rdelay_1(tab,ctot);
  if (rdelay == 0.0) {
    rdelay = 1e+3; // 1kohm
  }
  r = rdelay;
  r = ra_get_r(D,tab,rdelay,ctot);
  if (! (r>0.0
	 && r<100e+3)) // 100khom
    rdelay = 1e+3; // 1kohm

  bool bad = (r<rdelay);
  s = ra_get_s(D,tab,r,ctot);
  if (! (s>0.0
	 && s<100e-9)) // 100ns
    s = 0.5e-9; // .5ns

  if (debug_->check("arnoldi", 1)) {
    double p = 1.0/(r*ctot);
    double thix,tlox;
    if (bad) printf("bad\n");
    debugPrint2(debug_, "arnoldi", 1, "at r=%s s=%s\n",
		units_->resistanceUnit()->asString(r),
		units_->timeUnit()->asString(s));
    thix = ra_solve_for_t(p,s,vhi);
    tlox = ra_solve_for_t(p,s,vlo);
    tab->table->gateDelay(tab->cell, tab->pvt,tab->in_slew,
			  ctot, tab->relcap, pocv_enabled_, df, sf);
    debugPrint3(debug_, "arnoldi", 1,
		"table slew (in_slew %s ctot %s) = %s\n",
		units_->timeUnit()->asString(tab->in_slew),
		units_->capacitanceUnit()->asString(ctot),
		delayAsString(sf, this));
    tlohi = slew_derate*delayAsFloat(sf);
    debugPrint2(debug_, "arnoldi", 1, "tlohi %s %s\n",
		units_->timeUnit()->asString(tlohi),
		units_->timeUnit()->asString(tlox-thix));
  }
  ceff = ctot;
  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew,
                        ceff, tab->relcap, pocv_enabled_, df, sf);
  t50_sy = delayAsFloat(df);
  t50_sr = ra_solve_for_t(1.0/(r*ceff),s,0.5);

  // calculate s,r,mod -> t50_srmod,
  // then   t50_srmod+t50_sy-t50_sr

  mod->calculate_poles_res(D,r);
  double *p = D->poles;
  double *rr = delay_work_get_residues(D,0);
  double thi,tlo,t50_srmod;
  pr_solve1(s,mod->order,p,rr,0.5,&t50_srmod);

  int ceff_it,j;
  double ceff_time=0.0;

  if (!bad) {
    for (ceff_it=0;ceff_it<3;ceff_it++) {

      // calculate ceff
      ceff_time = s;
      ceff = pr_ceff(s,r,mod->order,p,rr,ceff_time);

      if((ceff-1e-20) < 0.0) {  // 1e-8pf
	debugPrint0(debug_, "arnoldi", 1,
		    "Invalid effective capacitance, using total capacitance\n");
	ceff = ctot;
      }

      // new mvs at ceff
      s = ra_get_s(D,tab,r,ceff);
      debugPrint1(debug_, "arnoldi", 1, "new mvs  s = %s\n",
		  units_->timeUnit()->asString(s));
    }
  }
  debugPrint4(debug_, "arnoldi", 1, "r %s s %s ceff_time %s ceff %s\n",
	      units_->resistanceUnit()->asString(r),
	      units_->timeUnit()->asString(s),
	      units_->timeUnit()->asString(ceff_time),
	      units_->capacitanceUnit()->asString(ceff));

  tab->table->gateDelay(tab->cell, tab->pvt, tab->in_slew, ceff,
			tab->relcap, pocv_enabled_, df, sf);
  t50_sy = delayAsFloat(df);
  t50_sr = ra_solve_for_t(1.0/(r*ceff),s,0.5);
  for (j=0;j<mod->n;j++) {
    rr = delay_work_get_residues(D,j);
    pr_solve3(s,mod->order,p,rr,vhi,&thi,0.5,&t50_srmod,vlo,&tlo);
    delays[j] = t50_srmod + t50_sy - t50_sr;
    slews[j] = (tlo-thi)/slew_derate;
  }
  debugPrint3(debug_, "arnoldi", 1, "slews[0] %s thi %s tlo %s\n",
	      units_->timeUnit()->asString(slews[0]),
	      units_->timeUnit()->asString(tlo),
	      units_->timeUnit()->asString(thi));
}

} // namespace
