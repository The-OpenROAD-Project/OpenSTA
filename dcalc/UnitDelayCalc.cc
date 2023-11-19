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

ReducedParasiticType
UnitDelayCalc::reducedParasiticType() const
{
  return ReducedParasiticType::none;
}

void
UnitDelayCalc::inputPortDelay(const Pin *,
			      float,
			      const RiseFall *,
			      const Parasitic *,
			      const DcalcAnalysisPt *)
{
}

void
UnitDelayCalc::gateDelay(const TimingArc *,
			 const Slew &,
			 float,
			 const Parasitic *,
			 float,
			 const Pvt *, const DcalcAnalysisPt *,
			 // Return values.
			 ArcDelay &gate_delay, Slew &drvr_slew)
{
  gate_delay = units_->timeUnit()->scale();
  drvr_slew = 0.0;
}

void
UnitDelayCalc::findParallelGateDelays(const MultiDrvrNet *,
                                      GraphDelayCalc *)
{
}

void
UnitDelayCalc::parallelGateDelay(const Pin *,
                                 const TimingArc *,
                                 const Slew &,
                                 float,
                                 const Parasitic *,
                                 float,
                                 const Pvt *,
                                 const DcalcAnalysisPt *,
                                 // Return values.
                                 ArcDelay &gate_delay,
                                 Slew &gate_slew)
{
  gate_delay = units_->timeUnit()->scale();
  gate_slew = 0.0;
}

void
UnitDelayCalc::loadDelay(const Pin *,
			 ArcDelay &wire_delay,
			 Slew &load_slew)
{
  wire_delay = 0.0;
  load_slew = 0.0;
}

float
UnitDelayCalc::ceff(const TimingArc *,
		    const Slew &,
		    float,
		    const Parasitic *,
		    float,
		    const Pvt *,
		    const DcalcAnalysisPt *)
{
  return 0.0;
}

string
UnitDelayCalc::reportGateDelay(const TimingArc *,
			       const Slew &,
			       float,
			       const Parasitic *,
			       float,
			       const Pvt *,
			       const DcalcAnalysisPt *,
			       int)
{
  string result("Delay = 1.0\n");
  result += "Slew = 0.0\n";
  return result;
}

void
UnitDelayCalc::checkDelay(const TimingArc *,
			  const Slew &,
			  const Slew &,
			  float,
			  const Pvt *,
			  const DcalcAnalysisPt *,
			  // Return values.
			  ArcDelay &margin)
{
  margin = units_->timeUnit()->scale();
}

string
UnitDelayCalc::reportCheckDelay(const TimingArc *,
				const Slew &,
				const char *,
				const Slew &,
				float,
				const Pvt *,
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
