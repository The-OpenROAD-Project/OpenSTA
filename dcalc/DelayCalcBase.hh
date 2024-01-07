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

class GateTableModel;

class DelayCalcBase : public ArcDelayCalc
{
public:
  explicit DelayCalcBase(StaState *sta);
  void finishDrvrPin() override;

  ArcDelay checkDelay(const Pin *check_pin,
                      const TimingArc *arc,
                      const Slew &from_slew,
                      const Slew &to_slew,
                      float related_out_cap,
                      const DcalcAnalysisPt *dcalc_ap) override;

  string reportCheckDelay(const Pin *check_pin,
                          const TimingArc *arc,
                          const Slew &from_slew,
                          const char *from_slew_annotation,
                          const Slew &to_slew,
                          float related_out_cap,
                          const DcalcAnalysisPt *dcalc_ap,
                          int digits) override;

protected:
  GateTimingModel *gateModel(const TimingArc *arc,
			     const DcalcAnalysisPt *dcalc_ap) const;
  GateTableModel *gateTableModel(const TimingArc *arc,
                                 const DcalcAnalysisPt *dcalc_ap) const;
  CheckTimingModel *checkModel(const TimingArc *arc,
			       const DcalcAnalysisPt *dcalc_ap) const;
  TimingModel *model(const TimingArc *arc,
		     const DcalcAnalysisPt *dcalc_ap) const;
  // Find the liberty library to use for logic/slew thresholds.
  LibertyLibrary *thresholdLibrary(const Pin *load_pin);
  // Adjust load_delay and load_slew from driver thresholds to load thresholds.
  void thresholdAdjust(const Pin *load_pin,
                       const LibertyLibrary *drvr_library,
                       const RiseFall *rf,
		       ArcDelay &load_delay,
		       Slew &load_slew);
  // Helper function for input ports driving dspf parasitic.
  void dspfWireDelaySlew(const Pin *load_pin,
			 const RiseFall *rf,
                         Slew drvr_slew,
                         float elmore,
                         // Return values.
			 ArcDelay &wire_delay,
			 Slew &load_slew);
  const Pvt *pinPvt(const Pin *pin,
                    const DcalcAnalysisPt *dcalc_ap);

  // Parasitics returned by findParasitic that are reduced or estimated
  // that can be deleted after delay calculation for the driver pin
  // is finished.
  Vector<Parasitic*> unsaved_parasitics_;
  Vector<const Pin *> reduced_parasitic_drvrs_;
};

} // namespace
