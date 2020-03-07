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
#include "Units.hh"
#include "UnitDelayCalc.hh"

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

void
UnitDelayCalc::inputPortDelay(const Pin *,
			      float,
			      const RiseFall *,
			      Parasitic *,
			      const DcalcAnalysisPt *)
{
}

void
UnitDelayCalc::gateDelay(const LibertyCell *,
			 TimingArc *,
			 const Slew &,
			 float,
			 Parasitic *,
			 float,
			 const Pvt *, const DcalcAnalysisPt *,
			 // Return values.
			 ArcDelay &gate_delay, Slew &drvr_slew)
{
  gate_delay = units_->timeUnit()->scale();
  drvr_slew = 0.0;
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
UnitDelayCalc::ceff(const LibertyCell *,
		    TimingArc *,
		    const Slew &,
		    float,
		    Parasitic *,
		    float,
		    const Pvt *,
		    const DcalcAnalysisPt *)
{
  return 0.0;
}

void
UnitDelayCalc::reportGateDelay(const LibertyCell *,
			       TimingArc *,
			       const Slew &,
			       float,
			       Parasitic *,
			       float,
			       const Pvt *,
			       const DcalcAnalysisPt *,
			       int,
			       string *result)
{
  *result += "Delay = 1.0\n";
  *result += "Slew = 0.0\n";
}

void
UnitDelayCalc::checkDelay(const LibertyCell *,
			  TimingArc *,
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

void
UnitDelayCalc::reportCheckDelay(const LibertyCell *,
				TimingArc *,
				const Slew &,
				const char *,
				const Slew &,
				float,
				const Pvt *,
				const DcalcAnalysisPt *,
				int,
				string *result)
{
  *result += "Check = 1.0\n";
}

void
UnitDelayCalc::finishDrvrPin()
{
}

} // namespace
