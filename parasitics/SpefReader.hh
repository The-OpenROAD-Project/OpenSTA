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

#include "Zlib.hh"
#include "MinMax.hh"
#include "ParasiticsClass.hh"

namespace sta {

class ParasiticAnalysisPt;
class Instance;
class Corner;
class OperatingConditions;
class StaState;

// Read a file single value parasitics into analysis point ap.
// In a Spef file with triplet values the first value is used.
// Constraint min/max cnst_min_max and operating condition op_cond
// are used for parasitic network reduction.
// Return true if successful.
bool
readSpefFile(const char *filename,
	     Instance *instance,
	     ParasiticAnalysisPt *ap,
	     bool pin_cap_included,
	     bool keep_coupling_caps,
	     float coupling_cap_factor,
	     bool reduce,
	     const Corner *corner,
	     const MinMaxAll *min_max,
	     StaState *sta);

} // namespace
