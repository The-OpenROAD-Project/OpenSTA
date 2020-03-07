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

#include "Machine.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Parasitics.hh"
#include "GraphDelayCalc.hh"
#include "RCDelayCalc.hh"

namespace sta {

RCDelayCalc::RCDelayCalc(StaState *sta) :
  LumpedCapDelayCalc(sta)
{
}

ArcDelayCalc *
RCDelayCalc::copy()
{
  return new RCDelayCalc(this);
}

void
RCDelayCalc::inputPortDelay(const Pin *,
			    float in_slew,
			    const RiseFall *rf,
			    Parasitic *parasitic,
			    const DcalcAnalysisPt *)
{
  drvr_parasitic_ = parasitic;
  drvr_slew_ = in_slew;
  drvr_rf_ = rf;
  drvr_cell_ = nullptr;
  drvr_library_ = network_->defaultLibertyLibrary();
  multi_drvr_slew_factor_ = 1.0F;
}

// For DSPF on an input port the elmore delay is used as the time
// constant of an exponential waveform.  The delay to the logic
// threshold and slew are computed for the exponential waveform.
// Note that this uses the driver thresholds and relies on
// thresholdAdjust to convert the delay and slew to the load's thresholds.
void
RCDelayCalc::dspfWireDelaySlew(const Pin *,
			       float elmore,
			       ArcDelay &wire_delay,
			       Slew &load_slew)
{
  float vth = .5;
  float vl = .2;
  float vh = .8;
  float slew_derate = 1.0;
  if (drvr_library_) {
    vth = drvr_library_->inputThreshold(drvr_rf_);
    vl = drvr_library_->slewLowerThreshold(drvr_rf_);
    vh = drvr_library_->slewUpperThreshold(drvr_rf_);
    slew_derate = drvr_library_->slewDerateFromLibrary();
  }
  wire_delay = static_cast<float>(-elmore * log(1.0 - vth));
  load_slew = (drvr_slew_ + elmore * log((1.0 - vl) / (1.0 - vh))
	       / slew_derate) * multi_drvr_slew_factor_;
}

} // namespace
