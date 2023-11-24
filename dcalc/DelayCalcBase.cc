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

#include "DelayCalcBase.hh"

#include "Liberty.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Parasitics.hh"

namespace sta {

DelayCalcBase::DelayCalcBase(StaState *sta) :
  ArcDelayCalc(sta)
{
}

void
DelayCalcBase::finishDrvrPin()
{
  for (auto parasitic : unsaved_parasitics_)
    parasitics_->deleteUnsavedParasitic(parasitic);
  unsaved_parasitics_.clear();
  for (auto drvr_pin : reduced_parasitic_drvrs_)
    parasitics_->deleteDrvrReducedParasitics(drvr_pin);
  reduced_parasitic_drvrs_.clear();
}

void
DelayCalcBase::inputPortDelay(const Pin *,
                              float in_slew,
                              const RiseFall *rf,
                              const Parasitic *parasitic,
                              const DcalcAnalysisPt *)
{
  drvr_cell_ = nullptr;
  drvr_library_ = network_->defaultLibertyLibrary();
  drvr_slew_ = in_slew;
  drvr_rf_ = rf;
  drvr_parasitic_ = parasitic;
  input_port_ = true;
}

void
DelayCalcBase::gateDelayInit(const TimingArc *arc,
                             const Slew &in_slew,
                             const Parasitic *drvr_parasitic)
{
  drvr_cell_ = arc->from()->libertyCell();
  drvr_library_ = drvr_cell_->libertyLibrary();
  drvr_rf_ = arc->toEdge()->asRiseFall();
  drvr_slew_ = in_slew;
  drvr_parasitic_ = drvr_parasitic;
  input_port_ = false;
}

// For DSPF on an input port the elmore delay is used as the time
// constant of an exponential waveform.  The delay to the logic
// threshold and slew are computed for the exponential waveform.
// Note that this uses the driver thresholds and relies on
// thresholdAdjust to convert the delay and slew to the load's thresholds.
void
DelayCalcBase::dspfWireDelaySlew(const Pin *,
                                 float elmore,
                                 ArcDelay &wire_delay,
                                 Slew &load_slew)
{
  float vth = drvr_library_->inputThreshold(drvr_rf_);
  float vl = drvr_library_->slewLowerThreshold(drvr_rf_);
  float vh = drvr_library_->slewUpperThreshold(drvr_rf_);
  float slew_derate = drvr_library_->slewDerateFromLibrary();
  wire_delay = -elmore * log(1.0 - vth);
  load_slew = drvr_slew_ + elmore * log((1.0 - vl) / (1.0 - vh)) / slew_derate;
}

void
DelayCalcBase::thresholdAdjust(const Pin *load_pin,
                               ArcDelay &load_delay,
                               Slew &load_slew)
{
  LibertyLibrary *load_library = thresholdLibrary(load_pin);
  if (load_library
      && drvr_library_
      && load_library != drvr_library_) {
    float drvr_vth = drvr_library_->outputThreshold(drvr_rf_);
    float load_vth = load_library->inputThreshold(drvr_rf_);
    float drvr_slew_delta = drvr_library_->slewUpperThreshold(drvr_rf_)
      - drvr_library_->slewLowerThreshold(drvr_rf_);
    float load_delay_delta =
      delayAsFloat(load_slew) * ((load_vth - drvr_vth) / drvr_slew_delta);
    load_delay += (drvr_rf_ == RiseFall::rise())
      ? load_delay_delta
      : -load_delay_delta;
    float load_slew_delta = load_library->slewUpperThreshold(drvr_rf_)
      - load_library->slewLowerThreshold(drvr_rf_);
    float drvr_slew_derate = drvr_library_->slewDerateFromLibrary();
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

} // namespace
