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

#pragma once

#include "ArcDelayCalc.hh"

namespace sta {

class DelayCalcBase : public ArcDelayCalc
{
public:
  explicit DelayCalcBase(StaState *sta);
  void inputPortDelay(const Pin *port_pin,
                      float in_slew,
                      const RiseFall *rf,
                      const Parasitic *parasitic,
                      const DcalcAnalysisPt *dcalc_ap) override;
 void finishDrvrPin() override;

protected:
  void gateDelayInit(const TimingArc *arc,
                     const Slew &in_slew,
                     const Parasitic *drvr_parasitic);
  // Find the liberty library to use for logic/slew thresholds.
  LibertyLibrary *thresholdLibrary(const Pin *load_pin);
  // Adjust load_delay and load_slew from driver thresholds to load thresholds.
  void thresholdAdjust(const Pin *load_pin,
		       ArcDelay &load_delay,
		       Slew &load_slew);
  // Helper function for input ports driving dspf parasitic.
  void dspfWireDelaySlew(const Pin *load_pin,
			 float elmore,
			 ArcDelay &wire_delay,
			 Slew &load_slew);

  Slew drvr_slew_;
  const LibertyCell *drvr_cell_;
  const LibertyLibrary *drvr_library_;
  const Parasitic *drvr_parasitic_;
  bool input_port_;
  const RiseFall *drvr_rf_;
  // Parasitics returned by findParasitic that are reduced or estimated
  // that can be deleted after delay calculation for the driver pin
  // is finished.
  Vector<Parasitic*> unsaved_parasitics_;
  Vector<const Pin *> reduced_parasitic_drvrs_;
};

} // namespace
