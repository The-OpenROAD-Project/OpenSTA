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

#include <vector>

#include "DelayCalcBase.hh"

namespace sta {

// Delay calculation for parallel gates based on using parallel drive resistance.
class ParallelDelayCalc : public DelayCalcBase
{
public:
  explicit ParallelDelayCalc(StaState *sta);
  void inputPortDelay(const Pin *port_pin,
                      float in_slew,
                      const RiseFall *rf,
                      const Parasitic *parasitic,
                      const DcalcAnalysisPt *dcalc_ap) override;
  void gateDelayInit(const TimingArc *arc,
                     const Slew &in_slew,
                     const Parasitic *drvr_parasitic);
  void findParallelGateDelays(const MultiDrvrNet *multi_drvr,
                              GraphDelayCalc *dcalc) override;
  void parallelGateDelay(const Pin *drvr_pin,
                         const TimingArc *arc,
                         const Slew &from_slew,
                         float load_cap,
                         const Parasitic *drvr_parasitic,
                         float related_out_cap,
                         const Pvt *pvt,
                         const DcalcAnalysisPt *dcalc_ap,
                         // Return values.
                         ArcDelay &gate_delay,
                         Slew &gate_slew) override;

protected:
  void findMultiDrvrGateDelay(const MultiDrvrNet *multi_drvr,
                              const RiseFall *drvr_rf,
                              const DcalcAnalysisPt *dcalc_ap,
                              GraphDelayCalc *dcalc,
                              // Return values.
                              ArcDelay &parallel_delay,
                              Slew &parallel_slew);

  // [drvr_rf->index][dcalc_ap->index]
  vector<ArcDelay> parallel_delays_;
  // [drvr_rf->index][dcalc_ap->index]
  vector<Slew> parallel_slews_;
  float multi_drvr_slew_factor_;
};

} // namespace
