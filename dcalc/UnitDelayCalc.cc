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

#include "UnitDelayCalc.hh"

#include "Units.hh"

namespace sta {

ArcDelayCalc *
makeUnitDelayCalc(StaState *sta)
{
  return new UnitDelayCalc(sta);
}

UnitDelayCalc::UnitDelayCalc(StaState *sta) :
  ArcDelayCalc(sta)
{
}

ArcDelayCalc *
UnitDelayCalc::copy()
{
  return new UnitDelayCalc(this);
}

Parasitic *
UnitDelayCalc::findParasitic(const Pin *,
			     const RiseFall *,
			     const DcalcAnalysisPt *)
{
  return nullptr;
}

Parasitic *
UnitDelayCalc::reduceParasitic(const Parasitic *,
                               const Pin *,
                               const RiseFall *,
                               const DcalcAnalysisPt *)
{
  return nullptr;
}

void
UnitDelayCalc::reduceParasitic(const Parasitic *,
                               const Net *,
                               const Corner *,
                               const MinMaxAll *)
{
}

void
UnitDelayCalc::setDcalcArgParasiticSlew(ArcDcalcArg &,
                                        const DcalcAnalysisPt *)
{
}

void
UnitDelayCalc::setDcalcArgParasiticSlew(ArcDcalcArgSeq &,
                                        const DcalcAnalysisPt *)
{
}

ArcDcalcResult
UnitDelayCalc::inputPortDelay(const Pin *,
			      float,
			      const RiseFall *,
			      const Parasitic *,
                              const LoadPinIndexMap &load_pin_index_map,
                              const DcalcAnalysisPt *)
{
  return unitDelayResult(load_pin_index_map);
}

ArcDcalcResult
UnitDelayCalc::gateDelay(const Pin *,
                         const TimingArc *,
			 const Slew &,
			 float,
			 const Parasitic *,
                         const LoadPinIndexMap &load_pin_index_map,
                         const DcalcAnalysisPt *)
{
  return unitDelayResult(load_pin_index_map);
}

ArcDcalcResultSeq
UnitDelayCalc::gateDelays(ArcDcalcArgSeq &dcalc_args,
                          const LoadPinIndexMap &load_pin_index_map,
                          const DcalcAnalysisPt *)
{
  size_t drvr_count = dcalc_args.size();
  ArcDcalcResultSeq dcalc_results(drvr_count);
  for (size_t drvr_idx = 0; drvr_idx < drvr_count; drvr_idx++) {
    ArcDcalcResult &dcalc_result = dcalc_results[drvr_idx];
    dcalc_result = unitDelayResult(load_pin_index_map);
  }
  return dcalc_results;
}

ArcDcalcResult
UnitDelayCalc::unitDelayResult(const LoadPinIndexMap &load_pin_index_map)
{
  size_t load_count = load_pin_index_map.size();
  ArcDcalcResult dcalc_result(load_count);
  dcalc_result.setGateDelay(units_->timeUnit()->scale());
  dcalc_result.setDrvrSlew(0.0);
  for (size_t load_idx = 0; load_idx < load_count; load_idx++) {
    dcalc_result.setWireDelay(load_idx, 0.0);
    dcalc_result.setLoadSlew(load_idx, 0.0);
  }
  return dcalc_result;
}

string
UnitDelayCalc::reportGateDelay(const Pin *,
                               const TimingArc *,
			       const Slew &,
			       float,
			       const Parasitic *,
                               const LoadPinIndexMap &,
			       const DcalcAnalysisPt *,
			       int)
{
  string result("Delay = 1.0\n");
  result += "Slew = 0.0\n";
  return result;
}

ArcDelay 
UnitDelayCalc::checkDelay(const Pin *,
                          const TimingArc *,
			  const Slew &,
			  const Slew &,
			  float,
			  const DcalcAnalysisPt *)
{
  return units_->timeUnit()->scale();
}

string
UnitDelayCalc::reportCheckDelay(const Pin *,
                                const TimingArc *,
				const Slew &,
				const char *,
				const Slew &,
				float,
				const DcalcAnalysisPt *,
				int)
{
  return "Check = 1.0\n";
}

void
UnitDelayCalc::finishDrvrPin()
{
}

} // namespace
