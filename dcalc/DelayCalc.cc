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

#include "DelayCalc.hh"

#include "Map.hh"
#include "StringUtil.hh"
#include "UnitDelayCalc.hh"
#include "LumpedCapDelayCalc.hh"
#include "DmpDelayCalc.hh"
#include "ArnoldiDelayCalc.hh"
#include "CcsCeffDelayCalc.hh"
#include "PrimaDelayCalc.hh"

namespace sta {

typedef Map<const char*, MakeArcDelayCalc, CharPtrLess> DelayCalcMap;

static DelayCalcMap *delay_calcs = nullptr;

void
registerDelayCalcs()
{
  registerDelayCalc("unit", makeUnitDelayCalc);
  registerDelayCalc("lumped_cap", makeLumpedCapDelayCalc);
  registerDelayCalc("dmp_ceff_elmore", makeDmpCeffElmoreDelayCalc);
  registerDelayCalc("dmp_ceff_two_pole", makeDmpCeffTwoPoleDelayCalc);
  registerDelayCalc("arnoldi", makeArnoldiDelayCalc);
  registerDelayCalc("ccs_ceff", makeCcsCeffDelayCalc);
  registerDelayCalc("prima", makePrimaDelayCalc);
}

void
registerDelayCalc(const char *name,
		  MakeArcDelayCalc maker)
{
  if (delay_calcs == nullptr)
    delay_calcs = new DelayCalcMap;
  (*delay_calcs)[name] = maker;
}

void
deleteDelayCalcs()
{
  delete delay_calcs;
  delay_calcs = nullptr;
}

ArcDelayCalc *
makeDelayCalc(const char *name,
	      StaState *sta)
{
  MakeArcDelayCalc maker = delay_calcs->findKey(name);
  if (maker)
    return maker(sta);
  else
    return nullptr;
}

bool
isDelayCalcName(const char *name)
{
  return delay_calcs->hasKey(name);
}

StringSeq
delayCalcNames()
{
  StringSeq names;
  for (const auto [name, make_dcalc] : *delay_calcs)
    names.push_back(name);
  return names;
}

} // namespace
