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
#include "GraphDelayCalc.hh"
#include "Variables.hh"

namespace sta {

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
                                  const Scene *scene,
                                  const MinMax *min_max)
{
  Parasitic *parasitic = nullptr;
  Parasitics *parasitics = scene->parasitics(min_max);
  const Sdc *sdc = scene->sdc();
  if (parasitics == nullptr
      // set_load net has precedence over parasitics.
      || sdc->drvrPinHasWireCap(drvr_pin)
      || network_->direction(drvr_pin)->isInternal())
    return nullptr;

  // Prefer PiElmore.
  parasitic = parasitics->findPiElmore(drvr_pin, rf, min_max);
  if (parasitic)
    return parasitic;
  Parasitic *parasitic_network = parasitics->findParasiticNetwork(drvr_pin);
  if (parasitic_network) {
    parasitic = reduceParasitic(parasitic_network, drvr_pin, rf, scene, min_max);
    if (parasitic)
      return parasitic;
  }

  Wireload *wireload = sdc->wireload(min_max);
  if (wireload) {
    float pin_cap, wire_cap, fanout;
    bool has_net_load;
    graph_delay_calc_->netCaps(drvr_pin, rf, scene, min_max,
                               pin_cap, wire_cap, fanout, has_net_load);
    parasitic = parasitics->estimatePiElmore(drvr_pin, rf, wireload, fanout,
                                             pin_cap, scene, min_max);
  }
  return parasitic;
}

Parasitic *
LumpedCapDelayCalc::reduceParasitic(const Parasitic *parasitic_network,
                                    const Pin *drvr_pin,
                                    const RiseFall *rf,
                                    const Scene *scene,
                                    const MinMax *min_max)

{
  Parasitics *parasitics = scene->parasitics(min_max);
  return parasitics->reduceToPiElmore(parasitic_network, drvr_pin, rf,
                                      scene, min_max);
}

ArcDcalcResult
LumpedCapDelayCalc::inputPortDelay(const Pin *,
                                   float in_slew,
                                   const RiseFall *rf,
                                   const Parasitic *,
                                   const LoadPinIndexMap &load_pin_index_map,
                                   const Scene *,
                                   const MinMax *)
{
  const LibertyLibrary *drvr_library = network_->defaultLibertyLibrary();
  return makeResult(drvr_library,rf, delay_zero, in_slew, load_pin_index_map);
}

ArcDcalcResult
LumpedCapDelayCalc::gateDelay(const Pin *drvr_pin,
                              const TimingArc *arc,
                              const Slew &in_slew,
                              float load_cap,
                              const Parasitic *,
                              const LoadPinIndexMap &load_pin_index_map,
                              const Scene *scene,
                              const MinMax *min_max)
{
  GateTimingModel *model = arc->gateModel(scene, min_max);
  debugPrint(debug_, "delay_calc", 3,
             "    in_slew = {} load_cap = {} lumped",
             delayAsString(in_slew, this),
             units()->capacitanceUnit()->asString(load_cap));
  const RiseFall *rf = arc->toEdge()->asRiseFall();
  const LibertyLibrary *drvr_library = arc->to()->libertyLibrary();
  if (model) {
    float gate_delay, drvr_slew;
    float in_slew1 = delayAsFloat(in_slew);
    // NaNs cause seg faults during table lookup.
    if (std::isnan(load_cap) || std::isnan(in_slew.mean()))
      report_->error(1350, "gate delay input variable is NaN");
    const Pvt *pvt = pinPvt(drvr_pin, scene, min_max);
    model->gateDelay(pvt, in_slew1, load_cap, gate_delay, drvr_slew);

    // Fill in pocv parameters.
    ArcDelay gate_delay2(gate_delay);
    Slew drvr_slew2(drvr_slew);
    if (variables_->pocvEnabled())
      model->gateDelayPocv(pvt, in_slew1, load_cap, min_max, variables_->pocvMode(),
                           gate_delay2, drvr_slew2);

    return makeResult(drvr_library, rf, gate_delay2, drvr_slew2, load_pin_index_map);
  }
  else
    return makeResult(drvr_library, rf, delay_zero, delay_zero, load_pin_index_map);
}

ArcDcalcResult
LumpedCapDelayCalc::makeResult(const LibertyLibrary *drvr_library,
                               const RiseFall *rf,
                               const ArcDelay &gate_delay,
                               const Slew &drvr_slew,
                               const LoadPinIndexMap &load_pin_index_map)
{
  ArcDcalcResult dcalc_result(load_pin_index_map.size());
  dcalc_result.setGateDelay(gate_delay);
  dcalc_result.setDrvrSlew(drvr_slew);

  double drvr_slew1 = delayAsFloat(drvr_slew);
  for (const auto [load_pin, load_idx] : load_pin_index_map) {
    double wire_delay = 0.0;
    double load_slew = drvr_slew1;
    thresholdAdjust(load_pin, drvr_library, rf, wire_delay, load_slew);
    dcalc_result.setWireDelay(load_idx, wire_delay);
    dcalc_result.setLoadSlew(load_idx, load_slew);
  }
  return dcalc_result;
}

std::string
LumpedCapDelayCalc::reportGateDelay(const Pin *check_pin,
                                    const TimingArc *arc,
                                    const Slew &in_slew,
                                    float load_cap,
                                    const Parasitic *,
                                    const LoadPinIndexMap &,
                                    const Scene *scene,
                                    const MinMax *min_max,
                                    int digits)
{
  GateTimingModel *model = arc->gateModel(scene, min_max);
  if (model) {
    float in_slew1 = delayAsFloat(in_slew);
    return model->reportGateDelay(pinPvt(check_pin, scene, min_max),
                                  in_slew1, load_cap, min_max,
                                  PocvMode::scalar, digits);
  }
  return "";
}

} // namespace
