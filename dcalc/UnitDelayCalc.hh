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

#pragma once

#include "ArcDelayCalc.hh"

namespace sta {

// Unit delay calculator.
class UnitDelayCalc : public ArcDelayCalc
{
public:
  UnitDelayCalc(StaState *sta);
  ArcDelayCalc *copy() override;
  const char *name() const override { return "unit"; }
  Parasitic *findParasitic(const Pin *drvr_pin,
                           const RiseFall *rf,
                           const DcalcAnalysisPt *dcalc_ap) override;
  bool reduceSupported() const override { return false; }
  Parasitic *reduceParasitic(const Parasitic *parasitic_network,
                             const Pin *drvr_pin,
                             const RiseFall *rf,
                             const DcalcAnalysisPt *dcalc_ap) override;
  void reduceParasitic(const Parasitic *parasitic_network,
                       const Net *net,
                       const Corner *corner,
                       const MinMaxAll *min_max) override;
  void setDcalcArgParasiticSlew(ArcDcalcArg &gate,
                                const DcalcAnalysisPt *dcalc_ap) override;
  void setDcalcArgParasiticSlew(ArcDcalcArgSeq &gates,
                                const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResult inputPortDelay(const Pin *port_pin,
                                float in_slew,
                                const RiseFall *rf,
                                const Parasitic *parasitic,
                                const LoadPinIndexMap &load_pin_index_map,
                                const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResult gateDelay(const Pin *drvr_pin,
                           const TimingArc *arc,
                           const Slew &in_slew,
                           // Pass in load_cap or parasitic.
                           float load_cap,
                           const Parasitic *parasitic,
                           const LoadPinIndexMap &load_pin_index_map,
                           const DcalcAnalysisPt *dcalc_ap) override;
  ArcDcalcResultSeq gateDelays(ArcDcalcArgSeq &args,
                               const LoadPinIndexMap &load_pin_index_map,
                               const DcalcAnalysisPt *dcalc_ap) override;
  ArcDelay checkDelay(const Pin *check_pin,
                      const TimingArc *arc,
                      const Slew &from_slew,
                      const Slew &to_slew,
                      float related_out_cap,
                      const DcalcAnalysisPt *dcalc_ap) override;
  string reportGateDelay(const Pin *drvr_pin,
                         const TimingArc *arc,
                         const Slew &in_slew,
                         float load_cap,
                         const Parasitic *parasitic,
                         const LoadPinIndexMap &load_pin_index_map,
                         const DcalcAnalysisPt *dcalc_ap,
                         int digits) override;
  string reportCheckDelay(const Pin *check_pin,
                          const TimingArc *arc,
                          const Slew &from_slew,
                          const char *from_slew_annotation,
                          const Slew &to_slew,
                          float related_out_cap,
                          const DcalcAnalysisPt *dcalc_ap,
                          int digits) override;
  void finishDrvrPin() override;

protected:
  ArcDcalcResult unitDelayResult(const LoadPinIndexMap &load_pin_index_map);
};

ArcDelayCalc *
makeUnitDelayCalc(StaState *sta);

} // namespace
