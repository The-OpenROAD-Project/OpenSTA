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

#include "NetworkClass.hh"
#include "SdcClass.hh"

namespace sta {

class Corner;
class Parasitic;
class ParasiticAnalysisPt;
class StaState;

// Reduce parasitic network to pi elmore model for drvr_pin.
Parasitic *
reduceToPiElmore(const Parasitic *parasitic_network,
		 const Pin *drvr_pin,
                 const RiseFall *rf,
		 float coupling_cap_factor,
		 const Corner *corner,
		 const MinMax *min_max,
		 const ParasiticAnalysisPt *ap,
		 StaState *sta);

// Reduce parasitic network to pi and 2nd order pole/residue models
// for drvr_pin.
Parasitic *
reduceToPiPoleResidue2(const Parasitic *parasitic_network,
		       const Pin *drvr_pin,
                       const RiseFall *rf,
		       float coupling_cap_factor,
		       const Corner *corner,
		       const MinMax *min_max,
		       const ParasiticAnalysisPt *ap,
		       StaState *sta);

} // namespace
