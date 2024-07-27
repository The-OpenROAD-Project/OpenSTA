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

#include "DmpDelayCalc.hh"

#include "TableModel.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "DmpCeff.hh"

namespace sta {

// PiElmore parasitic delay calculator using Dartu/Menezes/Pileggi
// effective capacitance and elmore delay.
class DmpCeffElmoreDelayCalc : public DmpCeffDelayCalc
{
public:
  DmpCeffElmoreDelayCalc(StaState *sta);
  ArcDelayCalc *copy() override;
  const char *name() const override { return "dmp_ceff_elmore"; }
  ArcDcalcResult inputPortDelay(const Pin *port_pin,
                                float in_slew,
                                const RiseFall *rf,
                                const Parasitic *parasitic,
                                const LoadPinIndexMap &load_pin_index_map,
                                const DcalcAnalysisPt *dcalc_ap) override;

protected:
  void loadDelaySlew(const Pin *load_pin,
                     double drvr_slew,
                     const RiseFall *rf,
                     const LibertyLibrary *drvr_library,
                     const Parasitic *parasitic,
                     // Return values.
                     ArcDelay &wire_delay,
                     Slew &load_slew) override;
};

ArcDelayCalc *
makeDmpCeffElmoreDelayCalc(StaState *sta)
{
  return new DmpCeffElmoreDelayCalc(sta);
}

DmpCeffElmoreDelayCalc::DmpCeffElmoreDelayCalc(StaState *sta) :
  DmpCeffDelayCalc(sta)
{
}

ArcDelayCalc *
DmpCeffElmoreDelayCalc::copy()
{
  return new DmpCeffElmoreDelayCalc(this);
}

ArcDcalcResult
DmpCeffElmoreDelayCalc::inputPortDelay(const Pin *,
                                       float in_slew,
                                       const RiseFall *rf,
                                       const Parasitic *parasitic,
                                       const LoadPinIndexMap &load_pin_index_map,
                                       const DcalcAnalysisPt *)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();
  for (auto [load_pin, load_idx] : load_pin_index_map) {
    ArcDelay wire_delay = 0.0;
    Slew load_slew = in_slew;
    bool elmore_exists = false;
    float elmore = 0.0;
    if (parasitic)
      parasitics_->findElmore(parasitic, load_pin, elmore, elmore_exists);
    if (elmore_exists)
      // Input port with no external driver.
      dspfWireDelaySlew(load_pin, rf, in_slew, elmore, wire_delay, load_slew);
    thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, load_slew);
  }
  return dcalc_result;
}

void
DmpCeffElmoreDelayCalc::loadDelaySlew(const Pin *load_pin,
                                      double drvr_slew,
                                      const RiseFall *rf,
                                      const LibertyLibrary *drvr_library,
                                      const Parasitic *parasitic,
                                      // Return values.
                                      ArcDelay &wire_delay,
                                      Slew &load_slew)
{
  wire_delay = 0.0;
  load_slew = drvr_slew;
  bool elmore_exists = false;
  float elmore = 0.0;
  if (parasitic)
    parasitics_->findElmore(parasitic, load_pin, elmore, elmore_exists);
  if (elmore_exists)
    loadDelaySlewElmore(load_pin, elmore, wire_delay, load_slew);
  thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
}

////////////////////////////////////////////////////////////////

