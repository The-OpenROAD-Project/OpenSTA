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

#include "DelayCalcBase.hh"

#include "Liberty.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "TableModel.hh"
#include "Network.hh"
#include "Parasitics.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"

namespace sta {

using std::log;

DelayCalcBase::DelayCalcBase(StaState *sta) :
  ArcDelayCalc(sta)
{
}

void
DelayCalcBase::reduceParasitic(const Parasitic *parasitic_network,
                               const Net *net,
                               const Corner *corner,
                               const MinMaxAll *min_max)
{
  NetConnectedPinIterator *pin_iter = network_->connectedPinIterator(net);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->isDriver(pin)) {
      for (RiseFall *rf : RiseFall::range()) {
        for (const MinMax *min_max : min_max->range()) {
          if (corner == nullptr) {
            for (const Corner *corner1 : *corners_) {
              DcalcAnalysisPt *dcalc_ap = corner1->findDcalcAnalysisPt(min_max);
              reduceParasitic(parasitic_network, pin, rf, dcalc_ap);
            }
          }
          else {
            DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
            reduceParasitic(parasitic_network, pin, rf, dcalc_ap);
          }
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
                          const DcalcAnalysisPt *dcalc_ap)
{
  CheckTimingModel *model = arc->checkModel(dcalc_ap);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    return model->checkDelay(pinPvt(check_pin, dcalc_ap), from_slew1, to_slew1,
                             related_out_cap, pocv_enabled_);
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
                                const DcalcAnalysisPt *dcalc_ap,
                                int digits)
{
  CheckTimingModel *model = arc->checkModel(dcalc_ap);
  if (model) {
    float from_slew1 = delayAsFloat(from_slew);
    float to_slew1 = delayAsFloat(to_slew);
    return model->reportCheckDelay(pinPvt(check_pin, dcalc_ap), from_slew1,
                                   from_slew_annotation, to_slew1,
                                   related_out_cap, false, digits);
  }
  return "";
}

const Pvt *
DelayCalcBase::pinPvt(const Pin *pin,
                      const DcalcAnalysisPt *dcalc_ap)
{
  const Instance *drvr_inst = network_->instance(pin);
  const Pvt *pvt = sdc_->pvt(drvr_inst, dcalc_ap->constraintMinMax());
  if (pvt == nullptr)
    pvt = dcalc_ap->operatingConditions();
  return pvt;
}

void
DelayCalcBase::setDcalcArgParasiticSlew(ArcDcalcArg &gate,
                                        const DcalcAnalysisPt *dcalc_ap)
{
  const Pin *drvr_pin = gate.drvrPin();
  if (drvr_pin) {
    const Parasitic *parasitic;
    float load_cap;
    graph_delay_calc_->parasiticLoad(drvr_pin, gate.drvrEdge(), dcalc_ap,
                                     nullptr, this, load_cap,
                                     parasitic);
    gate.setLoadCap(load_cap);
    gate.setParasitic(parasitic);
    const Pin *in_pin = gate.inPin();
    const Vertex *in_vertex = graph_->pinLoadVertex(in_pin);
    const Slew &in_slew = graph_delay_calc_->edgeFromSlew(in_vertex, gate.inEdge(),
                                                          gate.edge(), dcalc_ap);
    gate.setInSlew(in_slew);
  }
}

void
DelayCalcBase::setDcalcArgParasiticSlew(ArcDcalcArgSeq &gates,
                                        const DcalcAnalysisPt *dcalc_ap)
{
  for (ArcDcalcArg &gate : gates)
    setDcalcArgParasiticSlew(gate, dcalc_ap);
}

} // namespace
