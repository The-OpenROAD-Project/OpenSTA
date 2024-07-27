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

#include "LumpedCapDelayCalc.hh"

#include <cmath>  // isnan

#include "Debug.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
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
  if (sdc_->drvrPinHasWireCap(drvr_pin, corner)
      || network_->direction(drvr_pin)->isInternal())
   return nullptr;
  const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  // Prefer PiElmore.
  parasitic = parasitics_->findPiElmore(drvr_pin, rf, parasitic_ap);
  if (parasitic)
    return parasitic;
  Parasitic *parasitic_network = parasitics_->findParasiticNetwork(drvr_pin,
                                                                   parasitic_ap);
  if (parasitic_network) {
    parasitic = reduceParasitic(parasitic_network, drvr_pin, rf, dcalc_ap);
    if (parasitic)
      return parasitic;
  }
  const MinMax *min_max = dcalc_ap->constraintMinMax();
  Wireload *wireload = sdc_->wireload(min_max);
  if (wireload) {
    float pin_cap, wire_cap, fanout;
    bool has_net_load;
    graph_delay_calc_->netCaps(drvr_pin, rf, dcalc_ap,
                               pin_cap, wire_cap, fanout, has_net_load);
    parasitic = parasitics_->estimatePiElmore(drvr_pin, rf, wireload, fanout,
                                              pin_cap, corner, min_max);
  }
  return parasitic;
}

Parasitic *
LumpedCapDelayCalc::reduceParasitic(const Parasitic *parasitic_network,
                                    const Pin *drvr_pin,
                                    const RiseFall *rf,
                                    const DcalcAnalysisPt *dcalc_ap)

{
  const Corner *corner = dcalc_ap->corner();
  const ParasiticAnalysisPt *parasitic_ap = dcalc_ap->parasiticAnalysisPt();
  return parasitics_->reduceToPiElmore(parasitic_network, drvr_pin, rf,
                                       corner, dcalc_ap->constraintMinMax(),
                                       parasitic_ap);
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
  GateTimingModel *model = arc->gateModel(dcalc_ap);
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

  for (const auto [load_pin, load_idx] : load_pin_index_map) {
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
  GateTimingModel *model = arc->gateModel(dcalc_ap);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pinPvt(check_pin, dcalc_ap), in_slew1, load_cap,
                                  false, digits);
  }
  return "";
}

} // namespace