// PiPoleResidue parasitic delay calculator using Dartu/Menezes/Pileggi
// effective capacitance and two poles/residues.
class DmpCeffTwoPoleDelayCalc : public DmpCeffDelayCalc
{
public:
  DmpCeffTwoPoleDelayCalc(StaState *sta);
  ArcDelayCalc *copy() override;
  const char *name() const override { return "dmp_ceff_two_pole"; }
  Parasitic *findParasitic(const Pin *drvr_pin,
                           const RiseFall *rf,
                           const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResult inputPortDelay(const Pin *port_pin,
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

private:
  void loadDelaySlew(const Pin *load_pin,
                     double drvr_slew,
                     const RiseFall *rf,
                     const LibertyLibrary *drvr_library,
                     const Parasitic *parasitic,
                     // Return values.
                     ArcDelay &wire_delay,
                     Slew &load_slew) override;
  void loadDelay(double drvr_slew,
                 Parasitic *pole_residue,
		 double p1,
		 double k1,
		 ArcDelay &wire_delay,
		 Slew &load_slew);
  float loadDelay(double vth,
		  double p1,
		  double p2,
		  double k1,
		  double k2,
		  double B,
		  double k1_p1_2,
		  double k2_p2_2,
		  double tt,
		  double y_tt);

  bool parasitic_is_pole_residue_;
  float vth_;
  float vl_;
  float vh_;
  float slew_derate_;
};

ArcDelayCalc *
makeDmpCeffTwoPoleDelayCalc(StaState *sta)
{
  return new DmpCeffTwoPoleDelayCalc(sta);
}

DmpCeffTwoPoleDelayCalc::DmpCeffTwoPoleDelayCalc(StaState *sta) :
  DmpCeffDelayCalc(sta),
  parasitic_is_pole_residue_(false),
  vth_(0.0),
  vl_(0.0),
  vh_(0.0),
  slew_derate_(0.0)
{
}

ArcDelayCalc *
DmpCeffTwoPoleDelayCalc::copy()
{
  return new DmpCeffTwoPoleDelayCalc(this);
}

Parasitic *
DmpCeffTwoPoleDelayCalc::findParasitic(const Pin *drvr_pin,
				       const RiseFall *rf,
				       const DcalcAnalysisPt *dcalc_ap)
{
  Parasitic *parasitic = nullptr;
  const Corner *corner = dcalc_ap->corner();
  // set_load net has precedence over parasitics.
  if (sdc_->drvrPinHasWireCap(drvr_pin, corner)
      || network_->direction(drvr_pin)->isInternal())
    return nullptr;
  const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  // Prefer PiPoleResidue.
  parasitic = parasitics_->findPiPoleResidue(drvr_pin, rf, parasitic_ap);
  if (parasitic)
    return parasitic;
  parasitic = parasitics_->findPiElmore(drvr_pin, rf, parasitic_ap);
  if (parasitic)
    return parasitic;
  Parasitic *parasitic_network =
    parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
  if (parasitic_network) {
    parasitic = parasitics_->reduceToPiPoleResidue2(parasitic_network, drvr_pin, rf,
                                                    corner,
                                                    dcalc_ap->constraintMinMax(),
                                                    parasitic_ap);
    if (parasitic)
      return parasitic;
  }
  const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
  Wireload *wireload = sdc_->wireload(cnst_min_max);
  if (wireload) {
    float pin_cap, wire_cap, fanout;
    bool has_wire_cap;
    graph_delay_calc_->netCaps(drvr_pin, rf, dcalc_ap, pin_cap, wire_cap,
                               fanout, has_wire_cap);
    parasitic = parasitics_->estimatePiElmore(drvr_pin, rf, wireload,
                                              fanout, pin_cap, corner,
                                              cnst_min_max);
  }
  return parasitic;
}

ArcDcalcResult
DmpCeffTwoPoleDelayCalc::inputPortDelay(const Pin *,
                                        float in_slew,
                                        const RiseFall *rf,
                                        const Parasitic *parasitic,
                                        const LoadPinIndexMap &load_pin_index_map,
                                        const DcalcAnalysisPt *)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  ArcDelay wire_delay = 0.0;
  Slew load_slew = in_slew;
  LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();
  for (const auto [load_pin, load_idx] : load_pin_index_map) {
    if (parasitics_->isPiPoleResidue(parasitic)) {
      const Parasitic *pole_residue = parasitics_->findPoleResidue(parasitic, load_pin);
      if (pole_residue) {
        size_t pole_count = parasitics_->poleResidueCount(pole_residue);
        if (pole_count >= 1) {
          ComplexFloat pole1, residue1;
          // Find the 1st (elmore) pole.
          parasitics_->poleResidue(pole_residue, 0, pole1, residue1);
          if (pole1.imag() == 0.0
              && residue1.imag() == 0.0) {
            float p1 = pole1.real();
            float elmore = 1.0F / p1;
            dspfWireDelaySlew(load_pin, rf, in_slew, elmore, wire_delay, load_slew);
            thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
          }
        }
      }
    }
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, load_slew);
  }
  return dcalc_result;
}

ArcDcalcResult
DmpCeffTwoPoleDelayCalc::gateDelay(const Pin *drvr_pin,
                                   const TimingArc *arc,
                                   const Slew &in_slew,
                                   float load_cap,
                                   const Parasitic *parasitic,
                                   const LoadPinIndexMap &load_pin_index_map,
                                   const DcalcAnalysisPt *dcalc_ap)
{
  const LibertyLibrary *drvr_library = arc->to()->libertyLibrary();
  const RiseFall *rf = arc->toEdge()->asRiseFall();
  vth_ = drvr_library->outputThreshold(rf);
  vl_ = drvr_library->slewLowerThreshold(rf);
  vh_ = drvr_library->slewUpperThreshold(rf);
  slew_derate_ = drvr_library->slewDerateFromLibrary();
  return DmpCeffDelayCalc::gateDelay(drvr_pin, arc, in_slew, load_cap, parasitic,
                                     load_pin_index_map, dcalc_ap) ;
}

