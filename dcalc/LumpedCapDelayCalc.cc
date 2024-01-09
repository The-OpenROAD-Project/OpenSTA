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
  // set_load net has precedence over parasitics.
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

ArcDcalcResult
LumpedCapDelayCalc::inputPortDelay(const Pin *,
                                   float in_slew,
                                   const RiseFall *rf,
                                   const Parasitic *,
                                   const LoadPinIndexMap &load_pin_index_map,
                                   const DcalcAnalysisPt *)
{
  const LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();
  return makeResult(drvr_library,rf, 0.0, in_slew, load_pin_index_map);
}

ArcDcalcResult
LumpedCapDelayCalc::gateDelay(const Pin *drvr_pin,
                              const TimingArc *arc,
			      const Slew &in_slew,
			      float load_cap,
			      const Parasitic *,
                              const LoadPinIndexMap &load_pin_index_map,
			      const DcalcAnalysisPt *dcalc_ap)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  debugPrint(debug_, "delay_calc", 3,
             "    in_slew = %s load_cap = %s lumped",
             delayAsString(in_slew, this),
             units()->capacitanceUnit()->asString(load_cap));
  const RiseFall *rf = arc->toEdge()->asRiseFall();
  const LibertyLibrary *drvr_library = arc->to()->libertyLibrary();
  if (model) {
    ArcDelay gate_delay;
    Slew drvr_slew;
    float in_slew1 = delayAsFloat(in_slew);
    // NaNs cause seg faults during table lookup.
    if (isnan(load_cap) || isnan(delayAsFloat(in_slew)))
      report_->error(1350, "gate delay input variable is NaN");
    model->gateDelay(pinPvt(drvr_pin, dcalc_ap), in_slew1, load_cap, pocv_enabled_,
                     gate_delay, drvr_slew);
    return makeResult(drvr_library, rf, gate_delay, drvr_slew, load_pin_index_map);
  }
  else
    return makeResult(drvr_library, rf, delay_zero, delay_zero, load_pin_index_map);
}

ArcDcalcResult
LumpedCapDelayCalc::makeResult(const LibertyLibrary *drvr_library,
                               const RiseFall *rf,
                               ArcDelay gate_delay,
                               Slew drvr_slew,
                               const LoadPinIndexMap &load_pin_index_map)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  dcalc_result.setGateDelay(gate_delay);
  dcalc_result.setDrvrSlew(drvr_slew);

  for (auto load_pin_index : load_pin_index_map) {
    const Pin *load_pin = load_pin_index.first;
    size_t load_idx = load_pin_index.second;
    ArcDelay wire_delay = 0.0;
    thresholdAdjust(load_pin, drvr_library, rf, wire_delay, drvr_slew);
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, drvr_slew);
  }
  return dcalc_result;
}

string
LumpedCapDelayCalc::reportGateDelay(const Pin *check_pin,
                                    const TimingArc *arc,
                                    const Slew &in_slew,
                                    float load_cap,
                                    const Parasitic *,
                                    const LoadPinIndexMap &,
                                    const DcalcAnalysisPt *dcalc_ap,
                                    int digits)
{
  GateTimingModel *model = gateModel(arc, dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pinPvt(check_pin, dcalc_ap), in_slew1, load_cap,
                                  false, digits);
  }
  return "";
}

} // namespace
