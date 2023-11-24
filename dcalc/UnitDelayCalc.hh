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

// Unit delay calculator.
class UnitDelayCalc : public ArcDelayCalc
{
public:
  UnitDelayCalc(StaState *sta);
  ArcDelayCalc *copy() override;
  Parasitic *findParasitic(const Pin *drvr_pin,
                           const RiseFall *rf,
                           const DcalcAnalysisPt *dcalc_ap) override;
  ReducedParasiticType reducedParasiticType() const override;
  void inputPortDelay(const Pin *port_pin,
                      float in_slew,
                      const RiseFall *rf,
                      const Parasitic *parasitic,
                      const DcalcAnalysisPt *dcalc_ap) override;
  void gateDelay(const TimingArc *arc,
                 const Slew &in_slew,
                 float load_cap,
                 const Parasitic *drvr_parasitic,
                 float related_out_cap,
                 const Pvt *pvt,
                 const DcalcAnalysisPt *dcalc_ap,
                 // Return values.
                 ArcDelay &gate_delay,
                 Slew &drvr_slew) override;
  void findParallelGateDelays(const MultiDrvrNet *multi_drvr,
                              GraphDelayCalc *dcalc) override;
  // Retrieve the delay and slew for one parallel gate.
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
  void loadDelay(const Pin *load_pin,
                 // Return values.
                 ArcDelay &wire_delay,
                 Slew &load_slew) override;
  float ceff(const TimingArc *arc,
             const Slew &in_slew,
             float load_cap,
             const Parasitic *drvr_parasitic,
             float related_out_cap,
             const Pvt *pvt,
             const DcalcAnalysisPt *dcalc_ap) override;
  void checkDelay(const TimingArc *arc,
                  const Slew &from_slew,
                  const Slew &to_slew,
                  float related_out_cap,
                  const Pvt *pvt,
                  const DcalcAnalysisPt *dcalc_ap,
                  // Return values.
                  ArcDelay &margin) override;
  string reportGateDelay(const TimingArc *arc,
                         const Slew &in_slew,
                         float load_cap,
                         const Parasitic *drvr_parasitic,
                         float related_out_cap,
                         const Pvt *pvt,
                         const DcalcAnalysisPt *dcalc_ap,
                         int digits) override;
  string reportCheckDelay(const TimingArc *arc,
                          const Slew &from_slew,
                          const char *from_slew_annotation,
                          const Slew &to_slew,
                          float related_out_cap,
                          const Pvt *pvt,
                          const DcalcAnalysisPt *dcalc_ap,
                          int digits) override;
 void finishDrvrPin() override;
};

ArcDelayCalc *
makeUnitDelayCalc(StaState *sta);

} // namespace
