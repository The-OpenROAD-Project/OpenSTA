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

#pragma once

#include "StringSeq.hh"

namespace sta {

class ArcDelayCalc;
class StaState;

typedef ArcDelayCalc *(*MakeArcDelayCalc)(StaState *sta);

// Register builtin delay calculators.
void
registerDelayCalcs();
// Register a delay calculator for the set_delay_calc command.
void
registerDelayCalc(const char *name,
		  MakeArcDelayCalc maker);
bool
isDelayCalcName(const char *name);
// Caller owns return value.
StringSeq *
delayCalcNames();
void
deleteDelayCalcs();

// Make a registered delay calculator by name.
ArcDelayCalc *
makeDelayCalc(const char *name,
	      StaState *sta);

} // namespace
