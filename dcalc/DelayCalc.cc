// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#include "DelayCalc.hh"

#include <map>
#include <string>

#include "ArnoldiDelayCalc.hh"
#include "CcsCeffDelayCalc.hh"
#include "ContainerHelpers.hh"
#include "DmpDelayCalc.hh"
#include "LumpedCapDelayCalc.hh"
#include "PrimaDelayCalc.hh"
#include "StringUtil.hh"
#include "UnitDelayCalc.hh"

namespace sta {

using DelayCalcMap = std::map<std::string, MakeArcDelayCalc, std::less<>>;

static DelayCalcMap delay_calcs;

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
registerDelayCalc(std::string_view name,
                  MakeArcDelayCalc maker)
{
  delay_calcs[std::string(name)] = maker;
}

void
deleteDelayCalcs()
{
  delay_calcs.clear();
}

ArcDelayCalc *
makeDelayCalc(const std::string_view name,
              StaState *sta)
{
  MakeArcDelayCalc maker = findStringKey(delay_calcs, name);
  if (maker)
    return maker(sta);
  else
    return nullptr;
}

bool
isDelayCalcName(std::string_view name)
{
  return delay_calcs.contains(name);
}

StringSeq
delayCalcNames()
{
  StringSeq names;
  for (const auto &[name, make_dcalc] : delay_calcs)
    names.push_back(name);
  return names;
}

} // namespace sta