void
DmpCeffTwoPoleDelayCalc::loadDelaySlew(const Pin *load_pin,
                                       double drvr_slew,
                                       const RiseFall *rf,
                                       const LibertyLibrary *drvr_library,
                                       const Parasitic *parasitic,
                                       // Return values.
                                       ArcDelay &wire_delay,
                                       Slew &load_slew)
{
  parasitic_is_pole_residue_ = parasitics_->isPiPoleResidue(parasitic);
  // Should handle PiElmore parasitic.
  wire_delay = 0.0;
  load_slew = drvr_slew;
  Parasitic *pole_residue = 0;
  if (parasitic_is_pole_residue_)
    pole_residue = parasitics_->findPoleResidue(parasitic, load_pin);
  if (pole_residue) {
    size_t pole_count = parasitics_->poleResidueCount(pole_residue);
    if (pole_count >= 1) {
      ComplexFloat pole1, residue1;
      // Find the 1st (elmore) pole.
      parasitics_->poleResidue(pole_residue, 0, pole1, residue1);
      if (pole1.imag() == 0.0
	  && residue1.imag() == 0.0) {
	float p1 = pole1.real();
	float k1 = residue1.real();
        if (pole_count >= 2)
          loadDelay(drvr_slew, pole_residue, p1, k1, wire_delay, load_slew);
        else {
          float elmore = 1.0F / p1;
          wire_delay = elmore;
          load_slew = drvr_slew;
        }
      }
    }
  }
  thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
}

void
DmpCeffTwoPoleDelayCalc::loadDelay(double drvr_slew,
                                   Parasitic *pole_residue,
				   double p1,
                                   double k1,
				   // Return values.
                                   ArcDelay &wire_delay,
				   Slew &load_slew)
{
  ComplexFloat pole2, residue2;
  parasitics_->poleResidue(pole_residue, 1, pole2, residue2);
  if (!delayZero(drvr_slew)
      && pole2.imag() == 0.0
      && residue2.imag() == 0.0) {
    double p2 = pole2.real();
    double k2 = residue2.real();
    double k1_p1_2 = k1 / (p1 * p1);
    double k2_p2_2 = k2 / (p2 * p2);
    double B = k1_p1_2 + k2_p2_2;
    // Convert tt to 0:1 range.
    float tt = delayAsFloat(drvr_slew) * slew_derate_ / (vh_ - vl_);
    double y_tt = (tt - B + k1_p1_2 * exp(-p1 * tt)
		   + k2_p2_2 * exp(-p2 * tt)) / tt;
    wire_delay = loadDelay(vth_, p1, p2, k1, k2, B, k1_p1_2, k2_p2_2, tt, y_tt)
      - tt * vth_;

    float tl = loadDelay(vl_, p1, p2, k1, k2, B, k1_p1_2, k2_p2_2, tt, y_tt);
    float th = loadDelay(vh_, p1, p2, k1, k2, B, k1_p1_2, k2_p2_2, tt, y_tt);
    load_slew = (th - tl) / slew_derate_;
  }
}

float
DmpCeffTwoPoleDelayCalc::loadDelay(double vth,
				   double p1,
				   double p2,
				   double k1,
				   double k2,
				   double B,
				   double k1_p1_2,
				   double k2_p2_2,
				   double tt,
				   double y_tt)
{
  if (y_tt < vth) {
    // t1 > tt
    // Initial guess.
    double t1 = log(k1 * (exp(p1 * tt) - 1.0) / ((1.0 - vth) * p1 * p1 * tt))/p1;
    // Take one newton-raphson step.
    double exp_p1_t1 = exp(-p1 * t1);
    double exp_p2_t1 = exp(-p2 * t1);
    double exp_p1_t1_tt = exp(-p1 * (t1 - tt));
    double exp_p2_t1_tt = exp(-p2 * (t1 - tt));
    double y_t1 = (tt - k1_p1_2 * (exp_p1_t1_tt - exp_p1_t1)
		   - k2_p2_2 * (exp_p2_t1_tt - exp_p2_t1)) / tt;
    double yp_t1 = (k1 / p1 * (exp_p1_t1_tt - exp_p1_t1)
		    - k2 / p2 * (exp_p2_t1_tt - exp_p2_t1)) / tt;
    double delay = t1 - (y_t1 - vth) / yp_t1;
    return static_cast<float>(delay);
  }
  else {
    // t1 < tt
    // Initial guess based on y(tt).
    double t1 = vth * tt / y_tt;
    // Take one newton-raphson step.
    double exp_p1_t1 = exp(-p1 * t1);
    double exp_p2_t1 = exp(-p2 * t1);
    double y_t1 = (t1 - B + k1_p1_2 * exp_p1_t1
		   + k2_p2_2 * exp_p1_t1) / tt;
    double yp_t1 = (1 - k1 / p1 * exp_p1_t1
		    - k2 / p2 * exp_p2_t1) / tt;
    double delay = t1 - (y_t1 - vth) / yp_t1;
    return static_cast<float>(delay);
  }
}

} // namespace
