// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "DelayCalcBase.hh"

#include "Liberty.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "TableModel.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Scene.hh"
#include "GraphDelayCalc.hh"
#include "Variables.hh"

namespace sta {

using std::string;
using std::log;

DelayCalcBase::DelayCalcBase(StaState *sta) :
  ArcDelayCalc(sta)
{
}

void
DelayCalcBase::reduceParasitic(const Parasitic *parasitic_network,
                               const Net *net,
                               const Scene *scene,
                               const MinMaxAll *min_max)
{
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)) {
      for (const RiseFall *rf : RiseFall::range()) {
        for (const MinMax *min_max : min_max->range()) {
          if (scene == nullptr) {
            for (const Scene *scene1 : scenes_)
              reduceParasitic(parasitic_network, pin, rf, scene1, min_max);
          }
          else
            reduceParasitic(parasitic_network, pin, rf, scene, min_max);
        }
      }
    }
  }
  delete pin_iter;
}

void
DelayCalcBase::finishDrvrPin()
{
}

// For DSPF on an input port the elmore delay is used as the time
// constant of an exponential waveform.  The delay to the logic
// threshold and slew are computed for the exponential waveform.
// Note that this uses the driver thresholds and relies on
// thresholdAdjust to convert the delay and slew to the load's thresholds.
void
DelayCalcBase::dspfWireDelaySlew(const Pin *load_pin,
                                 const RiseFall *rf,
                                 Slew drvr_slew,
                                 float elmore,
                                 ArcDelay &wire_delay,
                                 Slew &load_slew)
{
  
  LibertyLibrary *load_library = thresholdLibrary(load_pin);
  float vth = 0.5;
  float vl = 0.2;
  float vh = 0.8;
  float slew_derate = 1.0;
  if (load_library) {
    vth = load_library->inputThreshold(rf);
    vl = load_library->slewLowerThreshold(rf);
    vh = load_library->slewUpperThreshold(rf);
    slew_derate = load_library->slewDerateFromLibrary();
  }
  wire_delay = -elmore * log(1.0 - vth);
  load_slew = drvr_slew + elmore * log((1.0 - vl) / (1.0 - vh)) / slew_derate;
  load_slew = drvr_slew + elmore * log((1.0 - vl) / (1.0 - vh)) / slew_derate;
}

void
DelayCalcBase::thresholdAdjust(const Pin *load_pin,
                               const LibertyLibrary *drvr_library,
                               const RiseFall *rf,
                               ArcDelay &load_delay,
                               Slew &load_slew)
{
  LibertyLibrary *load_library = thresholdLibrary(load_pin);
  if (load_library
      && drvr_library
      && load_library != drvr_library) {
    float drvr_vth = drvr_library->outputThreshold(rf);
    float load_vth = load_library->inputThreshold(rf);
    float drvr_slew_delta = drvr_library->slewUpperThreshold(rf)
      - drvr_library->slewLowerThreshold(rf);
    float load_delay_delta =
      delayAsFloat(load_slew) * ((load_vth - drvr_vth) / drvr_slew_delta);
    load_delay += (rf == RiseFall::rise())
      ? load_delay_delta
      : -load_delay_delta;
    float load_slew_delta = load_library->slewUpperThreshold(rf)
      - load_library->slewLowerThreshold(rf);
    float drvr_slew_derate = drvr_library->slewDerateFromLibrary();
    float load_slew_derate = load_library->slewDerateFromLibrary();
    load_slew = load_slew * ((load_slew_delta / load_slew_derate)
                             / (drvr_slew_delta / drvr_slew_derate));
  }
}

LibertyLibrary *
DelayCalcBase::thresholdLibrary(const Pin *load_pin)
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

ArcDelay
DelayCalcBase::checkDelay(const Pin *check_pin,
                          const TimingArc *arc,
                          const Slew &from_slew,
                          const Slew &to_slew,
                          float related_out_cap,
                          const Scene *scene,
                          const MinMax *min_max)
{
  CheckTimingModel *model = arc->checkModel(scene, min_max);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    return model->checkDelay(pinPvt(check_pin, scene, min_max),
                             from_slew1, to_slew1,
                             related_out_cap,
                             variables_->pocvEnabled());
  }
  else
    return delay_zero;
}

string
DelayCalcBase::reportCheckDelay(const Pin *check_pin,
                                const TimingArc *arc,
                                const Slew &from_slew,
                                const char *from_slew_annotation,
                                const Slew &to_slew,
                                float related_out_cap,
                                const Scene *scene,
                                const MinMax *min_max,
                                int digits)
{
  CheckTimingModel *model = arc->checkModel(scene, min_max);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    return model->reportCheckDelay(pinPvt(check_pin, scene, min_max),
                                   from_slew1, from_slew_annotation,
                                   to_slew1, related_out_cap, false,
                                   digits);
  }
  return "";
}

const Pvt *
DelayCalcBase::pinPvt(const Pin *pin,
                      const Scene *scene,
                      const MinMax *min_max)
{
  const Instance *drvr_inst = network_->instance(pin);
  const Sdc *sdc = scene->sdc();
  const Pvt *pvt = sdc->pvt(drvr_inst, min_max);
  if (pvt == nullptr)
    pvt = sdc->operatingConditions(min_max);
  return pvt;
}

void
DelayCalcBase::setDcalcArgParasiticSlew(ArcDcalcArg &gate,
                                        const Scene *scene,
                                        const MinMax *min_max)
{
  const Pin *drvr_pin = gate.drvrPin();
  if (drvr_pin) {
    const Parasitic *parasitic;
    float load_cap;
    graph_delay_calc_->parasiticLoad(drvr_pin, gate.drvrEdge(),
                                     scene, min_max,
                                     nullptr, this, load_cap,
                                     parasitic);
    gate.setLoadCap(load_cap);
    gate.setParasitic(parasitic);
    const Pin *in_pin = gate.inPin();
    const Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
    const Slew &in_slew = graph_delay_calc_->edgeFromSlew(in_vertex, gate.inEdge(),
                                                          gate.edge(),
                                                          scene, min_max);
    gate.setInSlew(in_slew);
  }
}

void
DelayCalcBase::setDcalcArgParasiticSlew(ArcDcalcArgSeq &gates,
                                        const Scene *scene,
                                        const MinMax *min_max)
{
  for (ArcDcalcArg &gate : gates)
    setDcalcArgParasiticSlew(gate, scene, min_max);
}

} // namespace
