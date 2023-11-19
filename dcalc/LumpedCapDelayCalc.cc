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

#include "LumpedCapDelayCalc.hh"

#include <cmath>  // isnan

#include "Debug.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"

namespace sta {

using std::isnan;

ArcDelayCalc *
makeLumpedCapDelayCalc(StaState *sta)
{
  return new LumpedCapDelayCalc(sta);
}

LumpedCapDelayCalc::LumpedCapDelayCalc(StaState *sta) :
  ParallelDelayCalc(sta)
{
}

ArcDelayCalc *
LumpedCapDelayCalc::copy()
{
  return new LumpedCapDelayCalc(this);
}

Parasitic *
LumpedCapDelayCalc::findParasitic(const Pin *drvr_pin,
				  const RiseFall *rf,
				  const DcalcAnalysisPt *dcalc_ap)
{
  Parasitic *parasitic = nullptr;
  const Corner *corner = dcalc_ap->corner();
  // set_load net has precidence over parasitics.
  if (!sdc_->drvrPinHasWireCap(drvr_pin, corner)) {
    const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
    if (parasitics_->haveParasitics()) {
      // Prefer PiElmore.
      parasitic = parasitics_->findPiElmore(drvr_pin, rf, parasitic_ap);
      if (parasitic == nullptr) {
        Parasitic *parasitic_network =
          parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
        if (parasitic_network) {
          parasitics_->reduceToPiElmore(parasitic_network, drvr_pin,
                                        dcalc_ap->operatingConditions(),
                                        corner,
                                        dcalc_ap->constraintMinMax(),
                                        parasitic_ap);
          parasitic = parasitics_->findPiElmore(drvr_pin, rf, parasitic_ap);
          reduced_parasitic_drvrs_.push_back(drvr_pin);
        }
      }
    }
    else {
      const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
      Wireload *wireload = sdc_->wireload(cnst_min_max);
      if (wireload) {
        float pin_cap, wire_cap, fanout;
        bool has_net_load;
        graph_delay_calc_->netCaps(drvr_pin, rf, dcalc_ap,
                                   pin_cap, wire_cap, fanout, has_net_load);
        parasitic = parasitics_->estimatePiElmore(drvr_pin, rf, wireload,
                                                  fanout, pin_cap,
                                                  dcalc_ap->operatingConditions(),
                                                  corner,
                                                  cnst_min_max,
                                                  parasitic_ap);
        // Estimated parasitics are not recorded in the "database", so save
        // it for deletion after the drvr pin delay calc is finished.
        if (parasitic)
          unsaved_parasitics_.push_back(parasitic);
      }
    }
  }
  return parasitic;
}

ReducedParasiticType
LumpedCapDelayCalc::reducedParasiticType() const
{
  return ReducedParasiticType::pi_elmore;
}

float
LumpedCapDelayCalc::ceff(const TimingArc *,
			 const Slew &,
			 float load_cap,
			 const Parasitic *,
			 float,
			 const Pvt *,
			 const DcalcAnalysisPt *)
{
  return load_cap;
}

void
LumpedCapDelayCalc::gateDelay(const TimingArc *arc,
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
  gateDelayInit(arc, in_slew, drvr_parasitic);
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  debugPrint(debug_, "delay_calc", 3,
             "    in_slew = %s load_cap = %s related_load_cap = %s lumped",
             delayAsString(in_slew, this),
             units()->capacitanceUnit()->asString(load_cap),
             units()->capacitanceUnit()->asString(related_out_cap));
  if (model) {
    ArcDelay gate_delay1;
    Slew drvr_slew1;
    float in_slew1 = delayAsFloat(in_slew);
    // NaNs cause seg faults during table lookup.
    if (isnan(load_cap) || isnan(related_out_cap) || isnan(delayAsFloat(in_slew)))
      report_->error(710, "gate delay input variable is NaN");
    model->gateDelay(pvt, in_slew1, load_cap, related_out_cap,
		     pocv_enabled_, gate_delay1, drvr_slew1);
    gate_delay = gate_delay1;
    drvr_slew = drvr_slew1;
    drvr_slew_ = drvr_slew1;
  }
  else {
    gate_delay = delay_zero;
    drvr_slew = delay_zero;
    drvr_slew_ = 0.0;
  }
}

void
LumpedCapDelayCalc::loadDelay(const Pin *load_pin,
			      ArcDelay &wire_delay,
			      Slew &load_slew)
{
  Delay wire_delay1 = 0.0;
  Slew load_slew1 = drvr_slew_ * multi_drvr_slew_factor_;
  thresholdAdjust(load_pin, wire_delay1, load_slew1);
  wire_delay = wire_delay1;
  load_slew = load_slew1;
}

string
LumpedCapDelayCalc::reportGateDelay(const TimingArc *arc,
				    const Slew &in_slew,
				    float load_cap,
				    const Parasitic *,
				    float related_out_cap,
				    const Pvt *pvt,
				    const DcalcAnalysisPt *dcalc_ap,
				    int digits)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pvt, in_slew1, load_cap, related_out_cap,
                                  false, digits);
  }
  return "";
}

void
LumpedCapDelayCalc::checkDelay(const TimingArc *arc,
			       const Slew &from_slew,
			       const Slew &to_slew,
			       float related_out_cap,
			       const Pvt *pvt,
			       const DcalcAnalysisPt *dcalc_ap,
			       // Return values.
			       ArcDelay &margin)
{
  CheckTimingModel *model = checkModel(arc, dcalc_ap);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    model->checkDelay(pvt, from_slew1, to_slew1, related_out_cap, pocv_enabled_, margin);
  }
  else
    margin = delay_zero;
}

string
LumpedCapDelayCalc::reportCheckDelay(const TimingArc *arc,
				     const Slew &from_slew,
				     const char *from_slew_annotation,
				     const Slew &to_slew,
				     float related_out_cap,
				     const Pvt *pvt,
				     const DcalcAnalysisPt *dcalc_ap,
				     int digits)
{
  CheckTimingModel *model = checkModel(arc, dcalc_ap);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    return model->reportCheckDelay(pvt, from_slew1, from_slew_annotation,
                                   to_slew1, related_out_cap, false, digits);
  }
  return "";
}

} // namespace
