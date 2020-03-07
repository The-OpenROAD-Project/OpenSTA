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

#include "StaState.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"

namespace sta {

InstanceSet *
findRegInstances(ClockSet *clks, const RiseFallBoth *clk_rf,
		 bool edge_triggered, bool latches, StaState *sta);
PinSet *
findRegDataPins(ClockSet *clks, const RiseFallBoth *clk_rf,
		bool edge_triggered, bool latches, StaState *sta);
PinSet *
findRegClkPins(ClockSet *clks, const RiseFallBoth *clk_rf,
	       bool edge_triggered, bool latches, StaState *sta);
PinSet *
findRegAsyncPins(ClockSet *clks, const RiseFallBoth *clk_rf,
		 bool edge_triggered, bool latches, StaState *sta);
PinSet *
findRegOutputPins(ClockSet *clks, const RiseFallBoth *clk_rf,
		  bool edge_triggered, bool latches, StaState *sta);

void
initPathSenseThru();

} // namespace
