// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "Machine.hh"
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
#include "LumpedCapDelayCalc.hh"

namespace sta {

ArcDelayCalc *
makeLumpedCapDelayCalc(StaState *sta)
{
  return new LumpedCapDelayCalc(sta);
}

LumpedCapDelayCalc::LumpedCapDelayCalc(StaState *sta) :
  ArcDelayCalc(sta)
{
}

ArcDelayCalc *
LumpedCapDelayCalc::copy()
{
  return new LumpedCapDelayCalc(this);
}

void
LumpedCapDelayCalc::findParasitic(const Pin *drvr_pin,
				  const TransRiseFall *tr,
				  const DcalcAnalysisPt *dcalc_ap,
				  // Return values.
				  Parasitic *&parasitic,
				  bool &delete_at_finish)
{
  parasitic = nullptr;
  delete_at_finish = false;
  // set_load has precidence over parasitics.
  if (!sdc_->drvrPinHasWireCap(drvr_pin)) {
    const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
    // Prefer capacitive load.
    parasitic = parasitics_->findLumpedElmore(drvr_pin, tr, parasitic_ap);
    if (parasitic == nullptr)
      parasitic = parasitics_->findPiElmore(drvr_pin, tr, parasitic_ap);
    if (parasitic == nullptr) {
      parasitic = parasitics_->findParasiticNetwork(drvr_pin, parasitic_ap);
      if (parasitic) {
	parasitic = parasitics_->reduceToPiElmore(parasitic, drvr_pin, tr,
						  dcalc_ap->operatingConditions(),
						  dcalc_ap->corner(),
						  dcalc_ap->constraintMinMax(),
						  parasitic_ap);
	delete_at_finish = true;
      }
    }
    if (parasitic == nullptr) {
      const MinMax *cnst_min_max = dcalc_ap->constraintMinMax();
      Wireload *wireload = sdc_->wireloadDefaulted(cnst_min_max);
      if (wireload) {
	float pin_cap, wire_cap, fanout;
	bool has_wire_cap;
	graph_delay_calc_->netCaps(drvr_pin, tr, dcalc_ap,
				   pin_cap, wire_cap, fanout, has_wire_cap);
	parasitic =
	  parasitics_->estimatePiElmore(drvr_pin, tr, wireload, fanout,pin_cap,
					dcalc_ap->operatingConditions(),
					dcalc_ap->corner(),
					cnst_min_max,
					parasitic_ap);
	delete_at_finish = true;
      }
    }
  }
}

void
LumpedCapDelayCalc::finish(const Pin *drvr_pin,
			   const TransRiseFall *tr,
			   const DcalcAnalysisPt *dcalc_ap,
			   Parasitic *parasitic,
			   bool delete_at_finish)
{
  if (parasitic) {
    const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
    if (delete_at_finish)
      parasitics_->deleteParasitic(drvr_pin, tr, parasitic_ap, parasitic);
    else
      parasitics_->finish(parasitic);
  }
}

void
LumpedCapDelayCalc::inputPortDelay(const Pin *, float in_slew,
				   const TransRiseFall *tr,
				   Parasitic *,
				   const DcalcAnalysisPt *)
{
  drvr_slew_ = in_slew;
  drvr_tr_ = tr;
  drvr_library_ = network_->defaultLibertyLibrary();
  multi_drvr_slew_factor_ = 1.0F;
}

float
LumpedCapDelayCalc::ceff(const LibertyCell *,
			 TimingArc *,
			 const Slew &,
			 float load_cap,
			 Parasitic *,
			 float,
			 const Pvt *,
			 const DcalcAnalysisPt *)
{
  return load_cap;
}

void
LumpedCapDelayCalc::gateDelay(const LibertyCell *drvr_cell,
			      TimingArc *arc,
			      const Slew &in_slew,
			      float load_cap,
			      Parasitic *,
			      float related_out_cap,
			      const Pvt *pvt,
			      const DcalcAnalysisPt *dcalc_ap,
			      // Return values.
			      ArcDelay &gate_delay,
			      Slew &drvr_slew)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  debugPrint3(debug_, "delay_calc", 3,
	      "    in_slew = %s load_cap = %s related_load_cap = %s lumped\n",
	      delayAsString(in_slew, this),
	      units()->capacitanceUnit()->asString(load_cap),
	      units()->capacitanceUnit()->asString(related_out_cap));
  if (model) {
    ArcDelay gate_delay1;
    Slew drvr_slew1;
    float in_slew1 = delayAsFloat(in_slew);
    model->gateDelay(drvr_cell, pvt, in_slew1, load_cap, related_out_cap,
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
  drvr_tr_ = arc->toTrans()->asRiseFall();
  drvr_library_ = drvr_cell->libertyLibrary();
  multi_drvr_slew_factor_ = 1.0F;
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
  load_slew = load_slew1 * multi_drvr_slew_factor_;
}

void
LumpedCapDelayCalc::thresholdAdjust(const Pin *load_pin,
				    ArcDelay &load_delay,
				    Slew &load_slew)
{
  LibertyLibrary *load_library = thresholdLibrary(load_pin);
  if (load_library
      && drvr_library_
      && load_library != drvr_library_) {
    float drvr_vth = drvr_library_->outputThreshold(drvr_tr_);
    float load_vth = load_library->inputThreshold(drvr_tr_);
    float drvr_slew_delta = drvr_library_->slewUpperThreshold(drvr_tr_)
      - drvr_library_->slewLowerThreshold(drvr_tr_);
    float load_delay_delta =
      delayAsFloat(load_slew) * ((load_vth - drvr_vth) / drvr_slew_delta);
    load_delay += (drvr_tr_ == TransRiseFall::rise())
      ? load_delay_delta
      : -load_delay_delta;
    float load_slew_delta = load_library->slewUpperThreshold(drvr_tr_)
      - load_library->slewLowerThreshold(drvr_tr_);
    float drvr_slew_derate = drvr_library_->slewDerateFromLibrary();
    float load_slew_derate = load_library->slewDerateFromLibrary();
    load_slew = load_slew * ((load_slew_delta / load_slew_derate)
			     / (drvr_slew_delta / drvr_slew_derate));
  }
}

LibertyLibrary *
LumpedCapDelayCalc::thresholdLibrary(const Pin *load_pin)
{
  if (network_->isTopLevelPort(load_pin))
    // Input/output slews use the default (first read) library
    // for slew thresholds.
    return network_->defaultLibertyLibrary();
  else {
    LibertyPort *lib_port = network_->libertyPort(load_pin);
    if (lib_port)
      return lib_port->libertyCell()->libertyLibrary();
    else
      return network_->defaultLibertyLibrary();
  }
}

void
LumpedCapDelayCalc::reportGateDelay(const LibertyCell *drvr_cell,
				    TimingArc *arc,
				    const Slew &in_slew,
				    float load_cap,
				    Parasitic *,
				    float related_out_cap,
				    const Pvt *pvt,
				    const DcalcAnalysisPt *dcalc_ap,
				    int digits,
				    string *result)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    model->reportGateDelay(drvr_cell, pvt, in_slew1, load_cap,
			   related_out_cap, false, digits, result);
  }
}

void
LumpedCapDelayCalc::checkDelay(const LibertyCell *cell,
			       TimingArc *arc,
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
    model->checkDelay(cell, pvt, from_slew1, to_slew1, related_out_cap,
		      false, margin);
  }
  else
    margin = delay_zero;
}

void
LumpedCapDelayCalc::reportCheckDelay(const LibertyCell *cell,
				     TimingArc *arc,
				     const Slew &from_slew,
				     const char *from_slew_annotation,
				     const Slew &to_slew,
				     float related_out_cap,
				     const Pvt *pvt,
				     const DcalcAnalysisPt *dcalc_ap,
				     int digits,
				     string *result)
{
  CheckTimingModel *model = checkModel(arc, dcalc_ap);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    model->reportCheckDelay(cell, pvt, from_slew1, from_slew_annotation, to_slew1,
			    related_out_cap, false, digits, result);
  }
}

void
LumpedCapDelayCalc::setMultiDrvrSlewFactor(float factor)
{
  multi_drvr_slew_factor_ = factor;
}

} // namespace
