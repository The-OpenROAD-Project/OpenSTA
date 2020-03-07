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
#include "TimingModel.hh"
#include "TimingArc.hh"
#include "GraphDelayCalc.hh"
#include "ArcDelayCalc.hh"

namespace sta {

ArcDelayCalc::ArcDelayCalc(StaState *sta):
  StaState(sta)
{
}

TimingModel *
ArcDelayCalc::model(TimingArc *arc,
		    const DcalcAnalysisPt *dcalc_ap) const
{
  const OperatingConditions *op_cond = dcalc_ap->operatingConditions();
  TimingArc *corner_arc = arc->cornerArc(dcalc_ap->libertyIndex());
  return corner_arc->model(op_cond);
}

GateTimingModel *
ArcDelayCalc::gateModel(TimingArc *arc,
			const DcalcAnalysisPt *dcalc_ap) const
{
  return dynamic_cast<GateTimingModel*>(model(arc, dcalc_ap));
}

CheckTimingModel *
ArcDelayCalc::checkModel(TimingArc *arc,
			 const DcalcAnalysisPt *dcalc_ap) const
{
  return dynamic_cast<CheckTimingModel*>(model(arc, dcalc_ap));
}

} // namespace
